# The `^` stack path operator

> Status: current as of 2026-09-11. Describes `T_StckDelim` — the third path delimiter —
> what it was for, why no path could use it, and what it resolves to now. Every listing and
> every line of output below was run against this tree; §6, §7 and §10 say how to
> reproduce them. §8 is a heap finding that is not about `^` at all.

## 1. Summary

A Msgcore object path has three delimiters, declared together in `P2Pmsg.h:76-79`:

```cpp
#define T_RootDelim  L'.'
#define T_DescDelim  L'.'     // descendants:  Item.Child
#define T_AttrDelim  L'@'     // attributes:   Item@Tag
#define T_StckDelim  L'^'     // the stack:    Item^        <-- this one
```

Every item carries a stack. `MsgStck::Push()` copies the whole item — name, data,
attributes and descendants — into a fresh `VBLockItem` and links it onto the item's
`aStack` address; `Pop()` copies it back and drops the copy. Pushes nest. That machinery
has always worked, and §2 shows it working.

What did not work was naming a pushed value **from a path**. `^` was parsed everywhere and
followed nowhere:

| Component | `^` was | Consequence |
|---|---|---|
| `ParseObjectPath` (`P2Pmsg.cpp:6761`) | a terminator, correctly | — |
| `P3Pmsg_IsPathDelimiter` (`P2Pmsg.cpp:7053`) | TRUE, correctly | — |
| `P3Pmsg_IsValidItemname` (`P2Pmsg.cpp:7006`) | refused inside a name, correctly | — |
| `P3Pmsg_SelectObjectRecurse`, field arm | `ASSERT(0); return P3PmsgObject();` | **a debug build asserted; a release build answered nothing** |
| `P3Pmsg_SelectObjectRecurse`, descendant arm | tested `T_StckDelim` where it meant `T_DescDelim` | `Desc^name` descended, `Desc.name` matched nothing |
| `P3Pmsg_SelectObjectRecurse`, list arm | `ASSERT(0)`, and nothing else | **no component of any kind resolved against a list** |
| `P3Pmsg_SelectObjectRecurse`, vectors | no arm at all | a vector fell to the `ASSERT(0)` closing the function |
| `P3Pmsg_SelectObject`, leading component | `IsField()`-only, then asserted and recursed anyway | a rooted path was never checked against the list it started at |
| `MsgStck::Push` / `Pop`, list and vector | `ASSERT(0)` | **a list could not be pushed at all, so `List^` was always empty** |
| `MsgStck::Pop`, any type | dropped the popped item while it still linked the one below | **one pop severed and leaked every generation under the one it restored** |
| `P3PmsgVect::Drop` | did not exist, so the base class ran | **deleting a vect from a container asserted; no stack needed** |
| `MsgStck::Drop` | unlinked every generation and freed none | **every pushed block leaked, for every caller** |
| `MsgStck::Drop`, free order | released the chain deepest-first | the blocks came back only as far as the allocator could coalesce them (§8) |
| `P2PmsgHeap_Collate*` | merged forwards only | **any ascending run of frees leaked its blocks — no stack needed (§8)** |
| `P2PmsgMgr::RootPath2Object` | stripped off every component | `^` and `@` were looked up as plain child names |
| `P3Pmsg_SplitRootPath` | refused a bare `^`, dropped a trailing one | **`.Root.Item^` silently answered with `Item` itself** |
| `RootPath2Object`, on a miss | assigned a void object into a `P3PmsgItem` | threw `"Invalid overloaded context"` instead of answering |

Fixed by `b6ae7c7` (the selector), `e22c271` (the root-path walk), `72e8f88` (lists and
vectors in a path, §6), `d2763ce` (pushing them, §7), `099417d` (releasing them, §7) and
`8729312` (releasing them in an order the heap can reclaim, §7-§8) and `4bb228a` (the
heap's own half of that, §8).

## 2. The stack itself — unchanged, and always worked

```cpp
P3PmsgField oField ( L"Quote", DataBSTR08(L"100.25") );
oField.r_Stck().Push();                        // snapshot the whole item
oField = P3PmsgName ( L"Quote-Revised" );      // mutate the live one
oField.r_Stck().Pop();                         // put it back
```

```
[A] MsgStck - push, mutate, pop
  after push+rename          -> name='Quote-Revised'  stacked=yes
  after pop                  -> name='Quote'
```

Identical before and after. The `r_Stck()` surface was the **only** way to reach a pushed
value; nothing else in the API could name one.

## 3. `^` in an object path

```cpp
P3PmsgItem oQuote ( L"Quote" );
oQuote.r_Desc ( P3PmsgField::AttrCMD_Create );
oQuote.r_Desc() += P3PmsgField ( L"Bid" );
oQuote.r_Stck().Push();                        // snapshot: name Quote, child Bid
oQuote = P3PmsgName ( L"Quote-Revised" );
oQuote.r_Desc() += P3PmsgField ( L"Ask" );     // added AFTER the push

P3Pmsg_SelectObject ( &oQuote.r_Object(), L"^"     );   // the pushed item
P3Pmsg_SelectObject ( &oQuote.r_Object(), L"^.Bid" );   // a child of it
P3Pmsg_SelectObject ( &oQuote.r_Object(), L"^.Ask" );   // NOT in the snapshot
P3Pmsg_SelectObject ( &oQuote.r_Object(), L"Ask"   );   // the live child
P3Pmsg_SelectObject ( &oQuote.r_Object(), L"^^"    );   // one push too far
```

Before — four `ASSERT(0)` hits on stderr, and nothing resolved:

```
[B] P3Pmsg_SelectObject - '^' on a field
  (live item)                -> name='Quote-Revised'  pos=897157011504
  ^                          -> (void)
  ^.Bid                      -> (void)
  ^.Ask                      -> (void)
  Ask                        -> name='Ask'  pos=2103014256944
  ^^                         -> (void)

...P2Pmsg.cpp(6834) : Assertion failed!      <- one per '^' path, on stderr
...P2Pmsg.cpp(6834) : Assertion failed!
...P2Pmsg.cpp(6834) : Assertion failed!
...P2Pmsg.cpp(6834) : Assertion failed!
```

After:

```
[B] P3Pmsg_SelectObject - '^' on a field
  (live item)                -> name='Quote-Revised'  pos=881280914640
  ^                          -> name='Quote'  pos=2275176432048
  ^.Bid                      -> name='Bid'  pos=2275176439856
  ^.Ask                      -> (void)
  Ask                        -> name='Ask'  pos=2275176437712
  ^^                         -> (void)
```

Three things to read off it. `^` carries the name the item had **when it was pushed**, not
the name it has now. A path does not stop at the `^` — a pushed item is a whole item, so
`^.Bid` reads the snapshot's descendants exactly as `.Bid` reads the live ones. And `^.Ask`
is void while `Ask` is not, which is the snapshot being a different object rather than
another route to the same one.

`^^` is void here because only one push happened; with two, it names the generation before
the last, since `Push` re-links the current head onto the new item. One `^` too many is a
miss, not a crash.

## 4. `^` and `@` through a root path

`P2PmsgMgr::RootPath2Object` splits a full path into components — each one carrying the
delimiter that introduced it — and walks them. Same tree, reached by path:

```cpp
P2PmsgMgr oMgr;
oMgr.r_name() = L"Store";

P3PmsgField oInst ( L"BHP" );
oInst.r_Attr ( P3PmsgField::AttrCMD_Create ) += P3PmsgField ( L"Currency" );
oMgr.r_Desc() += oInst;
oMgr.r_Desc() += P3PmsgField ( L"RIO" );       // never pushed

//  PushBack deep-copies, so the LIVE child has to come back out of the tree
//  before it can be pushed.
P3PmsgField oLive = oMgr.RootPath2Object ( L".Store.BHP" );
oLive.r_Desc ( P3PmsgField::AttrCMD_Create );
oLive.r_Desc() += P3PmsgField ( L"Last" );
oLive.r_Stck().Push();                         // snapshot: child Last
oLive.r_Desc() += P3PmsgField ( L"Close" );    // added AFTER the push
```

Before:

```
[C] P2PmsgMgr::RootPath2Object - '@' and '^' components
  .Store.BHP                 -> name='BHP'  pos=259
  .Store.BHP@Currency        -> THREW 'Invalid overloaded context'
  .Store.BHP^                -> name='BHP'  pos=259
  .Store.BHP^.Last           -> name='BHP'  pos=259
  .Store.BHP.Close           -> name='Close'  pos=1595
  .Store.RIO^                -> name='RIO'  pos=634
  .Store.RIO^.Last           -> name='RIO'  pos=634
  .Store.NoSuch              -> THREW '.Store.NoSuch
Path to object does not exist'
```

After:

```
[C] P2PmsgMgr::RootPath2Object - '@' and '^' components
  .Store.BHP                 -> name='BHP'  pos=259
  .Store.BHP@Currency        -> name='Currency'  pos=470
  .Store.BHP^                -> name='BHP'  pos=1009
  .Store.BHP^.Last           -> name='Last'  pos=1431
  .Store.BHP.Close           -> name='Close'  pos=1595
  .Store.RIO^                -> (void)
  .Store.RIO^.Last           -> (void)
  .Store.NoSuch              -> THREW '.Store.NoSuch
Path to object does not exist'
```

(The event carries the path and the text as two `Message()` calls, which is why it prints
over two lines.)

**The `P2Pos` is the point.** Before, `.Store.BHP^` answered `pos=259` — the same handle
`.Store.BHP` answers with. Not a snapshot that happened to look alike: the live item
itself. The name matched, the type matched, `IsVoid()` was false, and every check a caller
could make said the lookup had succeeded. After, it is `pos=1009`, a different object.

Three separate defects produced that one wrong answer:

1. **The walk stripped the delimiter off every component** (`P2PmsgMgr.cpp:825`). It has to
   for `.`, because `P3Pmsg_SelectObject` reads a leading dot as *"this component names the
   object you are standing on"* and matches it against the parent's own name. It must not
   for `@` and `^`, where the delimiter is the whole instruction. Stripped, `@Currency`
   became a search for a child called `Currency`, and `^` became a search for a child
   called nothing.
2. **The splitter refused a bare `^` and dropped a trailing one** (`P2Pmsg.cpp:7133`,
   `:7156`). `^` is the only delimiter that introduces no name, so a component can consist
   of it alone. Mid-path that made `P3Pmsg_SplitRootPath` return FALSE — which
   `RootPath2Object` does not check, so it walked the truncated list and answered with
   `BHP`. At the end of a path the component was simply never added, so `.Store.BHP^` and
   `.Store.BHP` were the same request.
3. **A miss raised instead of answering** (`P2PmsgMgr.cpp:881`). The walk assigned each
   selection straight into a `P3PmsgItem`, and `P3PmsgField::operator=(const P3PmsgObject&)`
   refuses anything that is not a field and throws `"Invalid overloaded context"` — which is
   what `@Currency` hit above once its miss became reachable. An item that was never pushed
   has no snapshot; that is an ordinary answer, and it is now the empty object the
   function's own contract calls a failed search.

`.Store.RIO^` is the clearest case of all: `RIO` was never pushed, so the path asks for
something that does not exist — and got a live object back.

`.Store.NoSuch` throws identically in both. That contract is deliberate and unchanged: a
missing **descendant** gets the populate callback and then `"Path to object does not
exist"`. Four of the examples in `_Msgcore_UseExamples` depend on it in as many words
(*"A MISS IS AN EXCEPTION, NOT AN EMPTY RESULT"*), and all four pass dotted paths.

## 5. The grammar

| Path | Resolves to |
|---|---|
| `Item` | the child `Item` of the current item |
| `Item.Child` | a descendant, one level down |
| `Item@Tag` | the attribute `Tag` |
| `Item^` | `Item` as it stood before its last push |
| `Item^^` | before the push before that |
| `Item^.Child` | a descendant **of the snapshot** |
| `Item^Child` | the same thing; the `.` after a `^` is optional, as it is after a name |
| `Item@Tag^` | a pushed attribute — attributes are items and carry stacks too |
| `.Root.Item^` | the same, as a full root path through `RootPath2Object` |
| `List@Tag` | an attribute of a list — a list is an item and carries its own |
| `List.Child` | a descendant of a list, likewise |
| `Vect@Tag` | the same for a vector |

## 6. Lists and vectors — one arm, three item types

`P3Pmsg_SelectObjectRecurse` has an arm per object kind. The list arm was `ASSERT(0)` and
nothing else, and there was no vector arm at all, so a vector fell past every test to the
`ASSERT(0)` that closes the function.

Landing **on** a list always worked: `P3PmsgCurs::Goto` connects `m_oP3PmsgList` for a
match of that type (`MsgCurs.cpp`), and the descendant arm returns straight from the
cursor. It was the step **after** that which had nowhere to go — and that is an ordinary
thing to ask for, because a list has attributes and descendants of its own. The
`VBLockItem` header is the same six addresses whatever the `ut` union under it holds:

```cpp
struct VBItemNN__            // P2PmsgVBLock.h:487
{
    ADDR__  aParent;
    ADDR__  aPrev;
    ADDR__  aNext;
    ADDR__  aExtra;          // Attributes
    ADDR__  aStack;          // Stack
    ADDR__  aDescn;          // Descendants
};
```

`P3PmsgList` and `P3PmsgVect` both derive from `P3PmsgField`, so the field arm's body is
already correct for all three; the fix is the guard, not new code:

```cpp
if ( pObject->IsField() || pObject->IsList() || pObject->IsVect() )
```

The one place the type does matter is the stack, because `MsgStck` keeps an accessor per
type and each *throws* if asked for the wrong one, so `^` dispatches to
`r_list()` / `r_vect()` / `r_item()`. All three arms are live — §7 is what made them so.

### Measured

`Test_ListPath` in `tests/MsgcoreSuite.cpp` pins five cases. Run against the library built
from the commit before the fix, all five fail — three assertions per lookup, then the
wrong answer:

```
  - '@' reaches an attribute of a list
      ASSERT  P2Pmsg.cpp(6871) : Assertion failed!      <- the list arm
      ASSERT  MsgVBHeap.cpp(4677) : Assertion failed!
      ASSERT  P2Pmsg.cpp(6952) : Assertion failed!      <- the function tail
      FAIL    !P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers@Unit").IsVoid()
  - a descendant name resolves through a list
      ...
      FAIL    !P3Pmsg_SelectObject(&oHost.r_Object(), L"Numbers.Kid").IsVoid()
  - '@' reaches an attribute of a vector
      ASSERT  MsgVBHeap.cpp(4677) : Assertion failed!
      ASSERT  P2Pmsg.cpp(6952) : Assertion failed!      <- no list arm to hit first
      FAIL    !P3Pmsg_SelectObject(&oHost.r_Object(), L"Payload@Unit").IsVoid()
  - a rooted path checks the name of a list it starts at
      ASSERT  P2Pmsg.cpp(6991) : Assertion failed!      <- P3Pmsg_SelectObject
      ...
      FAIL    !P3Pmsg_SelectObject(&oList.r_Object(), L".Numbers@Unit").IsVoid()

  cases   : 106  (5 with failures)
  checks  : 484  (34 failed)
  result  : FAIL
```

The vector case is worth reading twice: it never reaches `6871`, because there was no
list arm for it to hit. It goes straight to the tail.

`'^' on a list is a miss, not an assertion` is the fifth, and it is the one with no `FAIL`
line above — the answer was void before the fix and is void after it. What changed is that
asking the question no longer takes a debug build down.

After the fix, on the same tree: **106 cases, 484 checks, PASS** static and **PASS** dll,
and `build_run_c4.bat` PASS.

To reproduce the failure column, put the pre-fix selector back under the current tests and
rebuild the library — `build_run_suite.bat` compiles the test sources only, so a source
change does not reach it until the library itself is rebuilt:

```
git checkout 72e8f88~1 -- P2Pmsg.cpp
msbuild "Msgcore(2026).vcxproj" /p:Configuration=DebugLib /p:Platform=x64
tests\build_run_suite.bat static
git checkout HEAD -- P2Pmsg.cpp
msbuild "Msgcore(2026).vcxproj" /p:Configuration=DebugLib /p:Platform=x64
```

## 7. Pushing a list or a vector

§6 made `List^` resolve. It still answered nothing, because nothing could ever be *on* a
list's stack: `MsgStck::Push()` and `Pop()` were `ASSERT(0)` for both container types.

Almost nothing had to be written. `MsgStck__AllocItem` has dispatched on the item type
since it was written — it sizes with `P2PmsgList_SizeofItem` / `P2PmsgVect_SizeofItem` and
lays the block out with the matching `_InitItem` — so a list block was always allocatable.
Push simply never called it for one.

What the allocation does *not* carry is the payload. `VBLockList_Init` zeroes `aFirst`,
`aLast` and `nItems`; `VBLockVect_Init` starts at zero elements. The name and data cells
ride along with the block; the elements have to be walked over separately, with the same
loops `P3PmsgList::operator=` and `P3PmsgVect::operator=` already use.

The four parts every item type shares — name, data, attributes, descendants — are copied
piece by piece rather than by whole-object assignment, and that is not fastidiousness:

```cpp
P3PmsgField&                             // P2Pmsg.cpp:3235
P3PmsgField::operator = ( const P3PmsgField& rhs )
{
      ...
      // Pushed components
      if ( IsStacked() || rhs.IsStacked() )
        r_Stck() = ((P3PmsgField&)rhs).r_Stck();
}
```

`operator=` copies the **stack** as well, which inside a push would duplicate the very
generations the push is re-linking. The field arm had always sidestepped it by hand;
`MsgStck__CopyParts` now holds that in one place so the three arms cannot drift.

### Two things had to come with it

**`P3PmsgVect` had no `Drop()`.** The virtual call landed on `P3PmsgField::Drop`, whose
first line is `ASSERT(OBJ__IsField())` — false for a vect — and which then frees the item
block without a `Truncate()`, leaving every element block and `aExtra` continuation
allocated. Added, mirroring `P3PmsgList::Drop`, with `Truncate()` doing the type-specific
part.

This one was **not** waiting on the stack, which is what a first draft of this section
claimed. `P3PmsgCurs::Delete` has had a vect branch all along —

```cpp
    else if ( IsVect() )                 // MsgCurs.cpp:258
    {
      r_vect().Truncate ( );
      ...
      r_vect().Drop ( );                 // -> P3PmsgField::Drop, and the ASSERT
    }
```

— and `P3PmsgDesc::Truncate` runs it over every child. So deleting a vector out of a
descendant container, or emptying a container that held one, tripped the assertion in a
debug build with no stack involved at all. It needed only a vect in a tree and something
that empties the tree. The `Truncate()` on the line above the `Drop()` is why release
builds got away with it: the element blocks were already gone by the time the wrong `Drop`
ran, so the leak needed a `Drop` with no `Truncate` before it — which is exactly what
`Pop` does.

Two of the three cases that catch it (`Test_VectDrop`) never touch `MsgStck`:

```
  - a vector's elements survive a push and come back on the pop
      ASSERT  P2Pmsg.cpp(3442) : Assertion failed!     <- ASSERT(OBJ__IsField())
  - a vect can be deleted from a descendant container
      ASSERT  P2Pmsg.cpp(3442) : Assertion failed!
  - a descendant container holding a vect truncates cleanly
      ASSERT  P2Pmsg.cpp(3442) : Assertion failed!

  cases   : 114  (3 with failures)
```

**`Pop` severed what it restored.** This one is older than anything above and applies to a
plain field. `Pop` dropped the generation it had just restored from, and `Drop()` walks the
stack — `P3PmsgField::Drop` and `P3PmsgList::Drop` both end with
`if (IsStacked()) r_Stck().Drop()` — while `MsgStck::Drop` zeroed every link the rest of
the way down (see below). So the popped item had to be unlinked from the generation below
it *before* being dropped, and it was not:

```
push "Gen0" / "Gen1" / "Gen2" / "Gen3", then pop once

    before:   ^ -> Gen1        ^^ -> (void)      <- Gen0 still allocated, still correct
    after:    ^ -> Gen1        ^^ -> Gen0
```

A single push and pop cannot see it — there is nothing below to sever — and a single push
and pop was the only shape anything in the tree had exercised.

**`MsgStck::Drop` freed nothing.** It walked to the deepest generation, zeroed each
`aStack` on the way back up, and returned — so every pushed item block, and the name and
data blocks hanging off it, stayed allocated with nothing pointing at them. Every caller
wants the storage back: the three `Drop()`s above all reach it while dismantling an item,
MsgFacade's `FacadeNode` exposes it as the COM *drop the stack* verb, and TargetCore's
`P2PeerMsg` calls it when it replaces one stack with another.

The explicit recursion went with the fix. The `Drop()` it now calls on the generation
itself ends with `if (IsStacked()) r_Stck().Drop()`, so it re-enters `MsgStck::Drop` for
the next one down and the chain unwinds on its own; unlinking before the free is what keeps
that from reading a block that has already gone. The generation is dropped **by its own
type**, for the reason the paragraph above gives.

Measured inside an IOMAGE manager, where a `P2Pos` is an image offset and Msgcore's
allocator is the only claimant:

```
push, note the snapshot's P2Pos, Drop, push an identical item

    after:    first = 537    second = 537     <- the vacated block, reused
    before:   second != first                 <- a fresh block; the first never came back
```

A standalone `P3PmsgItem` is no good for this. It sits on the SYS heap, where a `P2Pos` is
a raw CRT pointer and Msgcore is not the only thing allocating from it — every transient
`P3PmsgDesc` a path selection news up competes for the same block, so reuse there is luck
rather than evidence.

**And the order of the frees turned out to matter.** Releasing the chain by letting
`Drop()` unwind it — which is what `if (IsStacked()) r_Stck().Drop()` does on its own —
frees the *deepest* generation first. Generations are pushed at ascending addresses, so
that is the one order this heap cannot take back, for the reason §8 sets out. `Drop` is
therefore a loop rather than a recursion: it unlinks each generation before dropping it,
which fixes the order head-first and stops `Drop()` recursing underneath the loop.

```
three generations, four rounds of push-push-push-Drop, reading the head generation

    deepest-first   949 1361 1773 2185     +412 a round, and linear
    head-first      949  949  949  949
```

### Measured

`Test_StackContainers` pins six cases, `Test_VectDrop` two more and `Test_StackDrop` three. Against the library built
from the commit before the fix they fail, and then the process dies:

```
  - a pop leaves the generations below it intact
      FAIL    !P3Pmsg_SelectObject(&oHost.r_Object(), L"^^").IsVoid()
  - a list's elements survive a push and come back on the pop
      ASSERT  MsgStck.cpp(145) : Assertion failed!     <- the Push list arm
      FAIL    oList.IsStacked()
      FAIL    !oWas.IsVoid()
      FAIL    (int)oList.GetCount() == 3
      FAIL    oList.GetNext(aPos).c_int() == 1

  exit = -1073741819                                   <- 0xC0000005, reading
                                                          elements off a list
                                                          Pop never restored
```

After the fix: **117 cases, 587 checks, PASS** static and **PASS** dll, and
`build_run_c4.bat` PASS. Reproduce it the way §6 says, checking out `MsgStck.cpp`,
`MsgVect.cpp`, `MsgVect.h` and `P2Pmsg.cpp` from `d2763ce~1` — or just `MsgVect.cpp` and
`MsgVect.h` to isolate the vect `Drop` on its own.

## 8. The heap only coalesced forwards

Found while measuring §7, and it is **not** a stack defect — the stack was only the thing
standing on it. It is recorded here because that is where the evidence is. Fixed by
`4bb228a`; the two properties below are what it was.

Two properties of the IOMAGE/BSTRio allocator combined badly:

- **`P2PmsgHeap_CollateIOMAGE` merges a freed block only with its NEXT physical
  neighbour.** There is no backward merge, and not by oversight: a `VBHeap` block carries
  no footer, so a block has no way to find its predecessor. Collation can only ever look
  forward.
- **`P2PmsgHeap_AllocIOMAGE` is first-fit from the head of a LIFO free list**, and it
  collates the head as it passes.

So a run of frees over adjacent blocks reclaims the space only if it runs **high to low**.
Low to high, every block's next neighbour is still allocated at the moment it is freed, so
nothing coalesces. The list keeps N separate blocks; the highest one merges with the image
tail; *that* merged block then sits at the head of the free list and satisfies every later
request — so the other N−1 are never looked at again. They are on the free list. They are
simply never reached.

### Measured, with no stack involved

`P3PmsgDesc::Truncate` empties a container by deleting child 0 repeatedly, which is
ascending order. Five children into a descendant container, `Truncate`, refill, repeat,
reading the first child's `P2Pos`:

```
round   0     1     2     3     4
kid0  331  1155  1979  2803  3627        <- +824 a round
```

That is ordinary use of the object model, and it drifts. §7's stack case is the same
mechanism reached by a different route.

### The fix: a boundary tag, in free blocks only

A free block now records its own size in its tail, so the block after it can work out where
it began. `P2PmsgHeap_Free` then collates **from the predecessor** as well as from the block
being freed — which absorbs it by the existing forward path, so there is no second merge
routine to keep in step with the first.

**The tag goes in free blocks only, and that is what makes it cheap.** A classic boundary
tag sits on every block, allocated ones included, which would move every byte of every
image — the cost this section quoted when it was written up as an open item, and it was
wrong. The tag is only ever *read* when the predecessor turns out to be free, so it only
needs to *exist* in a free block, and a free block's tail is dead space nothing else uses.
Allocated blocks are untouched, byte for byte.

**A stale tag cannot cause a wrong merge.** It is a hint; the candidate's own header is the
authority. `PrevFree` recomputes the address from the recorded size and then requires the
block it lands on to declare the same size, the same addressing mode, and to be free,
linked and allocated. A tag left behind inside a block since handed out, a coincidence in
payload bytes, a heap read off the wire — all fail that, and the answer is "no
predecessor", which is exactly where this started.

**Save scrubs the tags and puts them back.** A free block's tail is dead space in memory
but is still inside the arena that gets written out, and this tree keeps its serialised
slack deterministic on purpose. Persisting the tags would corrupt nothing — nothing reads
the inside of a free block back, and old and new builds load each other's images either way
— but it would make an image written by this build differ from one written before the tags
existed, which is the drift `golden_ref.p2p` exists to catch. So `Save` scrubs, writes, and
re-stamps through an RAII guard, because `Save` throws from a dozen places in between.

```
the same five children, Truncate, refill, repeat

    before   331  1155  1979  2803  3627      +824 a round
    after    331   331   331   331   331
```

`MscsUnitTests/golden_ref.p2p` is **byte-identical** against a `golden_utf16` built on the
patched library. Without the scrub it differed in exactly six bytes at offset 4092 — one
tag, in the trailing free block — which is the measurement that decided the scrub was
worth its cost.

Both arms, IOMAGE and BSTRio, take the same change, deliberately: they walk and merge by
the same rules, and a fix that lands on one and not the other is how the two drift apart.

### The other candidate, not taken

A **closer-fit search** — keep walking when the head of the free list is much larger than
the request, so the small blocks get taken — would have hidden most of the symptom without
addressing the cause, and it moves every allocation the library makes, which is the layout
`golden_ref.p2p` pins. The tag reclaims the space instead of routing around it, and costs
the golden image nothing.

## 9. What `^` still does not do

- **Collections.** `aStack` is a `VBLockItem` field; a `VBLockAttr` or `VBLockDesc` block
  has none. A `^` applied to the attribute or descendant *collection* is a broken path, not
  an assertion. The attributes and children **in** them are items and do have stacks.
- **Paths the library generates.** `P3Pmsg_GetPath` never emits `^`, so no path produced by
  Msgcore itself gains a component. It does emit a trailing `@` for an attribute path, and
  the splitter still drops that one — deliberately, so the `GetPath` → `RootPath2Object`
  round-trip is unchanged.
- **`RootPath2Object` on a non-field hit.** It walks the path in a `P3PmsgItem`, and
  `P3PmsgField::operator=(const P3PmsgObject&)` throws `"Invalid overloaded context"` for
  anything that is not a field, so a component that resolves to a list throws before it can
  answer. Unchanged here, and it predates all of this — but note that it is now easier to
  reach than it was, because §6 is what made such a component resolve in the first place.
  `Test_StackContainers` asks a descendant container directly for that reason.

## 10. Reproducing this document

The listings above are excerpts from one program. In full:

```cpp
// stack_paths_demo.cpp
#include <afx.h>
#include <afxwin.h>

#include <crtdbg.h>
#include <cstdio>

#include "P2Pmsg.h"
#include "MsgStck.h"
#include "MsgAttr.h"
#include "MsgDesc.h"
#include "P2PmsgMgr.h"
#include "Msgexception.h"

//  Name AND P2Pos, because the interesting failure is a path that answers with
//  the right NAME and the wrong OBJECT: a pushed item carries the name it had
//  when it was pushed, so only the handle tells the snapshot from the live one.
static void Show ( LPCWSTR lpszPath, const P3PmsgObject& oObject )
{
    if ( oObject.IsVoid() )
    {
      wprintf ( L"  %-26s -> (void)\n", lpszPath );
      return;
    }
    P3PmsgField oField = oObject;
    wprintf ( L"  %-26s -> name='%s'  pos=%llu\n", lpszPath,
              oField.r_name().c_name(),
              (unsigned long long)oField.GetP2Pos() );
}

static void ShowPath ( P2PmsgMgr& oMgr, LPCWSTR lpszPath )
{
    try
    {
      Show ( lpszPath, oMgr.RootPath2Object ( lpszPath ) );
    }
    catch ( P2Pevent *pEVT )
    {
      CString strMsg = pEVT->GetMessage();
      wprintf ( L"  %-26s -> THREW '%s'\n", lpszPath, (LPCWSTR)strMsg );
      pEVT->Cancel ( false );
    }
}

int main ( )
{
    //  No assert DIALOG: a pre-fix build asserts its way through most of this,
    //  and the point is to see what it returns afterwards.
    setvbuf ( stdout, nullptr, _IONBF, 0 );
    _CrtSetReportMode ( _CRT_ASSERT, _CRTDBG_MODE_FILE );
    _CrtSetReportFile ( _CRT_ASSERT, _CRTDBG_FILE_STDERR );

    // -------------------------------------------------------- A: the stack
    wprintf ( L"\n[A] MsgStck - push, mutate, pop\n" );
    {
      P3PmsgField oField ( L"Quote", DataBSTR08(L"100.25") );
      oField.r_Stck().Push();
      oField = P3PmsgName ( L"Quote-Revised" );
      wprintf ( L"  after push+rename          -> name='%s'  stacked=%s\n",
                oField.r_name().c_name(), oField.IsStacked() ? L"yes" : L"no" );
      oField.r_Stck().Pop();
      wprintf ( L"  after pop                  -> name='%s'\n",
                oField.r_name().c_name() );
    }

    // ------------------------------------------- B: '^' in an object path
    wprintf ( L"\n[B] P3Pmsg_SelectObject - '^' on a field\n" );
    {
      P3PmsgItem oQuote ( L"Quote" );
      oQuote.r_Desc ( P3PmsgField::AttrCMD_Create );
      oQuote.r_Desc() += P3PmsgField ( L"Bid" );
      oQuote.r_Stck().Push();                      // snapshot: name Quote, child Bid
      oQuote = P3PmsgName ( L"Quote-Revised" );
      oQuote.r_Desc() += P3PmsgField ( L"Ask" );   // added AFTER the push

      Show ( L"(live item)", oQuote.r_Object() );
      Show ( L"^",      P3Pmsg_SelectObject ( &oQuote.r_Object(), L"^"      ) );
      Show ( L"^.Bid",  P3Pmsg_SelectObject ( &oQuote.r_Object(), L"^.Bid"  ) );
      Show ( L"^.Ask",  P3Pmsg_SelectObject ( &oQuote.r_Object(), L"^.Ask"  ) );
      Show ( L"Ask",    P3Pmsg_SelectObject ( &oQuote.r_Object(), L"Ask"    ) );
      Show ( L"^^",     P3Pmsg_SelectObject ( &oQuote.r_Object(), L"^^"     ) );
    }

    // --------------------------------------------- C: '^' in a root path
    wprintf ( L"\n[C] P2PmsgMgr::RootPath2Object - '@' and '^' components\n" );
    {
      P2PmsgMgr oMgr;
      oMgr.r_name() = L"Store";

      P3PmsgField oInst ( L"BHP" );
      oInst.r_Attr ( P3PmsgField::AttrCMD_Create ) += P3PmsgField ( L"Currency" );
      oMgr.r_Desc() += oInst;
      oMgr.r_Desc() += P3PmsgField ( L"RIO" );     // never pushed

      P3PmsgField oLive = oMgr.RootPath2Object ( L".Store.BHP" );
      oLive.r_Desc ( P3PmsgField::AttrCMD_Create );
      oLive.r_Desc() += P3PmsgField ( L"Last" );
      oLive.r_Stck().Push();                       // snapshot: child Last
      oLive.r_Desc() += P3PmsgField ( L"Close" );  // added AFTER the push

      ShowPath ( oMgr, L".Store.BHP"           );
      ShowPath ( oMgr, L".Store.BHP@Currency"  );
      ShowPath ( oMgr, L".Store.BHP^"          );
      ShowPath ( oMgr, L".Store.BHP^.Last"     );
      ShowPath ( oMgr, L".Store.BHP.Close"     );
      ShowPath ( oMgr, L".Store.RIO^"          );
      ShowPath ( oMgr, L".Store.RIO^.Last"     );
      ShowPath ( oMgr, L".Store.NoSuch"        );
    }

    wprintf ( L"\n" );
    return 0;
}
```

Build it against the static library, from a VS x64 native tools prompt at the repository
root, after `msbuild "Msgcore(2026).vcxproj" /p:Configuration=DebugLib /p:Platform=x64`:

```
cl /nologo /EHsc /MDd /std:c++17 /Zc:wchar_t ^
   /D_DEBUG /D_CONSOLE /D_UNICODE /DUNICODE /D_AFXDLL /D_WIN32_WINNT=0x0603 ^
   /DMsgcore_STATIC /I. stack_paths_demo.cpp ^
   /link /LIBPATH:out\x64\DebugLib Msgcore.lib MsWsock.lib ws2_32.lib ^
   comsuppwd.lib Propsys.lib /OUT:stack_paths_demo.exe
```

The "before" column was produced by the same source against a `git worktree` of `ff0eb77`,
the commit before the first of the two fixes. `P2Pos` values differ run to run — they are
heap offsets — so read them for equality within one run, never across.

The cases are also pinned as regression tests: `Test_Stack` and `Test_RootPath` in
`tests/MsgcoreSuite.cpp`, run by `tests\build_run_suite.bat` in both link modes.
