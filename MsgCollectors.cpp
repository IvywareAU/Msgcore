// Copyright © 2006-2009, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2Peer templates and container implementations
//
#include "stdafx.h"

#include "MsgCollectors.h"

///////////////////////////////////////////////////////////////////////
//  P2Peer safe critical section container
//  NOTES: Allocated critical section is managed within the life cycle
//         of this object.

/*P2PsafeCS::P2PsafeCS ( )
{   RenderThisSafe(); }
P2PsafeCS::P2PsafeCS ( CRITICAL_SECTION& oCSection )
{   RenderThisSafe();
    *this = oCSection; }
P2PsafeCS::~P2PsafeCS ( )
{   if ( m_pCSection )
      LeaveCriticalSection ( m_pCSection ); }
void
P2PsafeCS::RenderThisSafe ( )
{   m_pCSection = 0; }
P2PsafeCS&
P2PsafeCS::operator = ( CRITICAL_SECTION& rhs )
{   EnterCriticalSection ( &rhs );
    if ( m_pCSection )
      LeaveCriticalSection ( m_pCSection );
    m_pCSection = &rhs;
    return *this; }*/
