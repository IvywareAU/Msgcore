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
//  P2Peer template library
//
#pragma once
#ifndef NO_DEBUG_NEW
#define new DEBUG_NEW
#endif
#include "Msgcore.h"

//
//  P2Peer library Safe Pointer template
//  NOTES: Primarily used to recover memory from stack based pointers
//         when exceptions are thrown
//       : Designed for use within the P2Peer environment and not
//         to conflict with alternatives
//
template<class SafePtrType>
class P2PSafePtr
{
    // Constructors and destruction
    public:
        P2PSafePtr ()
        {
          m_pSafePtrType =  0;
          m_pRefCount    = &m_nRefCount;
          m_nRefCount    =  0;
        }

        P2PSafePtr ( SafePtrType *pSafePtrType )
        {
          if ( pSafePtrType )
          {
            m_pSafePtrType =  pSafePtrType;
            m_pRefCount    = &m_nRefCount;;
            m_nRefCount    =  1;
          }
          else
          {
            m_pSafePtrType =  0;
            m_pRefCount    = &m_nRefCount;
            m_nRefCount    =  0;
          }
        }

        P2PSafePtr ( const P2PSafePtr& rSafePtr )
        {
          m_pSafePtrType = rSafePtr.m_pSafePtrType;
          if ( rSafePtr.m_pRefCount == &rSafePtr.m_pRefCount )
            rSafePtr.SwapRef2Shared ( );
          m_pRefCount    = rSafePtr.m_pRefCount;
        (*m_pRefCount)++;
        }

       ~P2PSafePtr ( ) { Delete(); }

    // Overloaded operators
    public:
      SafePtrType*
        operator->() const { return  m_pSafePtrType; }
      SafePtrType&
        operator*()  { return *m_pSafePtrType; }
      
        operator SafePtrType*() { return m_pSafePtrType; }

        operator bool () { return m_pSafePtrType ? true : false; }

      P2PSafePtr&
        operator = ( P2PSafePtr& rSafePtr )
        {
          if ( this == &rSafePtr )
            return *this;
          Delete();
          rSafePtr.SwapRef2Shared ( );
          m_pSafePtrType = rSafePtr.m_pSafePtrType;
          m_pRefCount    = rSafePtr.m_pRefCount;
          if ( m_pSafePtrType )
            (*m_pRefCount)++;
          return *this;
        }

      P2PSafePtr&
        operator = ( SafePtrType *pSafePtrType )
        {
          if ( m_pSafePtrType == pSafePtrType )
            return *this;
          Delete();
          if ( pSafePtrType == NULL )
            return *this;
          m_pSafePtrType =  pSafePtrType;
          m_pRefCount    = &m_nRefCount;
         *m_pRefCount    =  1;
          return *this;
        }

    // Operations
    public:
      SafePtrType*
        Dereference ()
        {
          SafePtrType *pSafePtrType = m_pSafePtrType;
          if ( m_pRefCount != &m_nRefCount )
            delete m_pRefCount;
                   m_pRefCount = NULL;
          m_pSafePtrType = 0;
          return pSafePtrType;
        }
      bool
        IsEmpty ()
        {
          return m_pSafePtrType == NULL ? true : false;
        }

    // Utilities
    private:
      void
        Delete ()
        {
          if (  m_pRefCount == NULL ||
               *m_pRefCount == NULL    )
          {
            ASSERT(m_pSafePtrType == NULL);
            return;
          }
          ASSERT(m_pSafePtrType != NULL);
          if ( --(*m_pRefCount) == 0 )
          {
            if ( m_pRefCount != &m_nRefCount )
              delete m_pRefCount;
                     m_pRefCount = NULL;
            delete m_pSafePtrType;
                   m_pSafePtrType  = NULL;
          }
        }
      void
        SwapRef2Shared ( )
        {
          if ( m_pRefCount == &m_nRefCount )
          {
            m_pRefCount = new int;
           *m_pRefCount = m_nRefCount;
            m_nRefCount = 0;
          }
        }

    // Attributes
    private:
        SafePtrType *m_pSafePtrType;
        int         *m_pRefCount;
        int          m_nRefCount;
};

//
//  Stack based P2Peer library Safe Lock template
//  NOTES: Primarily for use with objects supporting Lock() and
//         Unlock() methods.  Usually resides on the stack.
//
template<class SafeLockType>
class P2PSafeLock
{
    // Constructors and destruction
    public:
        P2PSafeLock ()
        {
          m_pSafeLockType =  0;
        }

        P2PSafeLock ( SafeLockType& oSafeLockType )
        {
          m_pSafeLockType =  &oSafeLockType;
          if ( m_pSafeLockType )
            m_pSafeLockType -> Lock ( );
        }

       ~P2PSafeLock ( ) { Unlock(); }

    // Overloaded operators
    public:
      P2PSafeLock&
        operator = ( SafeLockType *pSafeLockType )
        {
          if ( m_pSafeLockType == pSafeLockType )
            return *this;
          Unlock();
          if ( pSafeLockType == NULL )
            return *this;
          m_pSafeLockType =  pSafeLockType;
          return *this;
        }

    // Operations
    public:
      bool
        IsEmpty ()
        {
          return m_pSafeLockType == NULL ? true : false;
        }

    // Utilities
    private:
      void
        Unlock ()
        {
          if ( m_pSafeLockType )
            m_pSafeLockType -> Unlock();
          m_pSafeLockType = 0;
        }

    // Attributes
    private:
        SafeLockType *m_pSafeLockType;
};
