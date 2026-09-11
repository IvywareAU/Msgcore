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

        //  NOTES: The guard tests &m_nRefCount -- the address of the COUNT --
        //         because that is the sentinel every other member sets and
        //         reads. It said &m_pRefCount, the address of the POINTER, from
        //         b97bea3 "Initial Code" until this was fixed: a different
        //         address and a different type (int* against int* const*), so
        //         MSVC rejected it with C2446 and g++ with "comparison between
        //         distinct pointer types". Calling the then non-const
        //         SwapRef2Shared() through this const reference was the second
        //         error (C2662). Both are hard errors on both toolchains, so
        //         this constructor had never been instantiated and the template
        //         was accidentally non-copyable -- which is why nothing ever
        //         reported it.
        //       : Casting the mismatch away instead of correcting the operand
        //         compiles silently and is worse than the error: the guard is
        //         then permanently false, the count is never migrated, and the
        //         copy's m_pRefCount points into the SOURCE object. Outliving
        //         the source reads freed storage, and reaching zero there hands
        //         Delete() a pointer that was never new'd.
        //       : AN EMPTY SOURCE HAS NOTHING TO SHARE, which is the whole of
        //         the first guard below. Without it SwapRef2Shared() migrated a
        //         count of ZERO to the heap and this constructor then raised it
        //         to one, for a pointee that does not exist: the copy's
        //         destructor asserted, freed the count, and the SOURCE's
        //         destructor then read the freed int. Two objects, two asserts
        //         and a use-after-free, reachable only since this constructor
        //         began to compile at all.
        P2PSafePtr ( const P2PSafePtr& rSafePtr )
        {
          m_pSafePtrType = rSafePtr.m_pSafePtrType;
          if ( m_pSafePtrType == NULL )
          {
            m_pRefCount = &m_nRefCount;
            m_nRefCount = 0;
            return;
          }
          if ( rSafePtr.m_pRefCount == &rSafePtr.m_nRefCount )
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
        operator*()  const { return *m_pSafePtrType; }

      //  IMPLICIT, and measured that way rather than assumed. Deleting it and
      //  compiling the tree named eight call sites that hand a safe pointer
      //  straight to a function taking a raw one -- PostP2PeerMsg(spMsg),
      //  RemoveP2PmsgPump(spPump) and their kind -- so this conversion is the
      //  idiom the library is written in and not an accident. Const because
      //  reading the pointer out does not change who holds it.
        operator SafePtrType*() const { return m_pSafePtrType; }

      //  EXPLICIT, because the two conversions together answered questions
      //  nobody asked and refused the one this class is for. `int n = sp`
      //  compiled, through bool; `sp == sp` did NOT -- C2593, the two of them
      //  being equally good ways to reach a built-in ==. Explicit keeps the
      //  CONTEXTUAL conversions, which are the ones anybody wants: `if ( sp )`,
      //  `!sp`, `sp && x`, `sp ? a : b` and static_cast<bool>.
      //  NOTES: IT DOES NOT MAKE THIS A BOOL-FREE TYPE, and the table in §25
      //         says so: a SafePtrType* converts to bool on its own, so
      //         `bool b = sp` and `return sp` from a bool function still
      //         compile and always did. What explicit closes is the ARITHMETIC
      //         reading -- int, and the built-in operators reached through it.
      //       : Nothing in the built tree used this conversion at all. Deleting
      //         it outright and compiling nine solutions in both configurations
      //         gave 0 errors, which is why making it explicit costs nothing.
        explicit operator bool () const { return m_pSafePtrType ? true : false; }

      //  Do we point at the same thing?
      //  NOTES: The comparison this template always meant to offer, and the
      //         one expression involving it that did not compile. It is
      //         identity of the POINTEE, which is the only thing two safe
      //         pointers can sensibly be asked: each keeps its own count, and
      //         two counts say nothing about what is being counted.
      //       : The raw-pointer arm is not a convenience. Without it
      //         `sp == pThing` would build a TEMPORARY safe pointer around
      //         pThing through the implicit constructor above, and that
      //         temporary DELETES what it was handed when the comparison ends.
      bool
        operator == ( const P2PSafePtr& rSafePtr ) const
        { return m_pSafePtrType == rSafePtr.m_pSafePtrType; }
      bool
        operator != ( const P2PSafePtr& rSafePtr ) const
        { return m_pSafePtrType != rSafePtr.m_pSafePtrType; }
      bool
        operator == ( SafePtrType *pSafePtrType ) const
        { return m_pSafePtrType == pSafePtrType; }
      bool
        operator != ( SafePtrType *pSafePtrType ) const
        { return m_pSafePtrType != pSafePtrType; }

      //  NOTES: CONST, and that is a defect fix rather than tidying. Taking the
      //         source by NON-const reference meant a temporary could not bind
      //         here, so `sp = MakeSP()` fell through to the raw-pointer
      //         overload below by way of operator SafePtrType*(). That arm
      //         stamps a fresh count of ONE and knows nothing of the count the
      //         temporary is still holding, so the temporary's destructor freed
      //         the pointee and left the assignee addressing it -- a
      //         use-after-free, with a second delete behind it. Measured either
      //         side of this line.
      //       : A raw pointer still selects the overload below, an exact match
      //         beating a user-defined conversion, so `spMsg = Factory()` and
      //         `spPump = 0` take ownership the way they always did.
      //       : The empty-source guard is the copy constructor's, for the
      //         reason given there.
      P2PSafePtr&
        operator = ( const P2PSafePtr& rSafePtr )
        {
          if ( this == &rSafePtr )
            return *this;
          Delete();
          m_pSafePtrType = rSafePtr.m_pSafePtrType;
          if ( m_pSafePtrType == NULL )
          {
            m_pRefCount = &m_nRefCount;
            m_nRefCount = 0;
            return *this;
          }
          rSafePtr.SwapRef2Shared ( );
          m_pRefCount    = rSafePtr.m_pRefCount;
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
        IsEmpty () const
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
      //  const, and the two count members are mutable with it: migrating the
      //  count to the heap is a representation change, not a value change, and
      //  the copy constructor's source is a const reference.
      void
        SwapRef2Shared ( ) const
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
        mutable int *m_pRefCount;
        mutable int  m_nRefCount;
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
