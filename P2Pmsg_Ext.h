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
//
//  P2Pmsg extension definitions and prototypes
//
#pragma once

#ifndef NO_DEBUG_NEW
#define new DEBUG_NEW
#endif

#include "MsgCollectors.h"
#include "P2Pmsg.h"
#include "MsgAttr.h"
#include "MsgDesc.h"
#include "MsgStck.h"
#include "MsgCurs.h"


///////////////////////////////////////////////////////////////////////
//  P2Pmsg serialisation helpers
//  NOTES: Perform standard activities
//Msgcore_EXT P3PmsgNode&
//P3PmsgField_SERIALISE ( P3PmsgNode& oNode
//                      , LPCTSTR lpszFieldname, const P3PmsgData& oData
//                      , bool bDscAttr, LPCTSTR lpszDescription );
//
//Msgcore_EXT P3PmsgAttr&
//P2PmsgAttr_SERIALISE  ( P3PmsgAttr& oAttr, bool bOverwrite
//                      , LPCTSTR lpszFieldname, const P3PmsgData& oData
//                      , bool bDscAttr = false, LPCTSTR lpszDescription = 0 );

///////////////////////////////////////////////////////////////////////
//  P2Pmsg CMFCPropertyGridCtrl decoration helpers
//  NOTES: Perform standard activities
#define T_4GRID_TYPE_Lab        L"#Lab"
#define T_4GRID_TYPE_Dsc        L"#Dsc"
#define T_4GRID_TYPE_Typ        L"#Typ"
#define T_4GRID_TYPE_SPIN       L"SPIN"
#define T_4GRID_TYPE_FONT       L"FONT"
#define T_4GRID_TYPE_FONTcrx    L"FONTcrx"
#define T_4GRID_TYPE_COLOR      L"COLOR"
#define T_4GRID_TYPE_FILE       L"FILE"
#define T_4GRID_TYPE_OPTION     L"OPTION"
#define T_4GRID_TYPE_HEADING    L"HEADING"
#define T_4GRID_TYPE_ENUM       L"ENUM"

//  Grid decorations
const DWORD GRIDEC_Heading    = (1<<0);     // Decorates primary heading
const DWORD GRIDEC_Subheading = (1<<1);     // Decorate entries with sub-heading
const DWORD GRIDEC_Attribute  = (1<<2);     // Attribute of passed grid property
const DWORD GRIDEC_Append     = (1<<3);     // Append to passed properties

Msgcore_EXT P3PmsgField&
Decorate4Grid ( P3PmsgField& oField, UCHAR ucAttributes
              , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription );
//Msgcore_EXT P3PmsgNode&
//Decorate4Grid_HEADING ( P3PmsgNode& oNode, UCHAR ucAttributes
//                      , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription );
Msgcore_EXT P3PmsgField&
Decorate4Grid_HEADING ( P3PmsgField& oField, UCHAR ucAttributes
                      , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription );
Msgcore_EXT P3PmsgField&
Decorate4Grid_FONT ( P3PmsgField& oField, UCHAR ucAttributes
                   , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription );
Msgcore_EXT P3PmsgField&
Decorate4Grid_FONTcrx ( P3PmsgField& oField, UCHAR ucAttributes
                      , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription );
Msgcore_EXT P3PmsgField&
Decorate4Grid_COLOR ( P3PmsgField& oField, UCHAR ucAttributes
                    , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription );
Msgcore_EXT P3PmsgField&
Decorate4Grid_FILE ( P3PmsgField& oField, UCHAR ucAttributes
                   , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription );
Msgcore_EXT P3PmsgField&
Decorate4Grid_OPTION ( P3PmsgField& oField, UCHAR ucAttributes
                     , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription
                     , LPCTSTR lpszGridOptions );
Msgcore_EXT P3PmsgField&
Decorate4Grid_ENUM    ( P3PmsgField& oField, UCHAR ucAttributes
                      , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription
                      , LPCTSTR lpszGridEnumOptions );

///////////////////////////////////////////////////////////////////////
//  P2Pmsg object helpers

Msgcore_EXT CString
P3Pmsg_GetPath ( const P3PmsgField *pField );
Msgcore_EXT CString
P3Pmsg_GetPath ( const P3PmsgAttr *pAttr );
//Msgcore_EXT CString
//P3Pmsg_GetParentName ( const P3PmsgNode *pNode );
Msgcore_EXT P3PmsgObject
P3Pmsg_FindParentWithAttr ( const P3PmsgObject& oObject, LPCTSTR lpszAttributeName );
Msgcore_EXT P3PmsgObject
P3Pmsg_FindParentAttr ( const P3PmsgObject& oObject, LPCTSTR lpszAttributeName );
Msgcore_EXT P3PmsgObject
P3Pmsg_FindChildWithAttr ( const P3PmsgItem& oItem
                         , LPCTSTR lpszAttributeName, LPCTSTR lpszObjectName = nullptr );

Msgcore_EXT P3PmsgObject
P3Pmsg_SelectObject ( const P3PmsgObject *pObject, LPCTSTR lpszObjectPath );

///////////////////////////////////////////////////////////////////////
//  Boolean operations

Msgcore_EXT P3PmsgField&
P2PmsgField_AND ( P3PmsgField& oFieldLValue, const P3PmsgField& oFieldRValue );
Msgcore_EXT P3PmsgAttr&
P2PmsgAttr_AND ( P3PmsgAttr& oAttrLValue, const P3PmsgAttr& oAttrRValue );
Msgcore_EXT P3PmsgDesc&
P2PmsgDesc_AND ( P3PmsgDesc& oDescLValue, const P3PmsgDesc& oDescRValue );

Msgcore_EXT P3PmsgDesc&
P2PmsgDesc_ADD ( P3PmsgDesc& oDescLValue, const P3PmsgDesc& oDescRValue );

///////////////////////////////////////////////////////////////////////
//  Merge operations
//  NOTES: 
Msgcore_EXT P3PmsgAttr&
P2PmsgAttr_AbsoluteMerge ( P3PmsgAttr& oAttrLValue, const P3PmsgAttr& oAttrRValue );

Msgcore_EXT void
GuidToString ( GUID& oGUID, LPTSTR lpszGUID );

///////////////////////////////////////////////////////////////////////
//  Touch management

Msgcore_EXT void
P2Pmsg_Touch ( P3PmsgField& oField, bool bTouch, bool bRecurse = false );
Msgcore_EXT void
P2Pmsg_UntouchedDelete ( P3PmsgField& oField, bool bRecurse = false );

///////////////////////////////////////////////////////////////////////
//  Movement operations
//  NOTES: Move P3PmsgObject from source to destination.  Effectively
//         cut and paste operations

Msgcore_EXT BOOL
P2Pmsg_UpgradeMove ( LPCTSTR lpszItemname, P3PmsgItem& oItemSource, P3PmsgAttr& oAttrDestin );

