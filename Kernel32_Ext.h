// Copyright © 2002-2009, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  Definitions and prototypes for Microsoft Kernel32 C++ extensions
//
#pragma once
#include "Msgcore.h"

//
//  Stack based safe CRITICAL_SECTION container
//  NOTES: Allocated CRITICAL_SECTION is managed within the life cycle
//         of this object.
class Msgcore_EXT P2PsafeCS
{
    public:
        P2PsafeCS ( );
        P2PsafeCS ( CRITICAL_SECTION& oCSection);
       ~P2PsafeCS ( );
    public:
      P2PsafeCS&
        operator = ( CRITICAL_SECTION& oCSection );
    private:
        CRITICAL_SECTION *m_pCSection{nullptr};
};

//
//  Stack based safe HANDLE container
//  NOTES: Allocated HANDLE is managed within the life cycle
//         of this object.
class Msgcore_EXT P2PsafeHANDLE
{
    public:
        P2PsafeHANDLE ( ) noexcept;
        P2PsafeHANDLE ( HANDLE oHandle ) noexcept;
       ~P2PsafeHANDLE ( );
    public:
      P2PsafeHANDLE&
        operator = ( HANDLE oHandle ) noexcept;
        operator HANDLE( ) noexcept;
      HANDLE
        Dereference() noexcept;
    private:
        HANDLE  m_oHandle;
};

//
//  Stack based safe HGLOBAL container
//  NOTES: Allocated HGLOBAL is managed within the life cycle
//         of this object.
class Msgcore_EXT P2PsafeHGLOBAL
{
    public:
        P2PsafeHGLOBAL ( );
        P2PsafeHGLOBAL ( HGLOBAL oHGLOBAL );
       ~P2PsafeHGLOBAL ( );
    public:
      P2PsafeHGLOBAL&
        operator = ( HGLOBAL oHGLOBAL );
        operator HGLOBAL( );
      HGLOBAL
        Dereference() noexcept;
    private:
        HGLOBAL  m_oHGLOBAL;
};

//
//  Stack based safe DWORD_PTR global
//  NOTES: Sets global value and manages restoration of existing value
//         on the stack
//       : Sometime global values are set to manage behaviour which particular
//         sequences are in operation and then restored immediately after.
//         This stack based object can be used to make this exception safe.
//       : Intended to be transitory operation.
class Msgcore_EXT P2PsafeGlobal
{
    public:
        P2PsafeGlobal ( DWORD_PTR *pdwGlobal, DWORD_PTR dwGlobalSet );
       ~P2PsafeGlobal ( );
    private:
        DWORD_PTR *m_pdwGlobal{nullptr};
        DWORD_PTR  m_dwGlobalRestore{0};
};
