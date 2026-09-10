// Copyright © 2013, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2Pmsg extension definitions and prototypes
//
#include "stdafx.h"
#include "P2Pmsg.h"
#include "MsgAttr.h"
#include "MsgList.h"
#include "MsgVect.h"
#include "MsgCurs.h"
#include "P2PmsgMgr.h"
#include "P2Pmsg_Ext.h"
#include "P2PmsgBSTR.h"
#include "Msgexception.h"

///////////////////////////////////////////////////////////////////////
//  Recursive P2Pmsg[Field,Node,Attr]_Merge Utilities and helpers
//  NOTES: For clarity and simplicity operations isolated in series
//         of self contained static functions

P3PmsgField&
P2PmsgField_Merge ( P3PmsgField& oField, const P3PmsgField& rhs )
{
    // Field assignment
    (P3PmsgData&)oField = rhs;

    // Attribute assignment
    if ( !((P3PmsgField&)rhs).r_Attr().IsEmpty() )
      P2PmsgAttr_Merge ( oField.r_Attr(P3PmsgField::AttrCMD_Create)
                       , ((P3PmsgField&)rhs).r_Attr() );

    // Tidy up, and
    return oField;
}

/*P3PmsgNode&
P2PmsgNode_Merge ( P3PmsgNode& oItem, const P3PmsgNode& rhs )
{
    // Recursive copy
    P3PmsgCurs& oCurs = ((P3PmsgNode&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
//oCurs.AssertValid();
ASSERT(oCurs.Item()==i);
      if ( oCurs.IsNode() )
      {
        LPCTNAM lpszNodename = oCurs.r_node().c_name();
        if ( oItem.Exists(lpszNodename) )
          P2PmsgNode_Merge ( oItem.SelectNode(lpszNodename), oCurs.r_node() );
        else
          oItem += oCurs.r_node();
      }
      else if ( oCurs.IsList() )
      {
        ASSERT(0);
        oItem += oCurs.r_list();
      }
      else if ( oCurs.IsVect() )
      {
        ASSERT(0);
        oItem += oCurs.r_vect();
      }
      else if ( oCurs.IsItem() )
      {
        LPCTNAM lpszFieldname = oCurs.r_item().c_name();
        if ( oItem.Exists(lpszFieldname) )
          P2PmsgField_Merge ( oItem.SelectItem(lpszFieldname), oCurs.r_item() );
        else
          oItem += oCurs.r_item();
      }
      else
        ASSERT(0);
//oItem.assertValid();//TODO:LJM delete, testing
    }

    // Tidy up, and
    return oItem;
}*/

/*P3PmsgAttr&
P2PmsgAttr_Merge ( P3PmsgAttr& oAttrLHS, const P3PmsgNode& rhs )
{
    // Recursive copy
    P3PmsgCurs& oCurs = ((P3PmsgNode&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
//oCurs.AssertValid();
ASSERT(oCurs.Item()==i);
      if ( oCurs.IsNode() )
      {
        LPCTNAM lpszNodename = oCurs.r_node().c_name();
//oAttrLHS.AssertValid();
        if ( oAttrLHS.Exists(lpszNodename) )
          P2PmsgNode_Merge ( oAttrLHS.SelectNode(lpszNodename), oCurs.r_node() );
        else
          oAttrLHS += oCurs.r_node();
      }
      else if ( oCurs.IsList() )
      {
        ASSERT(0);
        oAttrLHS += oCurs.r_list();
      }
      else if ( oCurs.IsVect() )
      {
        ASSERT(0);
        oAttrLHS += oCurs.r_vect();
      }
      else if ( oCurs.IsItem() )
      {
        LPCTNAM lpszFieldname = oCurs.r_item().c_name();
        if ( oAttrLHS.Exists(lpszFieldname) )
          P2PmsgField_Merge ( oAttrLHS.SelectItem(lpszFieldname), oCurs.r_item() );
        else
          oAttrLHS += oCurs.r_item();
      }
      else
        ASSERT(0);
//oAttrLHS.AssertValid();//TODO:LJM delete, testing
    }

    // Tidy up, and
    return oAttrLHS;
}*/

P3PmsgAttr&
P2PmsgAttr_Merge ( P3PmsgAttr& oAttr, const P3PmsgAttr& rhs )
{
    // Recursive copy
    P3PmsgCurs& oCurs = ((P3PmsgAttr&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
//oCurs.AssertValid();
ASSERT(oCurs.Item()==i);
      /*if ( oCurs.IsNode() )
      {
        LPCTNAM lpszNodename = oCurs.r_node().c_name();
        if ( oAttrLHS.Exists(lpszNodename) )
          P2PmsgNode_Merge ( oAttrLHS.SelectNode(lpszNodename), oCurs.r_node() );
        else
          oAttrLHS += oCurs.r_node();
      }
      else*/ if ( oCurs.IsList() )
      {
        ASSERT(0);
        oAttr += oCurs.r_list();
      }
      else if ( oCurs.IsVect() )
      {
        ASSERT(0);
        oAttr += oCurs.r_vect();
      }
      else if ( oCurs.IsItem() )
      {
        //  COPY, never the bare c_name() pointer.  c_name() ends in
        //  p2p_wstr_from_store(), which off Win32 hands back a slot of a 16-entry
        //  thread-local widening RING (Platform/p2pstr.h:629-651).  Exists() and
        //  SelectItem() each rescan oAttr BY NAME, and every comparison in that
        //  scan spends one more slot (P3PmsgCurs::Goto -> c_wcsicmp -> c_name), so
        //  from about the eighth item on the held pointer is recycled mid-scan -
        //  and when the recycled slot is the one c_name() has just written, the
        //  comparison reads equal and the WRONG item is merged.  Win32 returns the
        //  store pointer unchanged, so this is byte-identical there.
        CString strFieldname  = oCurs.r_item().c_name();
        LPCTNAM lpszFieldname = strFieldname;
        if ( oAttr.Exists(lpszFieldname) )
          P2PmsgField_Merge ( oAttr.SelectItem(lpszFieldname), oCurs.r_item() );
        else
          oAttr += oCurs.r_item();
      }
      else
        ASSERT(0);
//oAttrLHS.AssertValid();//TODO:LJM delete, testing
    }

    // Tidy up, and
    return oAttr;
}

//
//  Absolute merge for P3PmsgAttr objects
//  NOTES: Should the same label be encountered in both the oAttrLHS
//         and oAttrRHS instances, the oAttrLHS instance will totally
//         over-written by the contents of the oAttrRHS label.
//
//  Parameters:  P3PmsgAttr& oAttrLHS
//               Destination or left hand side of the assignment
//
//               P3PmsgAttr& oAttrRHS
//               Source or right hand side of the assignment (pre-dominates)
//
//  Returns:     P3PmsgAttr&
//               Effectively oAttrLHS
P3PmsgAttr&
P2PmsgAttr_AbsoluteMerge ( P3PmsgAttr& oAttrLHS, const P3PmsgAttr& oRHS )
{
    // Recursive copy
    P3PmsgCurs& oCurs = ((P3PmsgAttr&)oRHS).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      //  COPY - see the P3PmsgCurs widening-ring note in P2PmsgAttr_Merge above.
      //  Delete() is the call that needs it most: it rescans oAttrLHS BY NAME,
      //  spending one ring slot per comparison, so a bare c_name() argument dies
      //  inside the very scan that is reading it.
      CString strFieldname  = oCurs.r_item().c_name();
      LPCTNAM lpszFieldname = strFieldname;
      if ( oAttrLHS.Exists(lpszFieldname) )
      {
        CString strDelname = oCurs.r_name().c_name();
        oAttrLHS.Delete ( strDelname );
      }
      oAttrLHS += oCurs.r_item();
    }

    // Tidy up, and
    return oAttrLHS;
}

///////////////////////////////////////////////////////////////////////
//  P2Pmsg serialisation helpers
//  NOTES: Perform standard activities

P3PmsgItem&
P3PmsgField_SERIALISE ( P3PmsgItem& oItem
                      , LPCTNAM lpszFieldname, const P3PmsgData& oData
                      , BOOL bDscAttr, LPCTSTR lpszDescription )
{
    P3PmsgField oField ( lpszFieldname, oData );
    if ( bDscAttr )
      oField.r_Attr(P3PmsgField::AttrCMD_Create)
        += P3PmsgField ( _T("Dsc"), lpszDescription );
    oItem += oField;
    return oItem;
}

P3PmsgAttr&
P2PmsgAttr_SERIALISE  ( P3PmsgAttr& oAttr, BOOL bOverwrite
                      , LPCTNAM lpszFieldname, const P3PmsgData& oData
                      , BOOL bDscAttr, LPCTSTR lpszDescription )
{
    P3PmsgField oField ( lpszFieldname, oData );
    if ( bDscAttr )
      oField.r_Attr(P3PmsgField::AttrCMD_Create)
        += P3PmsgField ( _T("Dsc"), lpszDescription );
    if ( !oAttr.Exists(lpszFieldname) )
      oAttr += oField;
    else if ( bOverwrite )
      oAttr.SelectItem(lpszFieldname) = oField;
    else
      EVERR->MODULE
           ->AFP(lpszFieldname)
           ->Message(_T("Field already exists, no overwrite permission") )
           ->Throw();
    return oAttr;
}

///////////////////////////////////////////////////////////////////////
//  P3PmsgObject extractions

//
//  Recursively searches backwards for parent with specified attribute
//
//  Parameters:  const P3PmsgObject& oObject
//               Search base point
//
//               LPCTSTR lpszAttributeName
//               Name of attribute to be searched for
//
//  Returns:     P3PmsgObject
//               Located object, null object flags failed search
//                  
P3PmsgObject
P3Pmsg_FindParentWithAttr ( const P3PmsgObject& oObject, LPCTNAM lpszAttributeName )
{
    if ( !oObject.HasParent() )
      return P3PmsgObject();           // Has no parent
    P3PmsgObject oParentObject = oObject.GetParent();
    if ( oParentObject.IsAttr() )
      oParentObject = oParentObject.GetParent( );
    if ( oParentObject.IsDesc() )
      oParentObject = oParentObject.GetParent( );
    if ( !oParentObject )
      return oParentObject;
    //if ( oParentObject.IsNode() )
    //{
    //  P3PmsgNode oNodeParent = oParentObject;
    //  if ( oNodeParent.r_Attr().Exists(lpszAttributeName) )
    //    return oParentObject;
    //}
    if ( oParentObject.IsField() )
    {
      P3PmsgField oFieldParent = oParentObject;
      if ( oFieldParent.r_Attr().Exists(lpszAttributeName) )
        return oParentObject;
    }
    else if ( oParentObject.IsList() )
    {
      ASSERT(0);
      //P3PmsgList oListParent = oParentObject;
      //if ( oListParent.r_Attr().Exists(lpszAttributeName) )
      //  return oParentObject;
    }
    else if ( oParentObject.IsVect() )
    {
      ASSERT(0);
      //P3PmsgVect oVectParent = oParentObject;
      //if ( oVectParent.r_Attr().Exists(lpszAttributeName) )
      //  return oParentObject;
    }
    return P3Pmsg_FindParentWithAttr ( oParentObject, lpszAttributeName );
}

//
//  Recursively searches backwards for specified attribute
//
//  Parameters:  const P3PmsgObject& oObject
//               Search base point
//
//               LPCTSTR lpszAttributeName
//               Name of attribute to be searched for
//
//  Returns:     P3PmsgObject
//               Located attribute, null object flags failed search
//                  
P3PmsgObject
P3Pmsg_FindParentAttr ( const P3PmsgObject& oObject, LPCTNAM lpszAttributeName )
{
    // Delegate
    P3PmsgItem oParent = P3Pmsg_FindParentWithAttr ( oObject, lpszAttributeName );
    if ( !oParent.IsVoid() )
      return oParent.ATTR.SelectObject(lpszAttributeName);
    return oParent.r_Object();
}

//
//  Recursively searches forwards for specified attribute
//
//  Parameters:  const P3PmsgItem& oItem
//               Search base point
//
//               LPCTSTR lpszAttributeName
//               Name of attribute to be searched for
//
//               LPCTNAM lpszChildname
//               Matching name wildcard
//  Returns:     P3PmsgObject
//               Located attribute, null object flags failed search
//                  
P3PmsgObject
P3Pmsg_FindChildWithAttr ( const P3PmsgItem& oItem
                         , LPCTNAM lpszAttributeName, LPCTNAM lpszChildname )
{
    if ( lpszChildname == 0 || wcslen(lpszChildname) <= 0 )
      lpszChildname = nullptr;
    //  Both names are held across scans that spend a ring slot per sibling --
    //  c_wcsicmpWC() widens each sibling's stored name through
    //  p2p_wstr_from_store (Platform/p2pstr.h), and Exists() rescans the whole
    //  attribute set by name on every iteration. A caller passing some other
    //  item's c_name() would have its key recycled part-way through, and the
    //  recycled slot compares EQUAL to whatever overwrote it. Snapshotting here
    //  covers the recursion too: the grandchild pass below hands these on, so
    //  one copy at the top protects the whole subtree walk.
    //      : A P3PmsgName COPY IS NOT A SNAPSHOT. It copies the name out of the
    //        caller's storage, but c_name() returns a RING SLOT (P2Pmsg.cpp:1131),
    //        so the key goes straight back into the ring the copy exists to escape
    //        -- and the first loop below spends a slot per sibling. p2p_wkey copies
    //        into its own inline buffer instead, which is the whole point of it.
    //  NULL-NESS IS LOAD-BEARING: the normalisation above collapses an empty name to
    //  nullptr, and both name tests below read that nullptr as "no name supplied,
    //  match every sibling". P3PmsgName does not preserve it -- c_name() hands back
    //  L"" for a null-constructed name, so taking it unconditionally turned every
    //  no-name search into a wildcard match against the EMPTY STRING, which matches
    //  nothing: P3Pmsg_FindChildWithAttr(oItem,attr) then returned void for a tree
    //  that does carry the attribute. p2p_wkey passes a null key through untouched
    //  (Platform/p2pstr.h), so the snapshot and the null-ness cost one line each.
    const p2p_wkey oAttrKey  ( lpszAttributeName );
    const p2p_wkey oChildKey ( lpszChildname );
    LPCTNAM        lpszAttr  = oAttrKey;
    LPCTNAM        lpszChild = oChildKey;
    //  NAME TEST SENSE.  c_wcsicmpWC() is a PREDICATE, not a comparison:
    //  it returns MsgcoreWildcard(pattern,name) (P2Pmsg.cpp:2099-2103), so
    //  NON-ZERO means the name MATCHES. The `cmp` in the spelling reads like
    //  wcsicmp, where zero means equal, and both tests here were written that
    //  way round -- so the base item was accepted only when its name did NOT
    //  match, and the child loop `continue`d past exactly the children that DID.
    //  With a name supplied this function could not find a child by name at all:
    //  it skipped every match, fell through to the grandchild pass, and
    //  descended into leaf children. Measured, not read: a 4-child tree with the
    //  attribute on `Item03` and lpszChildname="Item03" reported cmp=1 for that
    //  child and 0 for the other three.
    //      : Callers that pass NO name (the default, and every caller in this
    //        tree) are unaffected either way -- the name test is skipped
    //        entirely for them. Only a caller supplying lpszChildname sees a
    //        behaviour change, and for those the old answer was indefensible.
    // Check passed P3PmsgItem
    if ( ( lpszChild == nullptr                 ||
           oItem.r_name().c_wcsicmpWC(lpszChild)    ) &&
         oItem.r_Attr().Exists(lpszAttr)                 )
      return oItem.r_Object();
    // Search all immediate children
    P3PmsgCurs oCurs((P3PmsgItem&)oItem);
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      if ( lpszChild                              &&
          !oCurs.r_name().c_wcsicmpWC(lpszChild)     )
        continue;
      if ( oCurs.r_attr().Exists(lpszAttr) )
        return oCurs.r_Object();
    }

    //  Search all immediate grandchildren
    //
    //  IsDescendant() as well as IsItem(). Recursing needs a cursor over the
    //  child, and P3PmsgCurs(P3PmsgItem&) takes r_Desc() of it (MsgCurs.cpp:71-77)
    //  - which a LEAF child does not have. Every leaf therefore asserted at
    //  P2Pmsg.cpp:1814 (VBLock_IsAlloc) and fell through VBLock_pName
    //  (P2PmsgVBLock.cpp:601); in a Release build that fall-through now throws
    //  (session 27), so a search that simply did not match at the first level
    //  raised rather than returning void. Reached on EVERY failed search, since
    //  that is exactly when this loop runs.
    P3PmsgObject oObject;
    for ( int i = 0; oCurs.Goto(i) && oObject.IsVoid(); i++ )
    {
      if ( oCurs.IsItem() && oCurs.r_item().IsDescendant() )
        oObject = P3Pmsg_FindChildWithAttr ( oCurs.r_item(), lpszAttr, lpszChild );
    }
//oObject.AssertValid();
    return oObject;
}

///////////////////////////////////////////////////////////////////////
//  Boolean operations

/*P3PmsgNode&
P2PmsgNode_AND ( P3PmsgNode& oItem, const P3PmsgNode& rhs )
{
    // Immediate P3PmsgData AND operation
    if ( oItem.r_data().DataType() == rhs.r_data().DataType() )
      oItem.r_data() = rhs.r_data();

    // Immediate P3PmsgAttr AND operation
    if ( !oItem.IsAttributed() || !rhs.IsAttributed() )
      P2PmsgAttr_AND ( oItem.r_Attr(), rhs.r_Attr() );

    // Recursive AND operation
    P3PmsgCurs& oCurs = ((P3PmsgNode&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
//oCurs.AssertValid();
ASSERT(oCurs.Item()==i);
      if ( oCurs.IsNode() )
      {
        P3PmsgNode&   oNodeRValue = oCurs.r_node();
        LPCTNAM    lpszNodename   = oNodeRValue.c_name();
        if ( !oItem.Exists(lpszNodename) )
          continue;
        P3PmsgObject oObjectLValue = oItem.SelectObject(lpszNodename);
        if ( !oObjectLValue.IsNode() )
          continue;
        P3PmsgNode oNodeLValue ( oObjectLValue );
        P2PmsgNode_AND ( oNodeLValue, oNodeRValue );
      }
      else if ( oCurs.IsList() )
      {
        ASSERT(0);
      }
      else if ( oCurs.IsVect() )
      {
        ASSERT(0);
      }
      else if ( oCurs.IsItem() )
      {
        LPCTNAM lpszFieldname = oCurs.r_item().c_name();
        if ( !oItem.Exists(lpszFieldname) )
          continue;
        P2PmsgField_AND ( oItem.SelectItem(lpszFieldname), oCurs.r_item() );
      }
      else
        ASSERT(0);
//oItem.AssertValid();//TODO:LJM delete, testing
    }

    // Tidy up, and
    return oItem;
}*/

P3PmsgField&
P2PmsgField_AND ( P3PmsgField& oField, const P3PmsgField& rhs )
{
    // Immediate P3PmsgData AND operation
    if ( oField.r_data().DataType() == rhs.r_data().DataType() )
      oField.r_data() = rhs.r_data();

    // Immediate P3PmsgAttr AND operation
    if ( oField.IsAttributed() && rhs.IsAttributed() )
      P2PmsgAttr_AND ( oField.r_Attr(), rhs.r_Attr() );

    // Immediate P3PmsgDesc AND operation
    if ( oField.IsDescendant() && rhs.IsDescendant() )
      P2PmsgDesc_AND ( oField.r_Desc(), rhs.r_Desc() );

    // Tidy up, and
    return oField;
}

P3PmsgAttr&
P2PmsgAttr_AND ( P3PmsgAttr& oAttr, const P3PmsgAttr& rhs )
{
    // Recursive copy
    P3PmsgCurs& oCurs = ((P3PmsgAttr&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
//oCurs.AssertValid();
ASSERT(oCurs.Item()==i);
      //if ( oCurs.IsNode() )
      //{
      //  P3PmsgNode&   oNodeRValue = oCurs.r_node();
      //  LPCTNAM    lpszNodename   = oNodeRValue.c_name();
      //  if ( !oAttrLHS.Exists(lpszNodename) )
      //    continue;
      //  P3PmsgObject oObjectLValue = oAttrLHS.SelectObject(lpszNodename);
      //  if ( !oObjectLValue.IsNode() )
      //    continue;
      //  P3PmsgNode oNodeLValue ( oObjectLValue );
      //  P2PmsgNode_AND ( oNodeLValue, oNodeRValue );
      //}
      if ( oCurs.IsList() )
      {
        ASSERT(0);
        oAttr += oCurs.r_list();
      }
      else if ( oCurs.IsVect() )
      {
        ASSERT(0);
        oAttr += oCurs.r_vect();
      }
      else if ( oCurs.IsItem() )
      {
        //  COPY - see the P3PmsgCurs widening-ring note in P2PmsgAttr_Merge above.
        CString strFieldname  = oCurs.r_item().c_name();
        LPCTNAM lpszFieldname = strFieldname;
        if ( !oAttr.Exists(lpszFieldname) )
          continue;
        P2PmsgField_AND ( oAttr.SelectItem(lpszFieldname), oCurs.r_item() );
      }
      else
        ASSERT(0);
//oAttrLHS.AssertValid();//TODO:LJM delete, testing
    }

    // Tidy up, and
    return oAttr;
}

P3PmsgDesc&
P2PmsgDesc_AND ( P3PmsgDesc& oDesc, const P3PmsgDesc& rhs )
{
    // Recursive copy
    P3PmsgCurs& oCurs = ((P3PmsgDesc&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
//oCurs.AssertValid();
ASSERT(oCurs.Item()==i);
      //if ( oCurs.IsNode() )
      //{
      //  P3PmsgNode&   oNodeRValue = oCurs.r_node();
      //  LPCTNAM    lpszNodename   = oNodeRValue.c_name();
      //  if ( !oDesc.Exists(lpszNodename) )
      //    continue;
      //  P3PmsgObject oObjectLValue = oDesc.SelectObject(lpszNodename);
      //  if ( !oObjectLValue.IsNode() )
      //    continue;
      //  P3PmsgNode oNodeLValue ( oObjectLValue );
      //  P2PmsgNode_AND ( oNodeLValue, oNodeRValue );
      //}
      if ( oCurs.IsList() )
      {
        ASSERT(0);
        oDesc += oCurs.r_list();
      }
      else if ( oCurs.IsVect() )
      {
        ASSERT(0);
        oDesc += oCurs.r_vect();
      }
      else if ( oCurs.IsItem() )
      {
        //  COPY - see the P3PmsgCurs widening-ring note in P2PmsgAttr_Merge above.
        CString strFieldname  = oCurs.r_item().c_name();
        LPCTNAM lpszFieldname = strFieldname;
        if ( !oDesc.Exists(lpszFieldname) )
          continue;
        P2PmsgField_AND ( oDesc.SelectItem(lpszFieldname), oCurs.r_item() );
      }
      else
        ASSERT(0);
//oAttrLHS.AssertValid();//TODO:LJM delete, testing
    }

    // Tidy up, and
    return oDesc;
}

//
//  P3PmsgDesc additions
//  NOTES: May result in duplicate enteries
//
//  Parameters:  P3PmsgDesc& oDesc
//               Destination descendant.  r_Name(), r_Data() and r_Attr()
//               contents not copied.
//
//               const P3PmsgDesc& rhs
//               Source descendant to be copied in full
P3PmsgDesc&
P2PmsgDesc_ADD ( P3PmsgDesc& oDesc, const P3PmsgDesc& rhs )
{
    // Recursive addition
    P3PmsgCurs& oCurs = ((P3PmsgDesc&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
//oCurs.AssertValid();
ASSERT(oCurs.Item()==i);
      //if ( oCurs.IsNode() )
      //{
      //  P3PmsgNode&   oNodeRValue = oCurs.r_node();
      //  LPCTNAM    lpszNodename   = oNodeRValue.c_name();
      //  if ( !oDesc.Exists(lpszNodename) )
      //    continue;
      //  P3PmsgObject oObjectLValue = oDesc.SelectObject(lpszNodename);
      //  if ( !oObjectLValue.IsNode() )
      //    continue;
      //  P3PmsgNode oNodeLValue ( oObjectLValue );
      //  P2PmsgNode_AND ( oNodeLValue, oNodeRValue );
      //}
      if ( oCurs.IsList() )
      {
        ASSERT(0);
        oDesc += oCurs.r_list();
      }
      else if ( oCurs.IsVect() )
      {
        ASSERT(0);
        oDesc += oCurs.r_vect();
      }
      else if ( oCurs.IsItem() )
      {
        oDesc += oCurs.r_item();
      }
      else
        ASSERT(0);
//oAttrLHS.AssertValid();//TODO:LJM delete, testing
    }

    // Tidy up, and
    return oDesc;
}

CString bytetohex(byte item)
{
CString result;
result.Format(_T("%02.2x"),item);
return result.Right(2);
}

void
GuidToString ( GUID& oGUID, LPTSTR lpszGUID )
{
    byte *pbyGUID = (byte *)&oGUID;

    lpszGUID [0] = _T('0') +  pbyGUID [3] / 16;
    lpszGUID [1] = _T('0') +  pbyGUID [3] % 16;
    lpszGUID [2] = _T('0') +  pbyGUID [2] / 16;
    lpszGUID [3] = _T('0') +  pbyGUID [2] % 16;
    lpszGUID [4] = _T('0') +  pbyGUID [1] / 16;
    lpszGUID [5] = _T('0') +  pbyGUID [1] % 16;
    lpszGUID [6] = _T('0') +  pbyGUID [0] / 16;
    lpszGUID [7] = _T('0') +  pbyGUID [0] % 16;
    lpszGUID [8] = _T('-');

    lpszGUID [9] = _T('0') +  pbyGUID [5] / 16;
    lpszGUID[10] = _T('0') +  pbyGUID [5] % 16;
    lpszGUID[11] = _T('0') +  pbyGUID [4] / 16;
    lpszGUID[12] = _T('0') +  pbyGUID [4] % 16;
    lpszGUID[13] = _T('-');

    lpszGUID[14] = _T('0') +  pbyGUID [7] / 16;
    lpszGUID[15] = _T('0') +  pbyGUID [7] % 16;
    lpszGUID[16] = _T('0') +  pbyGUID [6] / 16;
    lpszGUID[17] = _T('0') +  pbyGUID [6] % 16;
    lpszGUID[18] = _T('-');

    lpszGUID[19] = _T('0') +  pbyGUID [8] / 16;
    lpszGUID[20] = _T('0') +  pbyGUID [8] % 16;
    lpszGUID[21] = _T('0') +  pbyGUID [9] / 16;
    lpszGUID[22] = _T('0') +  pbyGUID [9] % 16;
    lpszGUID[23] = _T('-');

    lpszGUID[24] = _T('0') +  pbyGUID[10] / 16;
    lpszGUID[25] = _T('0') +  pbyGUID[10] % 16;
    lpszGUID[26] = _T('0') +  pbyGUID[11] / 16;
    lpszGUID[27] = _T('0') +  pbyGUID[11] % 16;
    lpszGUID[28] = _T('0') +  pbyGUID[12] / 16;
    lpszGUID[29] = _T('0') +  pbyGUID[12] % 16;
    lpszGUID[30] = _T('0') +  pbyGUID[13] / 16;
    lpszGUID[31] = _T('0') +  pbyGUID[13] % 16;
    lpszGUID[32] = _T('0') +  pbyGUID[14] / 16;
    lpszGUID[33] = _T('0') +  pbyGUID[14] % 16;
    lpszGUID[34] = _T('0') +  pbyGUID[15] / 16;
    lpszGUID[35] = _T('0') +  pbyGUID[15] % 16;
    lpszGUID[36] =     0;
}

///////////////////////////////////////////////////////////////////////
//  Movement operations
//  NOTES: Move P3PmsgObject from source to destination.  Effectively
//         cut and paste operations

//
//  Performs P3PmsgItem upgrade type move
//  NOTES: Move negated if destination item already exists
//       : Source item deleted after move sequence
//
//  Parameters:  LPCTSTR lpszItemname
//               Name of item to be moved
//
//               P3PmsgItem& oItemSource
//               Move source
//
//               P3PmsgAttr& oAttrDestin
//               Move destination
BOOL
P2Pmsg_UpgradeMove ( LPCTSTR lpszItemname, P3PmsgItem& oItemSource, P3PmsgAttr& oAttrDestin )
{
    if ( !oItemSource.Exists(lpszItemname) )
      return FALSE;
    P3PmsgItem oItemMove = oItemSource.SelectItem(lpszItemname).r_Object();
    if ( !oAttrDestin.Exists(lpszItemname) )
      oAttrDestin += oItemMove;
    oItemSource.r_Desc().Delete ( oItemMove );
    return TRUE;
}
