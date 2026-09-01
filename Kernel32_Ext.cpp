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
//  Implementations for Microsoft Kernel32 C++ extensions
//
#include "stdafx.h"

#include "Kernel32_Ext.h"

///////////////////////////////////////////////////////////////////////
//  Safe CRITICAL_SECTION container
//  NOTES: Allocated CRITICAL_SECTION is managed within the life cycle
//         of this object.

P2PsafeCS::P2PsafeCS ( )
{   
}
P2PsafeCS::P2PsafeCS ( CRITICAL_SECTION& rhs )
{
    EnterCriticalSection ( &rhs );   
    m_pCSection = &rhs;
}
P2PsafeCS::~P2PsafeCS ( )
{   
    if ( m_pCSection )
      LeaveCriticalSection ( m_pCSection );
}
P2PsafeCS&
P2PsafeCS::operator = ( CRITICAL_SECTION& rhs )
{   
    EnterCriticalSection ( &rhs );
    if ( m_pCSection )
      LeaveCriticalSection ( m_pCSection );
    m_pCSection = &rhs;
    return *this;
}

///////////////////////////////////////////////////////////////////////
//  Safe HANDLE implementation
//  NOTES: Assigned HANDLE is managed within the life cycle of this
//         object.
//
P2PsafeHANDLE::P2PsafeHANDLE ( ) noexcept
{   
    m_oHandle = INVALID_HANDLE_VALUE;
}
P2PsafeHANDLE::P2PsafeHANDLE ( HANDLE oHandle ) noexcept
{
    m_oHandle = oHandle;
}
P2PsafeHANDLE::~P2PsafeHANDLE ( )
{   
    if ( m_oHandle != INVALID_HANDLE_VALUE )
      CloseHandle ( m_oHandle );
}
P2PsafeHANDLE&
P2PsafeHANDLE::operator = ( HANDLE oHandle ) noexcept
{
    if ( m_oHandle != oHandle              &&
         m_oHandle != INVALID_HANDLE_VALUE    )
      CloseHandle ( m_oHandle );
    m_oHandle = oHandle;
    return *this;
}
P2PsafeHANDLE::operator HANDLE ( ) noexcept
{   
    return m_oHandle;
}
HANDLE
P2PsafeHANDLE::Dereference() noexcept
{   
    HANDLE oHandle = m_oHandle;
    m_oHandle = INVALID_HANDLE_VALUE;
    return oHandle;
}


///////////////////////////////////////////////////////////////////////
//  Safe HGLOBAL implementation
//  NOTES: Assigned HGLOBAL is managed within the life cycle of this
//         object.
//
P2PsafeHGLOBAL::P2PsafeHGLOBAL ( )
{   
    m_oHGLOBAL = nullptr;
}
P2PsafeHGLOBAL::P2PsafeHGLOBAL ( HGLOBAL oHGLOBAL )
{
    m_oHGLOBAL = oHGLOBAL;
    if ( m_oHGLOBAL )
      GlobalLock(m_oHGLOBAL);
}
P2PsafeHGLOBAL::~P2PsafeHGLOBAL ( )
{   
    if ( m_oHGLOBAL != nullptr )
      GlobalUnlock ( m_oHGLOBAL );
}
P2PsafeHGLOBAL&
P2PsafeHGLOBAL::operator = ( HGLOBAL oHGLOBAL )
{
    if ( oHGLOBAL == m_oHGLOBAL )
      return *this;
    if ( m_oHGLOBAL != oHGLOBAL &&
         m_oHGLOBAL != nullptr     )
      GlobalUnlock ( m_oHGLOBAL );
    m_oHGLOBAL = oHGLOBAL;
    if ( m_oHGLOBAL != nullptr )
      GlobalLock ( m_oHGLOBAL );
    return *this;
}
P2PsafeHGLOBAL::operator HGLOBAL ( )
{   
    return m_oHGLOBAL;
}
HGLOBAL
P2PsafeHGLOBAL::Dereference() noexcept
{   
    HGLOBAL oHGLOBAL = m_oHGLOBAL;
    m_oHGLOBAL = nullptr;
    return oHGLOBAL;
}

//
//  Stack based safe DWORD_PTR global
//  NOTES: Sets global value and manages restoration of existing value
//         on the stack
//       : Sometime global values are set to manage behaviour which particular
//         sequences are in operation and then restored immediately after.
//         This stack based object can be used to make this exception safe.
//       : Intended to be transitory operation.
P2PsafeGlobal::P2PsafeGlobal ( DWORD_PTR *pdwGlobal, DWORD_PTR dwGlobalSet )
{
    m_pdwGlobal       =  pdwGlobal;
    m_dwGlobalRestore = *pdwGlobal;
   *m_pdwGlobal       =  dwGlobalSet;
}
P2PsafeGlobal::~P2PsafeGlobal ( )
{
    if ( m_pdwGlobal ) 
     *m_pdwGlobal = m_dwGlobalRestore;
}
