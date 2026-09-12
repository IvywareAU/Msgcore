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
// Msgcore_c.cpp
// Flat C wrapper implementation over the Msgcore C++ API.
// Add this file to the Msgcore VS project (Msgcore_EXPORTS defined in project properties).
//
#include "stdafx.h"
#include "Msgcore_c.h"
#include "P2PmsgMgr.h"
#include "MsgAttr.h"
#include "MsgDesc.h"
#include "MsgCurs.h"
#include "MsgList.h"
#include "MsgVect.h"
#include "MsgStck.h"

#include <string>
#include <cstring>
#include <cwchar>
#include <cstdio>
#include <map>
#include <mutex>

// ---------------------------------------------------------------------------
// Handle registry (SECURITY_REVIEW M1)
//
// Every handle this API hands to a C caller is recorded here on creation and
// forgotten on destroy; every entry point looks its raw void* up before using
// it. The load-bearing property is that the lookup NEVER DEREFERENCES the
// caller's pointer. A magic word in a wrapper struct would have to read through
// the pointer to check it, which is the defect itself rather than a fix for it:
// a hostile or merely stale value crashes (or worse) AT THE CHECK.
//
// This is the backport of the registry that closed M1 for TargetCore's flat C
// ABI in session 24. That surface has since been deleted from the tree; THIS one
// has not -- Msgcore_c is built into Msgcore.dll and still has consumers -- so
// the identical defect was still live here: nine helpers static_cast<>ing
// whatever void* arrived, spread over 217 entry points.
//
// The registry is TYPE-AWARE. All nine handle typedefs in the header are
// `void*`, so before this nothing stopped a caller spending a MsgCursHandle
// where a MsgMgrHandle was expected -- nine unrelated C++ classes with one ABI.
// Storing the kind alongside the key makes that a refusal rather than type
// confusion.
//
// std::map keyed by the address, exactly like the paging registry below
// (s_mapPaging / MsgcorePagingForget, called from msgcore_mgr_destroy): the key
// IS the handle, and the map's node stability means an entry never moves while
// it is registered.
//
// What this does NOT close, and cannot: a caller that destroys a handle on one
// thread while another thread is inside an entry point holding it. That window
// is only closable by refcounting every handle, which would change the ownership
// contract the header publishes. What it DOES close is the three holes reachable
// from a correct caller's mistake or from a hostile one -- a pointer we never
// issued, a pointer we issued and have since forgotten, and a pointer of the
// wrong kind.
//
// Cost: one uncontended mutex acquisition per handle argument, and the guard
// form spends a second lookup where the body uses the pointer again. That is
// tens of nanoseconds against an FFI transition, and it keeps every entry
// point's guard to a single line that cannot be got wrong halfway down a block
// of thirty similar-looking functions.
// ---------------------------------------------------------------------------
enum MsgcoreHandleKind_e
{
    MsgcoreHandle_Mgr = 1,
    MsgcoreHandle_Field,
    MsgcoreHandle_List,
    MsgcoreHandle_Vect,
    MsgcoreHandle_Attr,
    MsgcoreHandle_Desc,
    MsgcoreHandle_Curs,
    MsgcoreHandle_Stck,
    MsgcoreHandle_Recurs
};

static std::map<const void*, MsgcoreHandleKind_e> s_mapHandle;
static std::mutex                                 s_mtxHandle;

// Records a handle just produced and returns it, so a factory stays one line.
template <class T>
static T* MsgcoreHandleAdd(T* p, MsgcoreHandleKind_e eKind)
{
    if (!p) return nullptr;
    std::lock_guard<std::mutex> lock(s_mtxHandle);
    // Assignment rather than insert(): the allocator recycles addresses, and a
    // recycled address must take the NEW kind. insert() would silently keep the
    // dead entry's kind and turn address reuse into type confusion -- the exact
    // failure this registry exists to prevent. (The same hazard
    // MsgcorePagingForget is commented against below.)
    s_mapHandle[p] = eKind;
    return p;
}

// True only when h is a live handle of exactly eKind. Never dereferences it.
static bool MsgcoreHandleIs(const void* h, MsgcoreHandleKind_e eKind)
{
    if (!h) return false;
    std::lock_guard<std::mutex> lock(s_mtxHandle);
    std::map<const void*, MsgcoreHandleKind_e>::const_iterator it = s_mapHandle.find(h);
    return it != s_mapHandle.end() && it->second == eKind;
}

// Forgets a handle. Returns true ONLY for the caller that actually removed it,
// which is what makes destroy safe: a double destroy deletes once and refuses
// the second time, and two threads racing one destroy cannot both reach delete.
static bool MsgcoreHandleForget(const void* h, MsgcoreHandleKind_e eKind)
{
    if (!h) return false;
    std::lock_guard<std::mutex> lock(s_mtxHandle);
    std::map<const void*, MsgcoreHandleKind_e>::iterator it = s_mapHandle.find(h);
    if (it == s_mapHandle.end() || it->second != eKind) return false;
    s_mapHandle.erase(it);
    return true;
}

// Cast helpers. Each answers nullptr for anything that is not a LIVE handle of
// its own kind, so an entry point's guard reads `if (!toField(hField)) return 0;`
// and a bogus, dangling or wrong-typed handle takes the path a null one took.
static inline P2PmsgMgr*    toMgr  (MsgMgrHandle    h) { return MsgcoreHandleIs(h, MsgcoreHandle_Mgr)    ? static_cast<P2PmsgMgr*>(h)    : nullptr; }
static inline P3PmsgField*  toField(MsgFieldHandle  h) { return MsgcoreHandleIs(h, MsgcoreHandle_Field)  ? static_cast<P3PmsgField*>(h)  : nullptr; }
static inline P3PmsgList*   toList (MsgListHandle   h) { return MsgcoreHandleIs(h, MsgcoreHandle_List)   ? static_cast<P3PmsgList*>(h)   : nullptr; }
static inline P3PmsgVect*   toVect (MsgVectHandle   h) { return MsgcoreHandleIs(h, MsgcoreHandle_Vect)   ? static_cast<P3PmsgVect*>(h)   : nullptr; }
static inline P3PmsgAttr*   toAttr (MsgAttrHandle   h) { return MsgcoreHandleIs(h, MsgcoreHandle_Attr)   ? static_cast<P3PmsgAttr*>(h)   : nullptr; }
static inline P3PmsgDesc*   toDesc (MsgDescHandle   h) { return MsgcoreHandleIs(h, MsgcoreHandle_Desc)   ? static_cast<P3PmsgDesc*>(h)   : nullptr; }
static inline P3PmsgCurs*   toCurs (MsgCursHandle   h) { return MsgcoreHandleIs(h, MsgcoreHandle_Curs)   ? static_cast<P3PmsgCurs*>(h)   : nullptr; }
static inline MsgStck*      toStck (MsgStckHandle   h) { return MsgcoreHandleIs(h, MsgcoreHandle_Stck)   ? static_cast<MsgStck*>(h)      : nullptr; }
static inline P2PmsgRecurs* toRecurs(MsgRecursHandle h){ return MsgcoreHandleIs(h, MsgcoreHandle_Recurs) ? static_cast<P2PmsgRecurs*>(h) : nullptr; }

// ---------------------------------------------------------------------------
// Helpers for the Containers II / MsgStck / Recurs / paging blocks.
// These live OUTSIDE the extern "C" block below on purpose: a C-linkage
// function may not return a C++ class by value, and MsgcoreProtoData does.
// ---------------------------------------------------------------------------

// A zero-valued P3PmsgData of the requested MSGCORE_DATA_* type, used as the
// element prototype a P3PmsgVect constructor requires. An unrecognised type
// falls back to INT32, matching msgcore_field_declare_int_typed.
static P3PmsgData MsgcoreProtoData(unsigned char uDataType)
{
    switch (uDataType)
    {
    case MSGCORE_DATA_INT08:  return P3PmsgData((INT08)0);
    case MSGCORE_DATA_UINT08: return P3PmsgData((UINT08)0);
    case MSGCORE_DATA_INT16:  return P3PmsgData((INT16)0);
    case MSGCORE_DATA_UINT16: return P3PmsgData((UINT16)0);
    case MSGCORE_DATA_UINT32: return P3PmsgData((UINT32)0);
    case MSGCORE_DATA_INT64:  return P3PmsgData((INT64)0);
    case MSGCORE_DATA_UINT64: return P3PmsgData((UINT64)0);
    case MSGCORE_DATA_FLOAT:  return P3PmsgData((float)0.0f);
    case MSGCORE_DATA_DOUBLE: return P3PmsgData((double)0.0);
    case MSGCORE_DATA_BOOL:   return P3PmsgData((bool)false);
    case MSGCORE_DATA_WSTR16:
    case MSGCORE_DATA_BSTR16: return P3PmsgData(L"");
    case MSGCORE_DATA_INT32:
    default:                  return P3PmsgData((INT32)0);
    }
}

// The nIndex'th cell of a list, or nullptr when the index is out of range.
// P3PmsgList is a linked sequence walked by VBLaddr, so this is O(nIndex); see
// the header for why an index rather than the raw position crosses the ABI.
static P3PmsgData* MsgcoreListCellAt(P3PmsgList* pList, int nIndex)
{
    if (!pList || nIndex < 0) return nullptr;
    VBLaddr aPos = pList->GetHeadPos();
    for (int i = 0; i < nIndex && aPos; ++i)
        pList->GetNext(aPos);
    if (!aPos) return nullptr;
    return &pList->GetNext(aPos);
}

// --- paging sink registry --------------------------------------------------
// P2PmsgMgr::PageRegistration takes a PINT_PTR "callback key" that it hands
// back to every invocation -- already a user-data slot, so the flat sinks need
// no core change: the key IS the address of this record. std::map is chosen
// for its reference stability (the record must not move while registered).
struct MsgcorePagingRec
{
    msgcore_pagein_fn   pfnIn{nullptr};
    msgcore_pageout_fn  pfnOut{nullptr};
    msgcore_populate_fn pfnPopulate{nullptr};
    void*               pvUserPage{nullptr};
    void*               pvUserPopulate{nullptr};
};
static std::map<P2PmsgMgr*, MsgcorePagingRec> s_mapPaging;
static std::mutex                             s_mtxPaging;

static BOOL CALLBACK MsgcorePageinTramp(PINT_PTR nKey, P2Pos posItem)
{
    MsgcorePagingRec* p = reinterpret_cast<MsgcorePagingRec*>(nKey);
    if (!p || !p->pfnIn) return FALSE;
    return p->pfnIn(p->pvUserPage, (unsigned long long)posItem) ? TRUE : FALSE;
}
static BOOL CALLBACK MsgcorePageoutTramp(PINT_PTR nKey, P2Pos posItem, BOOL bFlush)
{
    MsgcorePagingRec* p = reinterpret_cast<MsgcorePagingRec*>(nKey);
    if (!p || !p->pfnOut) return FALSE;
    return p->pfnOut(p->pvUserPage, (unsigned long long)posItem, bFlush ? 1 : 0) ? TRUE : FALSE;
}
static BOOL CALLBACK MsgcorePopulateTramp(PINT_PTR nKey, P2Pos posItem, BOOL /*bSpare*/)
{
    MsgcorePagingRec* p = reinterpret_cast<MsgcorePagingRec*>(nKey);
    if (!p || !p->pfnPopulate) return FALSE;
    return p->pfnPopulate(p->pvUserPopulate, (unsigned long long)posItem) ? TRUE : FALSE;
}

// Forget a manager's paging record. Called from msgcore_mgr_destroy so a
// destroyed manager cannot leave a stale entry behind for a later manager that
// the allocator happens to place at the same address.
static void MsgcorePagingForget(P2PmsgMgr* pMgr)
{
    std::lock_guard<std::mutex> lock(s_mtxPaging);
    s_mapPaging.erase(pMgr);
}

// ---------------------------------------------------------------------------
// P2PmsgMgr
// ---------------------------------------------------------------------------

extern "C" {

MSGCORE_C_API MsgMgrHandle
msgcore_mgr_create()
{
    return MsgcoreHandleAdd(new P2PmsgMgr(), MsgcoreHandle_Mgr);
}

MSGCORE_C_API MsgMgrHandle
msgcore_mgr_create_nn(unsigned char uAddrNN, unsigned int nSizeInitial, unsigned int nSizeMax)
{
    return MsgcoreHandleAdd(new P2PmsgMgr(uAddrNN, nSizeInitial, nSizeMax), MsgcoreHandle_Mgr);
}

MSGCORE_C_API MsgMgrHandle
msgcore_mgr_open_file(const wchar_t* lpszFilename)
{
    if (!lpszFilename) return nullptr;
    P2PmsgMgr* pMgr = new P2PmsgMgr(lpszFilename);
    return MsgcoreHandleAdd(pMgr, MsgcoreHandle_Mgr);
}

MSGCORE_C_API void
msgcore_mgr_destroy(MsgMgrHandle hMgr)
{
    // Drop any paging registration first: the record is keyed by this pointer,
    // and the allocator may hand the same address to the next manager.
    P2PmsgMgr* pMgr = toMgr(hMgr);
    if (!pMgr) return;                       // never issued, already destroyed, or wrong kind
    MsgcorePagingForget(pMgr);
    // Forget BEFORE the delete, and act only on the caller that actually
    // removed the entry: that is what makes a double destroy a no-op rather
    // than a double free, and stops two threads racing one destroy from both
    // reaching delete.
    if (!MsgcoreHandleForget(hMgr, MsgcoreHandle_Mgr)) return;
    delete pMgr;
}

MSGCORE_C_API int
msgcore_mgr_load(MsgMgrHandle hMgr, const wchar_t* lpszFilename)
{
    if (!toMgr(hMgr) || !lpszFilename) return 0;
    return toMgr(hMgr)->Load(lpszFilename) ? 1 : 0;
}

MSGCORE_C_API int
msgcore_mgr_save(MsgMgrHandle hMgr, const wchar_t* lpszFilename)
{
    if (!toMgr(hMgr)) return 0;
    return toMgr(hMgr)->Save(lpszFilename) ? 1 : 0;
}

MSGCORE_C_API void
msgcore_mgr_nullify(MsgMgrHandle hMgr)
{
    if (toMgr(hMgr)) toMgr(hMgr)->Nullify();
}

MSGCORE_C_API int
msgcore_mgr_rename(MsgMgrHandle hMgr, const wchar_t* lpszNewname)
{
    if (!toMgr(hMgr) || !lpszNewname) return 0;
    return toMgr(hMgr)->Rename(lpszNewname) ? 1 : 0;
}

MSGCORE_C_API const wchar_t*
msgcore_mgr_get_filename(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return nullptr;
    return toMgr(hMgr)->GetFilename();
}

MSGCORE_C_API const wchar_t*
msgcore_mgr_get_rootname(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return nullptr;
    return toMgr(hMgr)->GetRootname();
}

MSGCORE_C_API int
msgcore_mgr_is_dirty(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return 0;
    return toMgr(hMgr)->IsDirty() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_mgr_set_dirty(MsgMgrHandle hMgr, int bDirty)
{
    if (!toMgr(hMgr)) return 0;
    return toMgr(hMgr)->SetDirty(bDirty ? TRUE : FALSE) ? 1 : 0;
}

MSGCORE_C_API unsigned int
msgcore_mgr_sizeof(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return 0;
    return (unsigned int)toMgr(hMgr)->Sizeof();
}

MSGCORE_C_API int
msgcore_mgr_is_valid(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return 0;
    return toMgr(hMgr)->IsValid() ? 1 : 0;
}

// ---------------------------------------------------------------------------
// P3PmsgField
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgFieldHandle
msgcore_mgr_as_field(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return nullptr;
    // P2PmsgMgr IS-A P3PmsgItem (=P3PmsgField) – copy-construct an independent field
    P3PmsgField* pField = new P3PmsgField(*toMgr(hMgr));
    return MsgcoreHandleAdd(pField, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_create()
{
    return MsgcoreHandleAdd(new P3PmsgField(), MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_clone(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    return MsgcoreHandleAdd(new P3PmsgField(*toField(hField)), MsgcoreHandle_Field);
}

MSGCORE_C_API void
msgcore_field_destroy(MsgFieldHandle hField)
{
    P3PmsgField* p = toField(hField);
    // Forget first, and act only on the caller that removed the entry -
    // a double destroy then deletes once and refuses the second time.
    if (!p || !MsgcoreHandleForget(hField, MsgcoreHandle_Field)) return;
    delete p;
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_select_item(MsgFieldHandle hField, const wchar_t* lpszName)
{
    if (!toField(hField) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toField(hField)->SelectItem(lpszName);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API int
msgcore_field_exists(MsgFieldHandle hField, const wchar_t* lpszName)
{
    if (!toField(hField) || !lpszName) return 0;
    return toField(hField)->Exists(lpszName) ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_delete_item(MsgFieldHandle hField, const wchar_t* lpszName)
{
    if (!toField(hField) || !lpszName) return 0;
    return toField(hField)->Delete(lpszName) ? 1 : 0;
}

MSGCORE_C_API void
msgcore_field_truncate(MsgFieldHandle hField)
{
    if (toField(hField)) toField(hField)->Truncate();
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_int(MsgFieldHandle hField, const wchar_t* lpszName, int value, int bUpdate)
{
    if (!toField(hField) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData(value), bUpdate ? TRUE : FALSE);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_int64(MsgFieldHandle hField, const wchar_t* lpszName, long long value, int bUpdate)
{
    if (!toField(hField) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((INT64)value), bUpdate ? TRUE : FALSE);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_double(MsgFieldHandle hField, const wchar_t* lpszName, double value, int bUpdate)
{
    if (!toField(hField) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData(value), bUpdate ? TRUE : FALSE);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_bool(MsgFieldHandle hField, const wchar_t* lpszName, int bValue, int bUpdate)
{
    if (!toField(hField) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((bool)(bValue != 0)), bUpdate ? TRUE : FALSE);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

// UTF-16 unit count of a wide string (an astral code point is two units). This is
// the width a value is stored at (P2PWCHAR, 16-bit): on Windows wchar_t already IS a
// UTF-16 unit; on the Linux port a >U+FFFF wchar_t encodes to a surrogate pair. Used
// to reject an over-capacity value up front (see the guard in declare_wstr).
static size_t msgcore_wstr16_units(const wchar_t* s)
{
    size_t units = 0;
    for (; s && *s; ++s)
#if WCHAR_MAX > 0xFFFF
        units += (static_cast<unsigned long>(*s) > 0xFFFFu) ? 2u : 1u;   // UTF-32 wchar_t
#else
        units += 1u;                                                     // UTF-16 wchar_t
#endif
    return units;
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_wstr(MsgFieldHandle hField, const wchar_t* lpszName, const wchar_t* lpszValue, int bUpdate)
{
    if (!toField(hField) || !lpszName || !lpszValue) return nullptr;
    // A WSTR16 blob caps at 0xFFFF bytes -> 32767 UTF-16 units. Reject an over-cap
    // value HERE rather than let it reach the core: the over-cap store throws from deep
    // in the VBHeap copy and orphans this P3PmsgData's heap object on the C++ unwind
    // (a latent core exception-safety gap, ASan-confirmed). Rejecting up front is
    // behaviour-identical (returns null -> the caller sees the write rejected) and
    // leak-free. Regression: TreeFsPosixSuite "an over-cap value write is rejected
    // without corrupting the node".
    if (msgcore_wstr16_units(lpszValue) > 32767) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData(lpszValue), bUpdate ? TRUE : FALSE);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API const wchar_t*
msgcore_field_get_name(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    return toField(hField)->c_name();
}

MSGCORE_C_API unsigned char
msgcore_field_get_data_type(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->DataType();
}

MSGCORE_C_API int
msgcore_field_is_null(MsgFieldHandle hField)
{
    if (!toField(hField)) return 1;
    return toField(hField)->IsNull() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_is_void(MsgFieldHandle hField)
{
    if (!toField(hField)) return 1;
    return toField(hField)->IsVoid() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_get_int(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->c_int();
}

MSGCORE_C_API void
msgcore_field_set_int(MsgFieldHandle hField, int value)
{
    if (toField(hField)) toField(hField)->c_int(value);
}

MSGCORE_C_API long long
msgcore_field_get_int64(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0LL;
    return (long long)toField(hField)->c_int64();
}

MSGCORE_C_API void
msgcore_field_set_int64(MsgFieldHandle hField, long long value)
{
    if (toField(hField)) toField(hField)->c_int64((INT64)value);
}

MSGCORE_C_API double
msgcore_field_get_double(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0.0;
    return toField(hField)->c_double();
}

MSGCORE_C_API void
msgcore_field_set_double(MsgFieldHandle hField, double value)
{
    if (toField(hField)) toField(hField)->c_double(value);
}

MSGCORE_C_API int
msgcore_field_get_bool(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->c_bool() ? 1 : 0;
}

MSGCORE_C_API void
msgcore_field_set_bool(MsgFieldHandle hField, int bValue)
{
    if (toField(hField)) toField(hField)->c_bool((bValue != 0));
}

MSGCORE_C_API const wchar_t*
msgcore_field_get_wstr(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    return toField(hField)->c_wstr();
}

MSGCORE_C_API void
msgcore_field_set_wstr(MsgFieldHandle hField, const wchar_t* lpszValue)
{
    if (toField(hField) && lpszValue) toField(hField)->c_wcscpy(lpszValue);
}

MSGCORE_C_API int
msgcore_field_is_list(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->r_Object().IsList() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_is_vect(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->r_Object().IsVect() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_is_attr(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->r_Object().IsAttr() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_is_desc(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->r_Object().IsDesc() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_is_stacked(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->IsStacked() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_is_attributed(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->IsAttributed() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_field_is_descendant(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->IsDescendant() ? 1 : 0;
}

// P3PmsgField::IsSole, not P3PmsgObject::IsSole: the field's override subtracts
// the sub-objects it made itself, so a field that has merely been asked for its
// descendants still answers TRUE. The object's version cannot tell those from a
// stranger. The contract the header publishes -- TRUE guarantees, FALSE does not
// -- is the field's, and forwarding to r_Object() here would quietly narrow the
// TRUE half that the whole export exists for.
//
// The bad-handle answer is 0 and that is not arbitrary: 0 is the answer that
// promises nothing, so a stale or hostile handle can never be turned into a
// licence to write in place. (msgcore_field_is_null / _is_void answer 1 for the
// same reason -- in those, 1 is the pessimistic half.)
MSGCORE_C_API int
msgcore_field_is_sole(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0;
    return toField(hField)->IsSole() ? 1 : 0;
}

// ---------------------------------------------------------------------------
// P3PmsgList
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgListHandle
msgcore_list_from_field(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    return MsgcoreHandleAdd(new P3PmsgList(*toField(hField)), MsgcoreHandle_List);
}

MSGCORE_C_API void
msgcore_list_destroy(MsgListHandle hList)
{
    P3PmsgList* p = toList(hList);
    // Forget first, and act only on the caller that removed the entry -
    // a double destroy then deletes once and refuses the second time.
    if (!p || !MsgcoreHandleForget(hList, MsgcoreHandle_List)) return;
    delete p;
}

MSGCORE_C_API int
msgcore_list_get_count(MsgListHandle hList)
{
    if (!toList(hList)) return 0;
    return (int)toList(hList)->GetCount();
}

MSGCORE_C_API void
msgcore_list_add_head_int(MsgListHandle hList, int value)
{
    if (toList(hList)) toList(hList)->AddListHead(P3PmsgData(value));
}

MSGCORE_C_API void
msgcore_list_add_tail_int(MsgListHandle hList, int value)
{
    if (toList(hList)) toList(hList)->AddListTail(P3PmsgData(value));
}

MSGCORE_C_API void
msgcore_list_add_head_double(MsgListHandle hList, double value)
{
    if (toList(hList)) toList(hList)->AddListHead(P3PmsgData(value));
}

MSGCORE_C_API void
msgcore_list_add_tail_double(MsgListHandle hList, double value)
{
    if (toList(hList)) toList(hList)->AddListTail(P3PmsgData(value));
}

MSGCORE_C_API void
msgcore_list_add_head_wstr(MsgListHandle hList, const wchar_t* lpszValue)
{
    if (toList(hList) && lpszValue) toList(hList)->AddListHead(P3PmsgData(lpszValue));
}

MSGCORE_C_API void
msgcore_list_add_tail_wstr(MsgListHandle hList, const wchar_t* lpszValue)
{
    if (toList(hList) && lpszValue) toList(hList)->AddListTail(P3PmsgData(lpszValue));
}

MSGCORE_C_API void
msgcore_list_drop_head(MsgListHandle hList)
{
    if (toList(hList)) toList(hList)->DropHead();
}

MSGCORE_C_API void
msgcore_list_drop_tail(MsgListHandle hList)
{
    if (toList(hList)) toList(hList)->DropTail();
}

MSGCORE_C_API void
msgcore_list_truncate(MsgListHandle hList)
{
    if (toList(hList)) toList(hList)->Truncate();
}

// ---------------------------------------------------------------------------
// P3PmsgVect
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgVectHandle
msgcore_vect_from_field(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    return MsgcoreHandleAdd(new P3PmsgVect(toField(hField)->r_Object()), MsgcoreHandle_Vect);
}

MSGCORE_C_API void
msgcore_vect_destroy(MsgVectHandle hVect)
{
    P3PmsgVect* p = toVect(hVect);
    // Forget first, and act only on the caller that removed the entry -
    // a double destroy then deletes once and refuses the second time.
    if (!p || !MsgcoreHandleForget(hVect, MsgcoreHandle_Vect)) return;
    delete p;
}

MSGCORE_C_API int
msgcore_vect_is_data(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return 0;
    return toVect(hVect)->IsData(nElem) ? 1 : 0;
}

MSGCORE_C_API int
msgcore_vect_is_field(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return 0;
    return toVect(hVect)->IsField(nElem) ? 1 : 0;
}

MSGCORE_C_API int
msgcore_vect_is_list(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return 0;
    return toVect(hVect)->IsList(nElem) ? 1 : 0;
}

MSGCORE_C_API int
msgcore_vect_is_vect(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return 0;
    return toVect(hVect)->IsVect(nElem) ? 1 : 0;
}

MSGCORE_C_API int
msgcore_vect_get_int(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return 0;
    return toVect(hVect)->r_data(nElem).c_int();
}

MSGCORE_C_API double
msgcore_vect_get_double(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return 0.0;
    return toVect(hVect)->r_data(nElem).c_double();
}

MSGCORE_C_API const wchar_t*
msgcore_vect_get_wstr(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return nullptr;
    return toVect(hVect)->r_data(nElem).c_wstr();
}

MSGCORE_C_API MsgFieldHandle
msgcore_vect_get_item(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toVect(hVect)->r_item(nElem);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API void
msgcore_vect_truncate(MsgVectHandle hVect)
{
    if (toVect(hVect)) toVect(hVect)->Truncate();
}

// ---------------------------------------------------------------------------
// P3PmsgAttr
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgAttrHandle
msgcore_attr_from_field(MsgFieldHandle hField, int bCreate)
{
    if (!toField(hField)) return nullptr;
    P3PmsgAttr* pAttr = new P3PmsgAttr(toField(hField));
    if (bCreate && pAttr->IsEmpty())
        pAttr->Create();
    return MsgcoreHandleAdd(pAttr, MsgcoreHandle_Attr);
}

MSGCORE_C_API void
msgcore_attr_destroy(MsgAttrHandle hAttr)
{
    P3PmsgAttr* p = toAttr(hAttr);
    // Forget first, and act only on the caller that removed the entry -
    // a double destroy then deletes once and refuses the second time.
    if (!p || !MsgcoreHandleForget(hAttr, MsgcoreHandle_Attr)) return;
    delete p;
}

MSGCORE_C_API int
msgcore_attr_get_count(MsgAttrHandle hAttr)
{
    if (!toAttr(hAttr)) return 0;
    return (int)toAttr(hAttr)->GetCount();
}

MSGCORE_C_API int
msgcore_attr_is_empty(MsgAttrHandle hAttr)
{
    if (!toAttr(hAttr)) return 1;
    return toAttr(hAttr)->IsEmpty() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_attr_exists(MsgAttrHandle hAttr, const wchar_t* lpszName)
{
    if (!toAttr(hAttr) || !lpszName) return 0;
    return toAttr(hAttr)->Exists(lpszName) ? 1 : 0;
}

MSGCORE_C_API MsgFieldHandle
msgcore_attr_select_item(MsgAttrHandle hAttr, const wchar_t* lpszName)
{
    if (!toAttr(hAttr) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toAttr(hAttr)->SelectItem(lpszName);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_attr_declare_int(MsgAttrHandle hAttr, const wchar_t* lpszName, int value, int bUpdate)
{
    if (!toAttr(hAttr) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toAttr(hAttr)->DeclareItem(lpszName, P3PmsgData(value), bUpdate != 0);
    } catch (...) { delete pResult; return nullptr; }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_attr_declare_double(MsgAttrHandle hAttr, const wchar_t* lpszName, double value, int bUpdate)
{
    if (!toAttr(hAttr) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toAttr(hAttr)->DeclareItem(lpszName, P3PmsgData(value), bUpdate != 0);
    } catch (...) { delete pResult; return nullptr; }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_attr_declare_wstr(MsgAttrHandle hAttr, const wchar_t* lpszName, const wchar_t* lpszValue, int bUpdate)
{
    if (!toAttr(hAttr) || !lpszName || !lpszValue) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toAttr(hAttr)->DeclareItem(lpszName, P3PmsgData(lpszValue), bUpdate != 0);
    } catch (...) { delete pResult; return nullptr; }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API int
msgcore_attr_delete(MsgAttrHandle hAttr, const wchar_t* lpszName)
{
    if (!toAttr(hAttr) || !lpszName) return 0;
    return toAttr(hAttr)->Delete(lpszName) ? 1 : 0;
}

MSGCORE_C_API void
msgcore_attr_truncate(MsgAttrHandle hAttr)
{
    if (toAttr(hAttr)) toAttr(hAttr)->Truncate();
}

// ---------------------------------------------------------------------------
// P3PmsgDesc
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgDescHandle
msgcore_desc_from_field(MsgFieldHandle hField, int bCreate)
{
    if (!toField(hField)) return nullptr;
    P3PmsgDesc* pDesc = new P3PmsgDesc(toField(hField));
    if (bCreate && pDesc->IsEmpty())
        pDesc->Create();
    return MsgcoreHandleAdd(pDesc, MsgcoreHandle_Desc);
}

MSGCORE_C_API void
msgcore_desc_destroy(MsgDescHandle hDesc)
{
    P3PmsgDesc* p = toDesc(hDesc);
    // Forget first, and act only on the caller that removed the entry -
    // a double destroy then deletes once and refuses the second time.
    if (!p || !MsgcoreHandleForget(hDesc, MsgcoreHandle_Desc)) return;
    delete p;
}

MSGCORE_C_API int
msgcore_desc_get_count(MsgDescHandle hDesc)
{
    if (!toDesc(hDesc)) return 0;
    return (int)toDesc(hDesc)->GetCount();
}

MSGCORE_C_API int
msgcore_desc_is_empty(MsgDescHandle hDesc)
{
    if (!toDesc(hDesc)) return 1;
    return toDesc(hDesc)->IsEmpty() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_desc_exists(MsgDescHandle hDesc, const wchar_t* lpszName)
{
    if (!toDesc(hDesc) || !lpszName) return 0;
    return toDesc(hDesc)->Exists(lpszName) ? 1 : 0;
}

MSGCORE_C_API MsgFieldHandle
msgcore_desc_select_item(MsgDescHandle hDesc, const wchar_t* lpszName)
{
    if (!toDesc(hDesc) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toDesc(hDesc)->SelectItem(lpszName);
    } catch (...) { delete pResult; return nullptr; }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_desc_declare_int(MsgDescHandle hDesc, const wchar_t* lpszName, int value, int bUpdate)
{
    if (!toDesc(hDesc) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toDesc(hDesc)->DeclareItem(lpszName, P3PmsgData(value), bUpdate ? TRUE : FALSE);
    } catch (...) { delete pResult; return nullptr; }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_desc_declare_double(MsgDescHandle hDesc, const wchar_t* lpszName, double value, int bUpdate)
{
    if (!toDesc(hDesc) || !lpszName) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toDesc(hDesc)->DeclareItem(lpszName, P3PmsgData(value), bUpdate ? TRUE : FALSE);
    } catch (...) { delete pResult; return nullptr; }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API MsgFieldHandle
msgcore_desc_declare_wstr(MsgDescHandle hDesc, const wchar_t* lpszName, const wchar_t* lpszValue, int bUpdate)
{
    if (!toDesc(hDesc) || !lpszName || !lpszValue) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toDesc(hDesc)->DeclareItem(lpszName, P3PmsgData(lpszValue), bUpdate ? TRUE : FALSE);
    } catch (...) { delete pResult; return nullptr; }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API int
msgcore_desc_delete(MsgDescHandle hDesc, const wchar_t* lpszName)
{
    if (!toDesc(hDesc) || !lpszName) return 0;
    return toDesc(hDesc)->Delete(lpszName) ? 1 : 0;
}

MSGCORE_C_API void
msgcore_desc_truncate(MsgDescHandle hDesc)
{
    if (toDesc(hDesc)) toDesc(hDesc)->Truncate();
}

// ---------------------------------------------------------------------------
// P3PmsgCurs
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgCursHandle
msgcore_curs_from_field(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    return MsgcoreHandleAdd(new P3PmsgCurs(*toField(hField)), MsgcoreHandle_Curs);
}

MSGCORE_C_API MsgCursHandle
msgcore_curs_from_attr(MsgAttrHandle hAttr)
{
    if (!toAttr(hAttr)) return nullptr;
    return MsgcoreHandleAdd(new P3PmsgCurs(*toAttr(hAttr)), MsgcoreHandle_Curs);
}

MSGCORE_C_API MsgCursHandle
msgcore_curs_from_desc(MsgDescHandle hDesc)
{
    if (!toDesc(hDesc)) return nullptr;
    return MsgcoreHandleAdd(new P3PmsgCurs(*toDesc(hDesc)), MsgcoreHandle_Curs);
}

MSGCORE_C_API void
msgcore_curs_destroy(MsgCursHandle hCurs)
{
    P3PmsgCurs* p = toCurs(hCurs);
    // Forget first, and act only on the caller that removed the entry -
    // a double destroy then deletes once and refuses the second time.
    if (!p || !MsgcoreHandleForget(hCurs, MsgcoreHandle_Curs)) return;
    delete p;
}

MSGCORE_C_API void
msgcore_curs_next(MsgCursHandle hCurs)
{
    if (toCurs(hCurs)) ++(*toCurs(hCurs));
}

MSGCORE_C_API void
msgcore_curs_seek(MsgCursHandle hCurs)
{
    if (toCurs(hCurs)) toCurs(hCurs)->Seek();
}

MSGCORE_C_API int
msgcore_curs_goto_name(MsgCursHandle hCurs, const wchar_t* lpszName)
{
    if (!toCurs(hCurs) || !lpszName) return 0;
    return toCurs(hCurs)->Goto(lpszName) ? 1 : 0;
}

MSGCORE_C_API int
msgcore_curs_goto_index(MsgCursHandle hCurs, int nElem)
{
    if (!toCurs(hCurs)) return 0;
    return toCurs(hCurs)->Goto(nElem) ? 1 : 0;
}

MSGCORE_C_API int
msgcore_curs_is_eo_cursor(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return 1;
    return toCurs(hCurs)->IsEoCursor() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_curs_is_so_cursor(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return 1;
    return toCurs(hCurs)->IsSoCursor() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_curs_get_count(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return 0;
    return (int)toCurs(hCurs)->GetCount();
}

MSGCORE_C_API int
msgcore_curs_item_index(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return -1;
    return (int)toCurs(hCurs)->Item();
}

MSGCORE_C_API int
msgcore_curs_is_item(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return 0;
    return toCurs(hCurs)->IsItem() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_curs_is_list(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return 0;
    return toCurs(hCurs)->IsList() ? 1 : 0;
}

MSGCORE_C_API int
msgcore_curs_is_vect(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return 0;
    return toCurs(hCurs)->IsVect() ? 1 : 0;
}

MSGCORE_C_API MsgFieldHandle
msgcore_curs_get_field(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = (P3PmsgField&)(*toCurs(hCurs));
    } catch (...) { delete pResult; return nullptr; }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API const wchar_t*
msgcore_curs_get_name(MsgCursHandle hCurs)
{
    if (!toCurs(hCurs)) return nullptr;
    return toCurs(hCurs)->c_wstr();
}

MSGCORE_C_API void
msgcore_curs_delete(MsgCursHandle hCurs)
{
    if (toCurs(hCurs)) toCurs(hCurs)->Delete();
}

// ---------------------------------------------------------------------------
// FileSystem-layer support. (Motivating TreeFS design note is internal; the
// contract these wrappers carry is stated in full in Msgcore_c.h.)
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgFieldHandle
msgcore_mgr_root(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return nullptr;
    try {
        // Construct FROM the manager's live P3PmsgObject: the P3PmsgObject ctor
        // aliases the heap node (unlike copy-from-field, which deep-copies).
        return MsgcoreHandleAdd(new P3PmsgField(toMgr(hMgr)->r_Object()), MsgcoreHandle_Field);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_child(MsgFieldHandle hField, const wchar_t* lpszName)
{
    if (!toField(hField) || !lpszName) return nullptr;
    try {
        if (!toField(hField)->Exists(lpszName)) return nullptr;
        // .r_Object() yields the live heap address; wrapping it aliases the node.
        return MsgcoreHandleAdd(new P3PmsgField(toField(hField)->SelectItem(lpszName).r_Object()), MsgcoreHandle_Field);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API unsigned long long
msgcore_field_get_p2pos(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0ULL;
    try {
        return (unsigned long long)toField(hField)->GetP2Pos();
    } catch (...) { return 0ULL; }
}

MSGCORE_C_API MsgFieldHandle
msgcore_mgr_p2pos2field(MsgMgrHandle hMgr, unsigned long long pos)
{
    if (!toMgr(hMgr)) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toMgr(hMgr)->P2Pos2Field((P2Pos)pos);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API const wchar_t*
msgcore_mgr_p2pos2path(MsgMgrHandle hMgr, unsigned long long pos)
{
    if (!toMgr(hMgr)) return nullptr;
    // Owned by a thread-local so the returned pointer outlives this call but is
    // overwritten by the next call on the same thread (documented contract).
    static thread_local CString s_strPath;
    try {
        s_strPath = toMgr(hMgr)->P2Pos2Path((P2Pos)pos);
    } catch (...) {
        return nullptr;
    }
    return (LPCWSTR)s_strPath;
}

MSGCORE_C_API const char*
msgcore_type_name(unsigned char uDataType)
{
    switch (uDataType)
    {
    case MSGCORE_DATA_NULL:   return "NULL";
    case MSGCORE_DATA_INT08:  return "INT08";
    case MSGCORE_DATA_UINT08: return "UINT08";
    case MSGCORE_DATA_INT16:  return "INT16";
    case MSGCORE_DATA_UINT16: return "UINT16";
    case MSGCORE_DATA_INT32:  return "INT32";
    case MSGCORE_DATA_UINT32: return "UINT32";
    case MSGCORE_DATA_INT64:  return "INT64";
    case MSGCORE_DATA_UINT64: return "UINT64";
    case MSGCORE_DATA_FLOAT:  return "FLOAT";
    case MSGCORE_DATA_DOUBLE: return "DOUBLE";
    case MSGCORE_DATA_BOOL:   return "BOOL";
    case MSGCORE_DATA_BSTR16: return "BSTR16";
    case MSGCORE_DATA_WSTR16: return "WSTR16";
    case MSGCORE_DATA_BLOB16: return "BLOB16";
    case MSGCORE_DATA_GUID:   return "GUID";
    default:                  return "UNKNOWN";
    }
}

MSGCORE_C_API unsigned char
msgcore_type_from_name(const char* lpszTypeName)
{
    if (!lpszTypeName) return 0xFF;
    struct { const char* name; unsigned char code; } kMap[] = {
        { "NULL",   MSGCORE_DATA_NULL   },
        { "INT08",  MSGCORE_DATA_INT08  }, { "UINT08", MSGCORE_DATA_UINT08 },
        { "INT16",  MSGCORE_DATA_INT16  }, { "UINT16", MSGCORE_DATA_UINT16 },
        { "INT32",  MSGCORE_DATA_INT32  }, { "UINT32", MSGCORE_DATA_UINT32 },
        { "INT64",  MSGCORE_DATA_INT64  }, { "UINT64", MSGCORE_DATA_UINT64 },
        { "FLOAT",  MSGCORE_DATA_FLOAT  }, { "DOUBLE", MSGCORE_DATA_DOUBLE },
        { "BOOL",   MSGCORE_DATA_BOOL   },
        { "BSTR16", MSGCORE_DATA_BSTR16 }, { "WSTR16", MSGCORE_DATA_WSTR16 },
        { "BLOB16", MSGCORE_DATA_BLOB16 }, { "GUID",   MSGCORE_DATA_GUID   },
    };
    for (const auto& e : kMap)
        if (std::strcmp(lpszTypeName, e.name) == 0)
            return e.code;
    return 0xFF;
}

MSGCORE_C_API long long
msgcore_field_get_tstamp(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0LL;
    try {
        return (long long)P3Pmsg_GetTStamp(*toField(hField));
    } catch (...) { return 0LL; }
}

MSGCORE_C_API long long
msgcore_field_set_tstamp(MsgFieldHandle hField, long long tsValue)
{
    if (!toField(hField)) return 0LL;
    try {
        return (long long)P3Pmsg_SetTStamp(*toField(hField), (INT64)tsValue, FALSE);
    } catch (...) { return 0LL; }
}

MSGCORE_C_API int
msgcore_field_rename_child(MsgFieldHandle hField, const wchar_t* lpszOldName, const wchar_t* lpszNewName)
{
    if (!toField(hField) || !lpszOldName || !lpszNewName) return 0;
    try {
        if (!toField(hField)->Exists(lpszOldName)) return 0;
        P3PmsgRefactor_Rename(*toField(hField), lpszOldName, lpszNewName);
    } catch (...) { return 0; }
    return 1;
}

MSGCORE_C_API int
msgcore_field_move_child(MsgFieldHandle hSrcField, MsgFieldHandle hDstField, const wchar_t* lpszName)
{
    if (!toField(hSrcField) || !toField(hDstField) || !lpszName) return 0;
    try {
        if (!toField(hSrcField)->Exists(lpszName)) return 0;
        P3PmsgRefactor_Move(*toField(hSrcField), *toField(hDstField), lpszName);
    } catch (...) { return 0; }
    return 1;
}

static int msgcore_retype_child(MsgFieldHandle hField, const wchar_t* lpszName, const P3PmsgData& oData)
{
    if (!hField || !lpszName) return 0;
    try {
        if (!toField(hField)->Exists(lpszName)) return 0;
        P3PmsgRefactor_DataType(*toField(hField), lpszName, oData);
    } catch (...) { return 0; }
    return 1;
}

MSGCORE_C_API int
msgcore_field_retype_child_int(MsgFieldHandle hField, const wchar_t* lpszName, int value)
{
    return msgcore_retype_child(hField, lpszName, P3PmsgData(value));
}

MSGCORE_C_API int
msgcore_field_retype_child_int64(MsgFieldHandle hField, const wchar_t* lpszName, long long value)
{
    return msgcore_retype_child(hField, lpszName, P3PmsgData((INT64)value));
}

MSGCORE_C_API int
msgcore_field_retype_child_double(MsgFieldHandle hField, const wchar_t* lpszName, double value)
{
    return msgcore_retype_child(hField, lpszName, P3PmsgData(value));
}

MSGCORE_C_API int
msgcore_field_retype_child_bool(MsgFieldHandle hField, const wchar_t* lpszName, int bValue)
{
    return msgcore_retype_child(hField, lpszName, P3PmsgData((bool)(bValue != 0)));
}

MSGCORE_C_API int
msgcore_field_retype_child_wstr(MsgFieldHandle hField, const wchar_t* lpszName, const wchar_t* lpszValue)
{
    if (!lpszValue) return 0;
    return msgcore_retype_child(hField, lpszName, P3PmsgData(lpszValue));
}

// --- Extended value types (Phase 1 completion) -----------------------------

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_int_typed(MsgFieldHandle hField, const wchar_t* lpszName,
                                long long value, unsigned char uDataType, int bUpdate)
{
    if (!toField(hField) || !lpszName) return nullptr;
    // Build a P3PmsgData of the exact integer width so DeclareItem stamps the
    // child with that subtype (an unrecognised int type falls back to INT32).
    // Inlined rather than a helper: a C-linkage function may not return P3PmsgData.
    long long v = value;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        BOOL bUp = bUpdate ? TRUE : FALSE;
        switch (uDataType)
        {
        case MSGCORE_DATA_INT08:  *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((INT08)v),  bUp); break;
        case MSGCORE_DATA_UINT08: *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((UINT08)v), bUp); break;
        case MSGCORE_DATA_INT16:  *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((INT16)v),  bUp); break;
        case MSGCORE_DATA_UINT16: *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((UINT16)v), bUp); break;
        case MSGCORE_DATA_UINT32: *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((UINT32)v), bUp); break;
        case MSGCORE_DATA_INT64:  *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((INT64)v),  bUp); break;
        case MSGCORE_DATA_UINT64: *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((UINT64)v), bUp); break;
        case MSGCORE_DATA_INT32:
        default:                  *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData((INT32)v),  bUp); break;
        }
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

MSGCORE_C_API int
msgcore_field_get_int_any(MsgFieldHandle hField, long long* pValue, int* pUnsigned)
{
    if (!toField(hField)) return 0;
    INT64 v = 0; bool bUns = false;
    try {
        if (!toField(hField)->ReadAnyInt(v, bUns)) return 0;
    } catch (...) { return 0; }
    if (pValue)    *pValue    = (long long)v;
    if (pUnsigned) *pUnsigned = bUns ? 1 : 0;
    return 1;
}

MSGCORE_C_API double
msgcore_field_get_float(MsgFieldHandle hField)
{
    if (!toField(hField)) return 0.0;
    try { return (double)toField(hField)->c_float(); }
    catch (...) { return 0.0; }
}

MSGCORE_C_API const void*
msgcore_field_get_blob(MsgFieldHandle hField, int* pnSize)
{
    if (pnSize) *pnSize = 0;
    if (!toField(hField)) return nullptr;
    P3PmsgField* f = toField(hField);
    if (f->DataType() != VBLockData_BLOB16) return nullptr;
    const void* p = f->c_vBlob();
    if (pnSize) *pnSize = (int)f->P3PmsgData::c_size();
    return p;
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_blob(MsgFieldHandle hField, const wchar_t* lpszName,
                           const void* pvData, int nSize, int bUpdate)
{
    if (!toField(hField) || !lpszName || nSize < 0) return nullptr;
    if (nSize > 0 && !pvData) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toField(hField)->DeclareItem(lpszName,
                       P3PmsgData(pvData, (VBLsize)nSize, VBLockData_BLOB16), bUpdate ? TRUE : FALSE);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

// Read exactly n hex digits from p (advancing it); false on a non-hex char.
static bool msgcore_parse_hex(const wchar_t*& p, int n, unsigned long long& out)
{
    out = 0;
    for (int i = 0; i < n; ++i)
    {
        wchar_t c = *p++;
        unsigned d;
        if      (c >= L'0' && c <= L'9') d = (unsigned)(c - L'0');
        else if (c >= L'a' && c <= L'f') d = 10u + (unsigned)(c - L'a');
        else if (c >= L'A' && c <= L'F') d = 10u + (unsigned)(c - L'A');
        else return false;
        out = (out << 4) | d;
    }
    return true;
}

// Parse "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" (optional surrounding braces and
// leading/trailing spaces) into g. Returns false on any malformation.
static bool msgcore_parse_guid(const wchar_t* s, GUID& g)
{
    if (!s) return false;
    std::wstring str(s);
    size_t a = 0, b = str.size();
    const wchar_t* ws = L" \t\r\n";
    while (a < b && wcschr(ws, str[a]))     ++a;
    while (b > a && wcschr(ws, str[b - 1])) --b;
    str = str.substr(a, b - a);
    if (str.size() >= 2 && str.front() == L'{' && str.back() == L'}')
        str = str.substr(1, str.size() - 2);
    if (str.size() != 36) return false;

    const wchar_t* p = str.c_str();
    unsigned long long v, d4a, d4b;
    if (!msgcore_parse_hex(p, 8, v)) return false;  g.Data1 = (unsigned long)v;
    if (*p++ != L'-') return false;
    if (!msgcore_parse_hex(p, 4, v)) return false;  g.Data2 = (unsigned short)v;
    if (*p++ != L'-') return false;
    if (!msgcore_parse_hex(p, 4, v)) return false;  g.Data3 = (unsigned short)v;
    if (*p++ != L'-') return false;
    if (!msgcore_parse_hex(p, 4, d4a)) return false;
    if (*p++ != L'-') return false;
    if (!msgcore_parse_hex(p, 12, d4b)) return false;
    if (*p != 0) return false;

    g.Data4[0] = (unsigned char)(d4a >> 8);
    g.Data4[1] = (unsigned char)(d4a);
    g.Data4[2] = (unsigned char)(d4b >> 40);
    g.Data4[3] = (unsigned char)(d4b >> 32);
    g.Data4[4] = (unsigned char)(d4b >> 24);
    g.Data4[5] = (unsigned char)(d4b >> 16);
    g.Data4[6] = (unsigned char)(d4b >> 8);
    g.Data4[7] = (unsigned char)(d4b);
    return true;
}

MSGCORE_C_API const wchar_t*
msgcore_field_get_guid_str(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    P3PmsgField* f = toField(hField);
    if (f->DataType() != VBLockData_GUID) return nullptr;
    const void* pv = f->c_vGUID();
    if (!pv) return nullptr;
    GUID g;
    memcpy(&g, pv, sizeof(GUID));
    static thread_local wchar_t buf[40];
    swprintf(buf, 40, L"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             (unsigned long)g.Data1, (unsigned)g.Data2, (unsigned)g.Data3,
             (unsigned)g.Data4[0], (unsigned)g.Data4[1], (unsigned)g.Data4[2], (unsigned)g.Data4[3],
             (unsigned)g.Data4[4], (unsigned)g.Data4[5], (unsigned)g.Data4[6], (unsigned)g.Data4[7]);
    return buf;
}

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_guid_str(MsgFieldHandle hField, const wchar_t* lpszName,
                               const wchar_t* lpszGuid, int bUpdate)
{
    if (!toField(hField) || !lpszName || !lpszGuid) return nullptr;
    GUID g;
    if (!msgcore_parse_guid(lpszGuid, g)) return nullptr;
    P3PmsgField* pResult = new P3PmsgField();
    try {
        *pResult = toField(hField)->DeclareItem(lpszName, P3PmsgData(g), bUpdate ? TRUE : FALSE);
    } catch (...) {
        delete pResult;
        return nullptr;
    }
    return MsgcoreHandleAdd(pResult, MsgcoreHandle_Field);
}

// ---------------------------------------------------------------------------
// Change notification (headless triggers)
// ---------------------------------------------------------------------------

MSGCORE_C_API void
msgcore_mgr_set_trigger_sink(MsgMgrHandle hMgr, msgcore_trigger_fn fn, void* pUser)
{
    if (!toMgr(hMgr)) return;
    // msgcore_trigger_fn and P2PmsgTriggerSink are the identical function-pointer
    // type (void(*)(void*,unsigned int,unsigned long long)), so no adapter is
    // needed -- the user's callback becomes the heap sink directly.
    try { toMgr(hMgr)->SetTriggerSink(fn, pUser); } catch (...) {}
}

MSGCORE_C_API void
msgcore_mgr_create_trigger(MsgMgrHandle hMgr, unsigned int nMask, unsigned long long p2pos)
{
    if (!toMgr(hMgr)) return;
    // hWnd == NULL: headless arming. The HWND PostMessage in ProcTriggers becomes
    // a harmless no-op (PostMessage(NULL,...) fails / is a stub off-Windows); the
    // sink is what delivers the notification.
    try { toMgr(hMgr)->CreateTrigger(nMask, (HWND)nullptr, (P2Pos)p2pos); } catch (...) {}
}

MSGCORE_C_API void
msgcore_mgr_drop_triggers(MsgMgrHandle hMgr, unsigned int nMask, unsigned long long p2pos)
{
    if (!toMgr(hMgr)) return;
    try { toMgr(hMgr)->DropTriggers(nMask, (HWND)nullptr, (P2Pos)p2pos); } catch (...) {}
}

MSGCORE_C_API int
msgcore_mgr_fire_trigger(MsgMgrHandle hMgr, unsigned int nMask, unsigned long long p2pos)
{
    if (!toMgr(hMgr)) return 0;
    try { return (int)toMgr(hMgr)->FireTrigger((P2Pos)p2pos, nMask); }
    catch (...) { return 0; }
}

// ---------------------------------------------------------------------------
// Containers II -- creating a list or a vect, and completing both
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgListHandle
msgcore_field_declare_list(MsgFieldHandle hField, const wchar_t* lpszName)
{
    if (!toField(hField) || !lpszName) return nullptr;
    try {
        P3PmsgList oList(lpszName, P3PmsgData((INT32)0));
        P3PmsgDesc& oDesc = toField(hField)->r_Desc(P3PmsgField::AttrCMD_Create);
        oDesc += oList;
        // Read the attached node back LIVE: operator+= deep-copies into the
        // heap, so oList itself still refers to the caller-side temporary.
        // Via a cursor, for the reasons given above the selectors.
        P3PmsgCurs oCurs(oDesc);
        if (!oCurs.Goto(lpszName) || !oCurs.IsList()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgList(oCurs.r_Object()), MsgcoreHandle_List);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgVectHandle
msgcore_field_declare_vect(MsgFieldHandle hField, const wchar_t* lpszName,
                           int nElems, unsigned char uDataType)
{
    if (!toField(hField) || !lpszName || nElems < 0) return nullptr;
    try {
        P3PmsgVect oVect(nElems, lpszName, MsgcoreProtoData(uDataType));
        P3PmsgDesc& oDesc = toField(hField)->r_Desc(P3PmsgField::AttrCMD_Create);
        oDesc += oVect;
        P3PmsgCurs oCurs(oDesc);
        if (!oCurs.Goto(lpszName) || !oCurs.IsVect()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgVect(oCurs.r_Object()), MsgcoreHandle_Vect);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgListHandle
msgcore_attr_declare_list(MsgAttrHandle hAttr, const wchar_t* lpszName)
{
    if (!toAttr(hAttr) || !lpszName) return nullptr;
    try {
        P3PmsgList oList(lpszName, P3PmsgData((INT32)0));
        *toAttr(hAttr) += oList;
        P3PmsgCurs oCurs(*toAttr(hAttr));
        if (!oCurs.Goto(lpszName) || !oCurs.IsList()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgList(oCurs.r_Object()), MsgcoreHandle_List);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgVectHandle
msgcore_attr_declare_vect(MsgAttrHandle hAttr, const wchar_t* lpszName,
                          int nElems, unsigned char uDataType)
{
    if (!toAttr(hAttr) || !lpszName || nElems < 0) return nullptr;
    try {
        P3PmsgVect oVect(nElems, lpszName, MsgcoreProtoData(uDataType));
        *toAttr(hAttr) += oVect;
        P3PmsgCurs oCurs(*toAttr(hAttr));
        if (!oCurs.Goto(lpszName) || !oCurs.IsVect()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgVect(oCurs.r_Object()), MsgcoreHandle_Vect);
    } catch (...) { return nullptr; }
}

// --- live typed read-back --------------------------------------------------
// All six go through a P3PmsgCurs rather than the Select* family, and that is
// a correctness decision, not a stylistic one. Three separate traps rule the
// obvious spellings out:
//
//   SelectItem THROWS on a container node -- a list is not an "item" -- so
//   `SelectItem(name).r_Object().IsList()` can never answer true; it raises,
//   and a catch-all turns that into "no such list" for a list plainly there.
//   (The same trap is why msgcore_field_child, which is SelectItem-based,
//   returns NULL for a child that is a list or a vect. Use these instead.)
//
//   SelectObject answers correctly but SelectList/SelectVect still fail on a
//   P3PmsgDesc built by msgcore_desc_from_field. That constructor connects
//   with `Connectx(..., aVBLockDesc, 0)` -- a zero list size -- which leaves
//   GetCount and Exists working (they read the block header) while the typed
//   selectors do not. The field's own r_Desc() is connected properly, so the
//   same call succeeds there and fails here: a difference no caller of this
//   ABI can see or work around.
//
// A cursor is immune to both: Goto + IsList/IsVect + r_Object was verified to
// work on a desc from EITHER source. Kind is checked before wrapping, so
// "NULL when it is not a container of that kind" is true rather than
// aspirational.

MSGCORE_C_API MsgListHandle
msgcore_field_select_list(MsgFieldHandle hField, const wchar_t* lpszName)
{
    if (!toField(hField) || !lpszName) return nullptr;
    try {
        P3PmsgCurs oCurs(toField(hField)->r_Desc());
        if (!oCurs.Goto(lpszName) || !oCurs.IsList()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgList(oCurs.r_Object()), MsgcoreHandle_List);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgVectHandle
msgcore_field_select_vect(MsgFieldHandle hField, const wchar_t* lpszName)
{
    if (!toField(hField) || !lpszName) return nullptr;
    try {
        P3PmsgCurs oCurs(toField(hField)->r_Desc());
        if (!oCurs.Goto(lpszName) || !oCurs.IsVect()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgVect(oCurs.r_Object()), MsgcoreHandle_Vect);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgListHandle
msgcore_desc_select_list(MsgDescHandle hDesc, const wchar_t* lpszName)
{
    if (!toDesc(hDesc) || !lpszName) return nullptr;
    try {
        P3PmsgCurs oCurs(*toDesc(hDesc));
        if (!oCurs.Goto(lpszName) || !oCurs.IsList()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgList(oCurs.r_Object()), MsgcoreHandle_List);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgVectHandle
msgcore_desc_select_vect(MsgDescHandle hDesc, const wchar_t* lpszName)
{
    if (!toDesc(hDesc) || !lpszName) return nullptr;
    try {
        P3PmsgCurs oCurs(*toDesc(hDesc));
        if (!oCurs.Goto(lpszName) || !oCurs.IsVect()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgVect(oCurs.r_Object()), MsgcoreHandle_Vect);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgListHandle
msgcore_attr_select_list(MsgAttrHandle hAttr, const wchar_t* lpszName)
{
    if (!toAttr(hAttr) || !lpszName) return nullptr;
    try {
        P3PmsgCurs oCurs(*toAttr(hAttr));
        if (!oCurs.Goto(lpszName) || !oCurs.IsList()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgList(oCurs.r_Object()), MsgcoreHandle_List);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgVectHandle
msgcore_attr_select_vect(MsgAttrHandle hAttr, const wchar_t* lpszName)
{
    if (!toAttr(hAttr) || !lpszName) return nullptr;
    try {
        P3PmsgCurs oCurs(*toAttr(hAttr));
        if (!oCurs.Goto(lpszName) || !oCurs.IsVect()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgVect(oCurs.r_Object()), MsgcoreHandle_Vect);
    } catch (...) { return nullptr; }
}


// --- list: the widths the original block omitted ---------------------------

MSGCORE_C_API void
msgcore_list_add_head_int64(MsgListHandle hList, long long value)
{
    if (toList(hList)) try { toList(hList)->AddListHead(P3PmsgData((INT64)value)); } catch (...) {}
}

MSGCORE_C_API void
msgcore_list_add_tail_int64(MsgListHandle hList, long long value)
{
    if (toList(hList)) try { toList(hList)->AddListTail(P3PmsgData((INT64)value)); } catch (...) {}
}

MSGCORE_C_API void
msgcore_list_add_head_bool(MsgListHandle hList, int bValue)
{
    if (toList(hList)) try { toList(hList)->AddListHead(P3PmsgData(bValue ? true : false)); } catch (...) {}
}

MSGCORE_C_API void
msgcore_list_add_tail_bool(MsgListHandle hList, int bValue)
{
    if (toList(hList)) try { toList(hList)->AddListTail(P3PmsgData(bValue ? true : false)); } catch (...) {}
}

// --- list: indexed element access ------------------------------------------

MSGCORE_C_API unsigned char
msgcore_list_get_type_at(MsgListHandle hList, int nIndex)
{
    if (!toList(hList)) return MSGCORE_DATA_NULL;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        return p ? (unsigned char)p->DataType() : (unsigned char)MSGCORE_DATA_NULL;
    } catch (...) { return MSGCORE_DATA_NULL; }
}

MSGCORE_C_API int
msgcore_list_get_int_at(MsgListHandle hList, int nIndex)
{
    if (!toList(hList)) return 0;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        return p ? p->c_int() : 0;
    } catch (...) { return 0; }
}

MSGCORE_C_API long long
msgcore_list_get_int64_at(MsgListHandle hList, int nIndex)
{
    if (!toList(hList)) return 0LL;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        return p ? (long long)p->c_int64() : 0LL;
    } catch (...) { return 0LL; }
}

MSGCORE_C_API double
msgcore_list_get_double_at(MsgListHandle hList, int nIndex)
{
    if (!toList(hList)) return 0.0;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        return p ? p->c_double() : 0.0;
    } catch (...) { return 0.0; }
}

MSGCORE_C_API int
msgcore_list_get_bool_at(MsgListHandle hList, int nIndex)
{
    if (!toList(hList)) return 0;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        return (p && p->c_bool()) ? 1 : 0;
    } catch (...) { return 0; }
}

MSGCORE_C_API const wchar_t*
msgcore_list_get_wstr_at(MsgListHandle hList, int nIndex)
{
    // The cell's own c_wstr() points into the cell, which the next list
    // mutation may move; copy into this thread's buffer so the documented
    // "valid until the next call on this thread" contract actually holds.
    static thread_local std::wstring s_strCell;
    if (!toList(hList)) return nullptr;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        if (!p) return nullptr;
        LPCWSTR psz = p->c_wstr();
        if (!psz) return nullptr;
        s_strCell = psz;
        return s_strCell.c_str();
    } catch (...) { return nullptr; }
}

MSGCORE_C_API int
msgcore_list_set_int_at(MsgListHandle hList, int nIndex, int value)
{
    if (!toList(hList)) return 0;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        if (!p) return 0;
        p->c_int(value);
        return 1;
    } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_list_set_int64_at(MsgListHandle hList, int nIndex, long long value)
{
    if (!toList(hList)) return 0;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        if (!p) return 0;
        p->c_int64((INT64)value);
        return 1;
    } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_list_set_double_at(MsgListHandle hList, int nIndex, double value)
{
    if (!toList(hList)) return 0;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        if (!p) return 0;
        p->c_double(value);
        return 1;
    } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_list_set_bool_at(MsgListHandle hList, int nIndex, int bValue)
{
    if (!toList(hList)) return 0;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        if (!p) return 0;
        p->c_bool(bValue ? true : false);
        return 1;
    } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_list_set_wstr_at(MsgListHandle hList, int nIndex, const wchar_t* lpszValue)
{
    if (!toList(hList) || !lpszValue) return 0;
    // Same 0xFFFF WSTR16 cap as msgcore_field_declare_wstr: reject before the
    // core throws from inside the heap copy and orphans the cell.
    if (msgcore_wstr16_units(lpszValue) > 32767) return 0;
    try {
        P3PmsgData* p = MsgcoreListCellAt(toList(hList), nIndex);
        if (!p) return 0;
        p->c_wcscpy(lpszValue);
        return 1;
    } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_list_delete_at(MsgListHandle hList, int nIndex)
{
    if (!toList(hList) || nIndex < 0) return 0;
    try {
        P3PmsgList* pList = toList(hList);
        VBLaddr aPos = pList->GetHeadPos();
        for (int i = 0; i < nIndex && aPos; ++i)
            pList->GetNext(aPos);
        if (!aPos) return 0;
        return pList->Delete(aPos) ? 1 : 0;
    } catch (...) { return 0; }
}

// --- vect: the count, and the accessors the original block omitted ---------

MSGCORE_C_API int
msgcore_vect_get_count(MsgVectHandle hVect)
{
    if (!toVect(hVect)) return 0;
    try { return (int)toVect(hVect)->GetCount(); } catch (...) { return 0; }
}

MSGCORE_C_API unsigned char
msgcore_vect_get_type(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return MSGCORE_DATA_NULL;
    try { return (unsigned char)toVect(hVect)->r_data(nElem).DataType(); }
    catch (...) { return MSGCORE_DATA_NULL; }
}

MSGCORE_C_API long long
msgcore_vect_get_int64(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return 0LL;
    try { return (long long)toVect(hVect)->r_data(nElem).c_int64(); }
    catch (...) { return 0LL; }
}

MSGCORE_C_API int
msgcore_vect_get_bool(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return 0;
    try { return toVect(hVect)->r_data(nElem).c_bool() ? 1 : 0; }
    catch (...) { return 0; }
}

MSGCORE_C_API const wchar_t*
msgcore_vect_get_name(MsgVectHandle hVect, int nElem)
{
    static thread_local std::wstring s_strName;
    if (!toVect(hVect)) return nullptr;
    try {
        LPCWSTR psz = toVect(hVect)->r_name(nElem).c_name();
        if (!psz) return nullptr;
        s_strName = psz;
        return s_strName.c_str();
    } catch (...) { return nullptr; }
}

MSGCORE_C_API int
msgcore_vect_set_int(MsgVectHandle hVect, int nElem, int value)
{
    if (!toVect(hVect)) return 0;
    try { toVect(hVect)->r_data(nElem).c_int(value); return 1; }
    catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_vect_set_int64(MsgVectHandle hVect, int nElem, long long value)
{
    if (!toVect(hVect)) return 0;
    try { toVect(hVect)->r_data(nElem).c_int64((INT64)value); return 1; }
    catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_vect_set_double(MsgVectHandle hVect, int nElem, double value)
{
    if (!toVect(hVect)) return 0;
    try { toVect(hVect)->r_data(nElem).c_double(value); return 1; }
    catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_vect_set_bool(MsgVectHandle hVect, int nElem, int bValue)
{
    if (!toVect(hVect)) return 0;
    try { toVect(hVect)->r_data(nElem).c_bool(bValue ? true : false); return 1; }
    catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_vect_set_wstr(MsgVectHandle hVect, int nElem, const wchar_t* lpszValue)
{
    if (!toVect(hVect) || !lpszValue) return 0;
    if (msgcore_wstr16_units(lpszValue) > 32767) return 0;
    try { toVect(hVect)->r_data(nElem).c_wcscpy(lpszValue); return 1; }
    catch (...) { return 0; }
}

MSGCORE_C_API MsgListHandle
msgcore_vect_get_list(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return nullptr;
    try {
        if (!toVect(hVect)->IsList(nElem)) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgList(toVect(hVect)->r_list(nElem).r_Object()), MsgcoreHandle_List);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgVectHandle
msgcore_vect_get_vect(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect)) return nullptr;
    try {
        if (!toVect(hVect)->IsVect(nElem)) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgVect(toVect(hVect)->r_vect(nElem).r_Object()), MsgcoreHandle_Vect);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API int
msgcore_vect_insert_at(MsgVectHandle hVect, int nElem, MsgFieldHandle hField)
{
    if (!toVect(hVect) || !toField(hField) || nElem < 0) return 0;
    try { toVect(hVect)->InsertAt(nElem, *toField(hField)); return 1; }
    catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_vect_delete(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect) || nElem < 0) return 0;
    try { return toVect(hVect)->Delete(nElem) ? 1 : 0; } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_vect_goto(MsgVectHandle hVect, int nElem)
{
    if (!toVect(hVect) || nElem < 0) return -1;
    try { return toVect(hVect)->Goto(nElem); } catch (...) { return -1; }
}

// ---------------------------------------------------------------------------
// MsgStck -- the field position stack
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgStckHandle
msgcore_stck_create(void)
{
    try { return MsgcoreHandleAdd(new MsgStck(), MsgcoreHandle_Stck); } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgStckHandle
msgcore_stck_from_field(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    try { return MsgcoreHandleAdd(new MsgStck(toField(hField)), MsgcoreHandle_Stck); } catch (...) { return nullptr; }
}

MSGCORE_C_API void
msgcore_stck_destroy(MsgStckHandle hStck)
{
    MsgStck* p = toStck(hStck);
    // Forget first, and act only on the caller that removed the entry -
    // a double destroy then deletes once and refuses the second time.
    if (!p || !MsgcoreHandleForget(hStck, MsgcoreHandle_Stck)) return;
    delete p;
}

MSGCORE_C_API void
msgcore_stck_connect(MsgStckHandle hStck, MsgFieldHandle hField)
{
    // BOTH handles are checked. hField was the one entry point that used a
    // handle it never guarded; unchecked it would now reach Connect() as the
    // nullptr toField() answers for a bogus one.
    if (toStck(hStck) && toField(hField)) toStck(hStck)->Connect(toField(hField));
}

MSGCORE_C_API void
msgcore_stck_nullify(MsgStckHandle hStck)
{
    if (toStck(hStck)) toStck(hStck)->Nullify();
}

// MsgStck::Push and MsgStck::Pop both dereference m_pP3PmsgField with no null
// check of their own -- unlike Drop, Rename and r_item, which all guard -- so
// pushing or popping a stack that was created but never connected is a null
// dereference inside the core, not an exception a catch-all could absorb.
// GetField() is the published, noexcept way to ask, and answering 0 here is
// what makes msgcore_stck_create() safe to hand to a caller before Connect.
MSGCORE_C_API int
msgcore_stck_push(MsgStckHandle hStck)
{
    if (!toStck(hStck) || toStck(hStck)->GetField() == nullptr) return 0;
    try { toStck(hStck)->Push(); return 1; } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_stck_pop(MsgStckHandle hStck)
{
    if (!toStck(hStck) || toStck(hStck)->GetField() == nullptr) return 0;
    // Pop on a connected-but-empty stack is a silent no-op in the core (it
    // returns *this when nothing is stacked), so this cannot distinguish
    // "restored something" from "there was nothing" -- see the header.
    try { toStck(hStck)->Pop(); return 1; } catch (...) { return 0; }
}

MSGCORE_C_API void
msgcore_stck_drop(MsgStckHandle hStck)
{
    if (toStck(hStck)) try { toStck(hStck)->Drop(); } catch (...) {}
}

MSGCORE_C_API int
msgcore_stck_rename(MsgStckHandle hStck, const wchar_t* lpszName, int bRecurse)
{
    if (!toStck(hStck) || !lpszName) return 0;
    try { toStck(hStck)->Rename(lpszName, bRecurse ? true : false); return 1; }
    catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_stck_is_empty(MsgStckHandle hStck)
{
    if (!toStck(hStck)) return 1;
    // MsgStck::IsEmpty answers FALSE for an unconnected stack -- `if (m_pP3P-
    // msgField == nullptr) return false;` -- i.e. "not empty" for a stack that
    // holds nothing and is attached to nothing. Reporting that verbatim would
    // make the obvious `while (!is_empty) pop;` spin on a stack that can never
    // become empty. Unconnected is empty.
    if (toStck(hStck)->GetField() == nullptr) return 1;
    try { return toStck(hStck)->IsEmpty() ? 1 : 0; } catch (...) { return 1; }
}

MSGCORE_C_API const wchar_t*
msgcore_stck_get_name(MsgStckHandle hStck)
{
    static thread_local std::wstring s_strName;
    if (!toStck(hStck)) return nullptr;
    try {
        LPCWSTR psz = toStck(hStck)->r_name().c_name();
        if (!psz) return nullptr;
        s_strName = psz;
        return s_strName.c_str();
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgFieldHandle
msgcore_stck_get_field(MsgStckHandle hStck)
{
    if (!toStck(hStck)) return nullptr;
    try {
        P3PmsgField* pField = toStck(hStck)->GetField();
        if (!pField) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgField(pField->r_Object()), MsgcoreHandle_Field);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgFieldHandle
msgcore_stck_get_item(MsgStckHandle hStck)
{
    if (!toStck(hStck)) return nullptr;
    try { return MsgcoreHandleAdd(new P3PmsgField(toStck(hStck)->r_item().r_Object()), MsgcoreHandle_Field); }
    catch (...) { return nullptr; }
}

MSGCORE_C_API MsgListHandle
msgcore_stck_get_list(MsgStckHandle hStck)
{
    if (!toStck(hStck)) return nullptr;
    try { return MsgcoreHandleAdd(new P3PmsgList(toStck(hStck)->r_list().r_Object()), MsgcoreHandle_List); }
    catch (...) { return nullptr; }
}

MSGCORE_C_API MsgVectHandle
msgcore_stck_get_vect(MsgStckHandle hStck)
{
    if (!toStck(hStck)) return nullptr;
    try { return MsgcoreHandleAdd(new P3PmsgVect(toStck(hStck)->r_vect().r_Object()), MsgcoreHandle_Vect); }
    catch (...) { return nullptr; }
}

// ---------------------------------------------------------------------------
// P2PmsgRecurs -- the recursive subtree walker
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgRecursHandle
msgcore_recurs_from_field(MsgFieldHandle hField)
{
    if (!toField(hField)) return nullptr;
    try { return MsgcoreHandleAdd(new P2PmsgRecurs(*toField(hField)), MsgcoreHandle_Recurs); } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgRecursHandle
msgcore_recurs_from_attr(MsgAttrHandle hAttr)
{
    if (!toAttr(hAttr)) return nullptr;
    try { return MsgcoreHandleAdd(new P2PmsgRecurs(*toAttr(hAttr)), MsgcoreHandle_Recurs); } catch (...) { return nullptr; }
}

MSGCORE_C_API void
msgcore_recurs_destroy(MsgRecursHandle hRecurs)
{
    P2PmsgRecurs* p = toRecurs(hRecurs);
    // Forget first, and act only on the caller that removed the entry -
    // a double destroy then deletes once and refuses the second time.
    if (!p || !MsgcoreHandleForget(hRecurs, MsgcoreHandle_Recurs)) return;
    delete p;
}

MSGCORE_C_API void
msgcore_recurs_next(MsgRecursHandle hRecurs)
{
    if (toRecurs(hRecurs)) try { ++(*toRecurs(hRecurs)); } catch (...) {}
}

MSGCORE_C_API int
msgcore_recurs_push(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return -1;
    // Push throws when the current element is not descendable; -1 reports that
    // as a value so a caller may attempt-and-check instead of pre-testing.
    try { return toRecurs(hRecurs)->Push(); } catch (...) { return -1; }
}

MSGCORE_C_API int
msgcore_recurs_pop(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return -1;
    try { return toRecurs(hRecurs)->Pop(); } catch (...) { return -1; }
}

MSGCORE_C_API void
msgcore_recurs_break(MsgRecursHandle hRecurs)
{
    if (toRecurs(hRecurs)) try { toRecurs(hRecurs)->Break(); } catch (...) {}
}

MSGCORE_C_API int
msgcore_recurs_is_eo_recurs(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return 1;
    try { return toRecurs(hRecurs)->IsEoRecurs() ? 1 : 0; } catch (...) { return 1; }
}

MSGCORE_C_API int
msgcore_recurs_is_field(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return 0;
    try { return toRecurs(hRecurs)->IsField() ? 1 : 0; } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_recurs_is_list(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return 0;
    try { return toRecurs(hRecurs)->IsList() ? 1 : 0; } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_recurs_is_vect(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return 0;
    try { return toRecurs(hRecurs)->IsVect() ? 1 : 0; } catch (...) { return 0; }
}

MSGCORE_C_API const wchar_t*
msgcore_recurs_get_name(MsgRecursHandle hRecurs)
{
    static thread_local std::wstring s_strName;
    if (!toRecurs(hRecurs)) return nullptr;
    try {
        LPCTNAM psz = toRecurs(hRecurs)->c_wstr();
        if (!psz) return nullptr;
        s_strName = psz;
        return s_strName.c_str();
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgFieldHandle
msgcore_recurs_get_item(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return nullptr;
    try { return MsgcoreHandleAdd(new P3PmsgField(toRecurs(hRecurs)->r_item().r_Object()), MsgcoreHandle_Field); }
    catch (...) { return nullptr; }
}

MSGCORE_C_API MsgListHandle
msgcore_recurs_get_list(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return nullptr;
    try {
        if (!toRecurs(hRecurs)->IsList()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgList(toRecurs(hRecurs)->r_list().r_Object()), MsgcoreHandle_List);
    } catch (...) { return nullptr; }
}

MSGCORE_C_API MsgVectHandle
msgcore_recurs_get_vect(MsgRecursHandle hRecurs)
{
    if (!toRecurs(hRecurs)) return nullptr;
    try {
        if (!toRecurs(hRecurs)->IsVect()) return nullptr;
        return MsgcoreHandleAdd(new P3PmsgVect(toRecurs(hRecurs)->r_vect().r_Object()), MsgcoreHandle_Vect);
    } catch (...) { return nullptr; }
}

// ---------------------------------------------------------------------------
// Paging
// ---------------------------------------------------------------------------

MSGCORE_C_API int
msgcore_mgr_page_registration_push(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return 0;
    try { toMgr(hMgr)->PageRegistrationPush(); return 1; } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_mgr_page_registration_pop(MsgMgrHandle hMgr)
{
    if (!toMgr(hMgr)) return 0;
    try { toMgr(hMgr)->PageRegistrationPop(); return 1; } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_mgr_page_dataset_in(MsgMgrHandle hMgr, unsigned long long p2pos)
{
    if (!toMgr(hMgr)) return 0;
    try { return toMgr(hMgr)->PageDatasetIn((P2Pos)p2pos) ? 1 : 0; }
    catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_mgr_page_dataset_out(MsgMgrHandle hMgr, unsigned long long p2pos, int bFlush)
{
    if (!toMgr(hMgr)) return 0;
    try { return toMgr(hMgr)->PageDatasetOut((P2Pos)p2pos, bFlush ? TRUE : FALSE) ? 1 : 0; }
    catch (...) { return 0; }
}

MSGCORE_C_API unsigned int
msgcore_mgr_page_summ(MsgMgrHandle hMgr, MsgFieldHandle hItem,
                      unsigned int nAdditions, unsigned int nRemovals)
{
    if (!toMgr(hMgr) || !toField(hItem)) return 0;
    try { return (unsigned int)toMgr(hMgr)->PageSumm(*toField(hItem),
                                                     (DWORD)nAdditions, (DWORD)nRemovals); }
    catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_mgr_set_paging_sinks(MsgMgrHandle hMgr, msgcore_pagein_fn pfnIn,
                             msgcore_pageout_fn pfnOut, void* pUser)
{
    if (!toMgr(hMgr)) return 0;
    P2PmsgMgr* pMgr = toMgr(hMgr);
    try {
        std::lock_guard<std::mutex> lock(s_mtxPaging);
        if (!pfnIn && !pfnOut)
        {
            // Clearing: unregister at the core FIRST, so no in-flight call can
            // reach a record that is about to be erased.
            pMgr->PageRegistration((PINT_PTR)nullptr, nullptr, nullptr);
            std::map<P2PmsgMgr*, MsgcorePagingRec>::iterator it = s_mapPaging.find(pMgr);
            if (it != s_mapPaging.end() && !it->second.pfnPopulate)
                s_mapPaging.erase(it);          // keep it if populate still uses it
            else if (it != s_mapPaging.end())
            { it->second.pfnIn = nullptr; it->second.pfnOut = nullptr; }
            return 1;
        }
        MsgcorePagingRec& rec = s_mapPaging[pMgr];   // std::map: reference-stable
        rec.pfnIn      = pfnIn;
        rec.pfnOut     = pfnOut;
        rec.pvUserPage = pUser;
        // PageRegistration returns FALSE unconditionally in the core -- both
        // overloads end `return FALSE;` -- so its result says nothing about
        // success and is deliberately not propagated.
        pMgr->PageRegistration((PINT_PTR)&rec,
                               pfnIn  ? MsgcorePageinTramp  : nullptr,
                               pfnOut ? MsgcorePageoutTramp : nullptr);
        return 1;
    } catch (...) { return 0; }
}

MSGCORE_C_API int
msgcore_mgr_set_populate_sink(MsgMgrHandle hMgr, msgcore_populate_fn pfnPopulate,
                              void* pUser)
{
    if (!toMgr(hMgr)) return 0;
    P2PmsgMgr* pMgr = toMgr(hMgr);
    try {
        std::lock_guard<std::mutex> lock(s_mtxPaging);
        if (!pfnPopulate)
        {
            pMgr->PageRegistration((PINT_PTR)nullptr, (P2PopulateCBFnc)nullptr);
            std::map<P2PmsgMgr*, MsgcorePagingRec>::iterator it = s_mapPaging.find(pMgr);
            if (it != s_mapPaging.end() && !it->second.pfnIn && !it->second.pfnOut)
                s_mapPaging.erase(it);
            else if (it != s_mapPaging.end())
                it->second.pfnPopulate = nullptr;
            return 1;
        }
        MsgcorePagingRec& rec = s_mapPaging[pMgr];
        rec.pfnPopulate    = pfnPopulate;
        rec.pvUserPopulate = pUser;
        pMgr->PageRegistration((PINT_PTR)&rec, MsgcorePopulateTramp);
        return 1;
    } catch (...) { return 0; }
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

MSGCORE_C_API int
msgcore_wildcard_match(const wchar_t* lpszWildcard, const wchar_t* lpszName)
{
    if (!lpszWildcard || !lpszName) return 0;
    return MsgcoreWildcard(lpszWildcard, lpszName) ? 1 : 0;
}

} // extern "C"
