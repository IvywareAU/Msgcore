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
// MsgcoreSuite.cpp
//
// Unit tests for the Msgcore data-model library: the typed value cell
// (P3PmsgData), the named field (P3PmsgField), the container types
// (P3PmsgList / P3PmsgVect / P3PmsgAttr / P3PmsgDesc), the value stack
// (MsgStck), the event/exception builder (P2Pevent) and the DATE
// normalisation helpers.
//
// These exercise the same surface the legacy MsgcoreTests.cpp smoke code
// touched, but as isolated, asserted cases.

// Ported from MscsUnitTests for the release-readiness register, item 15 (Stage E). The
// cases are carried unchanged except for the two COleTime ones, which are
// dropped rather than stubbed -- see the note at the deletion site. The
// precompiled-header include is replaced because this repository's test build
// has no PCH and pulls the MFC headers directly, as tests/C4LoadTest.cpp does.
#include <afx.h>
#include <afxwin.h>

#include "P2Pmsg.h"
#include "P2Pmsg_Ext.h"
#include "MsgAttr.h"
#include "MsgDesc.h"
#include "MsgStck.h"
#include "MsgList.h"
#include "MsgVect.h"
#include "MsgCurs.h"
#include "Msgexception.h"
#include "P2PmsgMgr.h"
#include "P2PmsgBSTR.h"
#include "MsgVBHeap.h"                 // Test_ImageAddressBounds (F8/F9)
// "COleTime_Ext.h" -- NOT included; see the Test_DateNormalisation note below.

#include "TestFramework.h"

#include <cmath>
// Upstream got <string> transitively from its stdafx.h; this build has no PCH,
// so the std::wstring the name-bound cases use has to be asked for by name.
#include <string>

using namespace std;

// ---------------------------------------------------------------------------
// P3PmsgData : typed value cell
// ---------------------------------------------------------------------------
static void Test_Data_TypedValues()
{
    TF_CASE("P3PmsgData holds and round-trips each scalar type")
    {
        P3PmsgData oChar = (char)1;
        oChar.c_char(2);
        TF_CHECK(oChar.c_char() == 2);

        P3PmsgData oShort = (short)1;
        oShort.c_short(7);
        TF_CHECK(oShort.c_short() == 7);

        P3PmsgData oInt = (int)1;
        oInt.c_int(12345);
        TF_CHECK(oInt.c_int() == 12345);

        P3PmsgData oDouble = (double)1.0;
        oDouble.c_double(2.5);
        TF_CHECK(oDouble.c_double() == 2.5);

        P3PmsgData oBool = true;
        TF_CHECK(oBool.c_bool() == true);
        oBool.c_bool(false);
        TF_CHECK(oBool.c_bool() == false);
    }

    TF_CASE("P3PmsgData assignment copies the value and preserves type")
    {
        P3PmsgData oSrc = (int)99;
        P3PmsgData oDst;
        oDst = oSrc;
        TF_CHECK(oDst.c_int() == 99);
        TF_CHECK(oDst.DataType() == oSrc.DataType());
    }

    TF_CASE("P3PmsgData exposes a wide-string value")
    {
        P3PmsgData oStr = L"Hello";
        LPCWSTR pszValue = oStr.c_wstr();
        TF_CHECK(pszValue != nullptr);
        TF_CHECK(wcscmp(pszValue, L"Hello") == 0);
    }
}

// ---------------------------------------------------------------------------
// P3PmsgField : named field = name + data (+ optional Attr/Desc/Stack)
// ---------------------------------------------------------------------------
static void Test_Field_NameAndData()
{
    TF_CASE("P3PmsgField compares equal to its name")
    {
        P3PmsgField oField(L"Johnno");
        TF_CHECK(oField == L"Johnno");
        TF_CHECK(!(oField == L"Somebody"));
    }

    TF_CASE("assigning data to a field changes its data type")
    {
        P3PmsgField oField(L"Johnno");
        int uTypeBefore = oField.DataType();
        oField = P3PmsgData((int)1);
        int uTypeAfter = oField.DataType();
        oField.c_int(42);
        TF_CHECK(oField.c_int() == 42);
        TF_CHECK(uTypeBefore != uTypeAfter);
    }

    TF_CASE("copy-constructed field carries the same data")
    {
        // A field must be given a data cell (here INT) before c_int() may
        // write into it; c_int() on a name-only field has no sized cell.
        P3PmsgField oField(L"Bill");
        oField = P3PmsgData((int)1);
        TF_CHECK(oField.c_int() == 1);

        P3PmsgField oCopy = oField;
        oCopy.AssertValid();
        TF_CHECK(oCopy.c_int() == 1);
    }

    TF_CASE("assigning a name renames the field while keeping its data cell")
    {
        P3PmsgName  oName(L"Larry");
        P3PmsgField oField(L"Bill");
        oField = P3PmsgData((int)0);  // establish an INT data cell
        oField = oName;               // rename only -- cell survives
        oField.c_int(4);
        TF_CHECK(oField == L"Larry");
        TF_CHECK(oField.c_int() == 4);
    }

    TF_CASE("DeclareItem / SelectItem / Exists on a field's descendants")
    {
        P3PmsgField oRoot(L"Root");
        oRoot.DeclareItem(L"Age", P3PmsgData((int)30));
        TF_CHECK(oRoot.Exists(L"Age"));
        TF_CHECK(!oRoot.Exists(L"Missing"));
        TF_CHECK(oRoot.SelectItem(L"Age").c_int() == 30);
    }

    TF_CASE("field name honours the 63-UTF-16-unit bound; overrun throws cleanly (astral counts as 2 units)")
    {
        // The inline name field holds 63 P2PWCHAR units. c_name() enforces this on
        // the live object, so a rejected overrun leaves the prior name intact (no
        // partial write / corruption) -- proven by the post-throw checks. The bound
        // is measured in UTF-16 UNITS, not code points: one astral code point is two
        // units on both OSes (surrogate pair on Windows, an encoded pair from the
        // UTF-32 wchar on the Linux port). c_size() returns the stored unit count.
        auto rejects = []( P3PmsgName& n, const wchar_t* s ) -> bool {
            try { n.c_name( s, 0 ); return false; }
            catch ( P2Pevent* pEVT ) { if ( pEVT ) pEVT->Cancel(false); return true; }  // false: discard silently (no Display/MessageBox on Windows)
        };

        // 63 units: in bounds.
        P3PmsgName oBmp(L"seed");
        oBmp.c_name( std::wstring(63, L'a').c_str(), 0 );
        TF_CHECK(oBmp.c_size() == 63);

        // 64 units: overruns -> clean throw; the live name is untouched.
        P3PmsgName oBmpBad(L"keep");                        // 4 units
        TF_CHECK(rejects( oBmpBad, std::wstring(64, L'a').c_str() ));
        TF_CHECK(oBmpBad.c_size() == 4 && oBmpBad == L"keep");

        // 31 astral chars = 62 units (in bounds); 32 astral chars = 64 units (overruns).
        std::wstring a31; for ( int i = 0; i < 31; ++i ) a31 += L"\U0001F680";
        P3PmsgName oAstral(L"seed");
        oAstral.c_name( a31.c_str(), 0 );
        TF_CHECK(oAstral.c_size() == 62);

        std::wstring a32; for ( int i = 0; i < 32; ++i ) a32 += L"\U0001F680";
        P3PmsgName oAstralBad(L"keep");
        TF_CHECK(rejects( oAstralBad, a32.c_str() ));
        TF_CHECK(oAstralBad.c_size() == 4);
    }
}

// ---------------------------------------------------------------------------
// P3PmsgList : ordered list of data cells
// ---------------------------------------------------------------------------
static void Test_List()
{
    TF_CASE("AddListTail grows the count")
    {
        P3PmsgList oList;
        TF_CHECK_EQ((int)oList.GetCount(), 0);
        oList.AddListTail(P3PmsgData((int)1));
        oList.AddListTail(P3PmsgData((int)2));
        oList.AddListTail(P3PmsgData("Johnno"));
        TF_CHECK_EQ((int)oList.GetCount(), 3);
    }

    TF_CASE("operator += appends like AddListTail")
    {
        P3PmsgList oList;
        oList += P3PmsgData("Susan");
        oList += P3PmsgData("Whoever");
        TF_CHECK_EQ((int)oList.GetCount(), 2);
    }

    TF_CASE("forward iteration visits every element")
    {
        P3PmsgList oList;
        oList.AddListTail(P3PmsgData((int)10));
        oList.AddListTail(P3PmsgData((int)20));
        oList.AddListTail(P3PmsgData((int)30));

        int nVisited = 0;
        int nSum     = 0;
        VBLaddr aPos = oList.GetHeadPos();
        while (aPos)
        {
            P3PmsgData& oData = oList.GetNext(aPos);
            nSum += oData.c_int();
            ++nVisited;
        }
        TF_CHECK_EQ(nVisited, 3);
        TF_CHECK_EQ(nSum, 60);
    }

    TF_CASE("DropTail shrinks the count")
    {
        P3PmsgList oList;
        oList.AddListTail(P3PmsgData((int)1));
        oList.AddListTail(P3PmsgData((int)2));
        oList.DropTail();
        TF_CHECK_EQ((int)oList.GetCount(), 1);
    }

    // AddListHead once omitted the VBLockItem_Init(..., VBLock_Data) its twin
    // AddListTail makes, so the prepended item was linked in with no type tag
    // and the next read of it tripped the ASSERT(0) in VBLockItem_pData.
    TF_CASE("AddListHead prepends a readable item")
    {
        P3PmsgList oList;
        oList.AddListTail(P3PmsgData((int)20));
        oList.AddListTail(P3PmsgData((int)30));
        oList.AddListHead(P3PmsgData((int)10));
        TF_CHECK_EQ((int)oList.GetCount(), 3);

        VBLaddr aPos = oList.GetHeadPos();
        TF_CHECK_EQ(oList.GetNext(aPos).c_int(), 10);
        TF_CHECK_EQ(oList.GetNext(aPos).c_int(), 20);
        TF_CHECK_EQ(oList.GetNext(aPos).c_int(), 30);
        TF_CHECK_EQ(oList.GetTail().c_int(), 30);
    }

    TF_CASE("AddListHead into an empty list is also the tail")
    {
        P3PmsgList oList;
        oList.AddListHead(P3PmsgData((int)42));
        TF_CHECK_EQ((int)oList.GetCount(), 1);
        TF_CHECK_EQ(oList.GetTail().c_int(), 42);
        TF_CHECK(oList.GetHeadPos() == oList.GetTailPos());
    }

    // GetPrev was declared in MsgList.h but never defined -- an unresolved
    // external for any caller. It is the mirror of GetNext: it steps back over
    // the cell it returns, and the position falls to 0 past the head.
    TF_CASE("backward iteration visits every element in reverse")
    {
        P3PmsgList oList;
        oList.AddListTail(P3PmsgData((int)10));
        oList.AddListTail(P3PmsgData((int)20));
        oList.AddListTail(P3PmsgData((int)30));

        int nVisited = 0;
        int nSum     = 0;
        int aSeen[3] = { 0, 0, 0 };
        VBLaddr aPos = oList.GetTailPos();
        while (aPos)
        {
            P3PmsgData& oData = oList.GetPrev(aPos);
            if (nVisited < 3)
                aSeen[nVisited] = oData.c_int();
            nSum += oData.c_int();
            ++nVisited;
        }
        TF_CHECK_EQ(nVisited, 3);
        TF_CHECK_EQ(nSum, 60);
        TF_CHECK_EQ(aSeen[0], 30);
        TF_CHECK_EQ(aSeen[1], 20);
        TF_CHECK_EQ(aSeen[2], 10);
    }

    TF_CASE("a single-element list walks both ways")
    {
        P3PmsgList oList;
        oList.AddListTail(P3PmsgData((int)7));

        VBLaddr aFwd = oList.GetHeadPos();
        TF_CHECK_EQ(oList.GetNext(aFwd).c_int(), 7);
        TF_CHECK(aFwd == 0);

        VBLaddr aRev = oList.GetTailPos();
        TF_CHECK_EQ(oList.GetPrev(aRev).c_int(), 7);
        TF_CHECK(aRev == 0);
    }
}

// ---------------------------------------------------------------------------
// P3PmsgVect : random-access vector of typed elements
// ---------------------------------------------------------------------------
static void Test_Vect()
{
    TF_CASE("constructed vector has the requested element count")
    {
        P3PmsgVect oVect(3, L"Elem", P3PmsgData((int)0));
        TF_CHECK_EQ((int)oVect.GetCount(), 3);
    }

    TF_CASE("elements are addressable and independently mutable by index")
    {
        P3PmsgVect oVect(3, L"Elem", P3PmsgData((int)0));
        oVect.r_data(0).c_int(100);
        oVect.r_data(1).c_int(200);
        oVect.r_data(2).c_int(300);

        TF_CHECK(oVect.r_data(0).c_int() == 100);
        TF_CHECK(oVect.r_data(1).c_int() == 200);
        TF_CHECK(oVect.r_data(2).c_int() == 300);
        TF_CHECK(oVect.IsField(0));   // elements are named fields, not bare data cells
    }

    TF_CASE("out-of-range Goto fails without crashing")
    {
        P3PmsgVect oVect(2, L"Elem", P3PmsgData((int)0));
        TF_CHECK(oVect.Goto(0) != 0);
        TF_CHECK(oVect.Goto(2) == 0);
        TF_CHECK(oVect.Goto(-1) == 0);
    }

    TF_CASE("InsertAt appends and inserts, shifting later elements")
    {
        P3PmsgVect oVect(0, L"Elem", P3PmsgData((int)0));
        TF_CHECK_EQ((int)oVect.GetCount(), 0);
        oVect.InsertAt(0, P3PmsgField(L"e", P3PmsgData((int)10)));   // [10]
        oVect.InsertAt(1, P3PmsgField(L"e", P3PmsgData((int)30)));   // [10,30]
        oVect.InsertAt(1, P3PmsgField(L"e", P3PmsgData((int)20)));   // [10,20,30]
        TF_CHECK_EQ((int)oVect.GetCount(), 3);
        TF_CHECK(oVect.r_data(0).c_int() == 10);
        TF_CHECK(oVect.r_data(1).c_int() == 20);
        TF_CHECK(oVect.r_data(2).c_int() == 30);
    }

    TF_CASE("Delete removes an element and compacts the rest")
    {
        P3PmsgVect oVect(0, L"Elem", P3PmsgData((int)0));
        for (int i = 0; i < 4; i++)
            oVect.InsertAt(i, P3PmsgField(L"e", P3PmsgData((int)(i + 1))));  // [1,2,3,4]
        TF_CHECK(oVect.Delete(1));                                          // [1,3,4]
        TF_CHECK_EQ((int)oVect.GetCount(), 3);
        TF_CHECK(oVect.r_data(0).c_int() == 1);
        TF_CHECK(oVect.r_data(1).c_int() == 3);
        TF_CHECK(oVect.r_data(2).c_int() == 4);
        TF_CHECK(!oVect.Delete(9));                                        // out of range
    }

    TF_CASE("Truncate empties the vector")
    {
        P3PmsgVect oVect(5, L"Elem", P3PmsgData((int)7));
        TF_CHECK_EQ((int)oVect.GetCount(), 5);
        oVect.Truncate();
        TF_CHECK_EQ((int)oVect.GetCount(), 0);
    }

    TF_CASE("copy-assignment deep-copies every element")
    {
        P3PmsgVect oVect(3, L"Elem", P3PmsgData((int)0));
        oVect.r_data(0).c_int(11);
        oVect.r_data(1).c_int(22);
        oVect.r_data(2).c_int(33);

        P3PmsgVect oCopy(0, L"Copy", P3PmsgData((int)0));
        oCopy = oVect;
        TF_CHECK_EQ((int)oCopy.GetCount(), 3);
        TF_CHECK(oCopy.r_data(0).c_int() == 11);
        TF_CHECK(oCopy.r_data(2).c_int() == 33);

        // Mutating the copy must not disturb the original (independent storage).
        oCopy.r_data(0).c_int(99);
        TF_CHECK(oVect.r_data(0).c_int() == 11);
    }

    TF_CASE("overflow past 32 elements spills into aExtra continuation blocks")
    {
        // 70 elements spans the inline aAlloc[32] plus two continuation blocks.
        P3PmsgVect oVect(0, L"Elem", P3PmsgData((int)0));
        for (int i = 0; i < 70; i++)
            oVect.InsertAt(i, P3PmsgField(L"e", P3PmsgData((int)(i * 10))));
        TF_CHECK_EQ((int)oVect.GetCount(), 70);
        TF_CHECK(oVect.r_data(0).c_int()  == 0);
        TF_CHECK(oVect.r_data(31).c_int() == 310);   // last inline slot
        TF_CHECK(oVect.r_data(32).c_int() == 320);   // first continuation slot
        TF_CHECK(oVect.r_data(63).c_int() == 630);   // spans into 2nd continuation
        TF_CHECK(oVect.r_data(69).c_int() == 690);

        // Insert near the boundary shifts elements across the block seam.
        oVect.InsertAt(32, P3PmsgField(L"e", P3PmsgData((int)9999)));
        TF_CHECK_EQ((int)oVect.GetCount(), 71);
        TF_CHECK(oVect.r_data(32).c_int() == 9999);
        TF_CHECK(oVect.r_data(33).c_int() == 320);   // former [32] pushed right
        TF_CHECK(oVect.r_data(70).c_int() == 690);
    }

    TF_CASE("nested vector element is deep-copied and independently addressable")
    {
        P3PmsgVect oInner(2, L"Inner", P3PmsgData((int)0));
        oInner.r_data(0).c_int(7);
        oInner.r_data(1).c_int(8);

        P3PmsgVect oOuter(0, L"Outer", P3PmsgData((int)0));
        oOuter.InsertAt(0, P3PmsgField(L"scalar", P3PmsgData((int)1)));
        oOuter.InsertAt(1, oInner);                 // element 1 is itself a vect
        TF_CHECK_EQ((int)oOuter.GetCount(), 2);
        TF_CHECK(oOuter.IsVect(1));
        TF_CHECK_EQ((int)oOuter.r_vect(1).GetCount(), 2);
        TF_CHECK(oOuter.r_vect(1).r_data(1).c_int() == 8);

        // Mutating the original inner must not affect the copy held by oOuter.
        oInner.r_data(1).c_int(99);
        TF_CHECK(oOuter.r_vect(1).r_data(1).c_int() == 8);
    }

    TF_CASE("a vect round-trips through a P3PmsgDesc container")
    {
        P3PmsgVect oVect(3, L"Payload", P3PmsgData((int)0));
        oVect.r_data(0).c_int(5);
        oVect.r_data(1).c_int(6);
        oVect.r_data(2).c_int(7);

        P3PmsgField oHolder(L"Holder");
        oHolder.r_Desc(P3PmsgField::AttrCMD_Create);
        oHolder.r_Desc() += oVect;                  // PushBack deep-copies the vect
        TF_CHECK(oHolder.r_Desc().Exists(L"Payload"));

        P3PmsgVect oBack(oHolder.r_Desc().SelectVect(L"Payload").r_Object());
        TF_CHECK_EQ((int)oBack.GetCount(), 3);
        TF_CHECK(oBack.r_data(0).c_int() == 5);
        TF_CHECK(oBack.r_data(2).c_int() == 7);
    }

    TF_CASE("a vect persists across a P2PmsgMgr save/load (IOMAGE heap)")
    {
        wchar_t szDir[MAX_PATH]  = { 0 };
        wchar_t szPath[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, szDir);
        swprintf_s(szPath, MAX_PATH, L"%smscs_vect_persist.p2p", szDir);

        try
        {
            // Build a vect inside an IOMAGE-backed manager and save it. The
            // manager stores VBLock addresses as image offsets, so aAlloc[]
            // must survive serialisation.
            {
                P2PmsgMgr   oMgr(VBLock_Addr64, 4096, 1u << 20);
                P3PmsgVect  oVect(4, L"Persisted", P3PmsgData((int)0));
                for (int i = 0; i < 4; i++)
                    oVect.r_data(i).c_int((i + 1) * 100);
                oMgr.r_Desc(P3PmsgField::AttrCMD_Create);
                oMgr.r_Desc() += oVect;
                oMgr.Save(szPath);
            }
            // Reload into a fresh manager and read the vect back.
            {
                P2PmsgMgr oMgr(szPath);
                P3PmsgVect oBack(oMgr.r_Desc().SelectVect(L"Persisted").r_Object());
                TF_CHECK_EQ((int)oBack.GetCount(), 4);
                TF_CHECK(oBack.r_data(0).c_int() == 100);
                TF_CHECK(oBack.r_data(3).c_int() == 400);
            }
        }
        catch (P2Pevent* pEVT)
        {
            tf_fail(__FILE__, __LINE__, "unexpected P2Pevent during persistence");
            pEVT->Cancel(false);
        }
        _wremove(szPath);
    }
}

// ---------------------------------------------------------------------------
// P3PmsgAttr / P3PmsgDesc : attribute (@keyed) and descendant containers
// ---------------------------------------------------------------------------
static void Test_AttrDesc()
{
    TF_CASE("attributes can be created, populated and queried")
    {
        P3PmsgField oField(L"Larry", DataBSTR08(L"Data"));
        oField.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Johnno");
        oField.AssertValid();
        TF_CHECK(oField.r_Attr().Exists(L"Johnno"));
        TF_CHECK(!oField.r_Attr().Exists(L"Nobody"));
    }

    TF_CASE("descendants accept fields and a list")
    {
        P3PmsgList oList;
        oList += P3PmsgData("a");
        oList += P3PmsgData("b");

        P3PmsgItem oItem;
        oItem.r_name() = L"TestNodeName";
        oItem.r_Desc(P3PmsgField::AttrCMD_Create);
        oItem.r_Desc() += P3PmsgField(L"Johnno");
        oItem.r_Desc() += P3PmsgField(L"Bill");
        oItem.r_Desc() += oList;
        oItem.AssertValid();
        TF_CHECK(oItem.r_Desc().Exists(L"Johnno"));
        TF_CHECK(oItem.r_Desc().Exists(L"Bill"));

        P3PmsgItem oCopy;
        oCopy = oItem;                // deep copy must not corrupt the source
        oItem.AssertValid();
        oCopy.AssertValid();
        TF_CHECK(oCopy.r_Desc().Exists(L"Johnno"));
    }
}

// ---------------------------------------------------------------------------
// P3PmsgCurs::Goto : the search key must survive the walk
// ---------------------------------------------------------------------------
static void Test_Curs_GotoKeyLifetime()
{
    // Off Win32 a name obtained from c_name() is a slot of the 16-entry
    // thread-local widening ring (p2p_wstr_from_store, Platform/p2pstr.h), and
    // every sibling Goto compares spends one more slot -- so the budget is not
    // 16 calls the caller can count, it is 16 SIBLINGS. Past that the key is
    // rewritten with the CURRENT sibling's name while it is being compared
    // against, the comparison reads a buffer against itself, and the scan stops
    // on the WRONG item while reporting success. Goto snapshots its key
    // (p2p_wkey) precisely so that no caller can cause this.
    //
    // On Win32 c_name() returns the store pointer and there is no ring, so this
    // case passes with or without the snapshot there. It is the Linux build that
    // can fail it -- which is why it asserts the found item's IDENTITY, not just
    // that something was found.
    TF_CASE("Goto(name) honours a key that lives in the widening ring")
    {
        const int kItems = 24;               // comfortably past the ring's 16 slots

        P3PmsgItem oItem;
        oItem.r_name() = L"GotoKeyLifetime";
        oItem.r_Desc(P3PmsgField::AttrCMD_Create);
        for ( int i = 0; i < kItems; i++ )
        {
          wchar_t szName[32];
          swprintf_s(szName, 32, L"Item%02d", i);
          oItem.r_Desc() += P3PmsgField(szName);
        }
        TF_CHECK_EQ((int)oItem.r_Desc().GetCount(), kItems);

        // Park a second cursor on the LAST descendant and hand Goto its bare
        // c_name() -- a live ring slot, deliberately NOT copied first.
        P3PmsgCurs oCursKey(oItem.r_Desc());
        TF_CHECK(oCursKey.Goto(kItems - 1));
        LPCTNAM lpszKey = oCursKey.r_name().c_name();

        P3PmsgCurs oCurs(oItem.r_Desc());
        TF_CHECK(oCurs.Goto(lpszKey));
        TF_CHECK_EQ((int)oCurs.Item(), kItems - 1);
        TF_CHECK(CString(oCurs.r_name().c_name()).Compare(L"Item23") == 0);
    }

    // Goto snapshots its own key, but a scan that drives the cursor itself and
    // compares as it goes owns the hazard: P3PmsgRefactor_Rename spends a ring
    // slot per sibling in ITS loop, and consumes the NEW name inside that loop,
    // after those comparisons -- so a recycled key renames to the wrong name,
    // not merely at the wrong place.
    TF_CASE("Rename honours keys that live in the widening ring")
    {
        const int kItems = 24;

        P3PmsgItem oItem;
        oItem.r_name() = L"RenameKeyLifetime";
        oItem.r_Desc(P3PmsgField::AttrCMD_Create);
        for ( int i = 0; i < kItems; i++ )
        {
          wchar_t szName[32];
          swprintf_s(szName, 32, L"Item%02d", i);
          oItem.r_Desc() += P3PmsgField(szName);
        }

        // Both arguments are bare c_name() pointers, deliberately not copied.
        P3PmsgCurs oCursOld(oItem.r_Desc());
        TF_CHECK(oCursOld.Goto(kItems - 1));          // rename the LAST one
        LPCTNAM lpszOld = oCursOld.r_name().c_name();

        P3PmsgItem oNewName;
        oNewName.r_name() = L"Renamed";
        LPCTNAM lpszNew = oNewName.r_name().c_name();

        P3PmsgRefactor_Rename ( oItem, lpszOld, lpszNew );

        TF_CHECK(oItem.r_Desc().Exists(L"Renamed"));
        TF_CHECK(!oItem.r_Desc().Exists(L"Item23"));
        // Every other name must be untouched: a recycled key renames a
        // bystander instead of, or as well as, the intended item.
        for ( int i = 0; i < kItems - 1; i++ )
        {
          wchar_t szName[32];
          swprintf_s(szName, 32, L"Item%02d", i);
          TF_CHECK(oItem.r_Desc().Exists(szName));
        }
        TF_CHECK_EQ((int)oItem.r_Desc().GetCount(), kItems);
    }

    //  The third member of this class. Session 26 fixed its key by inspection;
    //  guarding it took first finding out why it could not find anything by
    //  name at all -- c_wcsicmpWC() is a PREDICATE (non-zero = matches), and
    //  both name tests in P3Pmsg_FindChildWithAttr read it as a wcsicmp-style
    //  comparison, so the function skipped exactly the children that matched.
    //  With that corrected the child is found in the first loop, which is the
    //  loop that spends a ring slot per sibling.
    //
    //  Same platform note as the two above: on Win32 c_name() is the store
    //  pointer and there is no ring, so this passes there either way. It
    //  asserts the found object's IDENTITY so the Linux build can fail it.
    TF_CASE("FindChildWithAttr honours keys that live in the widening ring")
    {
        const int kItems = 24;

        //  Exactly ONE sibling carries the attribute, and it is the LAST one -
        //  so a key recycled part-way through the scan matches a bystander that
        //  does not have it, or stops on a name it never was.
        P3PmsgItem oItem;
        oItem.r_name() = L"FindChildKeyLifetime";
        oItem.r_Desc(P3PmsgField::AttrCMD_Create);
        for ( int i = 0; i < kItems; i++ )
        {
          wchar_t szName[32];
          swprintf_s(szName, 32, L"Item%02d", i);
          P3PmsgField oChild(szName);
          if ( i == kItems - 1 )
            oChild.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Marked");
          oItem.r_Desc() += oChild;
        }

        //  Both keys are bare c_name() pointers - live ring slots, not copies.
        P3PmsgCurs oCursKey(oItem.r_Desc());
        TF_CHECK(oCursKey.Goto(kItems - 1));
        LPCTNAM lpszChild = oCursKey.r_name().c_name();

        P3PmsgItem oAttrName;
        oAttrName.r_name() = L"Marked";
        LPCTNAM lpszAttr = oAttrName.r_name().c_name();

        P3PmsgObject oFound =
            P3Pmsg_FindChildWithAttr ( oItem, lpszAttr, lpszChild );

        TF_CHECK(!oFound.IsVoid());
        if (!oFound.IsVoid())
        {
          P3PmsgField oFoundField = oFound;
          TF_CHECK(oFoundField == L"Item23");
        }
    }

    //  The name test's SENSE, pinned separately from the ring: a name that
    //  matches must be found, and a name that matches nothing must not come
    //  back with some other child that merely has the attribute. Before the
    //  correction the first of these returned void and the second returned the
    //  marked child - both exactly backwards.
    TF_CASE("FindChildWithAttr matches the name it is given, not the others")
    {
        P3PmsgItem oItem;
        oItem.r_name() = L"FindChildSense";
        oItem.r_Desc(P3PmsgField::AttrCMD_Create);
        for ( int i = 0; i < 4; i++ )
        {
          wchar_t szName[32];
          swprintf_s(szName, 32, L"Kid%d", i);
          P3PmsgField oChild(szName);
          if ( i == 2 )
            oChild.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Tag");
          oItem.r_Desc() += oChild;
        }

        P3PmsgObject oHit = P3Pmsg_FindChildWithAttr ( oItem, L"Tag", L"Kid2" );
        TF_CHECK(!oHit.IsVoid());
        if (!oHit.IsVoid())
        {
          P3PmsgField oHitField = oHit;
          TF_CHECK(oHitField == L"Kid2");
        }

        //  A wildcard that matches the marked child.
        P3PmsgObject oWild = P3Pmsg_FindChildWithAttr ( oItem, L"Tag", L"Kid*" );
        TF_CHECK(!oWild.IsVoid());

        //  No name given at all - the documented default - still finds it.
        P3PmsgObject oAny = P3Pmsg_FindChildWithAttr ( oItem, L"Tag" );
        TF_CHECK(!oAny.IsVoid());
    }

    //  A search that matches NOTHING must come back VOID. That is how every
    //  "not found" in this API is reported, and it did not work: the caller's
    //  copy of a void P3PmsgObject was not void, because the copy CONSTRUCTOR
    //  took its inline-block branch for a void source and pointed m_aVBLock at
    //  the copy's own block (Msgcore P2Pmsg.cpp). operator=() was already right
    //  - Connect() has nullified for a void source since 2025-02-18 - so
    //  `oA = oB` behaved and `P3PmsgObject oA = oB` did not, which is every
    //  by-value return.
    //
    //  Three misses, because they fail at three different points: no child of
    //  that name; a child of that name without the attribute; neither.
    TF_CASE("FindChildWithAttr returns void for a search that matches nothing")
    {
        P3PmsgItem oItem;
        oItem.r_name() = L"FindChildMiss";
        oItem.r_Desc(P3PmsgField::AttrCMD_Create);
        for ( int i = 0; i < 4; i++ )
        {
          wchar_t szName[32];
          swprintf_s(szName, 32, L"Kid%d", i);
          P3PmsgField oChild(szName);
          if ( i == 2 )
            oChild.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Tag");
          oItem.r_Desc() += oChild;      // all four are LEAVES - no r_Desc()
        }

        P3PmsgObject oNoName  = P3Pmsg_FindChildWithAttr ( oItem, L"Tag",        L"Nobody" );
        P3PmsgObject oNoAttr  = P3Pmsg_FindChildWithAttr ( oItem, L"NoSuchAttr", L"Kid1"   );
        P3PmsgObject oNeither = P3Pmsg_FindChildWithAttr ( oItem, L"NoSuchAttr", L"Nobody" );
        TF_CHECK(oNoName.IsVoid());
        TF_CHECK(oNoAttr.IsVoid());
        TF_CHECK(oNeither.IsVoid());
    }

    //  The same defect stated at the level it actually lives at: copying a void
    //  P3PmsgObject must produce a void one, whichever way the copy is spelled.
    //  Both spellings are checked because only ONE of them was broken.
    TF_CASE("a void P3PmsgObject stays void through copy and assignment")
    {
        P3PmsgObject oVoid;
        TF_CHECK(oVoid.IsVoid());

        P3PmsgObject oCopied = oVoid;      // copy CONSTRUCTOR - this was the bug
        TF_CHECK(oCopied.IsVoid());

        P3PmsgObject oAssigned;
        oAssigned = oVoid;                 // operator= - already correct
        TF_CHECK(oAssigned.IsVoid());

        //  And a copy of a copy, since the broken branch made its result look
        //  like a standalone object that would then propagate.
        P3PmsgObject oTwice = oCopied;
        TF_CHECK(oTwice.IsVoid());
    }

    //  Why this matters beyond one search function: THREE Exists() overloads -
    //  P3PmsgField (P2Pmsg.cpp:3332), P3PmsgNode (:4267) and P3PmsgDesc
    //  (MsgDesc.cpp:394) - all answer
    //      !P3Pmsg_SelectObject(&r_Object(), name).IsVoid()
    //  on a P3PmsgObject returned BY VALUE. Any of those returns whose copy is
    //  not elided would report "exists" for a name that does not.
    TF_CASE("Exists() says no to a name that is not there")
    {
        P3PmsgItem oItem;
        oItem.r_name() = L"ExistsHost";
        oItem.r_Desc(P3PmsgField::AttrCMD_Create);
        oItem.r_Desc() += P3PmsgField(L"Present");

        TF_CHECK(oItem.r_Desc().Exists(L"Present"));
        TF_CHECK(!oItem.r_Desc().Exists(L"Absent"));

        //  And through the field-level overload, which is the one most callers
        //  reach for.
        TF_CHECK(oItem.Exists(L"Present"));
        TF_CHECK(!oItem.Exists(L"Absent"));
    }
}

// ---------------------------------------------------------------------------
// MsgStck : per-field value stack (push a value, mutate, pop to restore)
// ---------------------------------------------------------------------------
static void Test_Stack()
{
    TF_CASE("push/pop restores the field's prior name")
    {
        P3PmsgField oField(L"Larry", DataBSTR08(L"Data"));
        oField.r_Stck().Push();
        oField = P3PmsgName(L"Larry-Pushed");
        TF_CHECK(oField == L"Larry-Pushed");

        if (oField.IsStacked())
            oField.r_Stck().Pop();
        oField.AssertValid();
        TF_CHECK(oField == L"Larry");
    }

    //  The '^' path delimiter, which is what the stack link is FOR from a
    //  caller's side. It has been in the grammar since the beginning --
    //  ParseObjectPath stops on it, P3Pmsg_IsPathDelimiter answers TRUE for it,
    //  P3Pmsg_IsValidItemname refuses it in a name -- and every arm of
    //  P3Pmsg_SelectObjectRecurse that could have followed it was ASSERT(0).
    //  So the stack was reachable only through r_Stck(); no path could name a
    //  pushed value.
    TF_CASE("'^' selects the value the field held before the push")
    {
        P3PmsgItem oHost(L"Host", DataBSTR08(L"live"));
        oHost.r_Stck().Push();
        oHost = P3PmsgName(L"Host-Changed");

        P3PmsgObject oWas = P3Pmsg_SelectObject ( &oHost.r_Object(), L"^" );
        TF_CHECK(!oWas.IsVoid());
        if (!oWas.IsVoid())
        {
          P3PmsgField oWasField = oWas;
          TF_CHECK(oWasField == L"Host");
        }

        //  ... and once it is popped there is nothing to select again.
        oHost.r_Stck().Pop();
        TF_CHECK(P3Pmsg_SelectObject(&oHost.r_Object(), L"^").IsVoid());
    }

    //  Pushes nest -- MsgStck::Push reads the current head before allocating
    //  and re-links it onto the new item -- so the delimiter has to repeat.
    TF_CASE("'^^' reaches the generation before the last one")
    {
        P3PmsgItem oHost(L"Gen0");
        oHost.r_Stck().Push();
        oHost = P3PmsgName(L"Gen1");
        oHost.r_Stck().Push();
        oHost = P3PmsgName(L"Gen2");

        P3PmsgObject oOne = P3Pmsg_SelectObject ( &oHost.r_Object(), L"^"  );
        P3PmsgObject oTwo = P3Pmsg_SelectObject ( &oHost.r_Object(), L"^^" );
        TF_CHECK(!oOne.IsVoid());
        TF_CHECK(!oTwo.IsVoid());
        if (!oOne.IsVoid()) { P3PmsgField oF = oOne; TF_CHECK(oF == L"Gen1"); }
        if (!oTwo.IsVoid()) { P3PmsgField oF = oTwo; TF_CHECK(oF == L"Gen0"); }

        //  One more than there are generations is a broken path, not a crash.
        TF_CHECK(P3Pmsg_SelectObject(&oHost.r_Object(), L"^^^").IsVoid());
    }

    //  A pushed item is a WHOLE item -- Push copies name, data, attributes and
    //  descendants -- so a path does not have to stop at the '^'. This is also
    //  what makes the snapshot worth naming: it holds the children the live
    //  item has since lost or gained.
    TF_CASE("a path continues through the pushed item")
    {
        P3PmsgItem oHost;
        oHost.r_name() = L"Snap";
        oHost.r_Desc(P3PmsgField::AttrCMD_Create);
        oHost.r_Desc() += P3PmsgField(L"Before");
        oHost.r_Stck().Push();
        oHost.r_Desc() += P3PmsgField(L"After");

        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"^.Before").IsVoid());
        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"Before"  ).IsVoid());
        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"After"   ).IsVoid());

        //  The child added AFTER the push is not in the snapshot.
        TF_CHECK( P3Pmsg_SelectObject(&oHost.r_Object(), L"^.After" ).IsVoid());
    }

    //  Pop drops the generation it restored from, and Drop() walks the stack:
    //  P3PmsgField::Drop and P3PmsgList::Drop both end with
    //  "if (IsStacked()) r_Stck().Drop()", and MsgStck::Drop zeroes every link
    //  the rest of the way down without freeing a thing. So the popped item had
    //  to be unlinked from the generation below it BEFORE being dropped, and it
    //  was not: one pop severed everything under the generation it restored,
    //  and leaked it. A single push and pop cannot see this -- there is nothing
    //  below to sever -- and a single push and pop was the only shape anything
    //  in the tree had ever exercised.
    TF_CASE("a pop leaves the generations below it intact")
    {
        P3PmsgItem oHost(L"Gen0");
        oHost.r_Stck().Push();
        oHost = P3PmsgName(L"Gen1");
        oHost.r_Stck().Push();
        oHost = P3PmsgName(L"Gen2");
        oHost.r_Stck().Push();
        oHost = P3PmsgName(L"Gen3");

        //  Three pushes: "^" is Gen2, "^^" is Gen1, "^^^" is Gen0.
        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"^^^").IsVoid());

        oHost.r_Stck().Pop();                 // back to Gen2
        TF_CHECK(oHost == L"Gen2");

        //  Two generations should remain below it.
        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"^" ).IsVoid());
        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"^^").IsVoid());
        P3PmsgObject o1 = P3Pmsg_SelectObject(&oHost.r_Object(), L"^" );
        P3PmsgObject o2 = P3Pmsg_SelectObject(&oHost.r_Object(), L"^^");
        if (!o1.IsVoid()) { P3PmsgField f = o1; TF_CHECK(f == L"Gen1"); }
        if (!o2.IsVoid()) { P3PmsgField f = o2; TF_CHECK(f == L"Gen0"); }
    }

    //  A field that was never pushed has no aStack to follow. That is an
    //  ordinary miss -- the same void P3PmsgObject every other broken path in
    //  P3Pmsg_SelectObjectRecurse returns -- and NOT the ASSERT(0) that used to
    //  stand here, which took a debug build down for asking a legitimate
    //  question.
    TF_CASE("'^' on an unpushed field is a miss, not an assertion")
    {
        P3PmsgItem oPlain(L"Plain");
        TF_CHECK(P3Pmsg_SelectObject(&oPlain.r_Object(), L"^").IsVoid());
        TF_CHECK(P3Pmsg_SelectObject(&oPlain.r_Object(), L"^.Child").IsVoid());
    }
}

// ---------------------------------------------------------------------------
// MsgStck::Drop : releasing the generations, not just unlinking them
// ---------------------------------------------------------------------------
static void Test_StackDrop()
{
    //  MsgStck::Drop walked to the deepest generation, zeroed each aStack on the
    //  way back up, and returned. It freed NOTHING -- every pushed item block,
    //  and the name and data blocks hanging off it, stayed allocated with
    //  nothing left pointing at them. Every caller wants the storage back:
    //  P3PmsgField::Drop, P3PmsgList::Drop and P3PmsgVect::Drop all reach it
    //  while dismantling an item, MsgFacade's FacadeNode exposes it as the COM
    //  "drop the stack" verb, and TargetCore's P2PeerMsg calls it when it
    //  replaces one stack with another.
    //
    //  The observable is address reuse: a block that was freed goes back on the
    //  free list, so an identical allocation that follows lands in it.
    //
    //  IN AN IOMAGE-BACKED MANAGER, DELIBERATELY. A standalone P3PmsgItem sits
    //  on the SYS heap, where a P2Pos is a raw CRT pointer and Msgcore is not
    //  the only thing allocating from it -- every transient P3PmsgDesc that a
    //  path selection news up competes for the same block, so reuse there is
    //  luck rather than evidence. Inside a manager the address is an offset into
    //  one image and Msgcore's own allocator is the only claimant, which is what
    //  makes these checks mean what they say.
    TF_CASE("dropping a stack frees the generation's block")
    {
        P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
        mgr.r_name() = L"Root";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create);
        mgr.r_Desc() += P3PmsgField(L"Host", DataBSTR08(L"payload"));

        P3PmsgField oHost(mgr.r_Desc().SelectObject(L"Host"));
        TF_CHECK(!oHost.r_Object().IsVoid());

        oHost.r_Stck().Push();
        P3PmsgObject oGen = P3Pmsg_SelectObject(&oHost.r_Object(), L"^");
        TF_CHECK(!oGen.IsVoid());
        P2Pos posFirst = oGen.GetP2Pos();

        oHost.r_Stck().Drop();
        TF_CHECK(!oHost.IsStacked());
        TF_CHECK(P3Pmsg_SelectObject(&oHost.r_Object(), L"^").IsVoid());

        //  An identical push has to be able to reuse the block.
        oHost.r_Stck().Push();
        P3PmsgObject oAgain = P3Pmsg_SelectObject(&oHost.r_Object(), L"^");
        TF_CHECK(!oAgain.IsVoid());
        TF_CHECK(oAgain.GetP2Pos() == posFirst);
    }

    //  A stack more than one deep, which is where the ORDER of the frees
    //  starts to matter. Drop releases the generations head first, and it has
    //  to: they are pushed at ascending addresses, and the heap coalesces a
    //  freed block only with its next physical neighbour. Deepest-first -- the
    //  order an unwinding Drop() recursion would produce all by itself -- means
    //  every block's neighbour is still allocated at the moment it is freed, so
    //  nothing merges and only the highest of them is ever seen again.
    //
    //  Four rounds of push-push-push-Drop, reading the head generation's P2Pos:
    //
    //      deepest-first   949 1361 1773 2185     (+412 a round, and it is
    //                                              linear -- two generations'
    //                                              worth leaked every round)
    //      head-first      949  949  949  949
    //
    //  So this case asserts that a round leaves the heap exactly as it found
    //  it, which is the whole claim: every generation freed, and freed in an
    //  order the allocator can actually take back.
    TF_CASE("dropping a stack frees every generation, not just the head")
    {
        P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
        mgr.r_name() = L"Root";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create);
        mgr.r_Desc() += P3PmsgField(L"Host", DataBSTR08(L"payload"));

        P3PmsgField oHost(mgr.r_Desc().SelectObject(L"Host"));
        P2Pos posRound0 = 0;

        for (int nRound = 0; nRound < 4; nRound++)
        {
            oHost.r_Stck().Push();
            oHost.r_Stck().Push();
            oHost.r_Stck().Push();

            TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"^"  ).IsVoid());
            TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"^^" ).IsVoid());
            P3PmsgObject oHead = P3Pmsg_SelectObject(&oHost.r_Object(), L"^^^");
            TF_CHECK(!oHead.IsVoid());
            if (!oHead.IsVoid())
            {
                if (nRound == 0)
                    posRound0 = oHead.GetP2Pos();
                else
                    TF_CHECK(oHead.GetP2Pos() == posRound0);   // no drift
            }

            oHost.r_Stck().Drop();

            //  Every generation gone, not merely the one the live item pointed
            //  at, and the live item itself untouched by it.
            TF_CHECK(!oHost.IsStacked());
            TF_CHECK(P3Pmsg_SelectObject(&oHost.r_Object(), L"^"  ).IsVoid());
            TF_CHECK(P3Pmsg_SelectObject(&oHost.r_Object(), L"^^" ).IsVoid());
            TF_CHECK(P3Pmsg_SelectObject(&oHost.r_Object(), L"^^^").IsVoid());
            TF_CHECK(oHost == L"Host");
        }
        oHost.AssertValid();
    }

    //  Freed by the generation's OWN type. A pushed list is a list, and
    //  P3PmsgField::Drop opens ASSERT(OBJ__IsField()) and frees the item block
    //  with no Truncate() behind it, which would leave every element in the
    //  snapshot allocated -- the same trap P3PmsgVect::Drop was added to close.
    TF_CASE("dropping a pushed list frees the list generation")
    {
        P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
        mgr.r_name() = L"Root";
        P3PmsgList oSeed(L"Numbers", P3PmsgData((int)0));
        oSeed.AddListTail(P3PmsgData((int)1));
        oSeed.AddListTail(P3PmsgData((int)2));
        mgr.r_Desc(P3PmsgField::AttrCMD_Create);
        mgr.r_Desc() += oSeed;

        P3PmsgList oList(mgr.r_Desc().SelectObject(L"Numbers"));
        TF_CHECK(oList.r_Object().IsList());

        oList.r_Stck().Push();
        P3PmsgObject oGen = P3Pmsg_SelectObject(&oList.r_Object(), L"^");
        TF_CHECK(!oGen.IsVoid());
        TF_CHECK(oGen.IsList());
        P2Pos posFirst = oGen.GetP2Pos();

        oList.r_Stck().Drop();
        TF_CHECK(!oList.IsStacked());
        oList.AssertValid();

        //  The live list is untouched by its own stack being dropped.
        TF_CHECK_EQ((int)oList.GetCount(), 2);

        oList.r_Stck().Push();
        P3PmsgObject oAgain = P3Pmsg_SelectObject(&oList.r_Object(), L"^");
        TF_CHECK(!oAgain.IsVoid());
        TF_CHECK(oAgain.GetP2Pos() == posFirst);
        if (!oAgain.IsVoid())
        {
            P3PmsgList oAgainList(oAgain);
            TF_CHECK_EQ((int)oAgainList.GetCount(), 2);
        }
    }
}

// ---------------------------------------------------------------------------
// VBHeap : backward coalescing, via the free-block boundary tag
// ---------------------------------------------------------------------------
static void Test_HeapCoalesce()
{
    //  The heap could only ever coalesce FORWARDS: P2PmsgHeap_Collate* merges a
    //  freed block with its NEXT physical neighbour, and nothing in the image
    //  said how far back the previous block began. So a run of frees reclaimed
    //  its space only if it ran high address to low. Low to high -- which is
    //  what almost everything does -- every block's neighbour was still
    //  allocated when it was freed, nothing merged, and the free list filled
    //  with separate blocks that first-fit then walked straight past, because
    //  the one that had absorbed the image tail sat at the head and satisfied
    //  every request.
    //
    //  A free block now carries its own size in its tail, so the block after it
    //  can find it. Allocated blocks are untouched, which is why this costs no
    //  format change and golden_ref.p2p is still byte-identical.
    //
    //  P3PmsgDesc::Truncate is the ordinary way to provoke it: it empties a
    //  container by deleting child 0 over and over, which is ascending order.
    //  Measured before the tag, the first child walked 331, 1155, 1979, 2803,
    //  3627 -- 824 bytes a round, for ever.
    TF_CASE("a container truncated and refilled reuses its blocks")
    {
        P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
        mgr.r_name() = L"Root";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create);
        P2Pos posRound0 = 0;

        for (int nRound = 0; nRound < 5; nRound++)
        {
            for (int i = 0; i < 5; i++)
            {
                wchar_t sz[32];
                swprintf_s(sz, 32, L"Kid%d", i);
                mgr.r_Desc() += P3PmsgField(sz, DataBSTR08(L"payload"));
            }
            TF_CHECK(mgr.r_Desc().Exists(L"Kid0"));
            TF_CHECK(mgr.r_Desc().Exists(L"Kid4"));

            P3PmsgObject o = mgr.r_Desc().SelectObject(L"Kid0");
            TF_CHECK(!o.IsVoid());
            if (!o.IsVoid())
            {
                if (nRound == 0)
                    posRound0 = o.GetP2Pos();
                else
                    TF_CHECK(o.GetP2Pos() == posRound0);   // no drift
            }
            mgr.r_Desc().Truncate();
            TF_CHECK(!mgr.r_Desc().Exists(L"Kid0"));
        }
    }

    //  The same thing one block at a time, and in the order that used to be the
    //  bad one: two siblings side by side, the LOWER freed first. Before the
    //  tag its neighbour was still allocated, so it never merged with anything
    //  and the pair could not be handed back as one span.
    TF_CASE("two adjacent blocks freed low-to-high merge into one")
    {
        P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
        mgr.r_name() = L"Root";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create);
        mgr.r_Desc() += P3PmsgField(L"Low",  DataBSTR08(L"payload"));
        mgr.r_Desc() += P3PmsgField(L"High", DataBSTR08(L"payload"));

        P3PmsgObject oLow = mgr.r_Desc().SelectObject(L"Low");
        TF_CHECK(!oLow.IsVoid());
        const P2Pos posLow = oLow.IsVoid() ? 0 : oLow.GetP2Pos();

        mgr.r_Desc().r_Curs().Goto(L"Low");
        mgr.r_Desc().r_Curs().Delete();
        mgr.r_Desc().r_Curs().Goto(L"High");
        mgr.r_Desc().r_Curs().Delete();
        TF_CHECK(!mgr.r_Desc().Exists(L"Low"));
        TF_CHECK(!mgr.r_Desc().Exists(L"High"));

        //  One span again, so the next pair starts back at the bottom of it.
        mgr.r_Desc() += P3PmsgField(L"Low",  DataBSTR08(L"payload"));
        mgr.r_Desc() += P3PmsgField(L"High", DataBSTR08(L"payload"));
        P3PmsgObject oBack = mgr.r_Desc().SelectObject(L"Low");
        TF_CHECK(!oBack.IsVoid());
        if (!oBack.IsVoid())
            TF_CHECK(oBack.GetP2Pos() == posLow);
        mgr.AssertValid();
    }

    //  A tag lives in the tail of a free block, which is dead space in memory
    //  but is still inside the arena Save writes out. Save scrubs the tags,
    //  writes, and puts them back, so an image from this build is byte-for-byte
    //  what an image from a build without tags would have been -- the property
    //  MscsUnitTests/golden_ref.p2p gates. This checks the round trip still
    //  works either side of that, and that the heap keeps coalescing after a
    //  Save has scrubbed and restored it.
    TF_CASE("a save round-trips and leaves the tags working")
    {
        wchar_t szDir[MAX_PATH]  = { 0 };
        wchar_t szPath[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, szDir);
        swprintf_s(szPath, MAX_PATH, L"%smscs_heap_tags.p2p", szDir);

        try
        {
            P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
            mgr.r_name() = L"Root";
            mgr.r_Desc(P3PmsgField::AttrCMD_Create);
            mgr.r_Desc() += P3PmsgField(L"Keep", DataBSTR08(L"payload"));
            mgr.r_Desc() += P3PmsgField(L"Drop", DataBSTR08(L"payload"));
            mgr.r_Desc().r_Curs().Goto(L"Drop");
            mgr.r_Desc().r_Curs().Delete();      // leaves a tagged free block
            mgr.Save(szPath);

            //  Still coalescing after the scrub/restore.
            P3PmsgObject o1 = mgr.r_Desc().SelectObject(L"Keep");
            TF_CHECK(!o1.IsVoid());
            mgr.r_Desc() += P3PmsgField(L"Again", DataBSTR08(L"payload"));
            TF_CHECK(mgr.r_Desc().Exists(L"Again"));
            mgr.AssertValid();

            P2PmsgMgr oBack(szPath);
            TF_CHECK(oBack.r_Desc().Exists(L"Keep"));
            TF_CHECK(!oBack.r_Desc().Exists(L"Drop"));
            oBack.AssertValid();
        }
        catch (P2Pevent* pEVT)
        {
            tf_fail(__FILE__, __LINE__, "unexpected P2Pevent during save round-trip");
            pEVT->Cancel(false);
        }
        _wremove(szPath);
    }
}

// ---------------------------------------------------------------------------
// P3PmsgVect::Drop : deleting a vector out of a container
// ---------------------------------------------------------------------------
static void Test_VectDrop()
{
    //  P3PmsgCurs::Delete has a vect branch -- "r_vect().Truncate(); ...
    //  r_vect().Drop();" -- and P3PmsgDesc::Truncate runs it over every child.
    //  P3PmsgVect had no Drop() of its own, so that virtual call landed on
    //  P3PmsgField::Drop, whose first line is ASSERT(OBJ__IsField()) and is
    //  false for a vect. Deleting a vector out of a descendant container was
    //  therefore an assertion in a debug build, and it needed no stack to
    //  provoke: just a vect in a tree and something that empties the tree.
    TF_CASE("a vect can be deleted from a descendant container")
    {
        P3PmsgVect oVect(3, L"Payload", P3PmsgData((int)0));
        oVect.r_data(0).c_int(1);
        oVect.r_data(2).c_int(3);

        P3PmsgItem oHost(L"Host");
        oHost.r_Desc(P3PmsgField::AttrCMD_Create);
        oHost.r_Desc() += P3PmsgField(L"Keep");
        oHost.r_Desc() += oVect;
        TF_CHECK(oHost.r_Desc().Exists(L"Payload"));

        oHost.r_Desc().r_Curs().Goto(L"Payload");
        oHost.r_Desc().r_Curs().Delete();
        TF_CHECK(!oHost.r_Desc().Exists(L"Payload"));
        TF_CHECK(oHost.r_Desc().Exists(L"Keep"));
        oHost.AssertValid();
    }

    TF_CASE("a descendant container holding a vect truncates cleanly")
    {
        P3PmsgVect oVect(2, L"Payload", P3PmsgData((int)0));
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)9));

        P3PmsgItem oHost(L"Host");
        oHost.r_Desc(P3PmsgField::AttrCMD_Create);
        oHost.r_Desc() += P3PmsgField(L"Plain");
        oHost.r_Desc() += oList;
        oHost.r_Desc() += oVect;

        oHost.r_Desc().Truncate();
        TF_CHECK(!oHost.r_Desc().Exists(L"Payload"));
        TF_CHECK(!oHost.r_Desc().Exists(L"Numbers"));
        TF_CHECK(!oHost.r_Desc().Exists(L"Plain"));
        oHost.AssertValid();
    }
}

// ---------------------------------------------------------------------------
// MsgStck : pushing a list and a vector
// ---------------------------------------------------------------------------
static void Test_StackContainers()
{
    //  MsgStck::Push() and Pop() were ASSERT(0) for both, so a list could not be
    //  snapshotted at all and "List^" was a well-formed question with a
    //  permanently empty answer. Nothing had to be written to allocate one:
    //  MsgStck__AllocItem has dispatched on the item type since it was written,
    //  sizing with P2PmsgList_SizeofItem and laying the block out with
    //  P2PmsgList_InitItem. Push simply never called it for a list.
    //
    //  What the allocation does NOT carry is the payload -- VBLockList_Init
    //  zeroes aFirst, aLast and nItems -- so the elements are walked over
    //  separately, the way P3PmsgList::operator= does it.
    TF_CASE("a list's elements survive a push and come back on the pop")
    {
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)1));
        oList.AddListTail(P3PmsgData((int)2));
        oList.AddListTail(P3PmsgData((int)3));

        oList.r_Stck().Push();
        TF_CHECK(oList.IsStacked());

        //  Change the live list out of all recognition.
        oList.Truncate();
        oList.AddListTail(P3PmsgData((int)99));
        TF_CHECK_EQ((int)oList.GetCount(), 1);

        //  The snapshot still holds all three.
        P3PmsgObject oWas = P3Pmsg_SelectObject(&oList.r_Object(), L"^");
        TF_CHECK(!oWas.IsVoid());
        if (!oWas.IsVoid())
        {
            TF_CHECK(oWas.IsList());
            P3PmsgList oWasList(oWas);
            TF_CHECK_EQ((int)oWasList.GetCount(), 3);
            VBLaddr aPos = oWasList.GetHeadPos();
            TF_CHECK(oWasList.GetNext(aPos).c_int() == 1);
            TF_CHECK(oWasList.GetNext(aPos).c_int() == 2);
            TF_CHECK(oWasList.GetNext(aPos).c_int() == 3);
        }

        oList.r_Stck().Pop();
        TF_CHECK(!oList.IsStacked());
        TF_CHECK_EQ((int)oList.GetCount(), 3);
        VBLaddr aPos = oList.GetHeadPos();
        TF_CHECK(oList.GetNext(aPos).c_int() == 1);
        TF_CHECK(oList.GetNext(aPos).c_int() == 2);
        TF_CHECK(oList.GetNext(aPos).c_int() == 3);
        oList.AssertValid();
    }

    //  A list's name, attributes and descendants ride along too -- they are
    //  VBLockItem header fields, the same ones a plain field pushes.
    TF_CASE("a list pushes its name and attributes with it")
    {
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)7));
        oList.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Unit");

        oList.r_Stck().Push();
        oList.r_name() = L"Numbers-Changed";

        //  The '^' arm of the selector reaches the snapshot, and '@' keeps
        //  working one step past it.
        P3PmsgObject oWas = P3Pmsg_SelectObject(&oList.r_Object(), L"^");
        TF_CHECK(!oWas.IsVoid());
        if (!oWas.IsVoid())
        {
            P3PmsgField oWasField = oWas;
            TF_CHECK(oWasField == L"Numbers");
        }
        TF_CHECK(!P3Pmsg_SelectObject(&oList.r_Object(), L"^@Unit").IsVoid());

        oList.r_Stck().Pop();
        TF_CHECK(oList == L"Numbers");
        TF_CHECK(oList.r_Attr().Exists(L"Unit"));
    }

    //  A vector is the same story with an index instead of a chain. It needed
    //  one thing a list did not: P3PmsgVect had no Drop() of its own, so Pop
    //  would have reached P3PmsgField::Drop -- ASSERT(OBJ__IsField()), then a
    //  free of the item block with every element block still allocated.
    TF_CASE("a vector's elements survive a push and come back on the pop")
    {
        P3PmsgVect oVect(3, L"Payload", P3PmsgData((int)0));
        oVect.r_data(0).c_int(10);
        oVect.r_data(1).c_int(20);
        oVect.r_data(2).c_int(30);

        oVect.r_Stck().Push();
        oVect.Truncate();
        TF_CHECK_EQ((int)oVect.GetCount(), 0);

        P3PmsgObject oWas = P3Pmsg_SelectObject(&oVect.r_Object(), L"^");
        TF_CHECK(!oWas.IsVoid());
        if (!oWas.IsVoid())
        {
            TF_CHECK(oWas.IsVect());
            P3PmsgVect oWasVect(oWas);
            TF_CHECK_EQ((int)oWasVect.GetCount(), 3);
            TF_CHECK(oWasVect.r_data(0).c_int() == 10);
            TF_CHECK(oWasVect.r_data(2).c_int() == 30);
        }

        oVect.r_Stck().Pop();
        TF_CHECK_EQ((int)oVect.GetCount(), 3);
        TF_CHECK(oVect.r_data(0).c_int() == 10);
        TF_CHECK(oVect.r_data(1).c_int() == 20);
        TF_CHECK(oVect.r_data(2).c_int() == 30);
        oVect.AssertValid();
    }

    //  Pushes nest for a list exactly as they do for a field, and the '^^'
    //  form reads the generation before the last.
    TF_CASE("list pushes nest")
    {
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)1));       // gen 0: one element
        oList.r_Stck().Push();
        oList.AddListTail(P3PmsgData((int)2));       // gen 1: two
        oList.r_Stck().Push();
        oList.AddListTail(P3PmsgData((int)3));       // live: three

        P3PmsgObject oOne = P3Pmsg_SelectObject(&oList.r_Object(), L"^" );
        P3PmsgObject oTwo = P3Pmsg_SelectObject(&oList.r_Object(), L"^^");
        TF_CHECK(!oOne.IsVoid());
        TF_CHECK(!oTwo.IsVoid());
        if (!oOne.IsVoid()) { P3PmsgList o(oOne); TF_CHECK_EQ((int)o.GetCount(), 2); }
        if (!oTwo.IsVoid()) { P3PmsgList o(oTwo); TF_CHECK_EQ((int)o.GetCount(), 1); }

        TF_CHECK(P3Pmsg_SelectObject(&oList.r_Object(), L"^^^").IsVoid());

        //  And unwind all the way down.
        oList.r_Stck().Pop();
        TF_CHECK_EQ((int)oList.GetCount(), 2);
        oList.r_Stck().Pop();
        TF_CHECK_EQ((int)oList.GetCount(), 1);
        TF_CHECK(!oList.IsStacked());
    }

    //  A pushed list inside a tree rather than standing on its own.
    //
    //  NOT reached with RootPath2Object, deliberately: it walks the path in a
    //  P3PmsgItem, and P3PmsgField::operator=(const P3PmsgObject&) throws
    //  "Invalid overloaded context" for anything that is not a field, so a root
    //  path that lands on a list throws before it can answer. That is the one
    //  item left in stack_paths.md section 7 and it is untouched here -- it
    //  predates all of this, and it is the reason the descendant container is
    //  asked directly below.
    TF_CASE("a list pushed inside a tree keeps its own stack")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Root";
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)5));
        mgr.r_Desc() += oList;                       // PushBack deep-copies

        P3PmsgObject oLive = mgr.r_Desc().SelectList(L"Numbers").r_Object();
        TF_CHECK(!oLive.IsVoid());
        TF_CHECK(oLive.IsList());

        P3PmsgList oInTree(oLive);
        oInTree.r_Stck().Push();
        oInTree.AddListTail(P3PmsgData((int)6));
        TF_CHECK_EQ((int)oInTree.GetCount(), 2);

        //  The snapshot is a different object from the live one, and holds the
        //  single element the list had when it was pushed.
        P3PmsgObject oSnap = P3Pmsg_SelectObject(&oInTree.r_Object(), L"^");
        TF_CHECK(!oSnap.IsVoid());
        TF_CHECK(!(oSnap == oInTree.r_Object()));
        if (!oSnap.IsVoid()) { P3PmsgList o(oSnap); TF_CHECK_EQ((int)o.GetCount(), 1); }
    }
}

// ---------------------------------------------------------------------------
// P2PmsgMgr : root paths -- ".Root.Item@Attr", ".Root.Item^"
// ---------------------------------------------------------------------------
static void Test_RootPath()
{
    //  RootPath2Object splits a full path into components, each carrying the
    //  delimiter that introduced it, and walks them. It used to strip that
    //  delimiter off EVERY component before looking it up, which is right for
    //  '.' -- P3Pmsg_SelectObject reads a leading dot as naming the object you
    //  are standing on -- and wrong for the other two, where the delimiter is
    //  the whole instruction. So every '@' and '^' component was looked up as
    //  a plain descendant name.
    TF_CASE("a root path reaches an attribute through '@'")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Root";
        P3PmsgField oChild(L"Alpha");
        oChild.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Tag");
        mgr.r_Desc() += oChild;

        //  The descendant walk still works - that is the half that was right.
        TF_CHECK(!mgr.RootPath2Object(L".Root.Alpha").IsVoid());

        //  The attribute is NOT a child named "Tag", and before the fix that
        //  is exactly what was asked for.
        TF_CHECK(!mgr.RootPath2Object(L".Root.Alpha@Tag").IsVoid());
    }

    //  '^' is the harder half, because the component is the delimiter ALONE.
    //  P3Pmsg_SplitRootPath refused a component with no name after it, and
    //  dropped one that ended the path -- so ".Root.Beta^" came back as the
    //  component list for ".Root.Beta" and answered with the live item. The
    //  wrong object, silently, for the shortest way to spell the question.
    TF_CASE("a root path reaches a pushed value through '^'")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Root";
        mgr.r_Desc() += P3PmsgField(L"Beta");

        //  PushBack deep-copies, so the live child has to come back out of the
        //  tree before it can be pushed.
        P3PmsgField oBeta = mgr.RootPath2Object(L".Root.Beta");
        TF_CHECK(!oBeta.IsVoid());
        oBeta.r_Desc(P3PmsgField::AttrCMD_Create);
        oBeta.r_Desc() += P3PmsgField(L"Early");
        oBeta.r_Stck().Push();
        oBeta.r_Desc() += P3PmsgField(L"Late");

        TF_CHECK(!mgr.RootPath2Object(L".Root.Beta^").IsVoid());
        TF_CHECK(!mgr.RootPath2Object(L".Root.Beta^.Early").IsVoid());
        TF_CHECK(!mgr.RootPath2Object(L".Root.Beta^Early").IsVoid());

        //  "Late" was added after the push, so the live item has it ...
        TF_CHECK(!mgr.RootPath2Object(L".Root.Beta.Late").IsVoid());

        //  ... and the snapshot the path just selected is a different object
        //  from the live one.
        P3PmsgObject oSnap = mgr.RootPath2Object(L".Root.Beta^");
        TF_CHECK(!(oSnap == oBeta.r_Object()));
    }

    //  An item that was never pushed has no snapshot. That is an ordinary
    //  answer, not a missing path: only the descendant components throw for a
    //  miss, and the walk must stop on the empty object rather than ask the
    //  next component of it -- which walks a void P3PmsgObject into
    //  P3Pmsg_SelectObjectRecurse and trips the ASSERT(0) at its tail.
    TF_CASE("'^' on an unpushed item ends the walk empty")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Root";
        mgr.r_Desc() += P3PmsgField(L"Plain");

        TF_CHECK(mgr.RootPath2Object(L".Root.Plain^").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Root.Plain^.Kid").IsVoid());
    }

    //  A path with no components at all is still the root, and a path that
    //  does not begin at one is still refused -- that is what the splitter's
    //  verdict has left to say (§15).
    //
    //  It used to refuse a nameless component too, which is how ".Root..Alpha"
    //  and ".Root." were read. Every lone delimiter now names the collection it
    //  introduces, so neither is nameless: they carry the name of a collection
    //  rather than of an item.
    TF_CASE("a path with no components is the root; a rootless one is refused")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Root";
        mgr.r_Desc() += P3PmsgField(L"Alpha");

        CString        strRoot;
        CList<CString> oItems;

        TF_CHECK(P3Pmsg_SplitRootPath(L".Root", strRoot, oItems));
        TF_CHECK(strRoot == L"Root");
        TF_CHECK(oItems.GetCount() == 0);

        TF_CHECK(!P3Pmsg_SplitRootPath(L"Root.Alpha", strRoot, oItems));

        //  A trailing '.' is a component now, and it names the descendant
        //  collection -- so is a doubled one, part way along (§15).
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 1);
        if (oItems.GetCount() == 1)
            TF_CHECK(oItems.GetTail() == L".");

        TF_CHECK(P3Pmsg_SplitRootPath(L".Root..Alpha", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 2);

        //  A trailing '^' is not dropped either, and that was the first of
        //  these to change.
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Alpha^", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 2);
    }

    //  '^' is a delimiter, and it is the only one that carries no name of its
    //  own -- it names the pushed value of whatever stands to its left. So
    //  where it follows a delimiter that DOES carry a name, it has not begun a
    //  new component; it has qualified the one being read. "@^Tag" is a single
    //  question, and reading it as two components left the first of them
    //  nameless -- which the length test refused, taking the WHOLE path down
    //  with it.
    TF_CASE("'^' after '@' or '.' stays in the same component")
    {
        CString        strRoot;
        CList<CString> oItems;

        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Alpha@^Tag", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 2);
        if (oItems.GetCount() == 2)
            TF_CHECK(oItems.GetTail() == L"@^Tag");

        //  Pushes nest, so the delimiter repeats and all of it is one step.
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Alpha@^^Tag", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 2);
        if (oItems.GetCount() == 2)
            TF_CHECK(oItems.GetTail() == L"@^^Tag");

        //  The descendant collection reads the same way ...
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Alpha.^Kid", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 2);
        if (oItems.GetCount() == 2)
            TF_CHECK(oItems.GetTail() == L".^Kid");

        //  ... and so does a '^' qualifying another '^', which used to be two
        //  components that happened to walk to the same place.
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Alpha^^", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 2);
        if (oItems.GetCount() == 2)
            TF_CHECK(oItems.GetTail() == L"^^");

        //  A '^' that follows a NAME still ends the component: the attribute
        //  Tag has a stack of its own, and asking for it is a second step.
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Alpha@Tag^", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 3);
    }

    //  The splitter's verdict is the point of the call, and RootPath2Object
    //  discarded it -- walking whatever components had been collected before
    //  the refusal, and answering the object one step up from where it
    //  happened. A wrong object, with nothing to tell it from a right one.
    TF_CASE("a malformed root path throws rather than answering the parent")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Root";
        mgr.r_Desc() += P3PmsgField(L"Alpha");

        //  A path that does not begin at a root. Before: the root itself,
        //  because the walk ran over whatever components had been collected
        //  before the splitter gave up -- here, none at all.
        bool bThrewRootless = false;
        try   { mgr.RootPath2Object(L"Root.Alpha"); }
        catch (P2Pevent* pEVT) { bThrewRootless = true; pEVT->Cancel(false); }
        TF_CHECK(bThrewRootless);

        //  ".Root..Alpha" used to join it, as a nameless component. It is a
        //  redundant spelling now, not a malformed one -- the root's descendant
        //  collection, and then Alpha (§15) -- and it answers what the direct
        //  spelling answers.
        P3PmsgObject oVia    = mgr.RootPath2Object(L".Root..Alpha");
        P3PmsgObject oDirect = mgr.RootPath2Object(L".Root.Alpha");
        TF_CHECK(!oVia.IsVoid());
        if (!oVia.IsVoid() && !oDirect.IsVoid())
            TF_CHECK(oVia.GetP2Pos() == oDirect.GetP2Pos());

        //  A trailing delimiter names a COLLECTION, and Alpha has neither, so
        //  both are ordinary misses rather than refusals -- and both report it
        //  the same way, which is the point (§15).
        TF_CHECK(mgr.RootPath2Object(L".Root.Alpha@").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Root.Alpha.").IsVoid());
    }
}

// ---------------------------------------------------------------------------
// P3Pmsg_GetPath : naming an object that lives in a snapshot
// ---------------------------------------------------------------------------
static void Test_GeneratedStackPath()
{
    //  GetPath walks aParent, and a snapshot had none: MsgStck::Push left the
    //  field zero. A zero parent is what GetPath reads as a FLOATING item, so
    //  every snapshot of BHP answered ".BHP" wherever BHP actually lived --
    //  and every GENERATION answered the same ".BHP".
    //
    //  Not just a path that fails to resolve: ".BHP" parses as the root name
    //  BHP with no components at all, so RootPath2Object walks nothing and
    //  hands back the ROOT. A path the library generated for one object,
    //  naming another.
    //
    //  Nothing in the block could be walked to find the owner -- aStack runs
    //  owner -> newest -> older and a snapshot has no back-link along it -- so
    //  Push now records the owner in aParent, and GetPath counts the
    //  generation by walking the owner's chain. The walk doubles as the proof
    //  that the block is on it.
    TF_CASE("a snapshot's path is the owner's, plus '^'")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc() += P3PmsgField(L"BHP");

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        TF_CHECK(P3Pmsg_GetPath(&oLive) == L".Store.BHP");

        oLive.r_Stck().Push();
        P3PmsgField oSnap = oLive.SelectObject(L"^");
        TF_CHECK(!oSnap.IsVoid());

        const CString strSnap = P3Pmsg_GetPath(&oSnap);
        TF_CHECK(strSnap == L".Store.BHP^");

        //  And the path the library just produced comes back to the object it
        //  was produced for, which is the whole point of generating one.
        P3PmsgObject oBack = mgr.RootPath2Object(strSnap);
        TF_CHECK(!oBack.IsVoid());
        if (!oBack.IsVoid())
            TF_CHECK(oBack.GetP2Pos() == oSnap.GetP2Pos());

        //  A push does not move the live item, and must not rename its path.
        TF_CHECK(P3Pmsg_GetPath(&oLive) == L".Store.BHP");
    }

    //  Pushes nest, and the delimiter repeats. Two generations used to share
    //  one string; there is no reading of ".BHP" that distinguishes them.
    TF_CASE("every generation gets its own path, and each resolves")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc() += P3PmsgField(L"BHP");

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Desc(P3PmsgField::AttrCMD_Create);
        oLive.r_Desc() += P3PmsgField(L"Early");
        oLive.r_Stck().Push();                          // generation 1
        oLive.r_Desc() += P3PmsgField(L"Late");
        oLive.r_Stck().Push();                          // generation 2

        P3PmsgField oNewer = oLive.SelectObject(L"^");
        P3PmsgField oOlder = oLive.SelectObject(L"^^");
        TF_CHECK(!oNewer.IsVoid());
        TF_CHECK(!oOlder.IsVoid());
        TF_CHECK(oNewer.GetP2Pos() != oOlder.GetP2Pos());

        const CString strNewer = P3Pmsg_GetPath(&oNewer);
        const CString strOlder = P3Pmsg_GetPath(&oOlder);
        TF_CHECK(strNewer == L".Store.BHP^");
        TF_CHECK(strOlder == L".Store.BHP^^");

        P3PmsgObject oBackNewer = mgr.RootPath2Object(strNewer);
        P3PmsgObject oBackOlder = mgr.RootPath2Object(strOlder);
        TF_CHECK(!oBackNewer.IsVoid());
        TF_CHECK(!oBackOlder.IsVoid());
        if (!oBackNewer.IsVoid())
            TF_CHECK(oBackNewer.GetP2Pos() == oNewer.GetP2Pos());
        if (!oBackOlder.IsVoid())
            TF_CHECK(oBackOlder.GetP2Pos() == oOlder.GetP2Pos());
    }

    //  A snapshot holds a whole item, so the things inside it have paths too --
    //  and they always came out right ABOVE the snapshot and wrong at it, the
    //  recursion carrying ".BHP" up from the bottom. One fix at the snapshot
    //  fixes the subtree over it.
    TF_CASE("what is inside a snapshot names itself through the '^'")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Desc(P3PmsgField::AttrCMD_Create);
        oLive.r_Desc() += P3PmsgField(L"Last");
        oLive.r_Stck().Push();

        P3PmsgField oAttr = oLive.SelectObject(L"^@Currency");
        TF_CHECK(!oAttr.IsVoid());
        const CString strAttr = P3Pmsg_GetPath(&oAttr);
        TF_CHECK(strAttr == L".Store.BHP^@Currency");
        P3PmsgObject oBackAttr = mgr.RootPath2Object(strAttr);
        TF_CHECK(!oBackAttr.IsVoid());
        if (!oBackAttr.IsVoid())
            TF_CHECK(oBackAttr.GetP2Pos() == oAttr.GetP2Pos());

        P3PmsgField oKid = oLive.SelectObject(L"^.Last");
        TF_CHECK(!oKid.IsVoid());
        const CString strKid = P3Pmsg_GetPath(&oKid);
        TF_CHECK(strKid == L".Store.BHP^.Last");
        P3PmsgObject oBackKid = mgr.RootPath2Object(strKid);
        TF_CHECK(!oBackKid.IsVoid());
        if (!oBackKid.IsVoid())
            TF_CHECK(oBackKid.GetP2Pos() == oKid.GetP2Pos());
    }

    //  aParent on an item means "the collection I am linked into", and the
    //  three Drop implementations read it to decide which collection to unlink
    //  themselves from -- anything else is ASSERT(0). Push borrows the field
    //  to mean "the item I was pushed from", which is true only while the
    //  block is on that item's stack, so MsgStck returns the loan where it
    //  takes the block off the chain rather than leaving the free to trip over
    //  it. The framework counts a debug assertion as a failure, which is what
    //  makes "stays silent" a thing this case can test.
    TF_CASE("a pop and a drop leave no borrowed parent behind")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc() += P3PmsgField(L"BHP");

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();
        oLive.r_Stck().Push();

        oLive.r_Stck().Pop();                           // frees the newest
        TF_CHECK(oLive.IsStacked());
        TF_CHECK(P3Pmsg_GetPath(&oLive) == L".Store.BHP");

        oLive.r_Stck().Drop();                          // frees the rest
        TF_CHECK(!oLive.IsStacked());
        TF_CHECK(P3Pmsg_GetPath(&oLive) == L".Store.BHP");
    }

    //  An item with no parent at all is still a floating item, and still says
    //  so. A snapshot in an image written before any of this carries a zero
    //  parent and comes out of the same arm, unchanged.
    TF_CASE("an item with no parent still answers with its own name")
    {
        P3PmsgField oLoose(L"Loose");
        TF_CHECK(P3Pmsg_GetPath(&oLoose) == L".Loose");
    }
}

// ---------------------------------------------------------------------------
// P3Pmsg_GetPath : the attribute collection, and the bare '@' that names it
// ---------------------------------------------------------------------------
static void Test_AttrCollectionPath()
{
    //  The P3PmsgAttr overload of GetPath -- the one that ends a path in a
    //  bare '@' -- had no caller anywhere in the library, and it crashed.
    //  P2PmsgAttr_GetVBLockParentnn took GetField()->r_Object(), which is the
    //  owning ITEM's block, and read it through VBLock_pAttr: that picks
    //  VBLockAttr's fields out of a VBLockItem's ut union, so the parent came
    //  back as whatever bytes lay at that offset and Msg2Phys of it ran off
    //  the arena. A P3PmsgAttr obtained the ordinary way, oItem.r_Attr(), was
    //  a segfault.
    //
    //  Handed the right parent it was still wrong: a collection's parent is an
    //  ITEM, every real item is a VBLock_Item block, and the overload's arms
    //  tested VBLock_IsField and VBLock_IsList -- different block TYPES, never
    //  true here. So the walk fell to its own ASSERT(0) and emitted a lone
    //  '@' with no owner in front of it.
    TF_CASE("an attribute collection has a path, and it names the owner")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());

        const CString strColl = P3Pmsg_GetPath(&oLive.r_Attr());
        TF_CHECK(strColl == L".Store.BHP@");

        //  And an attribute in it still spells itself the way it always did,
        //  which is the round-trip the old drop was said to be protecting.
        P3PmsgField oAttr = oLive.SelectObject(L"@Currency");
        TF_CHECK(!oAttr.IsVoid());
        TF_CHECK(P3Pmsg_GetPath(&oAttr) == L".Store.BHP@Currency");
    }

    //  '@' with nothing after it names the COLLECTION, the way '^' with
    //  nothing after it names a snapshot. The selector recursed into the
    //  collection with an empty path, found no name to match and answered
    //  void -- so the one object with no other spelling could not be reached,
    //  and it is the object the overload above emits a path for.
    TF_CASE("a bare '@' selects the collection, through either spelling")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());

        P3PmsgObject oViaObject = oLive.SelectObject(L"@");
        TF_CHECK(!oViaObject.IsVoid());
        TF_CHECK(oViaObject.IsAttr());

        //  P3Pmsg_SplitRootPath used to drop a trailing '@' and hand back the
        //  components for ".Store.BHP", so the root path answered BHP.
        CString        strRoot;
        CList<CString> oItems;
        TF_CHECK(P3Pmsg_SplitRootPath(L".Store.BHP@", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 2);
        if (oItems.GetCount() == 2)
            TF_CHECK(oItems.GetTail() == L"@");

        P3PmsgObject oViaRoot = mgr.RootPath2Object(L".Store.BHP@");
        TF_CHECK(!oViaRoot.IsVoid());
        if (!oViaObject.IsVoid() && !oViaRoot.IsVoid())
            TF_CHECK(oViaObject.GetP2Pos() == oViaRoot.GetP2Pos());
        if (!oViaRoot.IsVoid())
            TF_CHECK(oViaRoot.GetP2Pos() != oLive.GetP2Pos());

        //  The path the library writes for the collection comes back to it.
        const CString strColl = P3Pmsg_GetPath(&oLive.r_Attr());
        P3PmsgObject oBack = mgr.RootPath2Object(strColl);
        TF_CHECK(!oBack.IsVoid());
        if (!oBack.IsVoid() && !oViaObject.IsVoid())
            TF_CHECK(oBack.GetP2Pos() == oViaObject.GetP2Pos());
    }

    //  An item with no attributes has no collection block, and r_Attr() hands
    //  back an object that keeps the heap handle and carries no address --
    //  which IsVoid() does not catch (§9). A miss, not the item, and not an
    //  assertion.
    TF_CASE("a bare '@' on an item with no attributes is a broken path")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc() += P3PmsgField(L"RIO");

        P3PmsgField oBare = mgr.RootPath2Object(L".Store.RIO");
        TF_CHECK(!oBare.IsVoid());
        TF_CHECK(!oBare.IsAttributed());

        TF_CHECK(oBare.SelectObject(L"@").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.RIO@").IsVoid());
    }

    //  A snapshot carries its own collections (§9), and since §11 gave the
    //  snapshot a path the collection inside it gets one too -- built by the
    //  same two steps, and resolved by the same two components.
    TF_CASE("the collection inside a snapshot names itself through the '^'")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();
        oLive.r_Attr() += P3PmsgField(L"Venue");        // after the push

        P3PmsgField oSnap = oLive.SelectObject(L"^");
        TF_CHECK(!oSnap.IsVoid());

        const CString strColl = P3Pmsg_GetPath(&oSnap.r_Attr());
        TF_CHECK(strColl == L".Store.BHP^@");

        P3PmsgObject oBack = mgr.RootPath2Object(strColl);
        TF_CHECK(!oBack.IsVoid());
        TF_CHECK(oBack.IsAttr());

        //  And it is the snapshot's collection, not the live one -- §9's
        //  identity, now reachable from a root path at both ends.
        P3PmsgObject oViaColl = oLive.SelectObject(L"@^");
        TF_CHECK(!oViaColl.IsVoid());
        if (!oBack.IsVoid() && !oViaColl.IsVoid())
            TF_CHECK(oBack.GetP2Pos() == oViaColl.GetP2Pos());
        if (!oBack.IsVoid())
            TF_CHECK(oBack.GetP2Pos() != oLive.SelectObject(L"@").GetP2Pos());
    }
}

// ---------------------------------------------------------------------------
// P3Pmsg_GetPath / SplitRootPath : the descendant collection's own path
// ---------------------------------------------------------------------------
static void Test_DescCollectionPath()
{
    //  The last collection with no path. §12 gave the attribute one a path and
    //  argued this one could not have one: a trailing '.' was dropped, and the
    //  reason given was that '.' is the root delimiter too. It is not, where it
    //  matters -- the root delimiter is consumed once, at position 0, before
    //  any component is parsed, and every other '.' is a descendant delimiter.
    //
    //  What actually stood in the way was the splitter refusing a component one
    //  character long. With that gone the rule is the same for all three:
    //  a delimiter with no name after it names the collection it introduces.
    TF_CASE("a descendant collection has a path, and it names the owner")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());

        const CString strColl = P3Pmsg_GetPath(&oLive.r_Desc());
        TF_CHECK(strColl == L".Store.BHP.");

        //  And it resolves, which is what §12 would not emit a path without.
        P3PmsgObject oBack = mgr.RootPath2Object(strColl);
        TF_CHECK(!oBack.IsVoid());
        TF_CHECK(oBack.IsDesc());

        //  The root has one too, and its path is the shortest there is.
        const CString strRootColl = P3Pmsg_GetPath(&mgr.r_Desc());
        TF_CHECK(strRootColl == L".Store.");
        TF_CHECK(mgr.RootPath2Object(strRootColl).IsDesc());

        //  P2PmsgDesc_GetVBLockParentnn carried the defect §12 found in the
        //  attribute one and fixed only there: it read the owning ITEM's block
        //  through VBLock_pDesc, picking VBLockDesc's fields out of a
        //  VBLockItem's ut union. It had no caller, so it had never fired.
        //  Every check above is one now.
        TF_CHECK(P3Pmsg_GetPath(&oLive.r_Desc()) == P3Pmsg_GetPath(&oLive) + L".");
    }

    //  A snapshot carries its own collections (§9) and its own path (§11), so
    //  the collection inside one gets a path built from both -- and it resolves
    //  by the same two components.
    TF_CASE("the descendant collection inside a snapshot names itself")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();
        oLive.r_Desc() += P3PmsgField(L"Close");        // after the push

        P3PmsgField oSnap = oLive.SelectObject(L"^");
        TF_CHECK(!oSnap.IsVoid());

        const CString strColl = P3Pmsg_GetPath(&oSnap.r_Desc());
        TF_CHECK(strColl == L".Store.BHP^.");

        P3PmsgObject oBack = mgr.RootPath2Object(strColl);
        TF_CHECK(!oBack.IsVoid());
        TF_CHECK(oBack.IsDesc());

        //  It is the snapshot's collection: Close post-dates the push, so it
        //  is in the live one and not in this one.
        TF_CHECK(!mgr.RootPath2Object(L".Store.BHP..Close").IsVoid());
        bool bThrewClose = false;
        try   { mgr.RootPath2Object(L".Store.BHP^..Close"); }
        catch (P2Pevent* pEVT) { bThrewClose = true; pEVT->Cancel(false); }
        TF_CHECK(bThrewClose);
        TF_CHECK(!mgr.RootPath2Object(L".Store.BHP^..Last").IsVoid());
    }

    //  A bare '.' part way along is the collection, and then a name looked up
    //  inside it -- the same two steps "@." would take through the attribute
    //  collection, and a redundant spelling of the one-step form.
    TF_CASE("'..Name' and '.Name' answer the same descendant")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;
        mgr.r_Desc() += P3PmsgField(L"RIO");            // no descendants at all

        P3PmsgObject oShort = mgr.RootPath2Object(L".Store.BHP.Last");
        P3PmsgObject oLong  = mgr.RootPath2Object(L".Store.BHP..Last");
        TF_CHECK(!oShort.IsVoid());
        TF_CHECK(!oLong.IsVoid());
        if (!oShort.IsVoid() && !oLong.IsVoid())
            TF_CHECK(oShort.GetP2Pos() == oLong.GetP2Pos());

        //  A component that carries a NAME and misses still throws, whichever
        //  route it took -- that is the paging rule, and it is untouched.
        bool bThrewName = false;
        try   { mgr.RootPath2Object(L".Store.BHP..Nobody"); }
        catch (P2Pevent* pEVT) { bThrewName = true; pEVT->Cancel(false); }
        TF_CHECK(bThrewName);

        //  A component that carries NO name never throws. An item that has
        //  never had a collection has no block for it, and that is the same
        //  fact ".Store.RIO@" reports as void (§12) -- reporting it two ways
        //  would be an accident of the delimiter.
        TF_CHECK(mgr.RootPath2Object(L".Store.RIO.").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.RIO@").IsVoid());
    }

    //  A collection is not an item: it has neither an attribute collection nor
    //  a descendant one of its own. All four spellings are well-formed
    //  questions that name nothing, and each used to ASSERT(0) on the way --
    //  two of them newly reachable because a bare '.' is a component now.
    TF_CASE("a collection asked for a collection is a miss, not an assertion")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@.").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP.@").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP..").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@@").IsVoid());

        //  The four that DO name something still do.
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@").IsAttr());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP.").IsDesc());
        TF_CHECK(!mgr.RootPath2Object(L".Store.BHP@Currency").IsVoid());
        TF_CHECK(!mgr.RootPath2Object(L".Store.BHP.Last").IsVoid());
    }
}

// ---------------------------------------------------------------------------
// P2PmsgMgr::P2Pos2Path : every kind of block a P2Pos can name
// ---------------------------------------------------------------------------
static void Test_P2Pos2Path()
{
    //  The manager answered for a plain field and asserted for everything
    //  else, so an object could have a path P3Pmsg_GetPath would build and the
    //  manager would not. Six of the eleven kinds fell through: a list and a
    //  vector, because the test was IsField(pos) and IsField(pos) is
    //  VBLockItem_IsField, which neither of them is; and all four collections,
    //  because they are not items at all.
    //
    //  Every case here asserted before, and the framework folds an ASSERT into
    //  the running case as a failure -- so these fail on the old code without
    //  needing to name the assertion.
    TF_CASE("a list and a vector have paths, not assertions")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgList oNums(L"Numbers");
        mgr.r_Desc(P3PmsgField::AttrCMD_Create);
        mgr.r_Desc() += oNums;

        P3PmsgVect oVect(3, L"Elem", P3PmsgData((int)0));
        oVect.r_name() = L"Vec";
        mgr.r_Desc() += oVect;

        P3PmsgObject oList = mgr.RootPath2Object(L".Store.Numbers");
        P3PmsgObject oVec  = mgr.RootPath2Object(L".Store.Vec");
        TF_CHECK(oList.IsList());
        TF_CHECK(oVec.IsVect());

        TF_CHECK(mgr.P2Pos2Path(oList.GetP2Pos()) == L".Store.Numbers");
        TF_CHECK(mgr.P2Pos2Path(oVec.GetP2Pos())  == L".Store.Vec");

        //  §6 is why this is a one-line change rather than two new arms:
        //  P3PmsgList and P3PmsgVect both derive from P3PmsgField, so the
        //  P3PmsgField overload has built their paths all along. Only the
        //  manager could not reach it -- and only because the CONVERTING
        //  CONSTRUCTOR was never used. Assignment throws for a non-field,
        //  which is the trap RootPath2Object carried until §13.
        P3PmsgField oAsField = oList;
        TF_CHECK(P3Pmsg_GetPath(&oAsField) == L".Store.Numbers");
    }

    //  §12 gave the attribute collection an overload and §15 the descendant
    //  one, and both build a path from the collection's OWNER. A P3PmsgAttr
    //  converted straight from a P3PmsgObject has no owner -- the constructor
    //  sets only m_oObject and leaves GetField() null -- so the manager has to
    //  find the owner first. GetParent() on a collection block is exactly the
    //  item it hangs off.
    TF_CASE("both collections have paths, and they name their owner")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());

        TF_CHECK(mgr.P2Pos2Path(oLive.r_Attr().r_Object().GetP2Pos()) == L".Store.BHP@");
        TF_CHECK(mgr.P2Pos2Path(oLive.r_Desc().r_Object().GetP2Pos()) == L".Store.BHP.");

        //  The root's own descendant collection, which is the shortest path
        //  there is (§15).
        TF_CHECK(mgr.P2Pos2Path(mgr.r_Desc().r_Object().GetP2Pos()) == L".Store.");

        //  And the manager agrees with the overloads it is now dispatching to,
        //  rather than merely producing something that looks similar.
        TF_CHECK(mgr.P2Pos2Path(oLive.r_Attr().r_Object().GetP2Pos()) ==
                 P3Pmsg_GetPath(&oLive.r_Attr()));
        TF_CHECK(mgr.P2Pos2Path(oLive.r_Desc().r_Object().GetP2Pos()) ==
                 P3Pmsg_GetPath(&oLive.r_Desc()));
    }

    //  A path that does not come back is not a path. Every spelling the
    //  manager emits is handed straight back to RootPath2Object and has to
    //  land on the P2Pos it was built from -- which is the check §12 made the
    //  condition of emitting a collection path at all.
    TF_CASE("every path the manager builds returns to the P2Pos it came from")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgList oNums(L"Numbers");
        oNums.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Unit");
        mgr.r_Desc() += oNums;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        P3PmsgField oList = mgr.RootPath2Object(L".Store.Numbers");
        TF_CHECK(!oLive.IsVoid());
        TF_CHECK(!oList.IsVoid());

        const P2Pos aPos[] = {
            mgr.r_Object().GetP2Pos(),                      // the root item
            oLive.GetP2Pos(),                               // a field
            mgr.RootPath2Object(L".Store.BHP.Last").GetP2Pos(),
            mgr.RootPath2Object(L".Store.BHP@Currency").GetP2Pos(),
            oList.GetP2Pos(),                               // a list
            oLive.r_Attr().r_Object().GetP2Pos(),           // its attr coll
            oLive.r_Desc().r_Object().GetP2Pos(),           // its desc coll
            mgr.r_Desc().r_Object().GetP2Pos(),             // the root's
            oList.r_Attr().r_Object().GetP2Pos(),           // a list's attr coll
        };

        for (size_t i = 0; i < sizeof(aPos) / sizeof(aPos[0]); ++i)
        {
            const CString strPath = mgr.P2Pos2Path(aPos[i]);
            TF_CHECK(!strPath.IsEmpty());
            if (strPath.IsEmpty())
                continue;
            P3PmsgObject oBack = mgr.RootPath2Object(strPath);
            TF_CHECK(!oBack.IsVoid());
            if (!oBack.IsVoid())
                TF_CHECK(oBack.GetP2Pos() == aPos[i]);
        }
    }

    //  A P2Pos naming nothing is not a caller error worth an assertion. §9's
    //  rule is that what a caller spells is the caller's business, and the
    //  empty string says "no path" the way a void P3PmsgObject says "no
    //  object" -- msgcore_mgr_p2pos2path keeps the two apart already, since it
    //  answers nullptr only when the call threw.
    TF_CASE("a P2Pos that names no object has no path, and does not assert")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"BHP");

        TF_CHECK(mgr.P2Pos2Path(0).IsEmpty());
    }
}

// ---------------------------------------------------------------------------
// '.' with no NAME after it is bare wherever it stands
// ---------------------------------------------------------------------------
static void Test_BareDotCommutes()
{
    //  §9's rule is that '^' commutes with '@' and with '.'. It did for '@':
    //  "Item@^" and "Item^@" both name the attribute collection inside the
    //  snapshot. For '.' only one direction worked -- "Item^." named the
    //  snapshot's descendant collection and "Item.^" named something else.
    //
    //  Three places read "a '.' at the END of the path" where the rule is "a
    //  '.' with no NAME after it": the root walk's strip, the selector's
    //  wrapper, and the field arm of the recurse. The splitter absorbs a
    //  following '^' into a component (it does the same for "@^"), so
    //  ".Store.BHP.^" reaches the walk as ".^" and was stripped to "^".
    TF_CASE("'.^' names the snapshot's descendant collection, as '^.' does")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();
        oLive.r_Desc() += P3PmsgField(L"Close");        // live only

        P3PmsgField oSnap = oLive.SelectObject(L"^");
        TF_CHECK(!oSnap.IsVoid());
        const P2Pos posSnapDesc = oSnap.r_Desc().r_Object().GetP2Pos();
        TF_CHECK(posSnapDesc != 0);

        //  The root path. It used to answer the snapshot ITEM -- two levels
        //  from the object "^." answers by the same reasoning.
        P3PmsgObject oRoot = mgr.RootPath2Object(L".Store.BHP.^");
        TF_CHECK(oRoot.IsDesc());
        TF_CHECK(oRoot.GetP2Pos() == posSnapDesc);
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP^.").GetP2Pos() == posSnapDesc);

        //  The object path. It used to be void: the wrapper read the leading
        //  '.' as "this component names the object you are standing on",
        //  ParseObjectPath stopped on the '^' with an empty name, and the
        //  empty name was matched against "BHP".
        P3PmsgObject oFrom = oLive.r_Object();
        P3PmsgObject oSel  = P3Pmsg_SelectObject(&oFrom, L".^");
        TF_CHECK(oSel.IsDesc());
        TF_CHECK(oSel.GetP2Pos() == posSnapDesc);
        TF_CHECK(P3Pmsg_SelectObject(&oFrom, L"^.").GetP2Pos() == posSnapDesc);

        //  And the collection the snapshot carries is the one taken AT the
        //  push, which is what makes this worth naming at all: Close went in
        //  afterwards and is not in it.
        TF_CHECK(!P3Pmsg_SelectObject(&oSel, L"Last").IsVoid());
        TF_CHECK(P3Pmsg_SelectObject(&oSel, L"Close").IsVoid());
    }

    //  A root path and an object path are two spellings of one question. The
    //  walk strips a name-carrying '.' off each component and hands the rest
    //  to P3Pmsg_SelectObject, so the two agree by construction -- except
    //  where a bare '.' was not recognised as bare, and then the walk answered
    //  and the object path did not.
    TF_CASE("a root path and an object path agree on every bare-'.' spelling")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();
        oLive.r_Desc() += P3PmsgField(L"Close");

        P3PmsgObject oFrom = oLive.r_Object();

        //  Every suffix begins with a delimiter, so ".Store.BHP" + suffix is a
        //  continuation of the path and not a longer item name.
        static LPCWSTR aSuffix[] = {
            L".", L"@", L"^", L".^", L"^.", L"@^", L"^@",
            L"..Last", L".^.Last", L"^.Last", L"..Close", L"@Currency",
            L"@^Currency", L"^@Currency",
        };

        for (size_t i = 0; i < sizeof(aSuffix) / sizeof(aSuffix[0]); ++i)
        {
            const CString strPath = CString(L".Store.BHP") + aSuffix[i];
            P3PmsgObject oRoot = mgr.RootPath2Object(strPath);
            P3PmsgObject oSel  = P3Pmsg_SelectObject(&oFrom, aSuffix[i]);
            TF_CHECK(!oRoot.IsVoid());
            TF_CHECK(!oSel.IsVoid());
            TF_CHECK(oRoot.GetP2Pos() == oSel.GetP2Pos());
        }

        //  ".Last" was the one spelling that differed, and §18 is why it no
        //  longer does: a leading '.' asserts the object's own name only where
        //  a path could be ROOTED, which is an object with no parent. BHP has
        //  one, so the '.' there is an ordinary descendant delimiter and both
        //  spellings descend.
        TF_CHECK(!mgr.RootPath2Object(L".Store.BHP.Last").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP.Last").GetP2Pos() ==
                 P3Pmsg_SelectObject(&oFrom, L".Last").GetP2Pos());
    }

    //  Legalising a bare '.' part way along makes more spellings well-formed,
    //  and each needs a defined answer. A collection is not an item: it has
    //  neither a descendant collection nor an attribute one of its own, which
    //  is what "Item@." and "Item@@" already answered (§15). The '.' spellings
    //  now answer the same way -- and by the same route, rather than by two.
    TF_CASE("a collection still has no collection of its own, by either spelling")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        oLive.r_Stck().Push();
        P3PmsgObject oFrom = oLive.r_Object();

        static LPCWSTR aVoid[] = { L"..", L".@", L"@.", L"@@", L"^^", L".^." };
        for (size_t i = 0; i < sizeof(aVoid) / sizeof(aVoid[0]); ++i)
        {
            TF_CHECK(mgr.RootPath2Object(CString(L".Store.BHP") + aVoid[i]).IsVoid());
            TF_CHECK(P3Pmsg_SelectObject(&oFrom, aVoid[i]).IsVoid());
        }

        //  ".^." used to answer the snapshot's descendant collection through
        //  the root path -- but only because ".^" meant the snapshot ITEM, so
        //  the trailing '.' was asking an ITEM for its collection. With ".^"
        //  naming the collection, the trailing '.' asks a COLLECTION for one,
        //  and that is the void every line above reports. The object is not
        //  lost: it is what ".^" and "^." now both answer.
        const P2Pos posSnapDesc = mgr.RootPath2Object(L".Store.BHP^.").GetP2Pos();
        TF_CHECK(posSnapDesc != 0);
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP.^").GetP2Pos() == posSnapDesc);
    }
}

// ---------------------------------------------------------------------------
// A leading '.' is the root marker only where a path could be rooted
// ---------------------------------------------------------------------------
static void Test_RootedLeadingDot()
{
    //  P3Pmsg_GetPath says what a path is rooted at, in its own first comment:
    //  "Process root. NOTES: Defined by absence of parent." It emits
    //  ".item1.item2" for a root object and ".item" for a floating one, and
    //  the leading component names the object the path starts from.
    //
    //  P3Pmsg_SelectObject asserted that name at every depth instead, so
    //  ".Last" asked BHP whether BHP is called Last -- while ".Store.BHP.Last"
    //  descends, because the walk strips a name-carrying '.' before selecting.
    TF_CASE("a leading '.' descends on an object that has a parent")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        TF_CHECK(oLive.r_Object().HasParent());

        P3PmsgObject oFrom = oLive.r_Object();
        const P2Pos posLast = mgr.RootPath2Object(L".Store.BHP.Last").GetP2Pos();
        TF_CHECK(posLast != 0);
        TF_CHECK(P3Pmsg_SelectObject(&oFrom, L".Last").GetP2Pos() == posLast);
        TF_CHECK(P3Pmsg_SelectObject(&oFrom, L"Last" ).GetP2Pos() == posLast);

        //  What goes with it. ".BHP.Last" asked OF BHP used to resolve, by
        //  matching BHP's own name and then descending -- the assertion
        //  reaching where no path is rooted. It is void now, and deliberately:
        //  BHP has no descendant called BHP.
        TF_CHECK(P3Pmsg_SelectObject(&oFrom, L".BHP.Last").IsVoid());
    }

    //  The root keeps the assertion, and P2PmsgMgr::Path2Object is what needs
    //  it: a path rooted somewhere else has to be refused rather than hunted
    //  for among the root's descendants.
    TF_CASE("the root still refuses a path rooted somewhere else")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"BHP");
        //  A descendant carrying the name the wrong root asks for, so a miss
        //  here cannot be an accident of there being nothing to find.
        mgr.r_Desc() += P3PmsgField(L"Elsewhere");

        P3PmsgObject oRoot = mgr.r_Object();
        TF_CHECK(!oRoot.HasParent());
        TF_CHECK(!mgr.Path2Object(L".Store.BHP").IsVoid());
        TF_CHECK(mgr.Path2Object(L".Elsewhere.BHP").IsVoid());
        TF_CHECK(mgr.Path2Object(L".Nobody.BHP").IsVoid());
    }

    //  And a component that matched with nothing after it is the answer. It
    //  used to fall through carrying an empty path, where ParseObjectPath
    //  produced an empty name and the Goto for it matched nothing -- so the
    //  library could not resolve the path it emits for a root.
    TF_CASE("the path the library emits for a root resolves at the root")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"BHP");

        const CString strRootPath = P3Pmsg_GetPath(&mgr);
        TF_CHECK(strRootPath == L".Store");

        P3PmsgObject oRoot = mgr.r_Object();
        P3PmsgObject oBack = P3Pmsg_SelectObject(&oRoot, strRootPath);
        TF_CHECK(!oBack.IsVoid());
        TF_CHECK(oBack.GetP2Pos() == oRoot.GetP2Pos());
        TF_CHECK(!mgr.Path2Object(strRootPath).IsVoid());
    }

    //  The same empty remainder, on the two collections. A leading '.' was
    //  never the root marker there -- both arms descend by name -- but the
    //  match was thrown away just the same, so "Tag" answered and ".Tag" did
    //  not.
    TF_CASE("a collection answers a leading '.' with nothing after the name")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        oInst.r_Desc(P3PmsgField::AttrCMD_Create);
        oInst.r_Desc() += P3PmsgField(L"Last");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());

        P3PmsgObject oDescColl = oLive.r_Desc().r_Object();
        P3PmsgObject oAttrColl = oLive.r_Attr().r_Object();

        TF_CHECK(P3Pmsg_SelectObject(&oDescColl, L".Last").GetP2Pos() ==
                 P3Pmsg_SelectObject(&oDescColl, L"Last" ).GetP2Pos());
        TF_CHECK(!P3Pmsg_SelectObject(&oDescColl, L".Last").IsVoid());

        TF_CHECK(P3Pmsg_SelectObject(&oAttrColl, L".Currency").GetP2Pos() ==
                 P3Pmsg_SelectObject(&oAttrColl, L"Currency" ).GetP2Pos());
        TF_CHECK(!P3Pmsg_SelectObject(&oAttrColl, L".Currency").IsVoid());

        //  A miss is still a miss.
        TF_CHECK(P3Pmsg_SelectObject(&oDescColl, L".Nobody").IsVoid());
        TF_CHECK(P3Pmsg_SelectObject(&oAttrColl, L".Nobody").IsVoid());
    }
}

// ---------------------------------------------------------------------------
// A floating item is one object, however many handles name it
// ---------------------------------------------------------------------------
static void Test_FloatingIdentity()
{
    //  A floating item's VBLock is built INSIDE the P3PmsgObject that carries
    //  it, so the object was the storage and a copy of the object was a second
    //  block rather than a second handle on one. Everything below follows from
    //  that, and all of it is fixed by moving the block onto a heap of its own
    //  the first time it is shared.
    TF_CASE("a floating item's P2Pos is one number through every handle")
    {
        P3PmsgField  oFloat(L"Floater");
        P3PmsgObject oA = oFloat.r_Object();
        P3PmsgObject oB = oFloat.r_Object();
        P3PmsgObject oC(oA);

        TF_CHECK(oFloat.GetP2Pos() != 0);
        TF_CHECK(oA.GetP2Pos() == oFloat.GetP2Pos());
        TF_CHECK(oB.GetP2Pos() == oFloat.GetP2Pos());
        TF_CHECK(oC.GetP2Pos() == oFloat.GetP2Pos());

        //  And the same answer twice, which was already true and is the half
        //  §19 reported as working.
        TF_CHECK(oA.GetP2Pos() == oA.GetP2Pos());
    }

    //  The number was the symptom. This is what it meant: the handle was a
    //  snapshot, so a write through it went somewhere else.
    TF_CASE("a write through a handle reaches the item")
    {
        P3PmsgField  oFloat(L"Floater");
        P3PmsgObject oSel = oFloat.r_Object();
        P3PmsgField  oSelF = oSel;
        oSelF.r_name() = L"Renamed";
        TF_CHECK(oFloat.r_name().c_wcsicmp(L"Renamed") == 0);
    }

    //  §19's own case. P3Pmsg_GetPath emits ".Floater" for a floating item
    //  (its first comment calls that a root), and selecting it answers the
    //  item -- not a duplicate of it.
    TF_CASE("a floating item selects to itself by its own path")
    {
        P3PmsgField  oFloat(L"Floater");
        const CString strPath = P3Pmsg_GetPath(&oFloat);
        TF_CHECK(strPath == L".Floater");

        P3PmsgObject oSelf = oFloat.r_Object();
        P3PmsgObject oBack = P3Pmsg_SelectObject(&oSelf, strPath);
        TF_CHECK(!oBack.IsVoid());
        TF_CHECK(oBack.GetP2Pos() == oFloat.GetP2Pos());
    }

    //  The hazard behind the number, and the one that does not announce
    //  itself. A handle taken from a floating item pointed into that object's
    //  own storage, so it outlived nothing: Connect created a heap, left
    //  m_aVBLock addressing the source, and a SYS heap resolves an address by
    //  returning it -- which reads the dead object's memory and looks right.
    TF_CASE("the block outlives the object that built it")
    {
        P3PmsgObject oKept;
        {
            P3PmsgField oFloat(L"Ephemeral");
            oKept = oFloat.r_Object();
        }
        TF_CHECK(!oKept.IsVoid());
        P3PmsgField oKeptF = oKept;
        TF_CHECK(oKeptF.r_name().c_wcsicmp(L"Ephemeral") == 0);
    }

    //  The other side of the rule, and a guard rather than a pin: these two
    //  pass against the unfixed library as well. Only an ITEM is rehomed. A
    //  standalone block that is not one is a VALUE -- P3PmsgData and
    //  P3PmsgName are built that way on purpose -- and copying a value still
    //  produces a second value.
    TF_CASE("a standalone value block is still copied as a value")
    {
        P3PmsgData oD1((int)42);
        P3PmsgData oD2(oD1);
        TF_CHECK(oD2.c_int() == 42);
        oD2 = P3PmsgData((int)99);
        TF_CHECK(oD2.c_int() == 99);
        TF_CHECK(oD1.c_int() == 42);        // the original is not the copy
    }

    //  And an item in a tree never had the problem: its block is on the
    //  message heap and a copy of the handle shares it.
    TF_CASE("an item in a tree is unaffected")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"BHP");

        P3PmsgField  oTree = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oTree.IsVoid());
        P3PmsgObject oCpy(oTree.r_Object());
        TF_CHECK(oCpy.GetP2Pos() == oTree.GetP2Pos());
    }

    //  A floating item that already had a heap -- any collection or push gives
    //  it one, through AllocVBLock -- keeps working, and keeps its number.
    //  This is the invariant the rehome relies on: an item with no heap can
    //  have no collections, so no back-pointer needs fixing up when it moves.
    TF_CASE("a floating item with descendants keeps one identity")
    {
        P3PmsgField oFloat(L"Floater");
        oFloat.r_Desc(P3PmsgField::AttrCMD_Create);
        oFloat.r_Desc() += P3PmsgField(L"Kid");

        const P2Pos pos = oFloat.GetP2Pos();
        P3PmsgObject oSelf = oFloat.r_Object();
        TF_CHECK(oSelf.GetP2Pos() == pos);

        P3PmsgObject oKid = P3Pmsg_SelectObject(&oSelf, L"Kid");
        TF_CHECK(!oKid.IsVoid());
        P3PmsgField oKidF = oKid;
        TF_CHECK(oKidF.r_name().c_wcsicmp(L"Kid") == 0);
        TF_CHECK(oFloat.GetP2Pos() == pos);   // asking did not move it
    }
}

// ---------------------------------------------------------------------------
// "Is there anything here?" is one question with one answer
// ---------------------------------------------------------------------------
static void Test_VoidPredicates()
{
    //  Every class in this family answers it twice -- IsVoid() and
    //  operator bool -- and the two used to disagree, in opposite directions
    //  between classes, and inverted outright on P3PmsgList and P3PmsgVect.
    TF_CASE("IsVoid and operator bool agree, in every state an item can be in")
    {
        P3PmsgField oNothing;
        TF_CHECK(oNothing.IsVoid() == !(bool)oNothing);
        TF_CHECK(oNothing.r_Object().IsVoid() == !(bool)oNothing.r_Object());
        TF_CHECK(oNothing.IsVoid() == oNothing.r_Object().IsVoid());

        P3PmsgField oFloat(L"Floater");
        TF_CHECK(oFloat.IsVoid() == !(bool)oFloat);
        TF_CHECK(oFloat.r_Object().IsVoid() == !(bool)oFloat.r_Object());
        TF_CHECK(oFloat.IsVoid() == oFloat.r_Object().IsVoid());

        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgField oTree = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        TF_CHECK(oTree.IsVoid() == !(bool)oTree);
        TF_CHECK(oTree.r_Object().IsVoid() == !(bool)oTree.r_Object());
        TF_CHECK(!oTree.IsVoid());

        P3PmsgField oCopy = mgr.r_Desc().SelectItem(L"AAA");
        TF_CHECK(oCopy.IsVoid() == !(bool)oCopy);
        TF_CHECK(oCopy.IsVoid() == oCopy.r_Object().IsVoid());
    }

    //  A floating item is something, and it was reported as nothing because
    //  the question asked about the HEAP rather than the item.
    TF_CASE("a floating item is something")
    {
        P3PmsgField oFloat(L"Floater");
        TF_CHECK(!oFloat.IsVoid());
        TF_CHECK((bool)oFloat);
        TF_CHECK(!oFloat.r_Object().IsVoid());
        TF_CHECK((bool)oFloat.r_Object());
    }

    //  And §19 is why asking about the heap could not stand: an inline item
    //  is rehomed the first time it is shared, so the heap appears when
    //  somebody takes a copy of the handle. Asking must not decide the answer.
    TF_CASE("sharing a floating item does not change whether it is anything")
    {
        P3PmsgField oFloat(L"Floater");
        const bool bVoidBefore = oFloat.IsVoid();
        const bool bBoolBefore = (bool)oFloat.r_Object();

        P3PmsgObject oShared = oFloat.r_Object();   // the copy that rehomes it

        TF_CHECK(oFloat.IsVoid()        == bVoidBefore);
        TF_CHECK((bool)oFloat.r_Object() == bBoolBefore);
        TF_CHECK(oFloat.r_name().c_wcsicmp(L"Floater") == 0);
    }

    //  The answer that must not change: a lookup that found nothing.
    TF_CASE("a lookup that found nothing is void, as a field and as an object")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgObject oRoot = mgr.r_Object();
        P3PmsgObject oMiss = P3Pmsg_SelectObject(&oRoot, L"NoSuchItem");
        TF_CHECK(oMiss.IsVoid());
        TF_CHECK(!(bool)oMiss);

        P3PmsgField oMissField(oMiss);
        TF_CHECK(oMissField.IsVoid());
        TF_CHECK(!(bool)oMissField);
    }

    TF_CASE("an emptied field is void")
    {
        P3PmsgField oFloat(L"Floater");
        TF_CHECK(!oFloat.IsVoid());
        oFloat.Nullify();
        TF_CHECK(oFloat.IsVoid());
        TF_CHECK(!(bool)oFloat);
        TF_CHECK(oFloat.r_Object().IsVoid());
    }

    //  P3PmsgList and P3PmsgVect returned IsVoid() from operator bool, so
    //  `if ( oList )` was true exactly when there was no list. Neither had a
    //  caller in this solution, which is why nothing had noticed.
    TF_CASE("a list and a vector answer the right way round")
    {
        P3PmsgList oList;
        oList.AddListTail(P3PmsgData((int)1));
        oList.AddListTail(P3PmsgData((int)2));
        TF_CHECK_EQ((int)oList.GetCount(), 2);
        TF_CHECK((bool)oList);
        TF_CHECK(oList.IsVoid() == !(bool)oList);

        P3PmsgVect oVect(3, L"Elem", P3PmsgData((int)0));
        TF_CHECK((bool)oVect);
        TF_CHECK(oVect.IsVoid() == !(bool)oVect);
    }

    //  §20's own statement, pinned so that the arm removed from the copy
    //  constructor is not revived by accident: a P3PmsgField copies as a
    //  VALUE, and the handle is spelled r_Object().
    TF_CASE("a field copy is a second item; r_Object() is the item")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgField oCopy = mgr.r_Desc().SelectItem(L"AAA");
        P3PmsgField oHnd  = mgr.r_Desc().SelectItem(L"AAA").r_Object();

        TF_CHECK(oCopy.GetP2Pos() != oHnd.GetP2Pos());
        TF_CHECK_EQ(oCopy.r_data().c_int(), 1);

        oCopy.r_data().c_int(222);
        TF_CHECK_EQ(mgr.r_Desc().SelectItem(L"AAA").r_data().c_int(), 1);

        oHnd.r_data().c_int(333);
        TF_CHECK_EQ(mgr.r_Desc().SelectItem(L"AAA").r_data().c_int(), 333);
    }

    //  And why the library writes r_Object() rather than binding a reference:
    //  a collection hands back its own cursor, and the next Select moves it.
    TF_CASE("a collection hands back its cursor, and the next Select moves it")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));
        mgr.r_Desc() += P3PmsgField(L"BBB", P3PmsgData((int)2));

        P3PmsgField& oCurs = mgr.r_Desc().SelectItem(L"AAA");
        TF_CHECK(oCurs.r_name().c_wcsicmp(L"AAA") == 0);

        P3PmsgField oHeld = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        mgr.r_Desc().SelectItem(L"BBB");

        TF_CHECK(oCurs.r_name().c_wcsicmp(L"BBB") == 0);   // the cursor moved
        TF_CHECK(oHeld.r_name().c_wcsicmp(L"AAA") == 0);   // the handle did not
    }
}

// ---------------------------------------------------------------------------
// "Are these two the same item?" is a question a field can be asked
// ---------------------------------------------------------------------------
static void Test_ItemIdentity()
{
    //  §20 left it recorded that a P3PmsgField copies as a value and
    //  r_Object() copies as a handle, and that nothing at the call site says
    //  which you have. The pair can be asked now, and this is the answer.
    TF_CASE("a handle is the same item; a value copy is not")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));
        mgr.r_Desc() += P3PmsgField(L"BBB", P3PmsgData((int)2));

        P3PmsgField oTree  = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        P3PmsgField oHnd   = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        P3PmsgField oCopy  = mgr.r_Desc().SelectItem(L"AAA");
        P3PmsgField oOther = mgr.r_Desc().SelectItem(L"BBB").r_Object();

        TF_CHECK(oTree == oTree);
        TF_CHECK(oTree == oHnd);            // the same item, twice
        TF_CHECK(!(oTree == oCopy));        // reads the same, IS not the same
        TF_CHECK(!(oTree == oOther));       // not even the same name
        TF_CHECK(oTree != oCopy);
        TF_CHECK(oTree != oOther);
        TF_CHECK(!(oTree != oHnd));
    }

    //  The identity test and the write-through agree, which is the only
    //  reason to have it: whoever == says is the item is the one a write
    //  reaches.
    TF_CASE("== agrees with what a write actually reaches")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgField oTree = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        P3PmsgField oHnd  = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        P3PmsgField oCopy = mgr.r_Desc().SelectItem(L"AAA");

        TF_CHECK(oTree == oHnd);
        oHnd.r_data().c_int(333);
        TF_CHECK_EQ(mgr.r_Desc().SelectItem(L"AAA").r_data().c_int(), 333);

        TF_CHECK(!(oTree == oCopy));
        oCopy.r_data().c_int(999);
        TF_CHECK_EQ(mgr.r_Desc().SelectItem(L"AAA").r_data().c_int(), 333);

        TF_CHECK_EQ((int)(oTree.GetP2Pos() == oHnd.GetP2Pos()), 1);
        TF_CHECK_EQ((int)(oTree.GetP2Pos() == oCopy.GetP2Pos()), 0);
    }

    //  An empty floating field is an item too -- §20 -- so it is not equal to
    //  every other item merely because both of them are something.
    TF_CASE("two unrelated floating items are not the same item")
    {
        P3PmsgField oEmpty;
        P3PmsgField oFloatA(L"Floater");
        P3PmsgField oFloatB(L"Floater");

        TF_CHECK(!(oEmpty == oFloatA));
        TF_CHECK(!(oFloatA == oFloatB));    // same name, two items
        TF_CHECK(oFloatA == oFloatA);
        TF_CHECK(oFloatA.r_name().c_wcsicmp(oFloatB.r_name().c_name()) == 0);
    }

    //  A void field denotes no item, so it is not the same item as anything
    //  that does -- and two of them are equally nothing.
    TF_CASE("void is not an item, and all of it is the same nothing")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgObject oRoot  = mgr.r_Object();
        P3PmsgField  oVoidA ( P3Pmsg_SelectObject(&oRoot, L"NoSuchItem") );
        P3PmsgField  oVoidB ( P3Pmsg_SelectObject(&oRoot, L"NorThisOne") );
        P3PmsgField  oTree  = mgr.r_Desc().SelectItem(L"AAA").r_Object();

        TF_CHECK(oVoidA.IsVoid());
        TF_CHECK(oVoidB.IsVoid());
        TF_CHECK(oVoidA == oVoidB);
        TF_CHECK(!(oVoidA == oTree));
        TF_CHECK(oVoidA != oTree);
    }

    //  The object answers the same question, because that is where this one
    //  is asked. P3PmsgObject gained != for the same reason the field did.
    TF_CASE("the object underneath gives the same answers")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgField oTree = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        P3PmsgField oHnd  = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        P3PmsgField oCopy = mgr.r_Desc().SelectItem(L"AAA");

        TF_CHECK((oTree == oHnd)  == (oTree.r_Object() == oHnd.r_Object()));
        TF_CHECK((oTree == oCopy) == (oTree.r_Object() == oCopy.r_Object()));
        TF_CHECK(oTree.r_Object() != oCopy.r_Object());
        TF_CHECK(!(oTree.r_Object() != oHnd.r_Object()));
    }

    //  And the comparison this class always meant to offer is still the one a
    //  string literal selects -- NOT an identity test against a temporary
    //  field that happens to carry that name.
    TF_CASE("comparing a field to a name still compares the name")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgField oTree = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        TF_CHECK(oTree == L"AAA");
        TF_CHECK(oTree == L"aaa");          // the name compare is caseless
        TF_CHECK(!(oTree == L"BBB"));
    }
}

// ---------------------------------------------------------------------------
// A const field can be compared to its name
// ---------------------------------------------------------------------------
static void Test_ConstFieldName()
{
    //  P3PmsgName declares both of its comparisons const; P3PmsgField hides
    //  them with one that was not, so this did not compile at all -- C2678 --
    //  while every meaningless comparison between two fields did.
    TF_CASE("a const field answers to its own name")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        const P3PmsgField oConst = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        TF_CHECK(oConst == L"AAA");
        TF_CHECK(!(oConst == L"BBB"));

        const P3PmsgField oOther = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        TF_CHECK(oConst == oOther);
        TF_CHECK(!(oConst != oOther));
    }
}

// ---------------------------------------------------------------------------
// A field can say whether a write through it reaches anyone else
// ---------------------------------------------------------------------------

//  A floating item whose payload will not fit the inline block.  128 wide
//  characters is enough: AllocVBLock gives it a private SYS heap for the data
//  and leaves the item block inline, so the object carries a heap AND
//  addresses itself.  That is the state §22 is about.
//  Filled IN PLACE: returning one by value would copy it, and a copy of a
//  field is a fresh inline item with no heap -- which is the opposite of what
//  these cases need.
static void GrowFloater ( P3PmsgField& oField, int nValue )
{
    CString sBig ( L'x', 1024 );
    oField.r_data() = P3PmsgData((LPCWSTR)(LPCTSTR)sBig);
    oField.r_data() = P3PmsgData((int)nValue);
}

//  Built here, handed back as the documented handle, read by the caller after
//  this frame is gone.
static P3PmsgObject HandleFromDeadFrame ( )
{
    CString sBig ( L'x', 1024 );
    P3PmsgField oField ( L"DEAD", P3PmsgData((LPCWSTR)(LPCTSTR)sBig) );
    oField.r_data() = P3PmsgData((int)4242);
    return oField.r_Object();
}

//  Overwrite whatever the frame above left behind.
static void ClobberFrame ( int nDepth )
{
    volatile TCHAR sz[512];
    for ( int i = 0; i < 512; i++ ) sz[i] = (TCHAR)0xCDCD;
    if ( nDepth > 0 ) ClobberFrame ( nDepth - 1 );
}

static void Test_HandleOrCopy()
{
    //  THE DEFECT.  RehomeInlineItem declined on "there is already a heap",
    //  which is not the question -- the question is whether the BLOCK is
    //  inline.  On a grown floater the two disagreed, so nothing was rehomed
    //  and the copy took m_aVBLock verbatim: the source object's own address.
    TF_CASE("a handle on a grown floater names the item, not the source")
    {
        P3PmsgField  oSrc(L"AAA");
        GrowFloater(oSrc, 7);
        P3PmsgObject oHeld = oSrc.r_Object();

        //  The one that used to fail: the copy addressed oSrc's inline array.
        TF_CHECK(oHeld.m_aVBLock != (VBLaddr)&oSrc.r_Object().m_oVBLock[0]);
        TF_CHECK(oHeld.m_aVBLock != 0);
        TF_CHECK(oHeld.m_hVBList == oSrc.r_Object().m_hVBList);

        //  And it is the item, so a write through it is seen by the source.
        P3PmsgField oHnd(oHeld);
        oHnd.r_data().c_int(77);
        TF_CHECK_EQ(oSrc.r_data().c_int(), 77);
        TF_CHECK(oHnd == oSrc);
    }

    //  The same block through assignment, which is Nullify + Connect and
    //  carried the identical guard.
    TF_CASE("assignment of a grown floater names the item too")
    {
        P3PmsgField  oSrc(L"BBB");
        GrowFloater(oSrc, 8);
        P3PmsgObject oHeld;
        oHeld = oSrc.r_Object();

        TF_CHECK(oHeld.m_aVBLock != (VBLaddr)&oSrc.r_Object().m_oVBLock[0]);
        TF_CHECK(oHeld.m_hVBList == oSrc.r_Object().m_hVBList);

        P3PmsgField oHnd(oHeld);
        oHnd.r_data().c_int(88);
        TF_CHECK_EQ(oSrc.r_data().c_int(), 88);
    }

    //  What that cost.  The handle outlives the frame it was built in, which
    //  is what a factory function IS, and used to read whatever had since been
    //  written over that stack -- when it did not fault outright.
    TF_CASE("a handle outlives the frame that built it")
    {
        P3PmsgObject oHeld = HandleFromDeadFrame();
        ClobberFrame(40);

        P3PmsgField oField(oHeld);
        TF_CHECK(oField.r_name().c_wcsicmp(L"DEAD") == 0);
        TF_CHECK_EQ(oField.r_data().c_int(), 4242);
    }

    //  THE QUESTION §22 SAID COULD NOT BE ASKED.  A callee holding only a
    //  reference can tell the item it was handed from a duplicate of one.
    TF_CASE("a field says whether its item is inside it")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgField oHnd  = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        P3PmsgField oCopy = mgr.r_Desc().SelectItem(L"AAA");

        TF_CHECK(!oHnd.IsInline());        // a name for the tree's item
        TF_CHECK(oCopy.IsInline());        // an item of its own

        //  and the answer is what a write does.
        oHnd.r_data().c_int(111);
        TF_CHECK_EQ(mgr.r_Desc().SelectItem(L"AAA").r_data().c_int(), 111);
        oCopy.r_data().c_int(222);
        TF_CHECK_EQ(mgr.r_Desc().SelectItem(L"AAA").r_data().c_int(), 111);
        TF_CHECK(!(oCopy == oHnd));        // §21 says whose it is
    }

    //  §19's rule, kept: an item's identity changes when it GROWS out of the
    //  object, never when somebody asks after it.  AllocVBLock moves the block
    //  at the instant it makes the heap, which is the last instant nothing can
    //  be pointing at it.
    TF_CASE("growing an item moves it; asking after it does not")
    {
        P3PmsgField oF(L"CCC", P3PmsgData((int)1));
        TF_CHECK(oF.IsInline());
        const P2Pos posInline = oF.GetP2Pos();

        GrowFloater(oF, 7);
        TF_CHECK(!oF.IsInline());          // it has a heap, and it is on it
        TF_CHECK(oF.GetP2PmsgHandle() != 0);
        const P2Pos pos = oF.GetP2Pos();
        TF_CHECK(!(pos == posInline));     // growing moved it

        P3PmsgObject oSelf = oF.r_Object();
        TF_CHECK(oF.GetP2Pos() == pos);    // asking did not
        TF_CHECK(oSelf.GetP2Pos() == pos);
        TF_CHECK_EQ(oF.r_data().c_int(), 7);
    }

    //  A void field denotes no item, so the item is not inside it either.
    TF_CASE("void is not an item anywhere")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        P3PmsgObject oRoot = mgr.r_Object();
        P3PmsgField  oVoid(P3Pmsg_SelectObject(&oRoot, L"NoSuchItem"));

        TF_CHECK(oVoid.IsVoid());
        TF_CHECK(!oVoid.IsInline());
    }

    //  Sharing a small floater is what moves it out of the object, and both
    //  ends say so from then on.
    TF_CASE("sharing an item is what makes it shared")
    {
        P3PmsgField oFloat(L"EEE", P3PmsgData((int)5));
        TF_CHECK(oFloat.IsInline());

        P3PmsgField oShare = oFloat.r_Object();
        TF_CHECK(!oFloat.IsInline());
        TF_CHECK(!oShare.IsInline());
        TF_CHECK(oShare == oFloat);

        oShare.r_data().c_int(55);
        TF_CHECK_EQ(oFloat.r_data().c_int(), 55);

        //  A value copy of it is an item of its own, and taking one changes
        //  neither of them.
        P3PmsgField oVal = oFloat;
        TF_CHECK(oVal.IsInline());
        TF_CHECK(!oFloat.IsInline());
        oVal.r_data().c_int(99);
        TF_CHECK_EQ(oFloat.r_data().c_int(), 55);
    }

    //  P3PmsgList and P3PmsgVect derive from P3PmsgField, so they inherit it,
    //  and a message root is reached the same way an item is.
    TF_CASE("a collection handed in by reference says the same")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        TF_CHECK(!mgr.IsInline());                      // the message itself
        TF_CHECK(!mgr.r_Desc().r_Object().IsInline());  // its descendants

        P3PmsgList oList;
        TF_CHECK(oList.IsInline());
        oList.AddListTail(P3PmsgData((int)1));          // ... which gives it a heap
        TF_CHECK(!oList.IsInline());

        P3PmsgList oShare;
        oShare = oList.r_Object();
        TF_CHECK(!oShare.IsInline());
        TF_CHECK_EQ((int)oShare.GetCount(), 1);
    }
}
// ---------------------------------------------------------------------------
// P3PmsgObject : can anything else see a write through this?
// ---------------------------------------------------------------------------

//  Grow a field past its inline block, in place. Returning one by value would
//  copy it, and a copy is a fresh inline item -- the opposite of what these
//  need.
static void GrowPastInline ( P3PmsgField& oField, int nValue )
{
    CString sBig(L'x', 1024);
    oField.r_data() = P3PmsgData((LPCWSTR)(LPCTSTR)sBig);
    oField.r_data() = P3PmsgData((int)nValue);
}

static void Test_SoleStorage()
{
    //  THE ROW §24 NAMED. A duplicate that has grown is on a heap, so IsInline
    //  says false -- and it is on a heap OF ITS OWN, which nothing else holds,
    //  so a write through it reaches nobody. IsInline's false was not a
    //  guarantee of anything; this is.
    TF_CASE("a duplicate that has grown is still nobody else's")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgField oStore = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        P3PmsgField oCopy  = mgr.r_Desc().SelectItem(L"AAA");
        GrowPastInline(oCopy, 5);

        TF_CHECK(!oCopy.IsInline());       // the block did leave the object
        TF_CHECK(oCopy.IsSole());          // ... and took nobody with it

        //  Which is what a write does.
        oCopy.r_data().c_int(4321);
        TF_CHECK_EQ(oStore.r_data().c_int(), 1);
        TF_CHECK(!(oCopy == oStore));      // §21 says whose it is
    }

    //  The two questions, asked of the same object, answered differently --
    //  which is the whole point of there being two.
    TF_CASE("where the block is and whether anyone else holds it")
    {
        P3PmsgField oF(L"BBB", P3PmsgData((int)1));
        TF_CHECK(oF.IsInline());           // in the object
        TF_CHECK(oF.IsSole());             // so nowhere else

        GrowPastInline(oF, 2);
        TF_CHECK(!oF.IsInline());          // on a heap now
        TF_CHECK(oF.IsSole());             // ... its own, and only its own
    }

    //  Sole stops the instant a second name exists, and comes back when that
    //  name goes away -- the count is of holders, and holders come and go.
    TF_CASE("sharing ends sole, and letting go restores it")
    {
        P3PmsgField oF(L"CCC");
        GrowPastInline(oF, 3);
        TF_CHECK(oF.IsSole());
        {
            P3PmsgField oHnd = oF.r_Object();
            TF_CHECK(!oF.IsSole());
            TF_CHECK(!oHnd.IsSole());
            TF_CHECK(oHnd == oF);
            oHnd.r_data().c_int(33);
        }
        TF_CHECK(oF.IsSole());
        TF_CHECK_EQ(oF.r_data().c_int(), 33);   // and the write stayed
    }

    //  A small floater is sole for the other reason: the block never left.
    TF_CASE("an item still inside its object is sole by being inside it")
    {
        P3PmsgField oF(L"DDD", P3PmsgData((int)4));
        TF_CHECK(oF.IsInline());
        TF_CHECK(oF.IsSole());

        P3PmsgField oCopy = oF;            // a value copy, not a handle
        TF_CHECK(oCopy.IsSole());
        TF_CHECK(oF.IsSole());             // taking one changed neither

        P3PmsgField oShare = oF.r_Object();
        TF_CHECK(!oF.IsSole());            // ... but taking a NAME did
        TF_CHECK(!oShare.IsSole());
    }

    //  Anything in a message is held by the message.
    TF_CASE("nothing in a tree is sole")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        TF_CHECK(!mgr.IsSole());                       // the root
        TF_CHECK(!mgr.r_Desc().r_Object().IsSole());   // its descendants
        TF_CHECK(!mgr.r_Desc().SelectItem(L"AAA").r_Object().IsSole());

        //  ... and a write through a handle proves why.
        P3PmsgField oHnd = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        oHnd.r_data().c_int(111);
        TF_CHECK_EQ(mgr.r_Desc().SelectItem(L"AAA").r_data().c_int(), 111);
    }

    //  A collection inherits it, and grows a heap the same way an item does.
    TF_CASE("a collection is sole until it is named twice")
    {
        P3PmsgList oList;
        TF_CHECK(oList.IsSole());
        oList.AddListTail(P3PmsgData((int)1));   // ... which gives it a heap
        TF_CHECK(!oList.IsInline());
        TF_CHECK(oList.IsSole());

        P3PmsgList oShare;
        oShare = oList.r_Object();
        TF_CHECK(!oList.IsSole());
        TF_CHECK(!oShare.IsSole());
        oShare.AddListTail(P3PmsgData((int)2));
        TF_CHECK_EQ((int)oList.GetCount(), 2);
    }

    //  A void field denotes no storage, so it is not the sole holder of any.
    TF_CASE("void is not sole either")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        P3PmsgObject oRoot = mgr.r_Object();
        P3PmsgField  oVoid(P3Pmsg_SelectObject(&oRoot, L"NoSuchItem"));

        TF_CHECK(oVoid.IsVoid());
        TF_CHECK(!oVoid.IsSole());
        TF_CHECK(!oVoid.IsInline());
    }

    //  THE ROW §24 PINNED, ANSWERED. The count is of holders of the HEAP, so a
    //  field that had been asked for its descendants kept a sub-object that
    //  held it and said false from then on. A field can see its own
    //  sub-objects; P3PmsgField::IsSole subtracts them. §26.
    TF_CASE("its own descendants do not stop a field being sole")
    {
        P3PmsgField oF(L"EEE", P3PmsgData((int)7));
        GrowPastInline(oF, 7);
        TF_CHECK(oF.IsSole());

        oF.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"kid", P3PmsgData((int)1));
        TF_CHECK(oF.IsSole());             // a second holder, and it is MINE

        TF_CHECK_EQ(oF.r_data().c_int(), 7);
        TF_CHECK_EQ((int)oF.r_Desc().GetCount(), 1);
    }

    TF_CASE("nor do its own attributes, nor both together")
    {
        P3PmsgField oF(L"FFF", P3PmsgData((int)8));
        GrowPastInline(oF, 8);
        oF.r_Attr(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"tag", P3PmsgData((int)2));
        TF_CHECK(oF.IsSole());
        oF.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"kid", P3PmsgData((int)3));
        TF_CHECK(oF.IsSole());             // two of mine, and still mine alone
        TF_CHECK_EQ(oF.r_data().c_int(), 8);
    }

    //  SUBTRACTING MY OWN MUST NOT SUBTRACT A STRANGER'S, which is the risk the
    //  change carries: true is a guarantee, so counting one holder too many
    //  would be reporting that nobody is looking while somebody is.
    TF_CASE("a tree item with descendants of its own is still not sole")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"AAA", P3PmsgData((int)1));

        P3PmsgField oItem = mgr.r_Desc().SelectItem(L"AAA").r_Object();
        oItem.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"kid", P3PmsgData((int)1));
        TF_CHECK(!oItem.IsSole());         // the STORE holds this heap

        //  ... which a write proves, the way it always did.
        oItem.r_data().c_int(99);
        TF_CHECK_EQ(mgr.r_Desc().SelectItem(L"AAA").r_data().c_int(), 99);
    }

    TF_CASE("a shared floater with descendants of its own is still not sole")
    {
        P3PmsgField oF(L"GGG");
        GrowPastInline(oF, 9);
        oF.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"kid", P3PmsgData((int)1));
        TF_CHECK(oF.IsSole());

        P3PmsgField oShare = oF.r_Object();
        TF_CHECK(!oF.IsSole());            // a stranger, not one of mine
        TF_CHECK(!oShare.IsSole());
        oShare.r_data().c_int(77);
        TF_CHECK_EQ(oF.r_data().c_int(), 77);
    }

    //  THE ROW §26 LEFT OPEN, ANSWERED. A cursor lives one level further in
    //  than the sub-objects §26 subtracted, and a cursor holds the heap, so a
    //  collection that had been WALKED went back to answering false. It is
    //  still not handed out -- a cursor is stateful and shared, and a const
    //  question that repositioned one would be its own wrong answer. The
    //  collection is asked what it is holding instead. §29.
    TF_CASE("a cursor on its own descendants does not stop it being sole")
    {
        P3PmsgField oF(L"HHH");
        GrowPastInline(oF, 10);
        oF.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"k1", P3PmsgData((int)1));
        oF.r_Desc() += P3PmsgField(L"k2", P3PmsgData((int)2));
        TF_CHECK(oF.IsSole());

        oF.r_Desc().r_Curs();
        TF_CHECK(oF.IsSole());             // a third holder, and it is MINE

        TF_CHECK_EQ(oF.r_data().c_int(), 10);
        TF_CHECK_EQ((int)oF.r_Desc().GetCount(), 2);
    }

    //  ONE CURSOR CAN HOLD THE HEAP TWICE, which is why the subtraction is a
    //  COUNT and not one-per-cursor. Goto connects whichever of its three
    //  by-value members matches the item it landed on and leaves the other two
    //  as they were, so walking past a list and then a field leaves two of them
    //  connected at once. Counting per cursor would have undercounted here --
    //  safe, but only by luck.
    TF_CASE("a cursor that has walked two kinds of item holds it twice")
    {
        P3PmsgField oF(L"III");
        GrowPastInline(oF, 11);
        oF.r_Desc(P3PmsgField::AttrCMD_Create) += P3PmsgList(L"aList");
        oF.r_Desc() += P3PmsgField(L"aField", P3PmsgData((int)2));
        TF_CHECK(oF.IsSole());

        oF.r_Desc().r_Curs().Goto(0);      // the list
        TF_CHECK(oF.IsSole());
        oF.r_Desc().r_Curs().Goto(1);      // ... and now the field, as well
        TF_CHECK(oF.IsSole());

        TF_CHECK_EQ(oF.r_data().c_int(), 11);
        TF_CHECK_EQ((int)oF.r_Desc().GetCount(), 2);
    }

    TF_CASE("an attribute collection's cursor counts the same way")
    {
        P3PmsgField oF(L"JJJ");
        GrowPastInline(oF, 12);
        oF.r_Attr(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"tag", P3PmsgData((int)1));
        oF.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"kid", P3PmsgData((int)1));
        oF.r_Attr().r_Curs();
        oF.r_Desc().r_Curs();
        TF_CHECK(oF.IsSole());             // four of mine, and still mine alone
        TF_CHECK_EQ(oF.r_data().c_int(), 12);
    }

    //  §26 RECORDED THE MsgStck AS UNREACHABLE and measured a push as adding no
    //  holder at all. Both readings were right and neither was the whole of it:
    //  READING the snapshot makes a field on this heap, and the same walk that
    //  reaches a cursor reaches that too.
    TF_CASE("a stacked snapshot that has been read does not stop it either")
    {
        P3PmsgField oF(L"KKK");
        GrowPastInline(oF, 13);
        TF_CHECK(oF.IsSole());

        oF.r_Stck().Push();
        TF_CHECK(oF.IsSole());             // a push alone holds nothing

        oF.r_Stck().r_item();              // ... and reading it holds one
        TF_CHECK(oF.IsSole());
        TF_CHECK_EQ(oF.r_data().c_int(), 13);
    }

    //  AND STILL NOT A STRANGER'S. Subtracting one holder too many would report
    //  that nobody is looking while somebody is, so every arm of the walk is
    //  checked against a partner that can see the write.
    TF_CASE("a shared floater whose descendants were walked is still not sole")
    {
        P3PmsgField oF(L"LLL");
        GrowPastInline(oF, 14);
        oF.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"kid", P3PmsgData((int)1));
        oF.r_Desc().r_Curs();
        TF_CHECK(oF.IsSole());

        P3PmsgField oShare = oF.r_Object();
        TF_CHECK(!oF.IsSole());            // a stranger, not one of mine
        oShare.r_data().c_int(66);
        TF_CHECK_EQ(oF.r_data().c_int(), 66);
    }

    TF_CASE("and a cursor held by the STRANGER is still the stranger's")
    {
        P3PmsgField oF(L"MMM");
        GrowPastInline(oF, 15);
        oF.r_Desc(P3PmsgField::AttrCMD_Create)
            += P3PmsgField(L"kid", P3PmsgData((int)1));

        P3PmsgField oShare = oF.r_Object();
        oShare.r_Desc().r_Curs();          // the partner's cursor, not mine
        TF_CHECK(!oF.IsSole());
        oShare.r_data().c_int(55);
        TF_CHECK_EQ(oF.r_data().c_int(), 55);
    }
}


// ---------------------------------------------------------------------------
// P3PmsgList / P3PmsgVect : the cursors a collection caches
// ---------------------------------------------------------------------------
// §31's first entry, measured rather than assumed. §29 stopped the walk at
// P3PmsgList::m_pP3PmsgData[], P3PmsgVect::m_pP3PmsgData[] and
// P3PmsgVect::m_pP3PmsgType because none of the three had been shown BOTH to
// hold this heap and to be owned outright, and a holder subtracted that was
// never mine reports TRUE with a stranger looking. Both halves are now facts of
// the code: each of those pointers is newed at its own use site, deleted by the
// collection's destructor and by Truncate, and never assigned a pointer that
// came from anywhere else; and each wrapper is Connect()ed on the collection's
// OWN handle, which reaches P3PmsgObject::Connecta and AddRefs.
//
// A VECT'S m_pP3PmsgData[] HOLDS NOTHING, and not because it is somebody
// else's. Every site that would fill it is inside a comment block
// (MsgVect.cpp:190-207 and :237-249), so the array is zeroed at construction,
// deleted as nullptrs and never populated. There is no case for it below
// because there is no call that can reach one; a vect's element cursor is
// m_pP3PmsgType and that is what the vect cases here walk.
//
// §29's probe row is printed alongside each: refs= is what the heap says,
// mine= is what the walk says, and sole= is what IsSole made of the two. refs=
// is a static-mode column only -- P2PmsgHeap_RefCount is not exported, which
// is the same bound Test_ImageAddressBounds runs into.
static void SoleRow ( const char *pszWhat, const P3PmsgField& oField )
{
    const P2PmsgHANDLE hVBList = oField.GetP2PmsgHandle ( );
#ifdef Msgcore_STATIC
    printf ( "      %-44s refs=%d  mine=%d  sole=%s\n", pszWhat
           , P2PmsgHeap_RefCount ( hVBList )
           , oField.HeapHolders  ( hVBList )
           , oField.IsSole ( ) ? "true" : "false" );
#else
    printf ( "      %-44s mine=%d  sole=%s\n", pszWhat
           , oField.HeapHolders  ( hVBList )
           , oField.IsSole ( ) ? "true" : "false" );
#endif
}

static void Test_SoleCollectionCursors()
{
    //  A LIST'S DATA CURSORS ARE ITS OWN. GetNext, GetPrev and GetTail each
    //  advance m_nCurs and Connect the wrapper in that slot on this list's
    //  handle, so a walked list is holding its own heap once per slot used --
    //  three of them, round-robin. Before §31 the walk stopped above them and a
    //  list that had been read answered "somebody else is looking".
    TF_CASE("a list's own data cursors do not stop it being sole")
    {
        P3PmsgList oList;
        oList.AddListTail(P3PmsgData((int)1));   // ... which gives it a heap
        oList.AddListTail(P3PmsgData((int)2));
        TF_CHECK(!oList.IsInline());
        SoleRow("a list nobody has read", oList);
        TF_CHECK(oList.IsSole());

        VBLaddr aPos = oList.GetHeadPos();
        oList.GetNext(aPos);               // one cursor, and it is MINE
        SoleRow("... once one element has been read", oList);
        TF_CHECK(oList.IsSole());

        oList.GetTail();                   // ... and a second slot, as well
        SoleRow("... and the tail, in a second slot", oList);
        TF_CHECK(oList.IsSole());

        TF_CHECK_EQ((int)oList.GetCount(), 2);
        TF_CHECK_EQ(oList.GetTail().c_int(), 2);
    }

    //  IT IS A COUNT AND NOT A FLAG here too: MAX_P3PmsgData_Curs is three and
    //  a walk of three elements leaves three wrappers connected at once.
    TF_CASE("a list walked end to end holds its heap once per cursor slot")
    {
        P3PmsgList oList;
        for (int i = 1; i <= 3; i++)
            oList.AddListTail(P3PmsgData((int)i));

        VBLaddr aPos = oList.GetHeadPos();
        int nSum = 0;
        while (aPos)
            nSum += oList.GetNext(aPos).c_int();
        TF_CHECK_EQ(nSum, 6);

        SoleRow("a list walked end to end", oList);
        TF_CHECK(oList.IsSole());
    }

    //  A VECT'S ELEMENT CURSOR IS ITS OWN. Goto deletes the previous wrapper,
    //  news one of the element's own kind and Connects it, so InsertAt -- which
    //  ends in a Goto -- already leaves one holding this heap. Reading an
    //  element is the same one holder, repositioned.
    TF_CASE("a vect's own element cursor does not stop it being sole")
    {
        P3PmsgVect oVect(0, L"Elem", P3PmsgData((int)0));
        oVect.InsertAt(0, P3PmsgField(L"e", P3PmsgData((int)10)));
        oVect.InsertAt(1, P3PmsgField(L"e", P3PmsgData((int)20)));
        SoleRow("a vect with two elements", oVect);
        TF_CHECK(oVect.IsSole());

        TF_CHECK(oVect.r_data(1).c_int() == 20);
        SoleRow("... once an element has been read", oVect);
        TF_CHECK(oVect.IsSole());

        TF_CHECK_EQ((int)oVect.GetCount(), 2);
    }

    //  AND IT IS THE ELEMENT'S OWN KIND, which is why HeapHolders had to become
    //  virtual. m_pP3PmsgType is declared P3PmsgField* and Goto news a
    //  P3PmsgList or a P3PmsgVect into it when the element is one; a
    //  non-virtual call would count the wrapper's own object and stop, missing
    //  everything the nested collection is holding one level further in.
    TF_CASE("a nested collection's cursors are reached through the element cursor")
    {
        P3PmsgVect oInner(2, L"Inner", P3PmsgData((int)0));
        oInner.r_data(0).c_int(7);
        oInner.r_data(1).c_int(8);

        P3PmsgVect oOuter(0, L"Outer", P3PmsgData((int)0));
        oOuter.InsertAt(0, P3PmsgField(L"scalar", P3PmsgData((int)1)));
        oOuter.InsertAt(1, oInner);        // element 1 is itself a vect
        SoleRow("a vect holding a vect", oOuter);
        TF_CHECK(oOuter.IsSole());

        //  Two wrappers on this heap now: the outer's, which IS a P3PmsgVect,
        //  and the one that vect made for its own element.
        TF_CHECK(oOuter.r_vect(1).r_data(1).c_int() == 8);
        SoleRow("... and the inner element read through it", oOuter);
        TF_CHECK(oOuter.IsSole());

        TF_CHECK_EQ((int)oOuter.GetCount(), 2);
    }

    //  A LIST INSIDE A VECT reaches the other override the same way: the
    //  element cursor is a P3PmsgList and its data cursors are a level below
    //  that again.
    TF_CASE("a list element's data cursor is reached through the element cursor")
    {
        P3PmsgList oInner;
        oInner.AddListTail(P3PmsgData((int)5));
        oInner.AddListTail(P3PmsgData((int)6));

        P3PmsgVect oOuter(0, L"Outer", P3PmsgData((int)0));
        oOuter.InsertAt(0, oInner);
        TF_CHECK(oOuter.IsList(0));
        SoleRow("a vect holding a list", oOuter);
        TF_CHECK(oOuter.IsSole());

        P3PmsgList& oHeld = oOuter.r_list(0);
        VBLaddr     aPos  = oHeld.GetHeadPos();
        TF_CHECK_EQ(oHeld.GetNext(aPos).c_int(), 5);
        SoleRow("... with that list read through it", oOuter);
        TF_CHECK(oOuter.IsSole());
    }

    //  THE ROWS THAT MUST NOT MOVE. Subtracting one holder too many reports a
    //  guarantee that is not true, so every arm added here is checked against a
    //  partner that can see the write.
    TF_CASE("a shared list whose cursors were used is still not sole")
    {
        P3PmsgList oList;
        oList.AddListTail(P3PmsgData((int)1));
        oList.AddListTail(P3PmsgData((int)2));

        P3PmsgList oShare;
        oShare = oList.r_Object();
        VBLaddr aPos = oList.GetHeadPos();
        oList.GetNext(aPos);               // my cursor does not buy me the heap
        oList.GetTail();
        SoleRow("a shared list, walked by me", oList);
        TF_CHECK(!oList.IsSole());
        TF_CHECK(!oShare.IsSole());

        oShare.AddListTail(P3PmsgData((int)3));
        TF_CHECK_EQ((int)oList.GetCount(), 3);
    }

    TF_CASE("and a vect the STRANGER is walking is still the stranger's")
    {
        P3PmsgVect oVect(0, L"Elem", P3PmsgData((int)0));
        oVect.InsertAt(0, P3PmsgField(L"e", P3PmsgData((int)10)));
        TF_CHECK(oVect.IsSole());

        P3PmsgVect oShare(0, L"Other", P3PmsgData((int)0));
        oShare = oVect.r_Object();
        TF_CHECK(oShare.r_data(0).c_int() == 10);   // the partner's cursor
        SoleRow("a shared vect, walked by the partner", oVect);
        TF_CHECK(!oVect.IsSole());
        TF_CHECK(!oShare.IsSole());

        oShare.r_data(0).c_int(44);
        TF_CHECK(oVect.r_data(0).c_int() == 44);
    }
}


// ---------------------------------------------------------------------------
// P3PmsgObject : a chain longer than one link
// ---------------------------------------------------------------------------
// §27 measured every in-process path as producing exactly ONE link, and that
// measurement stands: P2PmsgObject_NewVBLockData and P3PmsgName_ResizeName both
// REPLACE the chained block rather than appending to it, and a payload past
// 65535 bytes is refused rather than split. The walks in PrivatiseInlineChain,
// ReleaseInlineChain, NewVBLockData and every reader loop anyway, because an
// image can carry a longer chain -- and until §29 not one of them had ever been
// put to one.
//
// THE IMAGE IS THE ARENA. P2PmsgMgr::Save writes the heap whole with a single
// WriteFile and Load reads it back whole; a chain pointer is a plain offset into
// it, relocated by nothing and validated for length by nothing. So the state an
// image delivers is exactly the state built below with the heap's own
// allocator, and the last case saves and reloads one to show the round trip
// carries it.
//
// STATIC LINK ONLY, for the reason Test_ValueCopiesItsPayload gives: block
// navigation and the heap allocator are declared in the library's headers but
// not marked Msgcore_EXT. §29.
// ---------------------------------------------------------------------------
#ifdef Msgcore_STATIC

//  Blocks the value's chain runs through, from the head. 0 = nothing chained.
static int ChainLen29(const P3PmsgObject& oObject)
{
    VBLockData *pData = P2PmsgObject_pData(oObject, false);
    int         nLen  = 0;
    while (VBLockData_IsChained(pData))
    {
        const VBLaddr aNext = VBLockData_GetChain2Next(oObject.m_uVBLock, pData);
        pData = VBLock_pData((VBLock *)oObject.Msg2Phys(aNext));
        nLen++;
    }
    return nLen;
}

static VBLaddr ChainTail29(const P3PmsgObject& oObject)
{
    VBLockData *pData = P2PmsgObject_pData(oObject, false);
    VBLaddr     aTail = 0;
    while (VBLockData_IsChained(pData))
    {
        aTail = VBLockData_GetChain2Next(oObject.m_uVBLock, pData);
        pData = VBLock_pData((VBLock *)oObject.Msg2Phys(aTail));
    }
    return aTail;
}

//  Appends one link, exactly as an image would carry one: duplicate the block
//  holding the payload, then make the old one a pure link to the duplicate.
//  P2PmsgObject_CopyHeapVBLock's arithmetic -- ask Alloc for the declared size
//  LESS the header, because Alloc adds the header back.
static bool Lengthen29(P3PmsgObject& oObject)
{
    const VBLaddr aTail = ChainTail29(oObject);
    if (aTail == 0)
        return false;

    const P2PmsgHANDLE hVBList = oObject.m_hVBList;
    VBLock       *pTail = (VBLock *)oObject.Msg2Phys(aTail);
    const VBLsize nSize = VBLock_Hdr_u_SizeNN(pTail);
    const UCHAR   uType = pTail->oHdr.uVBLockDefs & VBLock_TypeMask;
    const VBLsize nHdr  = P2PmsgHeap_Sizeof_Hdr(hVBList);

    const VBLaddr aNew  = P2PmsgHeap_Alloc(hVBList, uType, nSize - nHdr);
    memcpy(P2PmsgHeap_Addr2Phys(hVBList, aNew),
           P2PmsgHeap_Addr2Phys(hVBList, aTail), nSize);

    pTail = (VBLock *)oObject.Msg2Phys(aTail);   // Alloc invalidates pointers
    VBLockData_SetChain2Next(oObject.m_uVBLock, VBLock_pData(pTail), aNew);
    return true;
}

#endif // Msgcore_STATIC

static void Test_ChainLongerThanOne()
{
#ifdef Msgcore_STATIC
    TF_CASE("a value chains one link, and a second can be appended to it")
    {
        CString sBig(L'x', 1024);
        sBig.SetAt(0, L'A');
        P3PmsgData    oSrc((LPCWSTR)(LPCTSTR)sBig);
        P3PmsgObject& oObj = const_cast<P3PmsgObject&>(*oSrc.p_Object());

        TF_CHECK_EQ(ChainLen29(oObj), 1);  // what every in-process path gives
        TF_CHECK(Lengthen29(oObj));
        TF_CHECK_EQ(ChainLen29(oObj), 2);
        TF_CHECK(Lengthen29(oObj));
        TF_CHECK_EQ(ChainLen29(oObj), 3);

        //  The readers walk to the end, and the end is where it always was.
        TF_CHECK(oSrc.c_wstr()[0] == L'A');
        TF_CHECK_EQ((int)oSrc.c_size(), 2048);
    }

    //  THE ONE THING A LONGER CHAIN BROKE. VerifyContainment sizes every link
    //  it steps to, and VBLockData_Sizeof_uv had no arm for the chained type
    //  byte -- it fell through all of them to ASSERT(0). A one-link chain never
    //  reached it, because the only block anyone sizes is the payload at the
    //  end. The assert trap in TestFramework folds an ASSERT into a failure of
    //  the case, so this case IS the check. §29.
    TF_CASE("every link of a long chain can be sized and contained")
    {
        CString sBig(L'x', 1024);
        P3PmsgData    oSrc((LPCWSTR)(LPCTSTR)sBig);
        P3PmsgObject& oObj = const_cast<P3PmsgObject&>(*oSrc.p_Object());
        Lengthen29(oObj);
        Lengthen29(oObj);
        TF_CHECK_EQ(ChainLen29(oObj), 3);

        TF_CHECK(oSrc.VerifyContainment() ? true : false);
        oSrc.AssertValid();                // reaches VerifyContainment again
        TF_CHECK_EQ((int)oSrc.c_size(), 2048);
    }

    //  PrivatiseInlineChain, put to three links. §23 is the defect it fixes and
    //  its loop was written for exactly this, never having met it.
    TF_CASE("a copy of a three-link value owns every link of its own")
    {
        CString sBig(L'x', 1024);
        sBig.SetAt(0, L'A');
        P3PmsgData    oSrc((LPCWSTR)(LPCTSTR)sBig);
        P3PmsgObject& oObj = const_cast<P3PmsgObject&>(*oSrc.p_Object());
        Lengthen29(oObj);
        Lengthen29(oObj);

        P3PmsgObject oCopy(oObj);
        TF_CHECK_EQ(ChainLen29(oCopy), 3);

        //  Every link, not just the first -- which is the whole difference
        //  between a value copy and an alias.
        bool bAllPrivate = true;
        VBLockData *pA = P2PmsgObject_pData(oObj,  false);
        VBLockData *pB = P2PmsgObject_pData(oCopy, false);
        while (VBLockData_IsChained(pA) && VBLockData_IsChained(pB))
        {
            const VBLaddr aA = VBLockData_GetChain2Next(oObj .m_uVBLock, pA);
            const VBLaddr aB = VBLockData_GetChain2Next(oCopy.m_uVBLock, pB);
            if (aA == aB) bAllPrivate = false;
            pA = VBLock_pData((VBLock *)oObj .Msg2Phys(aA));
            pB = VBLock_pData((VBLock *)oCopy.Msg2Phys(aB));
        }
        TF_CHECK(bAllPrivate);

        //  Same payload, compared whole -- header, flags, bytes. Spelled out
        //  rather than borrowed: SamePayloadBytes is declared further down,
        //  with the §23 cases it was written for.
        VBLock       *pTA = (VBLock *)oObj .Msg2Phys(ChainTail29(oObj));
        VBLock       *pTB = (VBLock *)oCopy.Msg2Phys(ChainTail29(oCopy));
        const VBLsize nTA = VBLock_Hdr_u_SizeNN(pTA);
        const VBLsize nTB = VBLock_Hdr_u_SizeNN(pTB);
        TF_CHECK(nTA == nTB && memcmp(pTA, pTB, (size_t)nTA) == 0);
    }

    //  ReleaseInlineChain, put to three links. §27's method: freed storage is
    //  reused, so a tail address that stays put is proof every link came back.
    TF_CASE("and gives every link back when it goes")
    {
        CString sBig(L'x', 1024);
        P3PmsgData    oSrc((LPCWSTR)(LPCTSTR)sBig);
        P3PmsgObject& oObj = const_cast<P3PmsgObject&>(*oSrc.p_Object());
        Lengthen29(oObj);
        Lengthen29(oObj);

        //  EXHAUSTION, NOT ADDRESSES, and the difference matters here. §27
        //  watched the block address, because freed storage is reused and a
        //  climbing address is proof nothing came back. That works in a quiet
        //  process and not in this one: a floating value sits on a SYSTEM heap,
        //  whose VBLaddr is a machine pointer, so whether the next copy lands
        //  where the last one stood is a fact about the CRT allocator after two
        //  hundred other cases and not about this release. Measured both ways
        //  -- stable in a standalone probe, unstable here.
        //
        //  What does not depend on the allocator is the ceiling. Three links of
        //  a 2048-byte value is about 6KB an iteration, so a release that
        //  missed even one of them exhausts the heap's 10,000,000 bytes inside
        //  two thousand copies and the allocator refuses. Five thousand is the
        //  margin.
        bool bEveryCopyStood = true;
        try
        {
            for (int i = 0; i < 5000; i++)
            {
                P3PmsgObject oCopy(oObj);
                if (oCopy.IsVoid() || ChainLen29(oCopy) != 3)
                    bEveryCopyStood = false;
            }
        }
        catch (P2Pevent* pEVT)
        {
            //  Caught rather than left to escape: an exhausted heap throws, and
            //  a throw out of here stops the whole run at this case instead of
            //  reporting it. §27's loop case is the one that did that.
            if (pEVT) pEVT->Cancel(false);
            bEveryCopyStood = false;
        }
        TF_CHECK(bEveryCopyStood);
        TF_CHECK_EQ(ChainLen29(oObj), 3);  // and the source is untouched
        TF_CHECK_EQ((int)oSrc.c_size(), 2048);
    }

    //  NewVBLockData's collapse loop, put to three links.
    TF_CASE("a retype collapses a three-link chain back to one")
    {
        CString sBig(L'x', 1024);
        CString sBigger(L'y', 4096);
        sBigger.SetAt(0, L'B');

        P3PmsgData    oSrc((LPCWSTR)(LPCTSTR)sBig);
        P3PmsgObject& oObj = const_cast<P3PmsgObject&>(*oSrc.p_Object());
        Lengthen29(oObj);
        Lengthen29(oObj);
        TF_CHECK_EQ(ChainLen29(oObj), 3);

        oSrc = P3PmsgData((LPCWSTR)(LPCTSTR)sBigger);
        TF_CHECK_EQ(ChainLen29(oObj), 1);
        TF_CHECK(oSrc.c_wstr()[0] == L'B');
        TF_CHECK_EQ((int)oSrc.c_size(), 8192);
    }

    //  AND THROUGH AN IMAGE, which is where a long chain would actually come
    //  from. Nothing in Save or Load walks a chain or bounds its length.
    TF_CASE("a saved image carries a three-link chain and reads back")
    {
        wchar_t szDir[MAX_PATH]  = { 0 };
        wchar_t szPath[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, szDir);
        swprintf_s(szPath, MAX_PATH, L"%smscs_chain29.p2p", szDir);

        CString sBig(L'x', 1024);
        sBig.SetAt(0, L'A');
        {
            P2PmsgMgr oMgr;
            oMgr.r_name() = L"Store";
            oMgr.r_Desc(P3PmsgField::AttrCMD_Create)
                += P3PmsgField(L"EEE", P3PmsgData((int)1));
            P3PmsgField& oF = oMgr.r_Desc().SelectItem(L"EEE");
            oF.r_data() = P3PmsgData((LPCWSTR)(LPCTSTR)sBig);

            P3PmsgObject& oObj = const_cast<P3PmsgObject&>(oF.r_Object());
            TF_CHECK_EQ(ChainLen29(oObj), 1);
            Lengthen29(oObj);
            Lengthen29(oObj);
            TF_CHECK_EQ(ChainLen29(oObj), 3);
            TF_CHECK(oMgr.Save(szPath) ? true : false);
        }
        {
            P2PmsgMgr oMgr;
            TF_CHECK(oMgr.Load(szPath) ? true : false);
            P3PmsgField& oF = oMgr.r_Desc().SelectItem(L"EEE");
            TF_CHECK_EQ(ChainLen29(oF.r_Object()), 3);
            TF_CHECK(oF.r_data().c_wstr()[0] == L'A');
        }
        _wremove(szPath);
    }
#else
    //  Visible rather than silent, per SuiteMain.cpp's rule that anything
    //  compiled out must say so. Not a skipped SUITE: block navigation and the
    //  heap allocator do not cross the DLL boundary at all.
    TF_CASE("a chain longer than one link -- static link only, heap API is not exported")
    {
        TF_CHECK(true);
    }
#endif
}

// ---------------------------------------------------------------------------
// P3PmsgObject : a value copy gives its payload back
// ---------------------------------------------------------------------------
// §23 gave a value copy its own copy of the chained payload block, allocated on
// the heap the two objects SHARE -- which is right, the AddRef having settled
// the heap's lifetime. But a shared heap does not go away when the copy does,
// and ~P3PmsgObject closed the heap and freed nothing, so the block stayed
// allocated on a heap that was still open. A loop of copies over a source that
// outlives them allocated once an iteration and gave nothing back: measured at
// 4841 copies before the heap refused at its ceiling of 10000000 bytes.
//
// The chain is one link long and cannot be longer from this side: both
// P2PmsgObject_NewVBLockData and P3PmsgName_ResizeName REPLACE the chained
// block rather than appending to it. §26.
// ---------------------------------------------------------------------------
static void Test_ValueCopyReleasesItsPayload()
{
    //  The one that used to run out of heap. Well past 4841, and the source
    //  outlives every copy, which is what makes the block unreclaimable if
    //  nothing gives it back.
    TF_CASE("copying a grown value in a loop does not exhaust the heap")
    {
        CString sBig(L'z', 1024);
        P3PmsgData oGrown((LPCWSTR)(LPCTSTR)sBig);

        //  Counted rather than checked: ten thousand TF_CHECKs would say the
        //  same thing ten thousand times and drown the run.
        bool bEveryCopyStood = true;
        for ( int i = 0; i < 10000; i++ )
        {
            P3PmsgObject oCopy(*oGrown.p_Object());
            if ( oCopy.IsVoid() )
                bEveryCopyStood = false;
        }
        TF_CHECK(bEveryCopyStood);
        TF_CHECK_EQ((int)wcslen(oGrown.c_wstr()), 1024);   // and it is intact
    }

    //  Assignment takes the same arm and has the same duty.
    TF_CASE("assigning a grown value in a loop does not exhaust the heap")
    {
        CString sBig(L'y', 1024);
        P3PmsgData oGrown((LPCWSTR)(LPCTSTR)sBig);

        P3PmsgObject oTarget;
        for ( int i = 0; i < 10000; i++ )
            oTarget.Connect(*oGrown.p_Object());
        TF_CHECK(!oTarget.IsVoid());
        TF_CHECK_EQ((int)wcslen(oGrown.c_wstr()), 1024);
    }

    //  A value that fits in its block has no payload to give back, and the
    //  release must not go looking for one.
    TF_CASE("a value that fits in its block is unaffected")
    {
        P3PmsgData oSmall((int)42);
        bool bEveryCopyStood = true;
        for ( int i = 0; i < 1000; i++ )
        {
            P3PmsgObject oCopy(*oSmall.p_Object());
            if ( oCopy.IsVoid() )
                bEveryCopyStood = false;
        }
        TF_CHECK(bEveryCopyStood);
        TF_CHECK_EQ(oSmall.c_int(), 42);
    }

    //  And the copy still READS what it copied, which is §23's guarantee and
    //  the thing a release that freed too much would break.
    TF_CASE("a released copy still gave the right answer while it lived")
    {
        CString sBig(L'w', 600);
        P3PmsgData oGrown((LPCWSTR)(LPCTSTR)sBig);
        bool bEveryCopyRead = true;
        for ( int i = 0; i < 100; i++ )
        {
            P3PmsgData oCopy(oGrown);
            if ( wcscmp(oCopy.c_wstr(), oGrown.c_wstr()) != 0 )
                bEveryCopyRead = false;
        }
        TF_CHECK(bEveryCopyRead);
        TF_CHECK_EQ((int)wcslen(oGrown.c_wstr()), 600);
    }
}


// ---------------------------------------------------------------------------
// P3PmsgObject : a value copy copies the VALUE, not the address of one
// ---------------------------------------------------------------------------

//  WHERE A VALUE KEEPS ITS PAYLOAD IS A FACT ABOUT THE BLOCK, and block
//  navigation is internal: P2PmsgObject_pData, VBLockData_IsChained and
//  VBLock_Hdr_u_SizeNN are declared in the library's headers but not marked
//  Msgcore_EXT, so they link in the static build and not through the import
//  library. The choice was to export three functions for a test or to ask the
//  physical question in one of the two configurations; the behavioural cases
//  below the #endif run in both. §23.
#ifdef Msgcore_STATIC

//  While the whole value fits in its block the answer is "in the block" and
//  this returns 0. Once it does not, the payload is a SECOND block on the heap
//  and the first only chains to it.
static VBLaddr ChainOfValue ( const P3PmsgObject& oObject )
{
    VBLockData *pData = P2PmsgObject_pData ( oObject, false );
    return VBLockData_IsChained ( pData )
             ? VBLockData_GetChain2Next ( oObject.m_uVBLock, pData ) : 0;
}

//  Two payload blocks, compared whole -- header, flags, bytes.
static bool SamePayloadBytes ( const P3PmsgObject& oA, VBLaddr aA,
                               const P3PmsgObject& oB, VBLaddr aB )
{
    VBLock       *pA = (VBLock *)oA.Msg2Phys ( aA );
    VBLock       *pB = (VBLock *)oB.Msg2Phys ( aB );
    const VBLsize nA = VBLock_Hdr_u_SizeNN ( pA );
    const VBLsize nB = VBLock_Hdr_u_SizeNN ( pB );
    return nA == nB && memcmp ( pA, pB, (size_t)nA ) == 0;
}

#endif // Msgcore_STATIC

static void Test_ValueCopiesItsPayload()
{
#ifdef Msgcore_STATIC
    //  §23 recorded this, and §22 is why it is only now the question. Until
    //  §22 the copy's BLOCK was the source's block, so what the block chained
    //  to hardly mattered. The copy has a block of its own now. What it
    //  chained to was still the source's.
    TF_CASE("a grown value copies its payload, not the address of one")
    {
        CString sBig(L'x', 1024);
        sBig.SetAt(0, L'A');

        P3PmsgData   oSrc((LPCWSTR)(LPCTSTR)sBig);
        P3PmsgObject oCopy(*oSrc.p_Object());

        const VBLaddr aSrc  = ChainOfValue(*oSrc.p_Object());
        const VBLaddr aCopy = ChainOfValue(oCopy);

        TF_CHECK(aSrc != 0);               // it did outgrow the block
        TF_CHECK(aCopy != 0);
        TF_CHECK(aCopy != aSrc);           // ... and the copy owns its own
        TF_CHECK(SamePayloadBytes(*oSrc.p_Object(), aSrc, oCopy, aCopy));

        //  Unshared is only half of it. A copy is worthless unless it is also
        //  the same value, and the source has to be untouched by having been
        //  copied at all.
        TF_CHECK(oSrc.c_wstr()[0] == L'A');
        TF_CHECK_EQ((int)oSrc.c_size(), 2048);
    }

    //  Assignment is Nullify + Connect, and carries its own copy of the arm.
    TF_CASE("assignment gives the copy its own payload too")
    {
        CString sBig(L'x', 1024);
        P3PmsgData   oSrc((LPCWSTR)(LPCTSTR)sBig);
        P3PmsgObject oCopy;
        oCopy = *oSrc.p_Object();

        const VBLaddr aSrc  = ChainOfValue(*oSrc.p_Object());
        const VBLaddr aCopy = ChainOfValue(oCopy);
        TF_CHECK(aSrc != 0 && aCopy != 0 && aCopy != aSrc);
        TF_CHECK(SamePayloadBytes(*oSrc.p_Object(), aSrc, oCopy, aCopy));
    }

    //  A value that has been retyped and regrown chains once and not twice --
    //  NewVBLockData collapses what it finds before it adds a link -- but the
    //  readers walk the chain, so the copy walks it.
    TF_CASE("a value grown twice copies what it ended up with")
    {
        CString sBig(L'x', 1024);
        CString sBigger(L'y', 4096);
        sBigger.SetAt(0, L'B');

        P3PmsgData oSrc((LPCWSTR)(LPCTSTR)sBig);
        oSrc = P3PmsgData((LPCWSTR)(LPCTSTR)sBigger);

        P3PmsgObject  oCopy(*oSrc.p_Object());
        const VBLaddr aSrc  = ChainOfValue(*oSrc.p_Object());
        const VBLaddr aCopy = ChainOfValue(oCopy);

        TF_CHECK(aSrc != 0 && aCopy != 0 && aCopy != aSrc);
        TF_CHECK(SamePayloadBytes(*oSrc.p_Object(), aSrc, oCopy, aCopy));
        TF_CHECK(oSrc.c_wstr()[0] == L'B');
        TF_CHECK_EQ((int)oSrc.c_size(), 8192);
    }

    //  The copy has to survive the source, which is the point of a value copy
    //  and was the sharper half of the defect: the first of the two to retype
    //  FREES the payload block, and the other was still chained to it.
    TF_CASE("the copy keeps its payload after the source is gone")
    {
        CString      sBig(L'x', 1024);
        P3PmsgObject oCopy;
        VBLaddr      aCopy = 0;
        {
            P3PmsgData oSrc((LPCWSTR)(LPCTSTR)sBig);
            oCopy = *oSrc.p_Object();
            aCopy = ChainOfValue(oCopy);
            TF_CHECK(aCopy != 0);
            TF_CHECK(aCopy != ChainOfValue(*oSrc.p_Object()));
        }
        TF_CHECK(ChainOfValue(oCopy) == aCopy);
        //  0xFF is the chaining flag; a block that still holds a payload is
        //  not wearing it.
        TF_CHECK(P2PmsgObject_pData(oCopy, true)->uDataType != 0xFF);
    }

    //  Nothing is copied that was not there: a value still inside its block
    //  has no second block to duplicate.
    TF_CASE("a value that fits in its block has no payload to unshare")
    {
        P3PmsgData   oSrc((int)7);
        P3PmsgObject oCopy(*oSrc.p_Object());

        TF_CHECK(ChainOfValue(*oSrc.p_Object()) == 0);
        TF_CHECK(ChainOfValue(oCopy) == 0);
    }
#endif // Msgcore_STATIC

    //  THE HEAP IS SHARED AND IS MEANT TO BE. It is AddRef'd and outlives
    //  either object on its own, which is what makes allocating the copy's
    //  payload on it the right place to put it. The BLOCK is what the two of
    //  them must not share.
    TF_CASE("a copied value shares the heap and not the block")
    {
        CString sBig(L'x', 1024);
        P3PmsgData   oSrc((LPCWSTR)(LPCTSTR)sBig);
        P3PmsgObject oCopy(*oSrc.p_Object());

        TF_CHECK(oSrc.p_Object()->m_hVBList != 0);
        TF_CHECK(oCopy.m_hVBList == oSrc.p_Object()->m_hVBList);
        TF_CHECK(oCopy.IsInline());              // the block itself is ours
        TF_CHECK(oSrc.p_Object()->IsInline());   // ... and so is the source's
        TF_CHECK(oSrc.c_wstr()[0] == L'x');
    }

    //  A small value never grows a heap at all, so there is nothing to share
    //  and nothing to duplicate.
    TF_CASE("a value that fits in its block grows no heap to share")
    {
        P3PmsgData   oSrc((int)7);
        P3PmsgObject oCopy(*oSrc.p_Object());

        TF_CHECK(oSrc.p_Object()->m_hVBList == 0);
        TF_CHECK(oCopy.m_hVBList == 0);
        TF_CHECK(oCopy.IsInline());
        TF_CHECK_EQ(oSrc.c_int(), 7);
    }
}

// ---------------------------------------------------------------------------
// P3Pmsg_SplitRootPath : a bare '@' is a component wherever it stands
// ---------------------------------------------------------------------------
static void Test_BareAttrComponent()
{
    //  '^' and '@' are the two delimiters that carry no name of their own, and
    //  each names an object with no other spelling -- the pushed value, and the
    //  attribute COLLECTION. §12 let a bare '@' through, but by the clause
    //  AFTER the splitter's loop, not the test inside it: so it was a component
    //  only at the END of a path. ".Root.Item@" resolved; ".Root.Item@.Tag" was
    //  malformed -- while ".Root.Item@^.Tag", one step longer and through the
    //  collection as it stood at the last push, resolved (§13). The two clauses
    //  now ask the same question.
    TF_CASE("a bare '@' splits as a component part way along a path")
    {
        CString        strRoot;
        CList<CString> oItems;

        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Item@.Tag", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 3);
        if (oItems.GetCount() == 3)
        {
            POSITION pos = oItems.GetHeadPosition();
            TF_CHECK(oItems.GetNext(pos) == L".Item");
            TF_CHECK(oItems.GetNext(pos) == L"@");
            TF_CHECK(oItems.GetNext(pos) == L".Tag");
        }

        //  At the end it still splits the way §12 made it.
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Item@", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 2);

        //  A lone DESCENDANT delimiter was an empty name and stayed malformed
        //  until §15 finished the rule; both of these are components now.
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Item..Tag", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 3);
        TF_CHECK(P3Pmsg_SplitRootPath(L".Root.Item.@Tag", strRoot, oItems));
        TF_CHECK(oItems.GetCount() == 3);
    }

    //  And it answers the same object the short spelling does. "@Tag" carries
    //  its name after the delimiter and has always been one component; "@.Tag"
    //  is the collection and then a name looked up inside it, which is two --
    //  the same two steps "@^.Tag" takes through the snapshot's collection.
    TF_CASE("'@.Tag' and '@Tag' answer the same attribute")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;
        mgr.r_Desc() += P3PmsgField(L"RIO");            // no attributes at all

        P3PmsgObject oShort = mgr.RootPath2Object(L".Store.BHP@Currency");
        P3PmsgObject oLong  = mgr.RootPath2Object(L".Store.BHP@.Currency");
        TF_CHECK(!oShort.IsVoid());
        TF_CHECK(!oLong.IsVoid());
        if (!oShort.IsVoid() && !oLong.IsVoid())
            TF_CHECK(oShort.GetP2Pos() == oLong.GetP2Pos());

        //  A trailing '.' after the collection is a component of its own
        //  since §15, and it asks the attribute collection for a DESCENDANT
        //  collection -- which a collection has not got, because it is not an
        //  item. An ordinary miss, and no assertion.
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@.").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@").IsAttr());

        //  The root has a collection of its own, and this is the only root-path
        //  spelling that goes through it.
        mgr.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Venue");
        P3PmsgObject oRootAttr = mgr.RootPath2Object(L".Store@.Venue");
        TF_CHECK(!oRootAttr.IsVoid());
        P3PmsgObject oRootShort = mgr.RootPath2Object(L".Store@Venue");
        if (!oRootAttr.IsVoid() && !oRootShort.IsVoid())
            TF_CHECK(oRootAttr.GetP2Pos() == oRootShort.GetP2Pos());

        //  The miss rules are the component's own delimiter's, as ever: a '.'
        //  component that finds nothing throws, an '@' one answers void. Here
        //  it is the '@' that misses -- RIO has no collection at all.
        bool bThrewName = false;
        try   { mgr.RootPath2Object(L".Store.BHP@.Nobody"); }
        catch (P2Pevent* pEVT) { bThrewName = true; pEVT->Cancel(false); }
        TF_CHECK(bThrewName);

        TF_CHECK(mgr.RootPath2Object(L".Store.RIO@.Currency").IsVoid());
    }

    //  A COLLECTION HAS NO ATTRIBUTES OF ITS OWN -- it is not an item. So
    //  "Item@@Tag" names nothing. It is still a well-formed question, and the
    //  answer to one of those is the empty object: the selector's attribute arm
    //  used to ASSERT(0) first, which is a debug-build event for something only
    //  a caller could have spelled.
    //
    //  Not a new path. "Item@^@Tag" splits cleanly without any of this -- "@^"
    //  is the collection at the last push (§9) -- and asserted on the way to
    //  the same void. Legalising the shorter spelling is what made it worth
    //  fixing rather than noting.
    TF_CASE("asking a collection for an attribute is a miss, not an assertion")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();

        //  Each of these runs the arm; a raised assertion is a failed test.
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@@Currency").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@@").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@^@Currency").IsVoid());

        //  The spellings that DO name something still do.
        TF_CHECK(!mgr.RootPath2Object(L".Store.BHP@Currency").IsVoid());
        TF_CHECK(!mgr.RootPath2Object(L".Store.BHP@^.Currency").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@").IsAttr());
    }
}

// ---------------------------------------------------------------------------
// P2PmsgMgr::RootPath2Object : a non-field reached part way along the walk
// ---------------------------------------------------------------------------
static void Test_RootPathContainers()
{
    //  The walk held a P3PmsgItem, so the assignment that carries it forward
    //  was P3PmsgField::operator=(const P3PmsgObject&) -- which throws
    //  "Invalid overloaded context" for anything that is not a field. A list
    //  or a vector is an item but not a field (VBLockItem_IsField is false
    //  for both), so a container anywhere BUT the last position ended the
    //  path with a raised event.
    //
    //  The object-path spellings of the same questions have answered since §6
    //  taught the selector about containers; it was only the root-path walk
    //  that could not step off one. Every step of it is P3Pmsg_SelectObject,
    //  a free function over P3PmsgObject with an arm for each kind there is,
    //  so nothing about the walk needed a field -- only the variable did.
    TF_CASE("a root path walks over a list and keeps going")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)1));
        oList.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Unit");
        oList.r_Desc(P3PmsgField::AttrCMD_Create);
        oList.r_Desc() += P3PmsgField(L"Leaf");
        mgr.r_Desc() += oList;

        //  Landing ON the list was never the problem -- §10 made a non-field
        //  last component an answer rather than a throw.
        P3PmsgObject oOn = mgr.RootPath2Object(L".Store.Numbers");
        TF_CHECK(!oOn.IsVoid());
        TF_CHECK(oOn.IsList());

        //  One step further is the half that threw.
        P3PmsgObject oAttr = mgr.RootPath2Object(L".Store.Numbers@Unit");
        TF_CHECK(!oAttr.IsVoid());
        P3PmsgObject oKid = mgr.RootPath2Object(L".Store.Numbers.Leaf");
        TF_CHECK(!oKid.IsVoid());

        //  And they are the same objects the object-path spellings answer.
        P3PmsgObject oRoot = mgr.r_Object();
        P3PmsgObject oAttrSel = P3Pmsg_SelectObject(&oRoot, L"Numbers@Unit");
        P3PmsgObject oKidSel  = P3Pmsg_SelectObject(&oRoot, L"Numbers.Leaf");
        TF_CHECK(!oAttrSel.IsVoid());
        TF_CHECK(!oKidSel.IsVoid());
        if (!oAttr.IsVoid() && !oAttrSel.IsVoid())
            TF_CHECK(oAttr.GetP2Pos() == oAttrSel.GetP2Pos());
        if (!oKid.IsVoid() && !oKidSel.IsVoid())
            TF_CHECK(oKid.GetP2Pos() == oKidSel.GetP2Pos());

        //  A list carries an attribute COLLECTION like any other item (§12).
        P3PmsgObject oColl = mgr.RootPath2Object(L".Store.Numbers@");
        TF_CHECK(!oColl.IsVoid());
        TF_CHECK(oColl.IsAttr());
    }

    //  A vector is the third item type and reads identically -- the VBLockItem
    //  header is the same six addresses whatever the ut union holds (§6).
    TF_CASE("a root path walks over a vector and keeps going")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgVect oVect(2, L"Payload", P3PmsgData((int)0));
        oVect.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Scale");
        oVect.r_Desc(P3PmsgField::AttrCMD_Create);
        oVect.r_Desc() += P3PmsgField(L"Note");
        mgr.r_Desc() += oVect;

        TF_CHECK(mgr.RootPath2Object(L".Store.Payload").IsVect());
        TF_CHECK(!mgr.RootPath2Object(L".Store.Payload@Scale").IsVoid());
        TF_CHECK(!mgr.RootPath2Object(L".Store.Payload.Note").IsVoid());
    }

    //  A COLLECTION part way along is the other non-field the walk can reach.
    //  "@^" names the attribute collection as it stood at the last push (§9),
    //  and the component after it is an ordinary name looked up inside it --
    //  so "@^.Currency" and "^@Currency" are two spellings of one object, and
    //  only the second of them resolved.
    TF_CASE("a root path walks over an attribute collection")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();
        oLive.r_Attr() += P3PmsgField(L"Venue");        // after the push

        P3PmsgObject oThrough = mgr.RootPath2Object(L".Store.BHP@^.Currency");
        P3PmsgObject oAround  = mgr.RootPath2Object(L".Store.BHP^@Currency");
        TF_CHECK(!oThrough.IsVoid());
        TF_CHECK(!oAround.IsVoid());
        if (!oThrough.IsVoid() && !oAround.IsVoid())
            TF_CHECK(oThrough.GetP2Pos() == oAround.GetP2Pos());

        //  It really is the snapshot's collection: Venue was added after the
        //  push, so it is in the live one and not in this one.
        TF_CHECK(!mgr.RootPath2Object(L".Store.BHP@Venue").IsVoid());
        bool bThrewVenue = false;
        try   { mgr.RootPath2Object(L".Store.BHP@^.Venue"); }
        catch (P2Pevent* pEVT) { bThrewVenue = true; pEVT->Cancel(false); }
        TF_CHECK(bThrewVenue);
    }

    //  The miss rules are the ones already on record, and the walk reaches
    //  them now instead of dying one component early. A DESCENDANT component
    //  that finds nothing throws "Path to object does not exist" -- it used to
    //  throw "Invalid overloaded context" from the list above it, having never
    //  asked about the name at all. An '@' or a '^' that finds nothing is an
    //  ordinary empty answer. The component's own delimiter decides, as it
    //  always has, not what is being searched.
    TF_CASE("a miss under a container is the miss it always was")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Unit");
        oList.r_Desc(P3PmsgField::AttrCMD_Create);
        oList.r_Desc() += P3PmsgField(L"Leaf");
        mgr.r_Desc() += oList;

        bool bThrewKid = false;
        try   { mgr.RootPath2Object(L".Store.Numbers.Nobody"); }
        catch (P2Pevent* pEVT) { bThrewKid = true; pEVT->Cancel(false); }
        TF_CHECK(bThrewKid);

        TF_CHECK(mgr.RootPath2Object(L".Store.Numbers@None").IsVoid());
        TF_CHECK(mgr.RootPath2Object(L".Store.Numbers^").IsVoid());

        //  And a root with no components at all still answers the root.
        P3PmsgObject oRoot = mgr.RootPath2Object(L".Store");
        TF_CHECK(!oRoot.IsVoid());
        TF_CHECK(oRoot.GetP2Pos() == mgr.r_Object().GetP2Pos());
    }
}

// ---------------------------------------------------------------------------
// P3Pmsg_SelectObject : path components against a list or a vector
// ---------------------------------------------------------------------------
static void Test_ListPath()
{
    //  A list and a vector are ITEMS. The VBLockItem header carries the same
    //  six addresses -- aParent, aPrev, aNext, aExtra, aStack, aDescn --
    //  whatever the ut union under it holds, so a list has attributes and
    //  descendants of its own exactly as a field does. The selector did not:
    //  its list arm was ASSERT(0) in its entirety and there was no vector arm
    //  at all, so a path that named a list and then kept going tripped an
    //  assertion in a debug build and came back void in a release one.
    //
    //  Landing ON a list always worked -- P3PmsgCurs::Goto connects
    //  m_oP3PmsgList for a match of that type and the descendant arm returns
    //  straight from the cursor. It is the step AFTER that which had nowhere
    //  to go.
    TF_CASE("'@' reaches an attribute of a list")
    {
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)1));
        oList.AddListTail(P3PmsgData((int)2));
        oList.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Unit");

        P3PmsgItem oHost(L"Host");
        oHost.r_Desc(P3PmsgField::AttrCMD_Create);
        oHost.r_Desc() += oList;

        //  The list itself: this half was never broken.
        P3PmsgObject oFound = P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers");
        TF_CHECK(!oFound.IsVoid());
        TF_CHECK(oFound.IsList());

        //  One step further is the half that was.
        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers@Unit").IsVoid());
        TF_CHECK( P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers@None").IsVoid());
    }

    TF_CASE("a descendant name resolves through a list")
    {
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)1));
        oList.r_Desc(P3PmsgField::AttrCMD_Create);
        oList.r_Desc() += P3PmsgField(L"Kid");

        P3PmsgItem oHost(L"Host");
        oHost.r_Desc(P3PmsgField::AttrCMD_Create);
        oHost.r_Desc() += oList;

        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers.Kid").IsVoid());
        TF_CHECK( P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers.Nobody").IsVoid());
    }

    //  A list that was never pushed has no aStack to follow, exactly as an
    //  unpushed field does not. The answer is the ordinary void -- not the
    //  assertion the list arm used to raise for every component alike. What a
    //  PUSHED list answers is Test_StackContainers' business.
    TF_CASE("'^' on an unpushed list is a miss, not an assertion")
    {
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)1));

        P3PmsgItem oHost(L"Host");
        oHost.r_Desc(P3PmsgField::AttrCMD_Create);
        oHost.r_Desc() += oList;

        TF_CHECK(P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers^").IsVoid());
        TF_CHECK(P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers^.Kid").IsVoid());
    }

    //  A vector was worse off than a list: it had no arm of its own, so it fell
    //  past every test to the ASSERT(0) that closes the function.
    TF_CASE("'@' reaches an attribute of a vector")
    {
        P3PmsgVect oVect(2, L"Payload", P3PmsgData((int)0));
        oVect.r_data(0).c_int(5);
        oVect.r_data(1).c_int(6);
        oVect.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Unit");

        P3PmsgItem oHost(L"Host");
        oHost.r_Desc(P3PmsgField::AttrCMD_Create);
        oHost.r_Desc() += oVect;

        P3PmsgObject oFound = P3Pmsg_SelectObject(&oHost.r_Object(), L"Payload");
        TF_CHECK(!oFound.IsVoid());
        TF_CHECK(oFound.IsVect());

        TF_CHECK(!P3Pmsg_SelectObject(&oHost.r_Object(), L"Payload@Unit").IsVoid());
        TF_CHECK( P3Pmsg_SelectObject(&oHost.r_Object(), L"Payload^"    ).IsVoid());
    }

    //  The rooted form. P3Pmsg_SelectObject checks the leading component
    //  against the object it is standing on before recursing, and that test was
    //  IsField()-only: a list reached its ASSERT(0) and then recursed anyway,
    //  so the name was never actually checked.
    TF_CASE("a rooted path checks the name of a list it starts at")
    {
        P3PmsgList oList(L"Numbers", P3PmsgData((int)0));
        oList.AddListTail(P3PmsgData((int)1));
        oList.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Unit");

        TF_CHECK(!P3Pmsg_SelectObject(&oList.r_Object(), L".Numbers@Unit").IsVoid());

        //  A leading component that names something else is a broken path.
        TF_CHECK(P3Pmsg_SelectObject(&oList.r_Object(), L".Other@Unit").IsVoid());
    }
}

// ---------------------------------------------------------------------------
// P2Pevent : fluent event / exception builder
// ---------------------------------------------------------------------------
static void Test_Event()
{
    TF_CASE("MakeEvent builds an ERROR-class event")
    {
        P2Pevent* pEVT = P2Pevent::MakeEvent(P2Pevent_ERROR);
        TF_CHECK(pEVT != nullptr);
        TF_CHECK(pEVT->GetClass() == P2Pevent_ERROR);
        pEVT->Cancel(false);
    }

    TF_CASE("a wide format renders wide and narrow arguments in full")
    {
        //  The defect this guards: a bare "..." literal is NARROW in this tree,
        //  so Message("[%s]", wideArg) selects the NARROW overload, where %s
        //  means a narrow string. Off Win32 the wide argument is then read as
        //  char* and stops at its first embedded NUL -- after ONE character --
        //  and the specs it never consumed leak into the text as a literal %s.
        //  It truncates on Windows too. A L"..." literal selects the wide overload,
        //  where p2p_fix_wformat maps %s to %ls for glibc and MSVC takes it
        //  natively; a genuinely narrow argument such as __FUNCTION__ is %hs
        //  there, whose 'h' p2p_fix_wformat erases for the same reason.
        P2Pevent* pEVT = EVERR->Message(L"addr=[%s] fn=[%hs]",
                                        L"Alpha.Bravo", "TheFunction");
        TF_CHECK(pEVT != nullptr);
        if ( pEVT )
        {
          const CString strMsg = pEVT->GetMessage();
          TF_CHECK(strMsg.Find(L"Alpha.Bravo") >= 0);   // not just "A"
          TF_CHECK(strMsg.Find(L"TheFunction") >= 0);   // narrow arg, via %hs
          TF_CHECK(strMsg.Find(L"%s")          <  0);   // nothing left unconsumed
          pEVT->Cancel(false);
        }
    }

    TF_CASE("fluent setters round-trip through the getters")
    {
        P2Pevent* pEVT = EVERR->Module("Module")
                              ->Message("Message")
                              ->Group("Group")
                              ->Advice("Advice");
        TF_CHECK(pEVT != nullptr);

        //  GetModule()/GetGroup() end in P3PmsgData::c_wstr(), i.e. in
        //  p2p_wstr_from_store(), whose result off Win32 is a slot of a 16-entry
        //  thread-local RING (Platform/p2pstr.h:629-651).  Each of the four
        //  getters spends several slots of its own - Exists() and operator[] both
        //  rescan the event BY NAME and every comparison widens one name, and
        //  GetMessage()/GetAdvice() widen once per list entry besides - so
        //  holding all four raw leaves the first read a couple of calls short of
        //  being recycled.  Copy the two that ARE ring pointers;
        //  GetMessage()/GetAdvice() return const CString& into P2Pevent's own
        //  members and are stable either way.  Byte-identical on Win32.
        CString strModule   = pEVT->GetModule();
        CString strGroup    = pEVT->GetGroup();
        LPCTSTR lpszModule  = strModule;
        LPCTSTR lpszMessage = pEVT->GetMessage();
        LPCTSTR lpszGroup   = strGroup;
        LPCTSTR lpszAdvice  = pEVT->GetAdvice();

        TF_CHECK(lpszModule  != nullptr && wcslen(lpszModule)  > 0);
        TF_CHECK(lpszMessage != nullptr && wcslen(lpszMessage) > 0);
        TF_CHECK(lpszGroup   != nullptr && wcslen(lpszGroup)   > 0);
        TF_CHECK(lpszAdvice  != nullptr && wcslen(lpszAdvice)  > 0);

        TF_CHECK(wcscmp(lpszGroup, L"Group") == 0);

        pEVT->Cancel(false);
    }

    TF_CASE("AFP attaches typed function parameters without loss")
    {
        int   vInt   = 2;
        short vShort = 3;
        P2Pevent* pEVT = EVERR->Module("Mod")
                              ->AFP(vInt)->AFP(vShort)
                              ->Message("Message")->Group("Group")->Advice("Advice");
        TF_CHECK(pEVT != nullptr);
        pEVT->AssertValid();
        pEVT->Cancel(false);
    }

    TF_CASE("copy-constructed event carries the same fields")
    {
        P2Pevent* pEVT = EVERR->Module("Module")
                              ->Message("Message")->Group("Group")->Advice("Advice");
        P2Pevent* pCopy = new P2Pevent(*pEVT);
        TF_CHECK(wcscmp(pEVT->GetGroup(), pCopy->GetGroup()) == 0);
        delete pCopy;
        pEVT->Cancel(false);
    }
}

// ---------------------------------------------------------------------------
// Test_DateNormalisation was here upstream and is NOT ported (item 15).
//
// It exercised DATE2Normalised / Normalised2DATE, which live in COleTime_Ext.h
// in the MsgcoreMFC component. MsgcoreMFC is not part of this repository and is
// not published with it, so those two cases test code a reader of this
// repository cannot see, using a header they cannot include.
//
// Dropped rather than stubbed, deliberately. A stub that always passes is worse
// than an absent case: it inflates the case count and reads like coverage of
// something nothing here covers. The count below is 2 cases / 8 checks lighter
// than upstream's for exactly this reason, and that is the honest number.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// P2PmsgData_var (COM VARIANT bridge): the WSTR cases return the wide value.
// On Linux the cell is 16-bit P2PWCHAR storage; the variant must widen it to
// native wchar_t rather than reinterpret the raw store pointer (the deferred
// variant/display path). Exercise it through the public
// P2PmsgData_var() so a wrong reinterpret shows up as a corrupted string.
// ---------------------------------------------------------------------------
static void Test_VariantWideString()
{
    TF_CASE("P2PmsgData_var widens a BMP wide-string value")
    {
        const wchar_t* kText = L"Hello, €uro world";   // includes U+20AC euro
        P3PmsgData oData(kText);
        _variant_t  var  = P2PmsgData_var(oData);
        _bstr_t     bstr(var);
        const wchar_t* got = (const wchar_t*)bstr;
        TF_CHECK(got != nullptr && wcscmp(got, kText) == 0);
    }

    TF_CASE("P2PmsgData_var widens an astral wide-string value")
    {
        const wchar_t* kAstral = L"A\U0001F680€B";     // surrogate pair on Windows
        P3PmsgData oData(kAstral);
        _variant_t  var  = P2PmsgData_var(oData);
        _bstr_t     bstr(var);
        const wchar_t* got = (const wchar_t*)bstr;
        TF_CHECK(got != nullptr && wcscmp(got, kAstral) == 0);
    }
}

// ---------------------------------------------------------------------------
// P3PmsgData copy semantics
//
// P3PmsgData owns its cell through a raw pointer and deletes it in the
// destructor, but declared no copy constructor -- so the compiler generated a
// shallow one and two objects ended up owning the same cell. The source read
// back as zero once the copy died, and the second destructor faulted.
// ---------------------------------------------------------------------------
static void Test_Data_CopySemantics()
{
    TF_CASE("a copied data cell carries the value and the type")
    {
        P3PmsgData oSrc((int)4210);
        P3PmsgData oCopy(oSrc);
        TF_CHECK_EQ(oCopy.DataType(), (VBLockType)VBLockData_INT32);
        TF_CHECK_EQ(oCopy.c_int(), 4210);
    }

    TF_CASE("the copy is independent of the source")
    {
        P3PmsgData oSrc((int)10);
        P3PmsgData oCopy(oSrc);
        oCopy.c_int(99);
        TF_CHECK_EQ(oCopy.c_int(), 99);
        TF_CHECK_EQ(oSrc.c_int(), 10);
    }

    TF_CASE("the source outlives a destroyed copy intact")
    {
        P3PmsgData oSrc((int)7);
        { P3PmsgData oCopy(oSrc); TF_CHECK_EQ(oCopy.c_int(), 7); }
        TF_CHECK_EQ(oSrc.c_int(), 7);          // shallow copy read 0 here
    }

    TF_CASE("copying works for a string cell too")
    {
        P3PmsgData oSrc(L"a value");
        P3PmsgData oCopy(oSrc);
        TF_CHECK(wcscmp(oCopy.c_wstr(), L"a value") == 0);
    }
}

// ---------------------------------------------------------------------------
// P2PSafePtr : the copy constructor, which never compiled
// ---------------------------------------------------------------------------
// MsgCollectors.h's copy constructor carried two defects from b97bea3 "Initial
// Code". It compared m_pRefCount against &m_pRefCount -- the address of the
// pointer rather than of the count that pointer guards -- and then called the
// non-const SwapRef2Shared() through a const reference. Both are hard errors on
// both toolchains (MSVC C2446 and C2662; g++ "distinct pointer types" and
// "discards qualifiers"), so this constructor was never instantiated anywhere
// in MSCS and P2PSafePtr was accidentally non-copyable. Nothing reported it
// because nothing ever copied one.
//
// These cases exist first of all to INSTANTIATE it: if either defect returns
// this file stops compiling, which catches the regression earlier than any
// assertion could. What they then check is the part a cast would have hidden --
// that the count is migrated off the source, so a copy may outlive it.
// ---------------------------------------------------------------------------
namespace
{
    struct SafePtrProbe
    {
        static int nLive;
        int        nValue;
        SafePtrProbe ( int n ) : nValue(n) { ++nLive; }
       ~SafePtrProbe ( )                   { --nLive; }
    };
    int SafePtrProbe::nLive = 0;
}

static void Test_SafePtr_CopySemantics()
{
    TF_CASE("a copy keeps the payload alive after the source is destroyed")
    {
        SafePtrProbe::nLive = 0;
        P2PSafePtr<SafePtrProbe>* pCopy = NULL;
        {
            P2PSafePtr<SafePtrProbe> oSrc(new SafePtrProbe(4210));
            pCopy = new P2PSafePtr<SafePtrProbe>(oSrc);
            TF_CHECK_EQ(SafePtrProbe::nLive, 1);
        }
        // The source is gone. Before the fix the count lived inside it, so this
        // read was of expired storage.
        TF_CHECK_EQ(SafePtrProbe::nLive, 1);
        TF_CHECK_EQ((*pCopy)->nValue, 4210);
        delete pCopy;
        TF_CHECK_EQ(SafePtrProbe::nLive, 0);        // destroyed exactly once
    }

    TF_CASE("the source outlives a destroyed copy intact")
    {
        SafePtrProbe::nLive = 0;
        P2PSafePtr<SafePtrProbe> oSrc(new SafePtrProbe(7));
        {
            P2PSafePtr<SafePtrProbe> oCopy(oSrc);
            TF_CHECK_EQ(oCopy->nValue, 7);
        }
        TF_CHECK_EQ(SafePtrProbe::nLive, 1);
        TF_CHECK_EQ(oSrc->nValue, 7);
    }

    TF_CASE("a chain of copies frees the payload exactly once")
    {
        SafePtrProbe::nLive = 0;
        {
            P2PSafePtr<SafePtrProbe> oFirst(new SafePtrProbe(3));
            P2PSafePtr<SafePtrProbe> oSecond(oFirst);
            P2PSafePtr<SafePtrProbe> oThird(oSecond);
            TF_CHECK_EQ(SafePtrProbe::nLive, 1);
            TF_CHECK_EQ(oThird->nValue, 3);
        }
        TF_CHECK_EQ(SafePtrProbe::nLive, 0);
    }
}

// ---------------------------------------------------------------------------
// P2PSafePtr : what the two conversions were answering
// ---------------------------------------------------------------------------
// The template declared operator SafePtrType*() and operator bool() together
// and no comparison of its own, which is the shape §21 took off the six Msgcore
// objects: `int n = sp` compiled and `sp == sp` did not (C2593, the two
// conversions equally good ways to reach a built-in ==).
//
// The conversions are not symmetrical in worth and were not treated so. The
// POINTER one is the idiom the library is written in -- deleting it named eight
// call sites that hand a safe pointer straight to a function taking a raw one
// -- so it stays implicit. The BOOL one is used by nothing: deleting it gave 0
// errors over nine solutions in both configurations, so it is explicit now.
//
// The cases below are behavioural rather than about what compiles, because what
// compiles is measured by compiling it. They pin the two lifetime defects the
// conversions were hiding.
// ---------------------------------------------------------------------------
namespace
{
    //  Returns BY VALUE on purpose: the assignment under test is from a
    //  temporary, which is the one a non-const operator= could not bind.
    static P2PSafePtr<SafePtrProbe> MakeSafePtrProbe ( int n )
    {
        return P2PSafePtr<SafePtrProbe> ( new SafePtrProbe ( n ) );
    }
}

static void Test_SafePtr_Conversions()
{
    //  THE ARM THAT WAS BEING TAKEN. operator = ( P2PSafePtr& ) could not bind
    //  a temporary, so this fell through to operator = ( SafePtrType* ) by way
    //  of the implicit pointer conversion -- an arm that stamps a fresh count
    //  of one and knows nothing of the count the temporary still holds. The
    //  temporary then freed the payload and left the assignee addressing it.
    TF_CASE("assigning from a temporary shares it instead of seizing it")
    {
        SafePtrProbe::nLive = 0;
        {
            P2PSafePtr<SafePtrProbe> oPtr;
            oPtr = MakeSafePtrProbe ( 1234 );

            //  The temporary has been destroyed by now. Before the fix so had
            //  the payload, and this read was of freed storage.
            TF_CHECK_EQ(SafePtrProbe::nLive, 1);
            TF_CHECK_EQ(oPtr->nValue, 1234);
        }
        TF_CHECK_EQ(SafePtrProbe::nLive, 0);        // freed exactly once
    }

    //  And a const source reaches the same arm, which it could not before:
    //  C2679, there being no overload taking one.
    TF_CASE("assigning from a const safe pointer shares it too")
    {
        SafePtrProbe::nLive = 0;
        {
            P2PSafePtr<SafePtrProbe>        oSrc ( new SafePtrProbe ( 55 ) );
            const P2PSafePtr<SafePtrProbe>& roSrc = oSrc;
            P2PSafePtr<SafePtrProbe>        oDst;
            oDst = roSrc;

            TF_CHECK_EQ(SafePtrProbe::nLive, 1);    // one payload, two holders
            TF_CHECK_EQ(oDst->nValue, 55);
            TF_CHECK(oDst == oSrc);
        }
        TF_CHECK_EQ(SafePtrProbe::nLive, 0);
    }

    //  AN EMPTY SOURCE HAS NOTHING TO SHARE. Neither copy path asked, so
    //  SwapRef2Shared() moved a count of zero to the heap and the copy
    //  constructor raised it to one for a pointee that does not exist: the
    //  copy asserted and freed the count on the way out, and the source then
    //  read the freed int.
    TF_CASE("copying an empty safe pointer leaves both of them empty")
    {
        P2PSafePtr<SafePtrProbe> oSrc;
        {
            P2PSafePtr<SafePtrProbe> oCopy ( oSrc );
            TF_CHECK(oCopy.IsEmpty());
            TF_CHECK(oSrc.IsEmpty());
            TF_CHECK(oCopy == oSrc);
        }
        TF_CHECK(oSrc.IsEmpty());                   // and the source survived it
    }

    TF_CASE("assigning from an empty safe pointer empties the target")
    {
        SafePtrProbe::nLive = 0;
        {
            P2PSafePtr<SafePtrProbe> oEmpty;
            P2PSafePtr<SafePtrProbe> oHeld ( new SafePtrProbe ( 9 ) );
            TF_CHECK_EQ(SafePtrProbe::nLive, 1);
            oHeld = oEmpty;                         // ... which lets the payload go
            TF_CHECK_EQ(SafePtrProbe::nLive, 0);
            TF_CHECK(oHeld.IsEmpty());
        }
        TF_CHECK_EQ(SafePtrProbe::nLive, 0);
    }

    //  THE COMPARISON THE CLASS WAS FOR, which did not compile at all. It is
    //  identity of the POINTEE: two safe pointers over one payload are equal
    //  however separately they came by it, and two payloads are never equal
    //  however identically they read.
    TF_CASE("two safe pointers are equal when they point at one thing")
    {
        P2PSafePtr<SafePtrProbe> oOne  ( new SafePtrProbe ( 6 ) );
        P2PSafePtr<SafePtrProbe> oSame ( oOne );
        P2PSafePtr<SafePtrProbe> oTwo  ( new SafePtrProbe ( 6 ) );

        TF_CHECK(oOne == oSame);                    // one payload, two holders
        TF_CHECK(!(oOne != oSame));
        TF_CHECK(oOne != oTwo);                     // two payloads, same reading
        TF_CHECK(!(oOne == oTwo));
        TF_CHECK_EQ(oOne->nValue, oTwo->nValue);    // ... which is the point
    }

    //  The raw-pointer arm exists so that this question does not go through the
    //  implicit CONSTRUCTOR, which would wrap pRaw in a temporary safe pointer
    //  and delete it when the comparison ended.
    TF_CASE("a safe pointer compares against the raw pointer it holds")
    {
        SafePtrProbe::nLive = 0;
        {
            SafePtrProbe            *pRaw = new SafePtrProbe ( 8 );
            P2PSafePtr<SafePtrProbe> oPtr ( pRaw );
            P2PSafePtr<SafePtrProbe> oOther ( new SafePtrProbe ( 8 ) );

            TF_CHECK(oPtr == pRaw);
            TF_CHECK(oOther != pRaw);
            TF_CHECK_EQ(SafePtrProbe::nLive, 2);    // and nothing was freed by asking
        }
        TF_CHECK_EQ(SafePtrProbe::nLive, 0);
    }

    //  An empty one is empty, and says so both ways round.
    TF_CASE("an empty safe pointer equals nullptr and is not equal to a payload")
    {
        P2PSafePtr<SafePtrProbe> oEmpty;
        P2PSafePtr<SafePtrProbe> oHeld ( new SafePtrProbe ( 2 ) );

        TF_CHECK(oEmpty == (SafePtrProbe *)NULL);
        TF_CHECK(oHeld  != (SafePtrProbe *)NULL);
        TF_CHECK(oEmpty != oHeld);
        TF_CHECK(oEmpty.IsEmpty());
        TF_CHECK(!oHeld.IsEmpty());
    }

    //  THE RAW POINTER, SAID BY NAME. The conversion that used to hand it over
    //  implicitly handed over everything a raw pointer can do with it, `delete`
    //  included; it is explicit now and p_SafePtr() is the spelling. It does
    //  NOT give up ownership, which is the whole difference from Dereference().
    TF_CASE("p_SafePtr hands over the pointer and keeps the ownership")
    {
        SafePtrProbe::nLive = 0;
        {
            P2PSafePtr<SafePtrProbe> oPtr(new SafePtrProbe(31));
            SafePtrProbe            *pRaw = oPtr.p_SafePtr();

            TF_CHECK(pRaw != NULL);
            TF_CHECK_EQ(pRaw->nValue, 31);
            TF_CHECK(oPtr == pRaw);              // the same thing, both ways
            TF_CHECK(!oPtr.IsEmpty());           // ... and still held
            TF_CHECK_EQ(SafePtrProbe::nLive, 1);
        }
        TF_CHECK_EQ(SafePtrProbe::nLive, 0);     // freed once, by the holder

        //  Dereference() is the other one: it hands over the pointer AND the
        //  duty to free it.
        SafePtrProbe::nLive = 0;
        {
            P2PSafePtr<SafePtrProbe> oPtr(new SafePtrProbe(32));
            SafePtrProbe            *pRaw = oPtr.Dereference();
            TF_CHECK(oPtr.IsEmpty());
            TF_CHECK_EQ(SafePtrProbe::nLive, 1);
            delete pRaw;
            TF_CHECK_EQ(SafePtrProbe::nLive, 0);
        }
        TF_CHECK_EQ(SafePtrProbe::nLive, 0);
    }

    TF_CASE("an empty safe pointer hands over nothing")
    {
        P2PSafePtr<SafePtrProbe> oEmpty;
        TF_CHECK(oEmpty.p_SafePtr() == NULL);
        TF_CHECK(oEmpty == oEmpty.p_SafePtr());
    }

    //  CONTEXTUAL CONVERSIONS SURVIVE EXPLICIT, which is the whole reason for
    //  spelling it explicit rather than deleting it. A const one answers too,
    //  which it could not before.
    TF_CASE("a safe pointer still tests as a condition, const or not")
    {
        P2PSafePtr<SafePtrProbe>        oHeld ( new SafePtrProbe ( 5 ) );
        const P2PSafePtr<SafePtrProbe>& roHeld = oHeld;
        P2PSafePtr<SafePtrProbe>        oEmpty;

        TF_CHECK(oHeld  ? true : false);
        TF_CHECK(roHeld ? true : false);            // C2451 before
        TF_CHECK(!oEmpty);
        TF_CHECK(static_cast<bool>(oHeld));
        TF_CHECK(oHeld && roHeld);

        //  ... and reading through a const one, which is the rest of it
        TF_CHECK_EQ(roHeld->nValue, 5);
        TF_CHECK_EQ((*roHeld).nValue, 5);           // C2678 before
        TF_CHECK(!roHeld.IsEmpty());                // C2662 before
        SafePtrProbe *pRaw = roHeld.p_SafePtr();    // C2440 before §25,
                                                   // and named since §26
        TF_CHECK_EQ(pRaw->nValue, 5);
    }
}

// ---------------------------------------------------------------------------
// P3PmsgTime and the scalar tags the sizeof ladder used to omit
// ---------------------------------------------------------------------------
static void Test_Time()
{
    TF_CASE("P3PmsgTime carries the TIME64 tag, not INT64")
    {
        P3PmsgTime oTime((__int64)1786249800);
        TF_CHECK_EQ(oTime.DataType(), (VBLockType)VBLockData_TIME64);
        TF_CHECK(!oTime.IsNull());
    }

    TF_CASE("c_time64 reads both 64-bit tags")
    {
        P3PmsgTime oTime((__int64)1786249800);
        P3PmsgData oInt((__int64)1786249800);
        TF_CHECK_EQ(oTime.c_time64(), (__int64)1786249800);
        TF_CHECK_EQ(oInt.c_time64(),  (__int64)1786249800);
    }

    TF_CASE("a default-constructed time is null but still typed")
    {
        P3PmsgTime oNull;
        TF_CHECK_EQ(oNull.DataType(), (VBLockType)VBLockData_TIME64);
        TF_CHECK(oNull.IsNull());
    }

    TF_CASE("assigning a time copies value, tag and null flag")
    {
        P3PmsgTime oSrc((__int64)1786249800);
        P3PmsgTime oDst;
        oDst = oSrc;
        TF_CHECK_EQ(oDst.DataType(), (VBLockType)VBLockData_TIME64);
        TF_CHECK_EQ(oDst.c_time64(), (__int64)1786249800);
        TF_CHECK(!oDst.IsNull());
    }

    // VBLockData_Sizeof_uv's scalar ladder handled INT/UINT/DOUBLE only, so
    // assigning any of these sized the copy from a fall-through.
    TF_CASE("float, bool and wchar cells survive assignment")
    {
        P3PmsgData oF((float)1.5f), oB(true), oW((wchar_t)L'Z');
        P3PmsgData oFd, oBd, oWd;
        oFd = oF;  oBd = oB;  oWd = oW;
        TF_CHECK_EQ(oFd.c_float(), 1.5f);
        TF_CHECK_EQ(oBd.c_bool(), true);
        TF_CHECK_EQ(oWd.c_wchar(), (wchar_t)L'Z');
    }
}

// ---------------------------------------------------------------------------
// 64-bit heap addressing
//
// The VBHeapRoot control-key accessors had Addr32/16/08 ladders; most were
// missing the Addr64 case and fell through to ASSERT(0) plus a "corruption"
// throw, so a 64-bit-addressed heap could not be built through P3PmsgBSTR.
// ---------------------------------------------------------------------------
static void Test_HeapWidths()
{
    TF_CASE("a manager can be built at every addressing width")
    {
        P2PmsgMgr16 oMgr16;
        P2PmsgMgr32 oMgr32;
        P2PmsgMgr64 oMgr64;
        TF_CHECK(oMgr16.IsValid());
        TF_CHECK(oMgr32.IsValid());
        TF_CHECK(oMgr64.IsValid());
    }

    TF_CASE("a 64-bit heap holds and returns a tree")
    {
        P2PmsgMgr64 oMgr;
        oMgr.r_Desc(P3PmsgField::AttrCMD_Create);
        oMgr.DeclareItem(L"Alpha", P3PmsgData((int)1));
        oMgr.DeclareItem(L"Beta",  P3PmsgData(L"two"));
        TF_CHECK_EQ((int)oMgr.r_Desc().GetCount(), 2);
        TF_CHECK_EQ(oMgr.SelectItem(L"Alpha").c_int(), 1);
    }
}

// ---------------------------------------------------------------------------
// VBListIOmage::oSync : endian sentinel (byte_order.md §4)
// ---------------------------------------------------------------------------

// Portable byte swap -- deliberately shift-based rather than _byteswap_ulong /
// __builtin_bswap32 so this case builds identically on both toolchains.
static UINT32 tf_bswap32 ( UINT32 v )
{
    return ( (v & 0x000000FFu) << 24 )
         | ( (v & 0x0000FF00u) <<  8 )
         | ( (v & 0x00FF0000u) >>  8 )
         | ( (v & 0xFF000000u) >> 24 );
}

static void Test_IOmageEndianSentinel()
{
    TF_CASE("VBLock_SyncMake stamps size, addressing mode and sentinel")
    {
        const UINT32 w = VBLock_SyncMake ( 0x50, VBLock_Addr32 );
        TF_CHECK_EQ((int)(w & 0x00FFFFFF), 0x50);
        TF_CHECK_EQ((int)VBLock_SyncAddr(w), (int)VBLock_Addr32);
        TF_CHECK_EQ(VBLock_SyncForm(w), VBLockSync_Native);
    }

    TF_CASE("a pre-sentinel header is still accepted as legacy")
    {
        // Exactly what the old writer emitted: size | uAddrType<<24, no sentinel.
        // This MUST stay readable -- rejecting it would invalidate stored images.
        const UINT32 legacy = 0x50u | ((UINT32)VBLock_Addr32 << 24);
        TF_CHECK_EQ(VBLock_SyncForm(legacy), VBLockSync_Legacy);
        TF_CHECK_EQ((int)VBLock_SyncAddr(legacy), (int)VBLock_Addr32);
    }

    TF_CASE("the historic complement check cannot see a byte swap")
    {
        // The regression this sentinel exists for: complement is per-bit and byte
        // swap is a bit permutation, so they commute and the pair survives intact.
        const UINT32 w  = VBLock_SyncMake ( 0x50, VBLock_Addr32 );
        const UINT32 c  = ~w;
        const UINT32 sw = tf_bswap32 ( w );
        const UINT32 sc = tf_bswap32 ( c );
        TF_CHECK((sw & sc) == 0u && (sw | sc) == ~0u);   // old test: passes
        TF_CHECK_EQ(VBLock_SyncForm(sw), VBLockSync_Swapped);   // new test: caught
    }

    TF_CASE("P2Piomage_Alloc emits the sentinel and round-trips its size")
    {
        const char data[] = "endian";
        P2Piomage *pIOmage = P2Piomage_Alloc ( data, (UINT32)sizeof(data) );
        TF_CHECK(pIOmage != nullptr);
        if ( pIOmage )
        {
          TF_CHECK_EQ(VBLock_SyncForm(pIOmage->oSync.uiSync1), VBLockSync_Native);
          TF_CHECK_EQ((int)VBLock_SyncAddr(pIOmage->oSync.uiSync1), (int)VBLock_Addr32);
          TF_CHECK_EQ((int)P2Piomage_Sizeof(pIOmage),
                      (int)(sizeof(VBListIOmage) + sizeof(data)));
          TF_CHECK((pIOmage->oSync.uiSync1 + pIOmage->oSync.uiSync2) == ~0u);
          P2Piomage_Release ( pIOmage );
        }
    }

    TF_CASE("a foreign-endian image is rejected instead of returning a bogus size")
    {
        const char data[] = "endian";
        P2Piomage *pIOmage = P2Piomage_Alloc ( data, (UINT32)sizeof(data) );
        TF_CHECK(pIOmage != nullptr);
        if ( pIOmage )
        {
          // Byte-swap the header in place: exactly what a big-endian peer's image
          // looks like to this host.
          pIOmage->oSync.uiSync1 = tf_bswap32 ( pIOmage->oSync.uiSync1 );
          pIOmage->oSync.uiSync2 = tf_bswap32 ( pIOmage->oSync.uiSync2 );

          bool bRejected = false;
          try { P2Piomage_Sizeof ( pIOmage ); }
          catch ( P2Pevent* pEVT ) { if ( pEVT ) pEVT->Cancel(false); bRejected = true; }
          TF_CHECK(bRejected);

          P2Piomage_Release ( pIOmage );   // frees the buffer; does not read oSync
        }
    }
}

// ---------------------------------------------------------------------------
// LAYOUT GENERATION (TargetCore's versioning note, §6, gate 2).
// The six sentinel bits are the message image's only version story. ONE code
// is defined - the one this build writes - and every OTHER non-zero pattern
// classifies as VBLockSync_Gen, "a layout this build does not implement".
// These cases exist because that fallback replaced an enumerated registry of
// reserved codes, and the two differ in exactly the four places tested below:
// what a FUTURE generation reports as, where VBLockSync_Invalid now comes
// from, what the byte-order diagnosis costs, and what the fallback gives up.
static UINT32 tf_syncword ( UINT32 uGenBits, UINT32 nSize, UINT08 uAddr )
{
    return (nSize & 0x00FFFFFFu)
         | ( ( uGenBits | (UINT32)(uAddr & VBLock_AddrMask) ) << 24 );
}

static void Test_IOmageLayoutGeneration()
{
    TF_CASE("the generation costs the format nothing - the word is unchanged")
    {
        // THE POINT OF THE WHOLE MECHANISM, as a literal. If promoting the
        // sentinel to a generation tag had moved one bit of what this build
        // writes, it would be a wire break; this is the constant that says it
        // did not. 0x50 bytes, Addr32 (2), generation 1 (0xA4) -> 0xA6000050.
        TF_CHECK_EQ((int)VBLock_SyncMake ( 0x50, VBLock_Addr32 ), (int)0xA6000050u);
        TF_CHECK_EQ((int)VBLock_SyncGenNow, (int)VBLock_SyncGen1);
        TF_CHECK_EQ((int)sizeof(VBListIOmage::oSync), 8);
    }

    TF_CASE("EVERY layout this build does not implement is named, not called corrupt")
    {
        // The case an enumerated registry could not make. It named the ONE
        // reserved code somebody had thought to declare and reported every
        // other as an unrecognised image, so it bought a diagnostic for
        // generation 2 and said nothing about generation 3. The fallback names
        // all 62 free codes, including the ones nobody has designed yet.
        static const UINT32 aCodes[] = { 0xA8, 0xAC, 0xB0, 0x04, 0xF8, 0xFC };
        for ( size_t i = 0; i < sizeof(aCodes)/sizeof(aCodes[0]); ++i )
        {
          const UINT32 w = tf_syncword ( aCodes[i], 0x50, VBLock_Addr32 );
          TF_CHECK_EQ(VBLock_SyncForm(w), VBLockSync_Gen);
          TF_CHECK_EQ((int)VBLock_SyncGenCode(w), (int)aCodes[i]);
          TF_CHECK_EQ((int)VBLock_SyncAddr(w), (int)VBLock_Addr32);
        }
    }

    TF_CASE("the classifier classifies EVERYTHING - Invalid is the complement gate's word")
    {
        // VBLock_SyncForm returns Gen as its fallback, so VBLockSync_Invalid
        // cannot come out of it any more. Swept over all 64 codes rather than
        // asserted of the two that happen to be interesting.
        for ( UINT32 uCode = 0; uCode < 64; ++uCode )
        {
          const UINT32 w = tf_syncword ( uCode << 2, 0x50, VBLock_Addr32 );
          TF_CHECK(VBLock_SyncForm(w) != VBLockSync_Invalid);
        }

        // And that is not a hole, which is the half this checks: the complement
        // pair is the structural filter, it runs BEFORE the classifier, and it
        // still refuses. A word reaches the classifier only by already being a
        // valid pair - which random bytes manage 2^-32 of the time.
        const char data[] = "complement";
        P2Piomage *pIOmage = P2Piomage_Alloc ( data, (UINT32)sizeof(data) );
        TF_CHECK(pIOmage != nullptr);
        if ( pIOmage )
        {
          pIOmage->oSync.uiSync2 ^= 1u;      // one bit off the complement
          bool bRejected = false;
          try { P2Piomage_Sizeof ( pIOmage ); }
          catch ( P2Pevent* pEVT ) { if ( pEVT ) pEVT->Cancel(false); bRejected = true; }
          TF_CHECK(bRejected);

          P2Piomage_Release ( pIOmage );     // frees the buffer; does not read oSync
        }
    }

    TF_CASE("a swapped image of THIS generation is still a swap")
    {
        // The arm the sentinel existed for in the first place, unchanged.
        const UINT32 mine = VBLock_SyncMake ( 0x50, VBLock_Addr32 );
        TF_CHECK_EQ(VBLock_SyncForm(tf_bswap32(mine)), VBLockSync_Swapped);
    }

    TF_CASE("the 1/64 the registry was spending is back - byte_order.md 4.4")
    {
        // Under the enumerated registry a swapped image whose size low byte
        // fell in 0xA8-0xAB was taken by the Gen arm before the swap arm could
        // see it: the third miss range, and the whole standing price of
        // reserving a code. 0x0000A8 bytes, swapped, is that exact image and
        // it is diagnosed correctly now. The miss set is two ranges again.
        const UINT32 mine = VBLock_SyncMake ( 0x0000A8, VBLock_Addr32 );
        TF_CHECK_EQ(VBLock_SyncForm(tf_bswap32(mine)), VBLockSync_Swapped);
    }

    TF_CASE("a swapped FUTURE generation reports as a generation - the residual")
    {
        // The honest cost of the fallback, pinned so it stays a known residual
        // rather than a surprise. The swap arm looks for a code THIS build
        // would have written, so a big-endian peer running generation 3 is
        // refused as an unimplemented layout rather than as an endianness
        // mismatch. Cross-endian AND cross-generation at once - and the
        // registry covered it for exactly one code, at 1/64 of the diagnosis.
        const UINT32 future = tf_syncword ( 0xB0, 0x50, VBLock_Addr32 );
        TF_CHECK_EQ(VBLock_SyncForm(tf_bswap32(future)), VBLockSync_Gen);
    }

    TF_CASE("the fallback is LAST - it swallows nothing the arms above it own")
    {
        // Ordering guard. Gen is the default return, so any arm placed after
        // it would be dead and any arm it displaced would be silently lost.
        const UINT32 mine = VBLock_SyncMake ( 0x50, VBLock_Addr32 );
        TF_CHECK_EQ(VBLock_SyncForm(mine), VBLockSync_Native);
        TF_CHECK_EQ(VBLock_SyncForm(tf_syncword ( 0, 0x50, VBLock_Addr32 )),
                    VBLockSync_Legacy);
        TF_CHECK(VBLock_SyncIsGen ( VBLock_SyncGenNow ) != 0);
        TF_CHECK(VBLock_SyncIsGen ( 0xB0 ) == 0);
    }
}

// ---------------------------------------------------------------------------
// An item's uItemType is WIRE data. The VBLockItem_p* resolvers recognise four
// of the sixteen values the type field can hold; the other twelve used to fall
// through an ASSERT(0) to `return 0`, and ASSERT is compiled out of Release.
// None of the seventeen call sites tests the result -- P2PmsgObject_pData hands
// it straight to VBLockData_IsChained, which dereferences it. A peer that set
// an item type this build does not know therefore faulted inside
// P2Peerio::RecvP2PeerMsg rather than having its frame refused.
//
// p2p_fuzzframe covers this on the wire path (case 0 iteration 36), but only on
// Windows: its assert trap is _CrtSetReportHook, so on glibc the harness aborts
// at the first ASSERT and never reaches iteration 36. These cases call the
// resolvers directly so the guard is checked on BOTH platforms.
static void Test_VBLockItem_UnknownType()
{
    // Addr32, Alloc|Linked -- the header byte the fuzzer's item block carried.
    const UCHAR uVBLock = VBLock_Addr32 | VBLock_Item | VBLock_Alloc | VBLock_Linked;

    auto rejects = []( auto fn ) -> bool {
        try { fn(); return false; }
        catch ( P2Pevent* pEVT ) { if ( pEVT ) pEVT->Cancel(false); return true; }
    };

    TF_CASE("an unknown VBLockItem type is refused, not resolved to NULL")
    {
        // VBLock_Root is a real type constant but not one of the four an item
        // may be (Field/Data/List/Vect), so every resolver falls through.
        VBLockItem oItem;
        std::memset ( &oItem, 0, sizeof(oItem) );
        oItem.uItemType = VBLock_Root;

        TF_CHECK(rejects([&]{ VBLockItem_pData  ( uVBLock, &oItem ); }));
        TF_CHECK(rejects([&]{ VBLockItem_pField ( uVBLock, &oItem ); }));
        TF_CHECK(rejects([&]{ VBLockItem_pName  ( uVBLock, &oItem ); }));
    }

    TF_CASE("a null VBLockItem is refused, not resolved to NULL")
    {
        // Every VBLockItem_Is* predicate answers false for NULL, so a null item
        // reaches the same fall-through. It arrives when an address does not
        // resolve in the image.
        TF_CHECK(rejects([&]{ VBLockItem_pData  ( uVBLock, nullptr ); }));
        TF_CHECK(rejects([&]{ VBLockItem_pField ( uVBLock, nullptr ); }));
        TF_CHECK(rejects([&]{ VBLockItem_pName  ( uVBLock, nullptr ); }));
    }
}

// ---------------------------------------------------------------------------
// F8 and F9: an address that came out of an image, used without being bounded
// against it.
//
// F8 was P2PmsgHeap_Sizeof(hHeap, aVBLock) translating with the UNCHECKED
// Addr2Phys -- which bounds the start with `>` rather than `>=`, so an address
// equal to the arena size resolved to one past the end -- and then reading a
// multi-byte block header from there.
//
// F9 is the same input one function further in, and fixing F8 is what exposed
// it: P3PmsgObject::Connectx ran its F1 span check only when the CALLER left
// the size to be worked out. Supply a size and the bound was skipped, so the
// object connected to an unbounded offset and P2PmsgObject_pData -> VBLock_pData
// read the header byte off the end.
//
// Both were found by tests\fuzz\fuzz_recv_image.cpp, on its fourth executed
// unit. These cases call the two entry points directly, for the reason the
// VBLockItem cases above give: a harness that only runs on Windows under ASan
// checks the guard where it happens to run, and a direct call checks it
// everywhere the suite runs.
//
// STATIC LINK ONLY, and the reason is worth stating rather than working around.
// Not one P2PmsgHeap_* symbol is on the DLL's exported surface -- checked
// against tools\ci\exports-cxx-x64.manifest, which carries zero of them -- and
// neither is P2PmsgObject_pData. So in `dll` mode there is nothing here to
// reach and nothing to check; a case that "passed" by not linking would be the
// stub this file's header comment warns against.
//
// That is also the honest bound on F8 and F9. They are NOT reachable through
// the exported surface: they need code linked against the static archive, or
// code inside the DLL itself -- which is exactly what the P3PmsgField
// constructor is, and what TargetCore's receive path reaches through. The
// defects are live where they matter and unreachable to an external consumer of
// the flat C ABI, which is the supported surface in 1.x.
#ifdef Msgcore_STATIC
static void Test_ImageAddressBounds()
{
    wchar_t szDir[MAX_PATH]  = { 0 };
    wchar_t szPath[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, szDir);
    swprintf_s(szPath, MAX_PATH, L"%smscs_image_bounds.p2p", szDir);

    // A real store, saved and read back as bytes -- the shape a receiver holds.
    {
        P2PmsgMgr oMgr(VBLock_Addr32, 2048, 1u << 20);
        oMgr.r_Desc(P3PmsgField::AttrCMD_Create);
        oMgr.r_Desc() += P3PmsgField(L"bounds", P3PmsgData((int)42));
        oMgr.Save(szPath);
    }

    char    *pImage = nullptr;
    VBLsize  nImage = 0;
    {
        FILE *f = nullptr;
        if (_wfopen_s(&f, szPath, L"rb") == 0 && f)
        {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (n > 0)
            {
                pImage = new char[(size_t)n];
                nImage = (VBLsize)fread(pImage, 1, (size_t)n, f);
            }
            fclose(f);
        }
    }
    _wremove(szPath);

    TF_CASE("an address at the arena size is refused, not translated past the end")
    {
        TF_CHECK(pImage != nullptr && nImage > 0);
        if (!pImage || !nImage) return;

        // Dispatch on the tag, as P2PmsgMgr::Load does. P2PmsgMgr builds a
        // BSTRio heap, so that is the arm a saved store takes; both are driven
        // through the length-validated overload either way.
        P2PmsgHANDLE hHeap = nullptr;
        try {
            if (P2PmsgHeap_IsBSTRio(pImage))
                hHeap = P2PmsgHeap_CreateBSTRio((VBListBSTRio *)pImage, nImage);
            else if (P2PmsgHeap_IsIOMAGE(pImage))
                hHeap = P2PmsgHeap_CreateIOMAGE((VBListIOmage *)pImage, nImage);
        }
        catch (P2Pevent* pEVT) { if (pEVT) pEVT->Cancel(false); }
        if (!hHeap) { delete[] pImage; TF_CHECK(false); return; }
        pImage = nullptr;                // ownership transferred to the heap

        auto rejects = [](auto fn) -> bool {
            try { fn(); return false; }
            catch (P2Pevent* pEVT) { if (pEVT) pEVT->Cancel(false); return true; }
        };

        // F8. nImage is one past the last byte and can never begin a block.
        TF_CHECK(rejects([&]{ P2PmsgHeap_Sizeof(hHeap, (VBLaddr)nImage); }));

        // And one byte under the limit, where the header still cannot fit --
        // the case the Addr2PhysChk declaration in MsgVBHeap.h describes and
        // the reason a start-only bound is not enough.
        TF_CHECK(rejects([&]{ P2PmsgHeap_Sizeof(hHeap, (VBLaddr)(nImage - 1)); }));

        // F9. A SUPPLIED size used to skip the bound entirely.
        TF_CHECK(rejects([&]{
            P3PmsgObject oObject;
            oObject.Connectx(hHeap, (VBLaddr)nImage, sizeof(VBLock));
            P2PmsgObject_pData(oObject);
        }));

        // The guards must not have become a blanket refusal: the heap's own root
        // still resolves and still sizes.
        VBLaddr aRoot = P2PmsgHeap_Connect(hHeap);
        TF_CHECK(aRoot != 0);
        if (aRoot)
            TF_CHECK(!rejects([&]{ P2PmsgHeap_Sizeof(hHeap, aRoot); }));

        P2PmsgHeap_Close(hHeap);
    }

    delete[] pImage;                     // null unless the create above threw
}
#else
static void Test_ImageAddressBounds()
{
    // Visible rather than silent, per the note above and SuiteMain.cpp's rule
    // that anything compiled out must say so. Not counted as a skipped SUITE --
    // the run is still a pass in `dll` mode -- because this is an API that does
    // not cross the DLL boundary at all, not a suite that went missing.
    TF_CASE("image address bounds (F8/F9) -- static link only, heap API is not exported")
    {
        TF_CHECK(true);
    }
}
#endif

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// VBHeap : the closer-fit search
// ---------------------------------------------------------------------------
static void Test_HeapCloserFit()
{
    //  The allocator was first-fit from the head of a LIFO free list. A freed
    //  block goes to the head, so the head is whichever block was freed last,
    //  and first-fit took it whatever its size: free a big block and then a
    //  small one, ask for something small, and the BIG one was split. The small
    //  block stayed on the list, and the next big request could no longer be
    //  served by what was left of the block that used to serve it.
    //
    //  The boundary tag (Test_HeapCoalesce) cannot help here: these blocks are
    //  not adjacent -- live data sits between them -- so there is nothing to
    //  merge. The only repair is to choose better among the blocks there are.
    //
    //  Measured on the case below, before and after:
    //
    //      Big = 331, Sml = 1125, both then freed
    //                          before      after
    //      a small request      331         1125     <- splits Big / takes Sml
    //      a big request        1517         331     <- fresh ground / takes Big
    //
    //  1517 is the cost in one line: a heap that had two free blocks and could
    //  use neither for what they were made for.
    TF_CASE("an allocation takes the block that fits, not the first that will do")
    {
        wchar_t szBig[201];
        for (int i = 0; i < 200; i++) szBig[i] = L'x';
        szBig[200] = 0;

        P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
        mgr.r_name() = L"Root";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create);

        //  Pad keeps Big and Sml apart, so freeing both cannot merge them and
        //  this stays a test of CHOICE rather than of coalescing.
        mgr.r_Desc() += P3PmsgField(L"Big",  DataBSTR16(szBig));
        mgr.r_Desc() += P3PmsgField(L"Pad",  DataBSTR16(L"pad"));
        mgr.r_Desc() += P3PmsgField(L"Sml",  DataBSTR16(L"s"));
        mgr.r_Desc() += P3PmsgField(L"Tail", DataBSTR16(L"t"));

        P3PmsgObject oBig = mgr.r_Desc().SelectObject(L"Big");
        P3PmsgObject oSml = mgr.r_Desc().SelectObject(L"Sml");
        TF_CHECK(!oBig.IsVoid());
        TF_CHECK(!oSml.IsVoid());
        const P2Pos posBig = oBig.IsVoid() ? 0 : oBig.GetP2Pos();
        const P2Pos posSml = oSml.IsVoid() ? 0 : oSml.GetP2Pos();
        TF_CHECK(posBig != posSml);

        //  Sml first, so that Big is the one left at the head of the list --
        //  which is the arrangement first-fit gets wrong.
        mgr.r_Desc().r_Curs().Goto(L"Sml");
        mgr.r_Desc().r_Curs().Delete();
        mgr.r_Desc().r_Curs().Goto(L"Big");
        mgr.r_Desc().r_Curs().Delete();

        mgr.r_Desc() += P3PmsgField(L"New1", DataBSTR16(L"s"));
        P3PmsgObject oNew1 = mgr.r_Desc().SelectObject(L"New1");
        TF_CHECK(!oNew1.IsVoid());
        if (!oNew1.IsVoid())
            TF_CHECK(oNew1.GetP2Pos() == posSml);      // not posBig

        mgr.r_Desc() += P3PmsgField(L"New2", DataBSTR16(szBig));
        P3PmsgObject oNew2 = mgr.r_Desc().SelectObject(L"New2");
        TF_CHECK(!oNew2.IsVoid());
        if (!oNew2.IsVoid())
            TF_CHECK(oNew2.GetP2Pos() == posBig);      // Big's block, whole

        mgr.AssertValid();
    }

    //  The same thing on a free list with more than one block of each size, so
    //  the walk has to pick rather than merely notice. Eight blocks is also
    //  kMaxFitWalk exactly: the budget must be enough to see the whole of a
    //  list this size, or the last of the eight would never be chosen.
    //
    //  Before the closer fit, the four small requests each split one of the big
    //  blocks, and the four big requests that followed then had nowhere to go
    //  but fresh ground -- so the "every block came back" check below is the
    //  one that fails.
    TF_CASE("a scattered free list hands every block back")
    {
        wchar_t szBig[201];
        for (int i = 0; i < 200; i++) szBig[i] = L'x';
        szBig[200] = 0;

        P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
        mgr.r_name() = L"Root";
        mgr.r_Desc(P3PmsgField::AttrCMD_Create);

        //  Interleaved with pads, so no two of the blocks that get freed are
        //  ever physically adjacent.
        for (int i = 0; i < 4; i++)
        {
            wchar_t sz[32];
            swprintf_s(sz, 32, L"PadA%d", i);
            mgr.r_Desc() += P3PmsgField(sz, DataBSTR16(L"p"));
            swprintf_s(sz, 32, L"Big%d", i);
            mgr.r_Desc() += P3PmsgField(sz, DataBSTR16(szBig));
            swprintf_s(sz, 32, L"PadB%d", i);
            mgr.r_Desc() += P3PmsgField(sz, DataBSTR16(L"p"));
            swprintf_s(sz, 32, L"Sml%d", i);
            mgr.r_Desc() += P3PmsgField(sz, DataBSTR16(L"s"));
        }
        mgr.r_Desc() += P3PmsgField(L"Tail", DataBSTR16(L"t"));

        P2Pos posWas[8] = { 0 };
        for (int i = 0; i < 4; i++)
        {
            wchar_t sz[32];
            swprintf_s(sz, 32, L"Big%d", i);
            P3PmsgObject oBig = mgr.r_Desc().SelectObject(sz);
            TF_CHECK(!oBig.IsVoid());
            posWas[i] = oBig.IsVoid() ? 0 : oBig.GetP2Pos();
            swprintf_s(sz, 32, L"Sml%d", i);
            P3PmsgObject oSml = mgr.r_Desc().SelectObject(sz);
            TF_CHECK(!oSml.IsVoid());
            posWas[4 + i] = oSml.IsVoid() ? 0 : oSml.GetP2Pos();
        }

        //  Smalls first, then bigs, so the head of the list ends up big.
        for (int i = 0; i < 4; i++)
        {
            wchar_t sz[32];
            swprintf_s(sz, 32, L"Sml%d", i);
            mgr.r_Desc().r_Curs().Goto(sz);
            mgr.r_Desc().r_Curs().Delete();
        }
        for (int i = 0; i < 4; i++)
        {
            wchar_t sz[32];
            swprintf_s(sz, 32, L"Big%d", i);
            mgr.r_Desc().r_Curs().Goto(sz);
            mgr.r_Desc().r_Curs().Delete();
        }

        //  Ask for the smalls first: that is the order that used to shred the
        //  big blocks.
        for (int i = 0; i < 4; i++)
        {
            wchar_t sz[32];
            swprintf_s(sz, 32, L"NewS%d", i);
            mgr.r_Desc() += P3PmsgField(sz, DataBSTR16(L"s"));
        }
        for (int i = 0; i < 4; i++)
        {
            wchar_t sz[32];
            swprintf_s(sz, 32, L"NewB%d", i);
            mgr.r_Desc() += P3PmsgField(sz, DataBSTR16(szBig));
        }

        //  Every one of the eight came out of a block that was already there.
        for (int i = 0; i < 8; i++)
        {
            wchar_t sz[32];
            swprintf_s(sz, 32, i < 4 ? L"NewS%d" : L"NewB%d", i < 4 ? i : i - 4);
            P3PmsgObject oNew = mgr.r_Desc().SelectObject(sz);
            TF_CHECK(!oNew.IsVoid());
            if (oNew.IsVoid())
                continue;
            const P2Pos pos = oNew.GetP2Pos();
            bool bReused = false;
            for (int k = 0; k < 8; k++)
                if (posWas[k] == pos)
                    bReused = true;
            TF_CHECK(bReused);
        }
        mgr.AssertValid();
    }

    //  The closer fit walks further than first-fit did, and it walks while
    //  COLLATING -- which is how a block it has already chosen can be swallowed
    //  by one it visits later (refer the re-check in P2PmsgHeap_AllocIOMAGE).
    //  That path is not reachable on demand from out here, so this does the
    //  next best thing: a lot of frees in a lot of orders, each leaving a free
    //  list long enough to be walked and collated, with the heap asked to
    //  validate itself and to survive a round trip at the end of it.
    TF_CASE("churn over a long free list keeps the heap valid")
    {
        wchar_t szDir[MAX_PATH]  = { 0 };
        wchar_t szPath[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, szDir);
        swprintf_s(szPath, MAX_PATH, L"%smscs_closer_fit.p2p", szDir);

        try
        {
            P2PmsgMgr mgr(VBLock_Addr64, 4096, 1u << 20);
            mgr.r_name() = L"Root";
            mgr.r_Desc(P3PmsgField::AttrCMD_Create);

            for (int nRound = 0; nRound < 6; nRound++)
            {
                for (int i = 0; i < 12; i++)
                {
                    wchar_t sz[32];
                    wchar_t szData[64];
                    swprintf_s(sz, 32, L"Kid%d", i);
                    //  Twelve different sizes, so the walk has something to
                    //  choose between rather than a list of equals.
                    for (int k = 0; k < (i * 5) + 1; k++)
                        szData[k] = L'd';
                    szData[(i * 5) + 1] = 0;
                    mgr.r_Desc() += P3PmsgField(sz, DataBSTR16(szData));
                }
                //  Every third one, then every second of what is left: the
                //  free list ends up long, mixed and in no address order.
                for (int i = 0; i < 12; i += 3)
                {
                    wchar_t sz[32];
                    swprintf_s(sz, 32, L"Kid%d", i);
                    mgr.r_Desc().r_Curs().Goto(sz);
                    mgr.r_Desc().r_Curs().Delete();
                }
                for (int i = 11; i > 0; i -= 2)
                {
                    wchar_t sz[32];
                    swprintf_s(sz, 32, L"Kid%d", i);
                    if (!mgr.r_Desc().Exists(sz))
                        continue;
                    mgr.r_Desc().r_Curs().Goto(sz);
                    mgr.r_Desc().r_Curs().Delete();
                }
                mgr.AssertValid();
                mgr.r_Desc().Truncate();
            }

            mgr.r_Desc() += P3PmsgField(L"Survivor", DataBSTR16(L"still here"));
            mgr.Save(szPath);
            mgr.AssertValid();

            P2PmsgMgr oBack(szPath);
            TF_CHECK(oBack.r_Desc().Exists(L"Survivor"));
            oBack.AssertValid();
        }
        catch (P2Pevent* pEVT)
        {
            tf_fail(__FILE__, __LINE__, "unexpected P2Pevent during closer-fit churn");
            pEVT->Cancel(false);
        }
        _wremove(szPath);
    }
}

// ---------------------------------------------------------------------------
// '^' on a COLLECTION, rather than on an item
// ---------------------------------------------------------------------------
static void Test_CollectionStack()
{
    //  aStack is a VBLockItem field. A VBLockAttr and a VBLockDesc do not have
    //  one, so a '^' applied to a COLLECTION used to answer "broken path" --
    //  correct, but less than the block can say. What those blocks DO carry is
    //  aParent, so the item that owns the collection can be found, and that
    //  item has a stack whose snapshot holds a copy of the whole collection.
    //
    //  So "Item@^" is the attribute collection as it stood at the last push,
    //  which is the attribute collection INSIDE the snapshot -- the same object
    //  "Item^@" names. That is what these cases pin: '^' COMMUTES with '@' and
    //  with '.', because a push copies a whole item and not a fragment of one.
    //
    //  Nothing was added to any block to make this work.
    //
    //  The store is built the way §4 of stack_paths.md builds it, and that is
    //  not incidental: attributes reached through a P2PmsgMgr(VBLock_Addr64,
    //  ...) store did not resolve at all when this was first written, by any
    //  path, '^' or no '^'. That is its own question and not this one's, so
    //  these cases use the arrangement that is known to work.
    TF_CASE("'^' on an attribute collection is the snapshot's collection")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();                          // snapshot: Currency
        oLive.r_Attr() += P3PmsgField(L"Venue");        // added AFTER the push

        P3PmsgObject oViaItem = oLive.SelectObject(L"^@Currency");
        P3PmsgObject oViaColl = oLive.SelectObject(L"@^Currency");
        TF_CHECK(!oViaItem.IsVoid());
        TF_CHECK(!oViaColl.IsVoid());
        if (!oViaItem.IsVoid() && !oViaColl.IsVoid())
            TF_CHECK(oViaItem.GetP2Pos() == oViaColl.GetP2Pos());   // commutes

        //  The collection itself, when the path ends on the '^' -- and it is
        //  the SNAPSHOT's, not the live one.
        P3PmsgObject oColl = oLive.SelectObject(L"@^");
        TF_CHECK(!oColl.IsVoid());
        TF_CHECK(oColl.IsAttr());
        if (!oColl.IsVoid())
            TF_CHECK(oColl.GetP2Pos() != oLive.r_Attr().r_Object().GetP2Pos());

        //  Venue went in after the push, so it is in the live collection and
        //  in neither spelling of the snapshot's.
        TF_CHECK(!oLive.SelectObject(L"@Venue").IsVoid());
        TF_CHECK( oLive.SelectObject(L"@^Venue").IsVoid());
        TF_CHECK( oLive.SelectObject(L"^@Venue").IsVoid());
    }

    //  Pushes nest, and a snapshot's own collections point at the SNAPSHOT
    //  rather than back at the live item -- measured, because if they pointed
    //  back at the live item "@^^" would loop on the first generation instead
    //  of descending. So the delimiter repeats here exactly as it does on an
    //  item: "@^" is the collection before the last push, "@^^" the one before
    //  that.
    TF_CASE("'@^^' descends a generation, as '^^' does")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Stck().Push();                          // generation 1
        oLive.r_Attr() += P3PmsgField(L"Venue");
        oLive.r_Stck().Push();                          // generation 2
        oLive.r_Attr() += P3PmsgField(L"Third");

        P3PmsgObject oOne = oLive.SelectObject(L"@^");
        P3PmsgObject oTwo = oLive.SelectObject(L"@^^");
        TF_CHECK(!oOne.IsVoid());
        TF_CHECK(!oTwo.IsVoid());
        if (!oOne.IsVoid() && !oTwo.IsVoid())
            TF_CHECK(oOne.GetP2Pos() != oTwo.GetP2Pos());

        //  Generation 2 has Venue; generation 1 does not. Neither has Third.
        TF_CHECK(!oLive.SelectObject(L"@^Venue").IsVoid());
        TF_CHECK( oLive.SelectObject(L"@^^Venue").IsVoid());
        TF_CHECK(!oLive.SelectObject(L"@^^Currency").IsVoid());
        TF_CHECK( oLive.SelectObject(L"@^Third").IsVoid());

        //  And the same answers by the other spelling.
        TF_CHECK(!oLive.SelectObject(L"^^@Currency").IsVoid());
        TF_CHECK( oLive.SelectObject(L"^^@Venue").IsVoid());
    }

    //  The descendant arm. It is reached by handing the collection to
    //  P3Pmsg_SelectObject directly -- NOT by P3PmsgDesc::SelectObject, which
    //  despite its name is a cursor Goto by plain name and parses no path. A
    //  path that goes through an item does not come this way either: the field
    //  arm's '.' re-enters on the item, so "Item.^" is the item's own stack --
    //  which the commuting rule says is the same object anyway, and that is
    //  what the last check here measures.
    TF_CASE("'^' on a descendant collection is the snapshot's collection")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc() += P3PmsgField(L"BHP");

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Desc(P3PmsgField::AttrCMD_Create);
        oLive.r_Desc() += P3PmsgField(L"Last");
        oLive.r_Stck().Push();                          // snapshot: Last
        oLive.r_Desc() += P3PmsgField(L"Close");        // added AFTER the push

        P3PmsgObject oDescObj = oLive.r_Desc().r_Object();
        TF_CHECK(oDescObj.IsDesc());

        P3PmsgObject oViaItem = oLive.SelectObject(L"^.Last");
        P3PmsgObject oViaColl = P3Pmsg_SelectObject(&oDescObj, L"^Last");
        TF_CHECK(!oViaItem.IsVoid());
        TF_CHECK(!oViaColl.IsVoid());
        if (!oViaItem.IsVoid() && !oViaColl.IsVoid())
            TF_CHECK(oViaItem.GetP2Pos() == oViaColl.GetP2Pos());

        P3PmsgObject oColl = P3Pmsg_SelectObject(&oDescObj, L"^");
        TF_CHECK(!oColl.IsVoid());
        TF_CHECK(oColl.IsDesc());
        if (!oColl.IsVoid())
            TF_CHECK(oColl.GetP2Pos() != oDescObj.GetP2Pos());

        TF_CHECK(!oLive.SelectObject(L"Close").IsVoid());
        TF_CHECK( P3Pmsg_SelectObject(&oDescObj, L"^Close").IsVoid());
    }

    //  An item that never had any attributes. r_Attr() hands back an object
    //  carrying no block, which matched no arm of the selector and fell through
    //  to the ASSERT(0) closing it -- by way of IsRoot(), which asserts a SECOND
    //  time on a SYS-heap object. The ANSWER was always right; the two
    //  assertions were not, and asking a bare item for an attribute it has not
    //  got is ordinary use.
    //
    //  IsVoid() was not the test that catches it: that wants m_hVBList and
    //  m_aVBLock both zero, and an empty collection keeps the handle of the
    //  heap it would have been allocated from.
    //
    //  This case is why the framework counts a debug assertion as a failure.
    //  Before the fix it returns these same void objects and still fails, on
    //  the assertions alone.
    TF_CASE("a collection that was never created answers, without asserting")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";
        mgr.r_Desc() += P3PmsgField(L"RIO");

        P3PmsgField oBare = mgr.RootPath2Object(L".Store.RIO");
        TF_CHECK(!oBare.IsVoid());
        oBare.r_Stck().Push();

        TF_CHECK(oBare.SelectObject(L"^@Any").IsVoid());
        TF_CHECK(oBare.SelectObject(L"@^").IsVoid());
        TF_CHECK(oBare.SelectObject(L"@Any").IsVoid());
    }

    //  Nothing pushed: still a broken path, rather than an assertion or -- the
    //  outcome that would be worst -- the live collection answering as though
    //  it were the snapshot.
    TF_CASE("a collection with nothing pushed is still a broken path")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        TF_CHECK(!oLive.IsStacked());

        TF_CHECK(!oLive.SelectObject(L"@Currency").IsVoid());
        TF_CHECK( oLive.SelectObject(L"@^").IsVoid());
        TF_CHECK( oLive.SelectObject(L"@^Currency").IsVoid());
        TF_CHECK( oLive.SelectObject(L"^").IsVoid());
    }

    //  A ROOT path carrying '@^' now answers what the object path answers,
    //  and it did not when the cases above were written. P3Pmsg_SplitRootPath
    //  broke the component at the '^' and left a nameless "@" that its length
    //  test refused; the split returned FALSE, and P2PmsgMgr::RootPath2Object
    //  never looked at it -- so the walk ran over the components collected
    //  before the refusal and ".Store.BHP@^Currency" came back as BHP. Not the
    //  attribute, not void: the item the path started from.
    TF_CASE("a root path carrying '@^' answers what the object path answers")
    {
        P2PmsgMgr mgr;
        mgr.r_name() = L"Store";

        P3PmsgField oInst(L"BHP");
        oInst.r_Attr(P3PmsgField::AttrCMD_Create) += P3PmsgField(L"Currency");
        mgr.r_Desc() += oInst;

        P3PmsgField oLive = mgr.RootPath2Object(L".Store.BHP");
        TF_CHECK(!oLive.IsVoid());
        oLive.r_Desc(P3PmsgField::AttrCMD_Create);
        oLive.r_Desc() += P3PmsgField(L"Last");
        oLive.r_Stck().Push();                          // Currency, Last
        oLive.r_Attr() += P3PmsgField(L"Venue");        // added AFTER the push

        P3PmsgObject oViaObject = oLive.SelectObject(L"@^Currency");
        P3PmsgObject oViaRoot   = mgr.RootPath2Object(L".Store.BHP@^Currency");
        TF_CHECK(!oViaObject.IsVoid());
        TF_CHECK(!oViaRoot.IsVoid());
        if (!oViaObject.IsVoid() && !oViaRoot.IsVoid())
            TF_CHECK(oViaObject.GetP2Pos() == oViaRoot.GetP2Pos());

        //  And it is not the item the path started from, which is the answer
        //  it used to give.
        if (!oViaRoot.IsVoid())
            TF_CHECK(oViaRoot.GetP2Pos() != oLive.GetP2Pos());

        //  Venue was added after the push, so the snapshot's collection does
        //  not carry it. An ordinary miss, and still an ordinary answer.
        TF_CHECK(mgr.RootPath2Object(L".Store.BHP@^Venue").IsVoid());

        //  The collection itself ends the walk, and it is not a field -- so
        //  even once the splitter produced the component, the walk could not
        //  assign it into the P3PmsgItem it carries. A last component needs no
        //  assignment: it IS the answer.
        P3PmsgObject oColl = mgr.RootPath2Object(L".Store.BHP@^");
        TF_CHECK(!oColl.IsVoid());
        TF_CHECK(oColl.IsAttr());
        if (!oColl.IsVoid())
            TF_CHECK(oColl.GetP2Pos() == oLive.SelectObject(L"@^").GetP2Pos());

        //  The descendant collection reads the same way, and '^' commutes
        //  with '.' in a root path just as it does in an object path.
        P3PmsgObject oKid = mgr.RootPath2Object(L".Store.BHP.^Last");
        TF_CHECK(!oKid.IsVoid());
        if (!oKid.IsVoid())
            TF_CHECK(oKid.GetP2Pos() == oLive.SelectObject(L"^.Last").GetP2Pos());
    }
}

void RunMsgcoreSuite()
{
    Test_Data_TypedValues();
    Test_Data_CopySemantics();
    Test_SafePtr_CopySemantics();
    Test_SafePtr_Conversions();
    Test_Time();
    Test_HeapWidths();
    Test_Field_NameAndData();
    Test_List();
    Test_Vect();
    Test_AttrDesc();
    Test_Curs_GotoKeyLifetime();
    Test_VBLockItem_UnknownType();
    Test_Stack();
    Test_StackContainers();
    Test_VectDrop();
    Test_HeapCoalesce();
    Test_HeapCloserFit();
    Test_StackDrop();
    Test_RootPath();
    Test_ListPath();
    Test_CollectionStack();
    Test_GeneratedStackPath();
    Test_AttrCollectionPath();
    Test_RootPathContainers();
    Test_BareAttrComponent();
    Test_DescCollectionPath();
    Test_P2Pos2Path();
    Test_BareDotCommutes();
    Test_RootedLeadingDot();
    Test_FloatingIdentity();
    Test_VoidPredicates();
    Test_ItemIdentity();
    Test_ConstFieldName();
    Test_HandleOrCopy();
    Test_ValueCopiesItsPayload();
    Test_SoleStorage();
    Test_SoleCollectionCursors();
    Test_ValueCopyReleasesItsPayload();
    Test_ChainLongerThanOne();
    Test_Event();
    // Test_DateNormalisation() -- not ported; see the note at its former site.
    Test_VariantWideString();
    Test_IOmageEndianSentinel();
    Test_IOmageLayoutGeneration();
    Test_ImageAddressBounds();
}
