// Copyright © 2026 Khrustal & Mann
//              MELBOURNE, VICTORIA, AUSTRALIA, 3000
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
// MsgFieldRef.hpp
//
// Field access by name over any P3PmsgItem, in the two forms C++ allows:
//
//     Field ( item, L"device" ) = L"sensor-04";      // name known at run time
//
//     struct Telemetry : MsgView                      // names known at compile
//     {                                               // time: a TYPED VIEW
//         MSG_FIELD ( device, std::wstring );
//         MSG_FIELD ( uptime, int );
//     };
//     MsgViewOf<Telemetry> msg ( item );
//     msg->device = L"sensor-04";
//     msg->uptime = 86400;
//     int up      = msg->uptime;
//
// `msg->unknownName` cannot be made to work: C++ has no hook that turns an
// undeclared member name into a lookup. That is what the dynamic form is for.
// Refer MsgFieldAccessPlan.md at the MSCS root.
//
// HEADER-ONLY, AND THAT IS THE POINT. Nothing here is exported, no exported
// class gains a member, so the Msgcore DLL's layout is untouched and nothing
// downstream (Targetcore -> TargetFacade -> TargetCom) has to be rebuilt to
// pick it up. Nothing past C++14 is used: the .vcxproj consumers build at
// C++17 (stdcpp17 is the floor across the tree) and the CMake build at C++23,
// so std::span/std::byte-style C++20 types are out of reach.
//
// WHAT IT GUARDS AGAINST, each measured, not supposed:
//
//   * THE (const void*) OVERLOAD TRAP. P3PmsgData overloads on LPCSTR, LPCWSTR
//     and const void*; a char* or wchar_t* silently picks a STRING constructor
//     whose length is in characters (TargetFacade FacadeHub.cpp: 26 bytes came
//     back as 52). So there is no template operator=; every type has its own,
//     and a blob is always built through (const void*) with an explicit tag.
//   * NARROW LITERALS. `ref = "assign this"` is UTF-8 by decision and is stored
//     as UTF-16 (WSTR16), so a reader can always ask for text the same way.
//     c_str()/c_wstr() throw on the wrong tag, so mixing narrow and wide
//     storage for one field would be a trap for every reader.
//   * CURSOR LIFETIME. DeclareItem and SelectItem hand back a reference to the
//     parent's CURSOR item, and the next lookup on that parent moves the cursor
//     -- the reference silently starts naming another item. A ref therefore
//     holds NAMES, never item references, and re-resolves from its anchor on
//     every read and write. That costs a cursor walk per access; it is the
//     price of `msg->x` syntax. (An F0 probe that held one across a later
//     lookup tripped P3PmsgObject::AssertCommon at scope exit.)
//   * UNALIGNED BLOBS. Payloads sit at arbitrary offsets in a pack(1) image, so
//     every read copies out through c_vBlobCopy; nothing reads through
//     c_vBlob()'s pointer.
//   * THE NAME CAP. A Msgcore name holds 63 UTF-16 units. MSG_FIELD checks that
//     at compile time; the dynamic form checks before every write, so the
//     refusal names the field and touches nothing (CheckName).
//
// NOT THREAD-SAFE, exactly as the item underneath is not.
//
// TWO ENCODINGS. MsgFieldCoding::Typed (the default) stores each type under
// its own tag -- INT32, INT64, DOUBLE, BOOL, WSTR16, BLOB16. MsgFieldCoding::
// Bytes stores EVERY value as a blob of its native bytes, which is how
// TargetFacade carries named fields on the wire (FacadeHub::PostMsg); see
// "THE BYTES ENCODING" below. The readers accept both, so a reader never has
// to know which one a writer used.

#pragma once

#include "P2Pmsg.h"
#include "Msgexception.h"

#include <cstring>
#include <cwchar>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// MsgBlob -- an opaque byte value
//
// A value type rather than std::span<const std::byte>, which is C++20 and so
// out of reach of the C++17 projects that include this. Owning, so a read hands
// back bytes that outlive the cursor they came from.
// ---------------------------------------------------------------------------
struct MsgBlob
{
    std::vector<unsigned char> bytes;

    MsgBlob ( ) { }
    MsgBlob ( const void *pv, size_t cb )
    {
        if ( pv && cb )
          bytes.assign ( (const unsigned char*)pv, (const unsigned char*)pv + cb );
    }
    explicit MsgBlob ( std::vector<unsigned char> v ) : bytes ( std::move ( v ) ) { }

    const unsigned char* data ( ) const { return bytes.empty() ? nullptr : &bytes[0]; }
    size_t               size ( ) const { return bytes.size(); }
    bool                 empty( ) const { return bytes.empty(); }

    bool operator == ( const MsgBlob& rhs ) const { return bytes == rhs.bytes; }
    bool operator != ( const MsgBlob& rhs ) const { return bytes != rhs.bytes; }
};

// ---------------------------------------------------------------------------
// THE BYTES ENCODING
//
// What a field's value looks like when it travels as a blob. It is
// TargetFacade's field format, made explicit -- the facade only ever moved
// bytes, so this is the first place the bytes are given a meaning:
//
//     int        4 bytes, native (little-endian on every target this builds for)
//     long long  8 bytes, native
//     double     8 bytes, IEEE 754, native
//     bool       1 byte, 0 or 1
//     text       UTF-16 code units, native, WITH a terminating NUL unit --
//                the shape IP2PMessage::SetFieldText writes, so a facade
//                client's fieldText() reads it unchanged
//     blob       the bytes, verbatim
//
// TargetFacadeFn.hpp carries the same table for facade clients, which cannot
// include this header. Change one and you must change the other.
// ---------------------------------------------------------------------------
enum class MsgFieldCoding { Typed, Bytes };

namespace MsgFieldDetail {

inline bool IsBlobTag ( UCHAR t ) { return t >= VBLockData_BLOB08 && t <= VBLockData_BLOB32var; }
inline bool IsWstrTag ( UCHAR t ) { return t >= VBLockData_WSTR08 && t <= VBLockData_WSTR32var; }

//  UTF-8 -> wchar_t (UTF-16 on Windows, UTF-32 elsewhere). Malformed input
//  becomes U+FFFD rather than an exception: a narrow literal in source code is
//  what this is for, and a bad byte in one is not worth losing the write over.
inline std::wstring Utf8ToWide ( const char *s )
{
    std::wstring out;
    if ( !s ) return out;
    const unsigned char *p = (const unsigned char*)s;
    while ( *p )
    {
        unsigned c  = *p++;
        unsigned cp = 0xFFFD;
        int      n  = -1;
        if      ( c < 0x80 )            { cp = c;        n = 0; }
        else if ( ( c & 0xE0 ) == 0xC0 ) { cp = c & 0x1F; n = 1; }
        else if ( ( c & 0xF0 ) == 0xE0 ) { cp = c & 0x0F; n = 2; }
        else if ( ( c & 0xF8 ) == 0xF0 ) { cp = c & 0x07; n = 3; }
        for ( int i = 0; i < n; ++i )
        {
            // A missing continuation byte is not consumed: it may be the NUL.
            if ( ( *p & 0xC0 ) != 0x80 ) { n = -1; break; }
            cp = ( cp << 6 ) | ( *p++ & 0x3F );
        }
        if ( n < 0 ||
             ( n == 1 && cp < 0x80 ) || ( n == 2 && cp < 0x800 ) ||
             ( n == 3 && cp < 0x10000 ) ||
             cp > 0x10FFFF || ( cp >= 0xD800 && cp <= 0xDFFF ) )
          cp = 0xFFFD;
        if ( sizeof(wchar_t) == 2 && cp > 0xFFFF )
        {
            cp -= 0x10000;
            out += (wchar_t)( 0xD800 + ( cp >> 10 ) );
            out += (wchar_t)( 0xDC00 + ( cp & 0x3FF ) );
        }
        else
          out += (wchar_t)cp;
    }
    return out;
}

//  The UTF-16 helpers below do what Platform/p2pstr.h's p2p_wide_units /
//  p2p_store_wide / p2p_load_wide do for c_wcscpy -- surrogate pairs where
//  wchar_t is 32-bit, byte-wise moves because a unit may sit at an odd offset
//  -- and are repeated here rather than called, because a consumer gets
//  P2PWCHAR from P2PmsgVBLock.h without the Platform layer, and a header that
//  only compiled with the right stdafx would be a trap (measured: every
//  _Msgcore_UseExamples harness has exactly that stdafx).

//  How many UTF-16 units n wchar_t elements need.
inline size_t WideUnits ( const wchar_t *s, size_t n )
{
    size_t m = n;
    if ( sizeof(wchar_t) > 2 )
      for ( size_t i = 0; i < n; ++i )
        if ( (unsigned long)s[i] > 0xFFFF ) ++m;
    return m;
}

//  wchar_t text -> UTF-16 units, as bytes, with the terminating NUL unit.
inline std::vector<unsigned char> TextToUnits ( const std::wstring& s )
{
    std::vector<unsigned char> out ( ( WideUnits ( s.c_str(), s.size() ) + 1 ) * 2, 0 );
    size_t o = 0;
    for ( size_t i = 0; i < s.size(); ++i )
    {
        unsigned long cp = (unsigned long)s[i];
        P2PWCHAR u[2];
        int      n = 1;
        if ( sizeof(wchar_t) > 2 && cp > 0xFFFF )
        {
            if ( cp > 0x10FFFF ) u[0] = (P2PWCHAR)0xFFFD;
            else
            {
                cp -= 0x10000;
                u[0] = (P2PWCHAR)( 0xD800 + ( cp >> 10 ) );
                u[1] = (P2PWCHAR)( 0xDC00 + ( cp & 0x3FF ) );
                n = 2;
            }
        }
        else
          u[0] = (P2PWCHAR)cp;
        std::memcpy ( &out[o], u, n * sizeof(P2PWCHAR) );
        o += n * sizeof(P2PWCHAR);
    }
    return out;
}

//  UTF-16 units (any alignment) -> wchar_t text.
inline std::wstring UnitsToText ( const unsigned char *pb, size_t nUnits )
{
    std::wstring out;
    out.reserve ( nUnits );
    for ( size_t i = 0; i < nUnits; ++i )
    {
        P2PWCHAR u; std::memcpy ( &u, pb + i * sizeof u, sizeof u );
        if ( sizeof(wchar_t) > 2 && u >= 0xD800 && u <= 0xDBFF && i + 1 < nUnits )
        {
            P2PWCHAR lo; std::memcpy ( &lo, pb + ( i + 1 ) * sizeof lo, sizeof lo );
            if ( lo >= 0xDC00 && lo <= 0xDFFF )
            {
                out += (wchar_t)( 0x10000 + ( ( (unsigned long)( u - 0xD800 ) << 10 )
                                              | (unsigned long)( lo - 0xDC00 ) ) );
                ++i;
                continue;
            }
        }
        out += (wchar_t)u;
    }
    return out;
}

} // namespace MsgFieldDetail

// ---------------------------------------------------------------------------
// MsgFieldAnchor -- where a ref's PARENT item comes from, and on what terms
//
// A plain value: function pointers, a context and a path of names, copied into
// every ref. The context is NOT owned -- whatever it points at (the item, the
// message) must outlive every ref and view made from it. The path IS owned.
//
//   pfnResolve  the parent item, looked up afresh; NULL when bCreate is false
//               and it does not exist yet. Must not hand back a reference that
//               a later lookup on some OTHER object can retarget -- which is
//               why it is a function and not a P3PmsgItem&.
//   aPath       names walked DOWN from that item, by name, on every access --
//               Child() builds it. Empty for Of(). A write creates what is
//               missing; a read creates nothing.
//   eCoding     how values are stored under it.
//   pfnAdmit    optional. Called before any write with each name this write
//               will create directly under the parent (bLeaf true for the
//               field itself, false for the first segment of a nested path);
//               throws to refuse. cbValue is the encoded size of a leaf.
//   pfnIndex    optional. Told after a leaf directly under the parent was
//               written (bAdded true) or erased (false).
// ---------------------------------------------------------------------------
struct MsgFieldAnchor
{
    typedef P3PmsgItem* (*ResolveFn) ( void *pvCtx, bool bCreate );
    typedef void        (*AdmitFn)   ( void *pvCtx, LPCWSTR lpszName
                                     , size_t cbValue, bool bLeaf );
    typedef void        (*IndexFn)   ( void *pvCtx, LPCWSTR lpszName, bool bAdded );

    ResolveFn      pfnResolve = nullptr;
    void          *pvCtx      = nullptr;
    MsgFieldCoding eCoding    = MsgFieldCoding::Typed;
    AdmitFn        pfnAdmit   = nullptr;
    IndexFn        pfnIndex   = nullptr;
    std::vector<std::wstring> aPath;

    // The common case: an item the CALLER holds still -- a P3PmsgField it
    // owns, or a P3PmsgBSTR's r_item(VBLockBSTR_ROOT), which is a member and
    // not a cursor. Not something SelectItem just returned: for a child item,
    // use Child().
    static MsgFieldAnchor Of ( P3PmsgItem& item
                             , MsgFieldCoding eCoding = MsgFieldCoding::Typed )
    {
        MsgFieldAnchor a;
        a.pfnResolve = &ResolveItem;
        a.pvCtx      = &item;
        a.eCoding    = eCoding;
        return a;
    }

    // A NAMED CHILD of an anchor, found afresh by name on every access:
    //
    //     MsgViewOf<Limits> lim ( MsgFieldAnchor::Child ( oMgr, L"Limits" ) );
    //     lim->Low = -40;                      // creates Limits on the write
    //
    // The tempting `MsgViewOf<Limits> lim ( oMgr.SelectItem(L"Limits") )` binds
    // the parent's CURSOR item, and the next lookup on the parent retargets it.
    // Chains: Child ( Child ( oRoot, L"a" ), L"b" ). Keeps the parent's coding
    // and hooks; a hook sees the FIRST name below its own parent, as it does
    // for `Field(anchor, L"a")[L"b"]`.
    static MsgFieldAnchor Child ( const MsgFieldAnchor& parent, LPCWSTR lpszName )
    {
        MsgFieldAnchor a ( parent );
        a.aPath.push_back ( lpszName ? lpszName : L"" );
        return a;
    }
    static MsgFieldAnchor Child ( P3PmsgItem& parent, LPCWSTR lpszName
                                , MsgFieldCoding eCoding = MsgFieldCoding::Typed )
    {
        return Child ( Of ( parent, eCoding ), lpszName );
    }

  private:
    static P3PmsgItem* ResolveItem ( void *pvCtx, bool ) { return (P3PmsgItem*)pvCtx; }
};

// ---------------------------------------------------------------------------
// MsgFieldRef -- one named field, written on operator=, read on As*()
// ---------------------------------------------------------------------------
class MsgFieldRef
{
  public:
    MsgFieldRef ( P3PmsgItem& parent, LPCWSTR lpszName )
      : m_oAnchor ( MsgFieldAnchor::Of ( parent ) ), m_strName ( Safe ( lpszName ) ) { }
    // The anchor's path becomes the head of this ref's own, so a Child()
    // anchor and `[L"child"]` nesting are one mechanism: the walk, the name
    // checks, create-on-write and the hooks all see a single path.
    MsgFieldRef ( const MsgFieldAnchor& anchor, LPCWSTR lpszName )
      : m_oAnchor ( anchor ), m_aPath ( anchor.aPath ), m_strName ( Safe ( lpszName ) )
    {
        m_oAnchor.aPath.clear ( );
    }

    MsgFieldRef ( const MsgFieldRef& ) = default;

    // `a = b` between two refs would have to choose between copying the
    // binding and copying the value, and either choice surprises someone.
    // Say which: `a = b.AsInt()`.
    MsgFieldRef& operator = ( const MsgFieldRef& ) = delete;

    // --- writes: create-or-replace, type and all ---------------------------
    // DeclareItem ( name, data, TRUE ) replaces the TAG as well as the value
    // (F0, measured), so a field may change type between writes.
    //
    // One overload per type, deliberately: see the overload trap above.
    // Anything that converts to two of them equally well (an unsigned, a
    // long) is a compile error rather than a guess -- cast it.
    MsgFieldRef& operator = ( int v )
    {
        if ( Bytes() ) { INT32 x = (INT32)v; WriteBlob ( &x, sizeof x ); }
        else           Write ( P3PmsgData ( (INT32)v ), sizeof(INT32) );
        return *this;
    }
    MsgFieldRef& operator = ( long long v )
    {
        if ( Bytes() ) { INT64 x = (INT64)v; WriteBlob ( &x, sizeof x ); }
        else           Write ( P3PmsgData ( (INT64)v ), sizeof(INT64) );
        return *this;
    }
    MsgFieldRef& operator = ( double v )
    {
        if ( Bytes() ) WriteBlob ( &v, sizeof v );
        else           Write ( P3PmsgData ( v ), sizeof(double) );
        return *this;
    }
    MsgFieldRef& operator = ( bool v )
    {
        if ( Bytes() ) { unsigned char x = v ? 1 : 0; WriteBlob ( &x, 1 ); }
        else           Write ( P3PmsgData ( v ), 1 );
        return *this;
    }
    MsgFieldRef& operator = ( LPCWSTR v )
    {
        return *this = std::wstring ( v ? v : L"" );
    }
    MsgFieldRef& operator = ( const std::wstring& v )
    {
        if ( Bytes() )
        {
            std::vector<unsigned char> units = MsgFieldDetail::TextToUnits ( v );
            WriteBlob ( &units[0], units.size() );
        }
        else
        {
            // LPCWSTR with an explicit length: WSTR16, not the LPCSTR or
            // (const void*) overload, and an embedded NUL is not a terminator.
            // A zero length means "measure it" to P3PmsgData, which for an
            // empty string measures zero -- the right answer either way.
            Write ( P3PmsgData ( (LPCWSTR)v.c_str(), v.size() )
                  , MsgFieldDetail::WideUnits ( v.c_str(), v.size() ) * sizeof(P2PWCHAR) );
        }
        return *this;
    }
    MsgFieldRef& operator = ( const char *utf8 )
    {
        return *this = MsgFieldDetail::Utf8ToWide ( utf8 );
    }
    MsgFieldRef& operator = ( const MsgBlob& v )
    {
        WriteBlob ( v.data(), v.size() );
        return *this;
    }

    // --- reads: throw P2Pevent* on a missing field or a wrong type ---------
    // Each accepts its own type under either encoding: the typed tag, or a
    // blob of exactly the encoded size.
    int AsInt ( ) const
    {
        const P3PmsgData& d = Cell();
        if ( d.DataType() == VBLockData_INT32 ) return d.c_int();
        INT32 x = 0; ReadExact ( d, &x, sizeof x, L"an int" ); return (int)x;
    }
    long long AsInt64 ( ) const
    {
        const P3PmsgData& d = Cell();
        if ( d.DataType() == VBLockData_INT64 ) return (long long)d.c_int64();
        INT64 x = 0; ReadExact ( d, &x, sizeof x, L"a 64-bit int" ); return (long long)x;
    }
    double AsReal ( ) const
    {
        const P3PmsgData& d = Cell();
        if ( d.DataType() == VBLockData_DOUBLE ) return d.c_double();
        double x = 0; ReadExact ( d, &x, sizeof x, L"a double" ); return x;
    }
    bool AsBool ( ) const
    {
        const P3PmsgData& d = Cell();
        if ( d.DataType() == VBLockData_BOOL ) return d.c_bool();
        unsigned char x = 0; ReadExact ( d, &x, 1, L"a bool" ); return x != 0;
    }
    std::wstring AsText ( ) const
    {
        // Through the stored UNITS, not c_wstr(): on Windows c_wstr() returns
        // a pointer into the image whose end is the stored size, not a NUL,
        // and on Linux it is one slot of a scratch ring. The units and their
        // count are the facts; the decode is UnitsToText.
        const P3PmsgData& d = Cell();
        const UCHAR t = d.DataType();
        if ( !MsgFieldDetail::IsWstrTag ( t ) && !MsgFieldDetail::IsBlobTag ( t ) )
          Fail ( L"Field [%ls] is not text" );
        std::vector<unsigned char> b = Copy ( d );
        if ( b.size() % sizeof(P2PWCHAR) )
          Fail ( L"Field [%ls] is not text: an odd number of bytes" );
        size_t nUnits = b.size() / sizeof(P2PWCHAR);
        // The bytes encoding stores the terminator; the typed one does not.
        if ( MsgFieldDetail::IsBlobTag ( t ) && nUnits &&
             b[ b.size() - 1 ] == 0 && b[ b.size() - 2 ] == 0 )
          --nUnits;
        return MsgFieldDetail::UnitsToText ( b.empty() ? nullptr : &b[0], nUnits );
    }
    MsgBlob AsBlob ( ) const
    {
        const P3PmsgData& d = Cell();
        if ( !MsgFieldDetail::IsBlobTag ( d.DataType() ) )
          Fail ( L"Field [%ls] is not a blob" );
        return MsgBlob ( Copy ( d ) );
    }

    // The stored tag (VBLockData_*). Throws if the field does not exist.
    UCHAR DataType ( ) const { return Cell().DataType(); }

    bool Exists ( ) const
    {
        P3PmsgItem *p = Parent ( false );
        return p && p->Exists ( m_strName.c_str() );
    }

    // Removes the field and everything under it. False if it was not there.
    bool Erase ( )
    {
        P3PmsgItem *p = Parent ( false );
        if ( !p || !p->Delete ( m_strName.c_str() ) )
          return false;
        if ( m_aPath.empty() && m_oAnchor.pfnIndex )
          m_oAnchor.pfnIndex ( m_oAnchor.pvCtx, m_strName.c_str(), false );
        return true;
    }

    // Nesting: `Field(item, L"pos")[L"x"] = 1.5;` creates `pos` on the write.
    // Reads create nothing.
    MsgFieldRef operator [] ( LPCWSTR lpszChild ) const
    {
        MsgFieldRef r ( *this );
        r.m_aPath.push_back ( m_strName );
        r.m_strName = Safe ( lpszChild );
        return r;
    }

    // This field as an ANCHOR, for a view of its children:
    //     MsgViewOf<Pos> pos ( Field ( oRoot, L"body" )[L"pos"].Anchor() );
    // The same thing MsgFieldAnchor::Child builds, name by name.
    MsgFieldAnchor Anchor ( ) const
    {
        MsgFieldAnchor a ( m_oAnchor );
        a.aPath = m_aPath;
        a.aPath.push_back ( m_strName );
        return a;
    }

    const std::wstring& Name ( ) const { return m_strName; }

  private:
    static std::wstring Safe ( LPCWSTR lpsz ) { return lpsz ? lpsz : L""; }

    bool Bytes ( ) const { return m_oAnchor.eCoding == MsgFieldCoding::Bytes; }

    // P2Pevent::Throw throws; the loop is for the compiler, which cannot know.
    [[noreturn]] void Fail ( LPCWSTR lpszFormat ) const
    {
        EVERR -> Module ( L"MsgFieldRef" )
              -> Message ( lpszFormat, m_strName.c_str() )
              -> Throw ( );
        for ( ;; ) { }
    }

    P3PmsgItem* Parent ( bool bCreate ) const
    {
        if ( !m_oAnchor.pfnResolve )
          Fail ( L"Field [%ls] belongs to a view that was never bound to an item" );
        P3PmsgItem *p = m_oAnchor.pfnResolve ( m_oAnchor.pvCtx, bCreate );
        for ( size_t i = 0; p && i < m_aPath.size(); ++i )
        {
            LPCWSTR lpszSeg = m_aPath[i].c_str();
            if ( !p->Exists ( lpszSeg ) )
            {
              if ( !bCreate )
                return nullptr;
              p->DeclareItem ( lpszSeg, P3PmsgData() );
            }
            // SelectItem's answer is p's cursor item, and nothing moves p's
            // cursor before the next step reads it -- the walk only ever
            // looks DOWN.
            p = &p->SelectItem ( lpszSeg );
        }
        return p;
    }

    const P3PmsgData& Cell ( ) const
    {
        P3PmsgItem *p = Parent ( false );
        if ( !p || !p->Exists ( m_strName.c_str() ) )
          Fail ( L"Field [%ls] does not exist" );
        return p->SelectItem ( m_strName.c_str() ).r_data();
    }

    // Refused HERE, before anything is constructed, so the error names the
    // field and the tree is never touched. Until 2026-10-02 this was also the
    // only thing standing between an overlong name and heap corruption: the
    // P3PmsgField(name, data) constructor DeclareItem builds aliased its
    // m_pObject onto its own member and THEN called c_name(), and the unwind
    // deleted that member address. Msgcore now undoes the alias on the way out
    // (P2Pmsg.cpp, P3PmsgField_UNDO_RENDER), so this is a nicer message, not a
    // safety net. Reads construct nothing, so they need no check.
    void CheckName ( LPCWSTR lpszName ) const
    {
        if ( MsgFieldDetail::WideUnits ( lpszName, std::wcslen ( lpszName ) ) > 63 )
          EVERR -> Module ( L"MsgFieldRef" )
                -> Message ( L"Field [%ls]: a Msgcore name holds at most 63 UTF-16 units"
                           , lpszName )
                -> Throw ( );
    }

    void Write ( const P3PmsgData& oData, size_t cbValue )
    {
        for ( size_t i = 0; i < m_aPath.size(); ++i )
          CheckName ( m_aPath[i].c_str() );
        CheckName ( m_strName.c_str() );
        if ( m_oAnchor.pfnAdmit )
        {
            if ( !m_aPath.empty() )
              m_oAnchor.pfnAdmit ( m_oAnchor.pvCtx, m_aPath[0].c_str(), 0, false );
            else
              m_oAnchor.pfnAdmit ( m_oAnchor.pvCtx, m_strName.c_str(), cbValue, true );
        }
        P3PmsgItem *p = Parent ( true );
        if ( !p )
          Fail ( L"Field [%ls] has no parent item to be written under" );
        p->DeclareItem ( m_strName.c_str(), oData, TRUE );
        // LAST: an index hook may move a cursor that `p` came from.
        if ( m_aPath.empty() && m_oAnchor.pfnIndex )
          m_oAnchor.pfnIndex ( m_oAnchor.pvCtx, m_strName.c_str(), true );
    }

    void WriteBlob ( const void *pv, size_t cb )
    {
        // (const void*) and an explicit tag, ALWAYS -- see the overload trap.
        // An empty blob still needs a non-NULL source to select that overload.
        static const unsigned char kEmpty = 0;
        Write ( P3PmsgData ( cb ? pv : (const void*)&kEmpty, (VBLsize)cb
                           , cb > 0xFFFF ? VBLockData_BLOB32 : VBLockData_BLOB16 )
              , cb );
    }

    static std::vector<unsigned char> Copy ( const P3PmsgData& d )
    {
        // c_size() is the STORED length for blob and string tags, not the
        // capacity (F0: a 5-byte blob rewritten as 3 reports 3).
        std::vector<unsigned char> b ( d.c_size() );
        if ( !b.empty() )
          b.resize ( d.c_vBlobCopy ( &b[0], b.size() ) );
        return b;
    }

    void ReadExact ( const P3PmsgData& d, void *pv, size_t cb, LPCWSTR ) const
    {
        if ( !MsgFieldDetail::IsBlobTag ( d.DataType() ) || d.c_size() != cb )
          Fail ( L"Field [%ls] holds a different type" );
        d.c_vBlobCopy ( pv, cb );
    }

    MsgFieldAnchor            m_oAnchor;
    std::vector<std::wstring> m_aPath;     // parent path below the anchor
    std::wstring              m_strName;
};

// The dynamic form.
inline MsgFieldRef Field ( P3PmsgItem& item, LPCWSTR lpszName )
{
    return MsgFieldRef ( item, lpszName );
}
inline MsgFieldRef Field ( const MsgFieldAnchor& anchor, LPCWSTR lpszName )
{
    return MsgFieldRef ( anchor, lpszName );
}

// ---------------------------------------------------------------------------
// Typed views
//
// A view is a struct of MSG_FIELD members deriving from MsgView. Each member
// is a proxy that knows its view and its own name; MsgViewOf<T> binds the view
// to an item and hands it out through operator->.
//
// Members accept only their declared type's family: an int field takes
// integers no wider than an int, a text field takes wide or UTF-8 strings, and
// `msg->uptime = L"x"` does not compile. bool is its own family both ways.
// ---------------------------------------------------------------------------
class MsgView
{
  public:
    MsgView ( ) { }
    MsgView             ( const MsgView& ) = delete;
    MsgView& operator = ( const MsgView& ) = delete;

    // The dynamic form, on the same item -- for the field the view does not
    // declare.
    MsgFieldRef operator [] ( LPCWSTR lpszName ) const { return Ref ( lpszName ); }
    MsgFieldRef Ref ( LPCWSTR lpszName ) const { return MsgFieldRef ( m_oAnchor, lpszName ); }

  private:
    template <class T> friend class MsgViewOf;
    MsgFieldAnchor m_oAnchor;
};

template <class T> struct MsgFieldTraits;   // undefined: an unsupported type

template <> struct MsgFieldTraits<int>
{
    template <class U> struct Accepts : std::integral_constant<bool,
        std::is_integral<U>::value && !std::is_same<U, bool>::value &&
        sizeof(U) <= sizeof(int)> { };
    static void Store ( MsgFieldRef& r, int v )   { r = v; }
    static int  Load  ( const MsgFieldRef& r )    { return r.AsInt(); }
};
template <> struct MsgFieldTraits<long long>
{
    template <class U> struct Accepts : std::integral_constant<bool,
        std::is_integral<U>::value && !std::is_same<U, bool>::value> { };
    static void      Store ( MsgFieldRef& r, long long v ) { r = v; }
    static long long Load  ( const MsgFieldRef& r )        { return r.AsInt64(); }
};
template <> struct MsgFieldTraits<double>
{
    template <class U> struct Accepts : std::integral_constant<bool,
        std::is_arithmetic<U>::value && !std::is_same<U, bool>::value> { };
    static void   Store ( MsgFieldRef& r, double v ) { r = v; }
    static double Load  ( const MsgFieldRef& r )     { return r.AsReal(); }
};
template <> struct MsgFieldTraits<bool>
{
    template <class U> struct Accepts : std::is_same<U, bool> { };
    static void Store ( MsgFieldRef& r, bool v )  { r = v; }
    static bool Load  ( const MsgFieldRef& r )    { return r.AsBool(); }
};
template <> struct MsgFieldTraits<std::wstring>
{
    template <class U> struct Accepts : std::integral_constant<bool,
        std::is_convertible<U, std::wstring>::value ||
        std::is_same<U, const char*>::value || std::is_same<U, char*>::value> { };
    static void Store ( MsgFieldRef& r, const std::wstring& v ) { r = v; }
    static void Store ( MsgFieldRef& r, const char *utf8 )      { r = utf8; }
    static std::wstring Load ( const MsgFieldRef& r )           { return r.AsText(); }
};
template <> struct MsgFieldTraits<MsgBlob>
{
    template <class U> struct Accepts : std::is_same<U, MsgBlob> { };
    static void    Store ( MsgFieldRef& r, const MsgBlob& v ) { r = v; }
    static MsgBlob Load  ( const MsgFieldRef& r )             { return r.AsBlob(); }
};

template <class T>
class MsgTypedField
{
  public:
    MsgTypedField ( const MsgView *pView, LPCWSTR lpszName )
      : m_pView ( pView ), m_lpszName ( lpszName ) { }

    // Not copyable: a copy would carry a pointer to the view it came from.
    MsgTypedField ( const MsgTypedField& ) = delete;

    template <class U, class = typename std::enable_if<
                 MsgFieldTraits<T>::template Accepts<typename std::decay<U>::type>::value>::type>
    MsgTypedField& operator = ( U&& v )
    {
        MsgFieldRef r = Ref();
        MsgFieldTraits<T>::Store ( r, std::forward<U>(v) );
        return *this;
    }

    // Everything else -- `msg->uptime = L"x"` -- stops here, with a sentence
    // rather than a page of overload candidates. Another field of the SAME
    // type is excluded so that it reaches the copy below.
    template <class U, class D = typename std::decay<U>::type
             , class = typename std::enable_if<
                 !MsgFieldTraits<T>::template Accepts<D>::value &&
                 !std::is_same<D, MsgTypedField>::value>::type
             , class = void>
    MsgTypedField& operator = ( U&& )
    {
        static_assert ( sizeof(U) == 0
                      , "MSG_FIELD: this value's type is not the field's declared type" );
        return *this;
    }

    // `msg->a = msg->b` copies the VALUE.
    MsgTypedField& operator = ( const MsgTypedField& rhs )
    {
        MsgFieldRef r = Ref();
        MsgFieldTraits<T>::Store ( r, rhs.Get() );
        return *this;
    }

    T    Get    ( ) const { return MsgFieldTraits<T>::Load ( Ref() ); }
    operator T  ( ) const { return Get(); }
    bool Exists ( ) const { return Ref().Exists(); }
    bool Erase  ( )       { return Ref().Erase(); }
    MsgFieldRef Ref ( ) const { return m_pView->Ref ( m_lpszName ); }

  private:
    const MsgView *m_pView;
    LPCWSTR        m_lpszName;      // a string literal, from MSG_FIELD
};

// One macro, because C++17 has no reflection to turn a member's name into a
// string. It takes the identifier once and makes both the member and its
// L"..." name, so the two cannot drift. Identifiers are ASCII, so characters
// and UTF-16 units are the same count on every platform.
#define MSG_FIELD(id, type)                                                     \
    static_assert ( sizeof ( L"" #id ) / sizeof ( wchar_t ) - 1 <= 63,         \
                    "MSG_FIELD(" #id "): a Msgcore name holds at most 63 "     \
                    "UTF-16 units" );                                           \
    ::MsgTypedField<type> id { this, L"" #id }

template <class T>
class MsgViewOf
{
    static_assert ( std::is_base_of<MsgView, T>::value
                  , "MsgViewOf<T>: T must derive from MsgView" );
  public:
    // The item must hold still for the life of the view -- see
    // MsgFieldAnchor::Of.
    explicit MsgViewOf ( P3PmsgItem& item, MsgFieldCoding eCoding = MsgFieldCoding::Typed )
    {
        static_cast<MsgView&>(m_oView).m_oAnchor = MsgFieldAnchor::Of ( item, eCoding );
    }
    explicit MsgViewOf ( const MsgFieldAnchor& anchor )
    {
        static_cast<MsgView&>(m_oView).m_oAnchor = anchor;
    }

    MsgViewOf             ( const MsgViewOf& ) = delete;
    MsgViewOf& operator = ( const MsgViewOf& ) = delete;

    T*       operator -> ( )       { return &m_oView; }
    const T* operator -> ( ) const { return &m_oView; }
    T&       operator *  ( )       { return m_oView; }
    const T& operator *  ( ) const { return m_oView; }

    MsgFieldRef operator [] ( LPCWSTR lpszName ) const { return m_oView.Ref ( lpszName ); }

  private:
    T m_oView;
};
