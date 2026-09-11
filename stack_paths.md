# The `^` stack path operator

> Status: current as of 2026-09-12. Describes `T_StckDelim` — the third path delimiter —
> what it was for, why no path could use it, and what it resolves to now. Every listing and
> every line of output below was run against this tree; §6, §7 and §25 say how to
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
| `P2PmsgHeap_Alloc*` | first-fit, from a LIFO free-list head | **a small request split the biggest free block, and the small ones were never chosen (§8)** |
| `P3Pmsg_SelectObjectRecurse`, attr and desc arms | answered nothing for `^` | a collection's stack could not be named, though the block can find it (§9) |
| `P3Pmsg_SelectObjectRecurse`, a collection never created | fell through every arm to the closing `ASSERT(0)` | **two debug assertions for an ordinary miss (§9)** |
| `P2PmsgMgr::RootPath2Object` | stripped off every component | `^` and `@` were looked up as plain child names |
| `P3Pmsg_SplitRootPath` | refused a bare `^`, dropped a trailing one | **`.Root.Item^` silently answered with `Item` itself** |
| `P3Pmsg_SplitRootPath` | a component boundary, even where no name preceded it | `@^Tag` was a nameless `@`, and the whole path came back FALSE (§10) |
| `RootPath2Object`, on a refused split | walked the components collected before the refusal | **a malformed path answered with an object, not an error (§10)** |
| `RootPath2Object`, on a miss | assigned a void object into a `P3PmsgItem` | threw `"Invalid overloaded context"` instead of answering |
| `MsgStck::Push` | left a snapshot's `aParent` zero | **`P3Pmsg_GetPath` named every snapshot `.BHP`, which resolves to the root (§11)** |
| `P2PmsgAttr_GetVBLockParentnn` | read the owning ITEM's block as a `VBLockAttr` | **`P3Pmsg_GetPath` on an attribute collection segfaulted (§12)** |
| `P3Pmsg_GetPath`, the `P3PmsgAttr` overload | arms on block types that are never an item | fell to its own `ASSERT(0)` and emitted a lone `@` (§12) |
| `P3Pmsg_SelectObjectRecurse`, field arm | recursed into the collection with an empty path | a bare `@` answered nothing, so no path could name a collection (§12) |
| `P3Pmsg_SplitRootPath` | dropped a trailing `@` | **`.Root.Item@` answered `Item`, not its attributes (§12)** |
| `RootPath2Object`, the walk itself | carried a `P3PmsgItem`, which only holds a field | **a list or a vector before the last component threw (§13)** |
| `RootPath2Object`, a miss under one | never reached the name test | an ordinary miss came back as `"Invalid overloaded context"` (§13) |
| `P3Pmsg_SplitRootPath`, inside the loop | knew about a bare `^`, not a bare `@` | **`.Root.Item@.Tag` was malformed where `@^.Tag` resolved (§14)** |
| `P3Pmsg_SelectObjectRecurse`, attr arm | `ASSERT(0)` for a `@` on the collection | a debug assertion for a path only a caller could write (§14) |
| `P3Pmsg_SplitRootPath` | refused a bare `.`, dropped a trailing one | **`.Root.Item.` answered `Item`, and no path could name a descendant collection (§15)** |
| `P2PmsgDesc_GetVBLockParentnn` | read the owning ITEM's block as a `VBLockDesc` | the same defect §12 fixed in the attribute one, left standing (§15) |
| `P3Pmsg_GetPath` | had no `P3PmsgDesc` overload at all | a descendant collection had no path to emit (§15) |
| `RootPath2Object`, a bare component that missed | took the descendant delimiter's throw | a collection an item never had was reported two different ways (§15) |
| `P2PmsgMgr::P2Pos2Path` | `IsField(pos)`, which is `VBLockItem_IsField` | **a list and a vector asserted, though `P3Pmsg_GetPath` builds both (§16)** |
| `P2PmsgMgr::P2Pos2Path` | no arm for a collection at all | an object had a path the manager could not emit (§16) |
| `P2PmsgMgr::P2Pos2Path`, anything else | `ASSERT(0)` | a debug assertion for a handle naming nothing (§16) |
| `RootPath2Object`, the walk's strip | fired on a `.` with anything after it | **`.Store.BHP.^` was the snapshot ITEM, where `@^` is the collection's (§17)** |
| `P3Pmsg_SelectObject`, the wrapper | guarded the leading-`.` rule on LENGTH | `.^` matched an empty name against the item's own, and came back void (§17) |
| `P3Pmsg_SelectObjectRecurse`, field arm `.` | handed the rest of the path to the ITEM | `Item.^` was the item's snapshot; `Item@^` is the collection's (§17) |
| `P3Pmsg_SelectObject`, the leading component | asserted the object's own name at any depth | **`.Last` asked `BHP` whether it is called `Last` (§18)** |
| `P3Pmsg_SelectObject`, a match with nothing after it | recursed with an empty path | **`P3Pmsg_GetPath(&mgr)` is `.Store`, and `.Store` at the root answered void (§18)** |
| `P3Pmsg_SelectObject`, the collection arms | the same discarded match | `Tag` answered where `.Tag` did not, on both collections (§18) |
| `P3PmsgObject`'s copy constructor | `memcpy`'d an inline block into the copy | **a handle on a floating item was a DUPLICATE of it (§19)** |
| `P3PmsgObject::Connect` | created the heap and placed nothing on it | **a handle addressed the source's own storage, and dangled (§19)** |
| `P3PmsgObject::Connect`, the `ASSERT` under it | a condition the guard above excluded | every path that reached that arm asserted (§19) |
| `P3PmsgObject::operator bool` | `m_hVBList!=0`, where `IsVoid` asks for a block | the two contradicted each other, and §19 made the answer move (§20) |
| `P3PmsgField::IsVoid` | `m_hVBList==0` | **a floating item reported that it denotes nothing (§20)** |
| `P3PmsgField::operator bool` | "is it populated", via `r_data()` | **evaluating it on a failed lookup FAULTED (§20)** |
| `P3PmsgList` / `P3PmsgVect::operator bool` | `IsVoid()` | **true exactly when there was no list (§20)** |
| `CListCtrl_Ext`, the column guard | tested the ROW, not the ITEM | **a missing column reached `r_data()` on a void field (§20)** |
| `P3PmsgField`'s copy constructor, its second arm | guarded on a member zeroed the line above | a handle copy that never ran, in a class that copies by value (§20) |
| `operator bool`, all six classes | an IMPLICIT conversion | **every comparison between two of them fell through to the built-in int one (§21)** |
| `P3PmsgField::operator ==` | inherited nothing, so `oA == oB` asked bool | **a value copy, an unrelated item and an empty one all compared EQUAL (§21)** |
| `P3PmsgField`, `operator !=` | `P3PmsgData`'s, which compares the DATA | **`a == b` and `a != b` were both true, and `!=` FAULTED on a void field (§21)** |
| `P3PmsgField::operator == ( LPCTNAM )` | not const, hiding `P3PmsgName`'s two | **the only comparison the class meant to offer was the only one refused (§21)** |
| `RehomeInlineItem`, its first guard | "already on a heap" — the heap, not the block | **an inline item with a heap was never rehomed (§22)** |
| `P3PmsgObject`'s copy constructor, and `Connect` | shared when there was a heap | **a copy took an address INSIDE the source object, and outlived it (§22)** |
| `AllocVBLock`, making an object its first heap | left the item block behind | **the state that made all three wrong, created once and never closed (§22)** |
| The value arm of that same copy constructor, and of `Connect` | "copy the block" — the block, not the value | **two standalone values named ONE payload, and the first to retype freed it (§23)** |

Fixed by `b6ae7c7` (the selector), `e22c271` (the root-path walk), `72e8f88` (lists and
vectors in a path, §6), `d2763ce` (pushing them, §7), `099417d` (releasing them, §7) and
`8729312` (releasing them in an order the heap can reclaim, §7-§8), `4bb228a` (the heap's
own half of that, §8), `dbfa789` (the half the tag could not reach, §8), `3f9ecfa`
(collections, §9), `95f039c` (root paths carrying both, §10), `24c6a59` (the paths
the library itself writes, §11), `d31f2c4` (the collection's own path, §12) and
`8218904` (stepping off a container, §13), `5d48e5a` (a bare `@` anywhere, §14) and
`2a03161` (the descendant collection, and the rule stated once, §15) and `0888659`
(one arm per block kind, §16) and `c0409cb` (a bare `.` wherever it stands, §17) and `2de26ac`
(the root marker only where a path is rooted, §18) and `48ff002` (a floating item is one
object, §19) and `c8f1af6` (one question, one answer, §20) and `4f4c334` (one question per operator, §21) and `da22f93` (where the block is, not whether there is a heap, §22) and `f77bb97` (copying the value and not the address of one, §23).

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

## 8. The heap only coalesced forwards, and then chose badly

Found while measuring §7, and it is **not** a stack defect — the stack was only the thing
standing on it. It is recorded here because that is where the evidence is. It turned out
to be two defects rather than one, fixed by `4bb228a` and `dbfa789`; the two properties
below are the first of them.

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

### The other half: a closer fit

The tag repairs adjacency. It cannot repair **choice**, and the allocator was choosing
badly for a second, independent reason: it was first-fit from the head of a **LIFO** free
list. A freed block goes to the head, so the head is whichever block was freed last, and
first-fit took it whatever its size.

Free an 800-byte block and then a 50-byte one, ask for 50, and the 800 is split. The
50-byte block stays on the list untouched, and the next 800-byte request can no longer be
served by what is left of the block that used to serve it — so it comes off the end of the
image instead, and the arena grows with two perfectly good free blocks sitting on the list.
The tag cannot help: live data sits between those two blocks, so there is nothing to merge.

Measured on exactly that, two fields with a live one between them:

```
Big = 331, Sml = 1125, both then freed

                        before      after
    a small request      331         1125     <- splits Big / takes Sml
    a big request        1517         331     <- fresh ground / takes Big
```

1517 is the whole cost in one number: a heap holding two free blocks that could use neither
for what they were made for.

**What this section said before was wrong twice**, and both corrections are the reason the
work was worth doing. It called a closer fit "hiding most of the symptom without addressing
the cause" — but the cause it addresses is a different one from the tag's, and nothing else
addresses it. And it said the search "moves every allocation the library makes, which is
the layout `golden_ref.p2p` pins" — it moves an allocation only when the free list holds
more than one block that fits, which a workload that never frees never does. The golden
workload never frees. `golden_ref.p2p` is **byte-identical**, unchanged, for the same
reason the tag left it alone.

**The walk is bounded at eight blocks that fit**, and eight is chosen against the list's own
order rather than as a round number: the list is LIFO, so the blocks nearest the head are
the most recently freed, which in a container being emptied and refilled — the workload
that produced the drift above — are exactly the blocks about to be asked for again. A walk
of the whole list would spend most of its time on the part least likely to help. Blocks too
small to serve the request do not count against the budget; they were walked past before
this change and are walked past after it.

**The chosen block is re-checked before it is used**, because the walk collates as it goes
and a collate absorbs the block that physically *follows* it. The free list is in no address
order, so a block visited late in the walk can sit immediately before a block chosen early
in it, and swallow it. An absorbed block has its defs byte zeroed, so the usual predicates
catch it; the answer is one more pass taking the first fit outright, which cannot go stale
because nothing is collated between finding that block and allocating it.

`P2PmsgHeap_AllocIOMAGE1` keeps first-fit, and that is the one place the two arms and their
dead twin part company. F11b's bound had to be carried into that unreferenced function
because a bound is a **safety** property and an unbounded twin is how a bound gets lost
again. A better choice of block is not: a revived copy would merely allocate the way the
library used to.

## 9. Collections — `^` commutes with `@` and `.`

`aStack` is a `VBLockItem` field. A `VBLockAttr` or a `VBLockDesc` block has none, so a `^`
applied to the attribute or descendant *collection* used to answer "broken path". That was
correct, and it was less than the block can say.

A collection block carries **`aParent`**. So the item that owns the collection can be found;
that item has a stack; and a snapshot holds a copy of the **whole** collection, because
`Push` copies name, data, attributes and descendants. `Item@^` — the attribute collection as
it stood at the last push — is therefore the attribute collection *inside* the snapshot,
which is the object `Item^@` already named.

**`^` commutes with `@` and with `.`**, and for the reason §3 gives about pushed items: a
snapshot is a whole item, not a fragment of one. That is the rule, and the tests pin it as
an identity rather than as two separate lookups that happen to agree.

### Nothing was added to any block

`aParent` was already there and already maintained — `P3Pmsg_GetPath` walks it to build a
path — and `P3PmsgObject::GetParent` already reads it. No field was added to `VBLockAttr` or
`VBLockDesc`, nothing moved, and no image changed. `MscsUnitTests/golden_ref.p2p` is
byte-identical.

The alternative was to give the collections an `aStack` of their own, and that would have
been the expensive answer to a question the blocks could already answer: a new field in two
block types is an on-disk format change, and every `.p2p` ever written would have shifted.

### Measured

```
                          before        after
  @^                      (void)        (attr coll) pos=1173
  @^Currency              (void)        Currency    pos=1220
  ^@Currency              Currency      Currency    pos=1220     <- the same object
  @^Venue                 (void)        (void)                   <- added after the push
  desc ^Last              (void)        Last        pos=1431
  Item^.Last              Last          Last        pos=1431     <- the same object
```

Pushes nest, so the delimiter repeats: `@^` is the collection before the last push, `@^^` the
one before that. That works because a snapshot's own collections point at the **snapshot**,
not back at the live item — measured, because if they pointed back the second `^` would loop
on the first generation instead of descending.

### A collection that was never created was asserting

Found while measuring the above, and it is not about `^`. `r_Attr()` on an item with no
attributes hands back an object carrying no block. It matches no arm of
`P3Pmsg_SelectObjectRecurse`, so it fell through to the `ASSERT(0)` that closes the
function — by way of `IsRoot()` on the way past, which asserts a **second** time on a
SYS-heap object, because `P2PmsgHeap_IsRoot` has no arm for one. Two debug assertions for
`Item@Tag` asked of a bare item, which is ordinary use. The value returned was always right.

`IsVoid()` is not the test that catches it: that wants `m_hVBList` **and** `m_aVBLock` both
zero, and an empty collection keeps the handle of the heap it would have been allocated
from. The address alone is the condition, and `GetVBLocknn()` reads it without dereferencing
anything — which matters here, because there is nothing to dereference.

### Two things this arm is not

`Item@Attr^` is a different path and always worked: the attribute **item** has a stack of
its own, and the field arm follows it. Only a `^` applied to the collection *itself* comes
through the new code.

And `P3PmsgDesc::SelectObject` is not a path lookup, despite the name — it is a cursor
`Goto` by plain name and never parses a path at all. (It is also defined twice, identically,
in `MsgDesc.cpp` and `P2Pmsg.cpp`.) The descendant arm is reached by handing the collection
to `P3Pmsg_SelectObject` directly, which is how `Test_CollectionStack` reaches it.

## 10. A root path could not carry `@^`

§9 made `Item@^Tag` resolve. `.Store.BHP@^Currency` still answered **BHP** — not the
attribute, not void, but the item the path started from.

`^` is a delimiter, and it is the only one that introduces no **name** of its own: it names
the pushed value of whatever stands to its left. `P3Pmsg_SplitRootPath` did not know that,
and cut a fresh component at every `^`. So `@^Currency` became a component that was the
single character `@`, carrying no name at all — which the length test refuses, and the
refusal is for the whole path, not for the component.

`P2PmsgMgr::RootPath2Object` then discarded the `FALSE` and walked whatever components had
been collected before the splitter gave up. The walk answers the last component it managed,
so the path came back as the object one step above the refusal. A wrong object, with
nothing in it to tell it from a right one.

### Both halves

The scan takes a `^` while the component is still nothing but delimiters, and stops at one
once a name has been read. `@^^Tag` is one component; `@Tag^` is two, because the attribute
`Tag` has a stack of its own and asking for it is a second step.

The walk honours the verdict. A malformed path throws, which is what a missing descendant
already did there; void stays reserved for a search that ran and found nothing.

A last component that resolves to a **collection** is handed back rather than assigned into
the `P3PmsgItem` the walk carries — `P3PmsgField::operator=` throws for a non-field, and
there is nothing to assign for when there is nothing left to walk. That is what
`.Store.BHP@^` needs, and the object-path spelling has always returned it. A non-field
reached *before* the last component still threw, and §13 is where that ends: the walk stops
carrying a `P3PmsgItem` at all, and this special case goes with it.

### Measured

```
                            before        after
  .Store.BHP@^Currency      BHP           Currency    pos=1970   <- == ^@Currency
  .Store.BHP@^^Currency     BHP           Currency    pos=1056
  .Store.BHP.^Last          BHP           Last        pos=2345   <- == ^.Last
  .Store.BHP@^              BHP           (attr coll) pos=1923   <- == @^
  .Store..BHP               Store         throws       ← until §15; now BHP
  Store.BHP                 Store         throws
```

Every root path now agrees with its object-path spelling. That is what §9's commuting rule
claims, and until this it was a claim no root path could make.

The `P3Pmsg_GetPath` → `RootPath2Object` round-trip is untouched: `GetPath` produces
`.Store.BHP` for the item and `.Store.BHP@Currency` for the attribute, and both resolve back
to the object they name, before and after.

### What did not change here

A trailing `@` was still dropped rather than refused — `.Root.Item@` was `Item` — and that
was deliberately outside this change. §12 came back to it: a `@` at the end of a path
introduces no name because it needs none, exactly as `^` needs none, and it now names the
attribute collection. A trailing `.` went the same way in §15.

So did `.Store..BHP`, and that one is worth naming. This section made it *throw*, on the
grounds that an empty component is an empty name. §15 found it is not an empty name at all
— it is the root's descendant collection, and then BHP, which is BHP. A redundant spelling
is not a malformed one. What survives here is the other refusal, `Store.BHP`: a path that
does not begin at a root, which is the one thing the splitter still says no to.

## 11. The paths the library writes

`P3Pmsg_GetPath` builds a path by walking `aParent`. `MsgStck::Push` left a snapshot's
`aParent` zero, and a zero parent is what GetPath reads as a **floating item** — so every
snapshot of `BHP` answered `.BHP`, wherever `BHP` actually lived, and every *generation*
answered the same `.BHP`.

That is worse than a path that fails to resolve. `.BHP` parses as the root name `BHP` with
no components at all, so `RootPath2Object` walks nothing and hands back **the root**. A path
the library generated for one object, naming another.

### Where the owner had to come from

Nothing in a snapshot's block could be walked to reach the owner. `aStack` runs
owner → newest → older, and a snapshot carries no back-link along it; the chain can only be
followed downwards, which is why `Pop` and `Drop` work and `GetPath` could not.

So Push records the owner in `aParent` — the **owner**, not the generation above it. The
older generations already point at the owner and go on pointing at it, so a push is one
line and `Pop` and `Drop` have nothing to maintain: the block they unlink is one they free.
GetPath counts the generation by walking the owner's chain, and the walk doubles as the
proof that the block is on it.

### Measured

```
              before                       after
  ^           .BHP          -> the root    .Store.BHP^           -> itself
  ^^          .BHP          -> the root    .Store.BHP^^          -> itself
  ^@Currency  .BHP@Currency -> (void)      .Store.BHP^@Currency  -> itself
  ^.Last      .BHP.Last     -> throws      .Store.BHP^.Last      -> itself
```

Only the snapshot's own arm was ever wrong. Everything *inside* a snapshot already recursed
correctly and simply inherited the bad string from the bottom, so the one fix names the
whole subtree. And these resolve because §10 taught the splitter to carry a `^`; before
that they would have been honest paths that still went nowhere.

### The field was on loan

`aParent` on an item means "the collection I am linked into", and the three `Drop`
implementations read it to decide which collection to unlink themselves from — anything else
is `ASSERT(0)`. Push is borrowing the field for a block that is in no collection at all, and
the loan holds only while the block is on the stack.

So `MsgStck` returns it where the block leaves the chain: `MsgStck__Unlink` for a pop, and
the walk in `MsgStck::Drop`. Left for the free to trip over, it asserted **22 times across
10 cases** — every push/pop and every drop in the suite. `Drop` itself is untouched.

`P3Pmsg_GetStckDepth` is exported rather than static because "is this block a snapshot of
that item" is a question nothing else in the image can answer.

### What images say

`MscsUnitTests/golden_ref.p2p` is byte-identical: the golden workload pushes nothing, so no
stack block reaches it. An image written before this carries a zero parent on its stack
blocks and still comes out of the floating-item arm, exactly as it did.

## 12. A collection's own path

`P3Pmsg_GetPath` has a second overload, for a `P3PmsgAttr` — the attribute collection
itself, the object a path names by ending in a bare `@`. Nothing in the library calls it, and it
segfaulted. Three defects, each hiding the one under it.

### Reading the wrong block

`P2PmsgAttr_GetVBLockParentnn` took `GetField()->r_Object()` and read it through
`VBLock_pAttr`. But `GetField()` is the **item** the collection hangs off — a `P3PmsgAttr`
holds a pointer back to its field, which is what the name means — so that picks
`VBLockAttr`'s fields out of a `VBLockItem`'s `ut` union. The parent came back as whatever bytes lay at
that offset, and `Msg2Phys` of it ran off the arena.

The collection's own block is the item's `aExtra`. `P3PmsgAttr__GetVBLocknn` already reads
it, and MsgAttr's own link routines already write it into every child's `aParent`.

### Arms that could not fire

Handed the right parent, the overload was still wrong. A collection's parent is an **item**,
and every real item is a `VBLock_Item` block — `VBLock_Field` and `VBLock_List` are
different block *types*, not item types, which is why every other walk in the file spells the test
`VBLock_IsX(pParent) || VBLockItem_IsX(...)`. Neither arm could ever be true, so the walk
fell to its own `ASSERT(0)` and emitted a lone `@` with no owner in front of it. Both arms
also appended a `.` before that `@`, which no spelling of an attribute path has ever
carried.

One arm replaces the two, and it covers all three item types: a list and a vector carry
attributes exactly as a field does (§6).

### A string that did not resolve

`@` with nothing after it names the **collection**, the way `^` with nothing after it names
a snapshot. The selector recursed into the collection with an empty path, looked for a name
that was not there and answered void; `P3Pmsg_SplitRootPath` then dropped a trailing `@`
outright, so `.Store.BHP@` came back as the components for `.Store.BHP` and answered `BHP`.

The reason on record for that drop — §10's own closing note — was that `P3Pmsg_GetPath`
emits a trailing `@` and the round-trip had to survive it. The only overload that emits one is this
one, which had no caller and crashed before it got there. A path to an *attribute*,
`@Currency`, carries a name after the delimiter and never came through that clause at all.
The drop was protecting a round-trip that could not happen, and keeping the component is
what makes one.

### Measured

```
                            before              after
  GetPath(attr coll)        segfault            .Store.BHP@
  .Store.BHP@               BHP                 (attr coll) pos=423
  BHP@  (object path)       (void)              (attr coll) pos=423
  .Store.RIO@   (no attrs)  RIO                 (void)
  GetPath(snapshot's coll)  segfault            .Store.BHP^@
```

The last line is the three sections compounding: §11 gave the snapshot a path, §9 gave it
its own collections, and this gives the collection one — and `.Store.BHP^@` resolves, because
§10 taught the splitter to carry both delimiters.

An item with no attributes has no collection block at all, and `r_Attr()` hands back an
object that keeps the heap handle and carries no address — which `IsVoid()` does not catch.
The selector tests the address, as the collection arms added for §9 do, so `.Store.RIO@` is
an ordinary miss rather than the item or an assertion.

A trailing `.` was still dropped here: `.Root.Item.` was `Item`. The reason given was that
`.` is also the root delimiter and `.Root.` has meant the root since long before any of
this — and that reason does not hold up. The root delimiter is consumed once, at position
zero, before a single component is parsed; every other `.` in a path is a descendant
delimiter. §15 is where the descendant collection gets its path.

## 13. A root path could not step off a container

`.Store.Numbers.Leaf`, where `Numbers` is a list, threw `"Invalid overloaded context"`. So
did `.Store.Numbers@Unit`, `.Store.Payload.Note` for a vector, and `.Store.BHP@^.Currency`
for a collection. The object-path spellings of the first three have answered since §6 taught
the selector about containers, and the fourth names the object `.Store.BHP^@Currency`
already answered with — the same two steps the other way round (§9). Only the root-path
walk could not carry them.

`P2PmsgMgr::RootPath2Object` held the walk in a `P3PmsgItem`, so the assignment that carries
it forward is `P3PmsgField::operator=(const P3PmsgObject&)` — and that refuses anything which
is not a field. A list and a vector **are** items; they are not fields. `VBLockItem_IsField`
is false for both, because the item type lives in the `ut` union and `VBLock_Item` is the
block type they all share (§6). So the walk could land on a container and could not leave it.

### The miss that was reported as the wrong error

`.Store.Numbers.Nobody` threw the same event. That is not a refusal of an odd path — it is
an ordinary miss, and the walk never got as far as asking about `Nobody`: it died on
`Numbers` one component earlier, before the name test the descendant arm runs. A caller
distinguishing "no such object" from "malformed" by catching the message got neither.

### The variable, not the walk

Every step of the walk is `P3Pmsg_SelectObject`, which is a free function over
`P3PmsgObject` with an arm for each kind there is — an item, a list, a vector, an attribute
collection, a descendant collection. Nothing about the walk needed a field. Only the
variable did.

Holding a `P3PmsgObject` is the whole fix, and it subsumes the special case §10 had to add:
a non-field that *ends* the path no longer needs its own return, because it is handed back
by the same statement that hands back a field.

`P3PmsgItem::Exists` is itself `!P3Pmsg_SelectObject(...).IsVoid()` (`P2Pmsg.cpp`), so the
walk was running the whole lookup twice for every component of every path — once to ask
whether it would work and once to do it. It now selects once, and looks a second time only
where the populate callback has just run and might have made the answer exist.

### Measured

```
                            before                          after
  .Store.Numbers            (list) pos=845                  (list) pos=845
  .Store.Numbers@Unit       THREW 'Invalid overloaded ...'  Unit pos=1069   ← == Numbers@Unit
  .Store.Numbers.Leaf       THREW 'Invalid overloaded ...'  Leaf pos=1280   ← == Numbers.Leaf
  .Store.Numbers@           THREW 'Invalid overloaded ...'  (attr coll) pos=1022
  .Store.Payload.Note       THREW 'Invalid overloaded ...'  Note pos=2043   ← == Payload.Note
  .Store.BHP@^.Currency     THREW 'Invalid overloaded ...'  Currency pos=2746
  .Store.BHP^@Currency      Currency pos=2746               Currency pos=2746
  .Store.Numbers.Nobody     THREW 'Invalid overloaded ...'  THREW 'Path to object does not exist'
  .Store.Numbers@None       THREW 'Invalid overloaded ...'  (void)
```

The miss rules are the ones already on record, and the walk reaches them now instead of
dying one component early: the component's own **delimiter** decides, as it always has, not
what is being searched. A descendant component that finds nothing gets the populate callback
and then `"Path to object does not exist"`; an `@` or a `^` that finds nothing is an empty
answer, because an item that was never pushed has no snapshot. `.Store.BHP@^.Venue` throws
rather than answering void for that reason — `Venue` was added after the push, so it is
not in the snapshot's collection, and the component asking for it is spelled with a `.`.

### What did not change here

A bare `@` was still a component only at the **end** of a path, so
`.Store.BHP@.Currency` stayed malformed while `.Store.BHP@^.Currency` — one step longer —
resolved. That is the splitter's asymmetry rather than the walk's, and §14 is where it
ends.

## 14. A bare `@` was a component only at the end

`^` and `@` are the two delimiters that carry no **name** of their own, and each names an
object that has no other spelling: the pushed value, and the attribute collection. The
splitter tests for that in two places — once inside its loop, once in the clause after it —
and only the second of them knew about `@`.

So a bare `@` was a component at the **end** of a path and nowhere else:

```
  .Store.BHP@             TRUE   [.BHP][@]
  .Store.BHP@.Currency    FALSE  [.BHP]                   ← malformed
  .Store.BHP@^.Currency   TRUE   [.BHP][@^][.Currency]
```

One step *longer* resolved — through the collection as it stood at the last push — while
the shorter spelling of the same two steps was refused. The two clauses now ask one
question, `P3Pmsg__IsBareComponent`.

At this point a lone **descendant** delimiter was still malformed, and the reason given was
that `.` always introduces a name where `@` does not. That is not a difference between the
two delimiters — `Item.Kid` and `Item@Tag` each introduce one, and `Item.` and `Item@` each
introduce none. §15 is where `.` joins them and the rule stops having exceptions.

### Measured

```
                          before                      after
  .Store.BHP@.Currency    THREW 'Malformed ...'       Currency pos=470   ← == @Currency
  .Store.BHP@.            THREW 'Malformed ...'       (attr coll) pos=423
  .Store@.Venue           THREW 'Malformed ...'       Venue pos=845      ← == .Store@Venue
  .Store.BHP@.Nobody      THREW 'Malformed ...'       THREW 'Path to object does not exist'
  .Store.RIO@.Currency    THREW 'Malformed ...'       (void)
  .Store.BHP..Currency    THREW 'Malformed ...'       THREW 'Malformed ...'   ← until §15
  .Store.BHP.@Currency    THREW 'Malformed ...'       THREW 'Malformed ...'   ← until §15
```

The miss rules are the component's own delimiter's, unchanged (§13): `.Nobody` throws because
it is spelled with a `.`, and `.Store.RIO@.Currency` answers void because it is the **`@`**
that misses — `RIO` has no collection at all.

`.Store@.Venue` is worth its own line. It is the only root-path spelling that reaches one of
the **root's** attributes through its collection, and until now the root's collection could
only be named at the very end of a path, where there is nothing left to ask it for.

### The assertion it uncovered

Legalising `@` makes `Item@@Tag` spellable, so it needs an answer. A collection has no
attributes of its own — it is not an item; a `VBLockAttr` carries `aParent` and its members
and nothing else — so the answer is void. The selector's attribute arm raised `ASSERT(0)`
first, which is a debug-build event for something only a caller could have written.

That is §9's rule again, and the same shape as the never-created collection it found: what a
caller spells is the caller's business, and only what the library itself could not have
meant is an assertion. Nor is it a new path — `Item@^@Tag` splits cleanly without any of
this, and asserted on its way to exactly this void. The probe's assertion count goes 1 to 0.

## 15. The descendant collection, and one rule for all three

The last collection with no path. §12 gave the attribute one a path and argued this one
could not have one:

> a trailing `.` is dropped, and it cannot stop being dropped, because `.` is the root
> delimiter too and `.Root.` has always meant the root.

That argument does not hold. The root delimiter is consumed **once**, at position zero,
before a single component is parsed — `if ( *lpszWorkingPath++ != L'.' ) return FALSE;` is
the whole of it. Every other `.` in a path is a descendant delimiter. What actually stood in
the way was narrower and duller: the splitter refused a component one character long, which
is the same clause that refused a bare `@` until §14.

With that gone, one sentence covers every delimiter there is, and
`P3Pmsg__IsBareComponent` is where it now lives:

> **A delimiter with no name after it names the collection it introduces.**

`^` names the pushed value of whatever is to its left, `@` the attribute collection, `.` —
with its aliases `\` and `/` — the descendant one. It arrived a delimiter at a time,
each step argued from the one before it: `^` in §10, `@` in §12 and then §14, `.` here.

### Measured

```
                          before                  after
  GetPath(desc coll)      (no overload)           .Store.BHP.
  GetPath(snap coll)      (no overload)           .Store.BHP^.
  GetPath(root coll)      (no overload)           .Store.
  .Store.BHP.             BHP                     (desc coll) pos=634
  .Store.                 Store                   (desc coll) pos=212
  .Store.BHP^.            BHP                     (desc coll) pos=1384
  .Store..BHP             THREW 'Malformed ...'   BHP  pos=259
  .Store.BHP..Last        THREW 'Malformed ...'   Last pos=681      ← == .Last
  .Store.BHP..Nobody      THREW 'Malformed ...'   THREW 'Path to object does not exist'
  .Store.RIO.  (no descs) RIO                     (void)
```

The first four "before" answers are the failure §10 was written to eliminate — a path asking
for one thing, answered with another, and nothing in the result to tell them apart. A
trailing `.` was the case §10 did not reach, and `.Store.` was the shortest way to spell it.

### The block underneath

`P2PmsgDesc_GetVBLockParentnn` carried the defect §12 found in `P2PmsgAttr_GetVBLockParentnn`
and fixed only there: it read `GetField()->r_Object()`, the owning **item's** block, through
`VBLock_pDesc`, picking `VBLockDesc`'s fields out of a `VBLockItem`'s `ut` union. It had no
caller, so it had never fired. `P3Pmsg_GetPath`'s new `P3PmsgDesc` overload is its first.

### Three consequences, each from the rule

**A bare component that misses never throws,** whichever delimiter introduced it. The paging
callback exists for a named child that may not be in memory yet; a bare delimiter asks for a
collection, and an item that has never had one has no block for it. `.Store.RIO.` reports
that the way `.Store.RIO@` always has (§12) — reporting the same fact two ways, depending on
which collection was asked for, would be nothing but an accident of the delimiter. A
component carrying a **name** still throws on a miss: `.Store.BHP..Nobody` does.

**A collection has neither collection of its own,** because it is not an item. `@.`, `.@`,
`..` and `@@` are four well-formed questions naming nothing, so all four answer void — where
three of them raised `ASSERT(0)` first, two newly reachable because a bare `.` is a
component now. §9's rule again: what a caller spells is the caller's business.

**`RootPath2Object` strips a descendant delimiter only where a name follows it.** `@` and
`^` were never stripped, because for those the delimiter *is* the instruction; that is just
as true of a lone `.`. Stripped anyway, it became the empty string, and the selector went
looking for a descendant with no name.

### What it cost

`.Root..Alpha` stops being malformed. §10 made it throw and pinned that as its headline, on
the grounds that an empty component is an empty name; under the rule above it is not empty
at all — it is the root's descendant collection, and then Alpha, which is Alpha. A
redundant spelling, the way `.Root.Item@.Tag` is a redundant spelling of `@Tag` (§14), and
redundant is not malformed.

What the splitter still refuses is a path that does not begin at a root, and `Store.BHP`
— §10's other case — is what now pins the verdict being honoured at all.

## 16. A `P2Pos` had one kind of path

`P2PmsgMgr::P2Pos2Path` is how a caller holding a raw handle asks what it is holding —
the C entry point `msgcore_mgr_p2pos2path` is a thin wrapper over it, and so is every
consumer that stores a `P2Pos` and wants to print where it points. It read:

```cpp
if ( IsField(pos) )
{
  P3PmsgField oField = P2Pos2Field(pos).r_Object();
  return P3Pmsg_GetPath ( &oField );
}
else ASSERT(0);
return CString();
```

`IsField(pos)` is `VBLockItem_IsField`. A list is not a field and a vector is not a
field; a collection is not an item at all. So of the eleven things a `P2Pos` can name in
a small store, four answered and seven asserted:

```
-- items --
  root                   -> '.Store'                 back=SAME
  field BHP              -> '.Store.BHP'             back=SAME
  field Last             -> '.Store.BHP.Last'        back=SAME
  attr Currency          -> '.Store.BHP@Currency'    back=SAME
  list Numbers           -> ''                       [ASSERT]
  vect Vec               -> ''                       [ASSERT]
-- collections --
  BHP attr coll          -> ''                       [ASSERT]
  BHP desc coll          -> ''                       [ASSERT]
  root desc coll         -> ''                       [ASSERT]
  list attr coll         -> ''                       [ASSERT]
-- neither --
  zero                   -> ''                       [ASSERT]
asserts=7
```

Every one of those six objects **has** a path. §6 taught
`P3Pmsg_GetPath(const P3PmsgField*)` about containers, and `P3PmsgList` and `P3PmsgVect`
both derive from `P3PmsgField`, so that overload has built a list's path and a vector's
path all along. §12 gave the attribute collection an overload of its own and §15 gave the
descendant collection one. Every piece was in place. This was the one caller that could
reach none of it — an object whose path `P3Pmsg_GetPath` would build and the manager
would not.

**The kind is already known.** `P2Pos2Object` hands back a `P3PmsgObject`, and a
`P3PmsgObject` answers `IsField`, `IsList`, `IsVect`, `IsAttr` and `IsDesc`. §13's walk
carries a `P3PmsgObject` for the same reason: it is the one type here that knows what it
is standing on.

**The order the questions are asked in is not cosmetic.** `IsAttr` and `IsDesc` read the
block header, which every block has. `IsField`, `IsList` and `IsVect` go straight to
`VBLock_pItem` and read a `VBLockItem`'s fields out of whatever block is there — on a
collection block that is the same `ut` union misread `P2PmsgAttr_GetVBLockParentnn`
records in §12. Asked collections-first, the question is never put to a block that cannot
answer it.

**A collection needs its owner before it can have a path.** Neither collection overload
takes a `P3PmsgObject`. Both reach the owner through the collection's back-pointer to the
field it hangs off — `GetField()` — and `P3PmsgAttr(const P3PmsgObject&)` sets only
`m_oObject`, leaving that pointer null. A collection converted straight from an object
therefore builds a lone `@` or `.` with nothing in front of it, which is a string and not
a path. So the owner is fetched first: `GetParent()` on a collection block is exactly the
item the collection hangs off, and `r_Attr()` / `r_Desc()` install the back-pointer the
overloads want.

**And the `ASSERT(0)` goes.** A `P2Pos` naming a name block, a data block or nothing at
all is not a caller error worth an assertion — §9's rule is that what a caller spells is
the caller's business. The empty string says "no path" the way a void `P3PmsgObject` says
"no object", and the two are already kept apart one level up: `msgcore_mgr_p2pos2path`
answers `nullptr` only when the call **threw**.

All eleven now answer, and every path handed straight back to `RootPath2Object` lands on
the `P2Pos` it was built from:

```
-- items --
  root                   -> '.Store'                 back=SAME
  field BHP              -> '.Store.BHP'             back=SAME
  field Last             -> '.Store.BHP.Last'        back=SAME
  attr Currency          -> '.Store.BHP@Currency'    back=SAME
  list Numbers           -> '.Store.Numbers'         back=SAME
  vect Vec               -> '.Store.Vec'             back=SAME
-- collections --
  BHP attr coll          -> '.Store.BHP@'            back=SAME
  BHP desc coll          -> '.Store.BHP.'            back=SAME
  root desc coll         -> '.Store.'                back=SAME
  list attr coll         -> '.Store.Numbers@'        back=SAME
-- neither --
  zero                   -> ''                       (no path, no assertion)
asserts=0
```

`Test_P2Pos2Path` pins all four groups, and it has teeth: the test framework folds a CRT
assertion into the running case as a failure, so against the unfixed library all four
cases fail — 25 checks — and the fourth reports `P2PmsgMgr.cpp(808) : Assertion failed!`
by name.

## 17. `.^`, and the last delimiter that was not bare everywhere

§9's rule is that `^` commutes with `@` and with `.`. It did for `@`: `Item@^` and
`Item^@` both name the attribute collection inside the snapshot, and §10 and §14 are the
two steps that made that true. For `.` only one direction worked.

| spelling | was | ought |
|---|---|---|
| `Item^.` | the snapshot's descendant collection | — |
| `Item.^`, as a root path | the snapshot **item** | the snapshot's descendant collection |
| `Item.^`, as an object path | void | the snapshot's descendant collection |

Three places tested for a `.` at the **end** of the path, where the rule §15 states is a
`.` with no **name** after it.

**The walk's strip, in `RootPath2Object`.** The splitter seeds a component with the
delimiter that introduces it and absorbs a following `^` — it does exactly this for `@^`
(§10) — so `.Store.BHP.^` reaches the walk as one component, `.^`:

```
  .Store.BHP.^           -> TRUE  root='Store' [.BHP][.^]
  .Store.BHP^.           -> TRUE  root='Store' [.BHP][^][.]
  .Store.BHP@^           -> TRUE  root='Store' [.BHP][@^]
```

The strip fired on "is there anything after the dot", so `.^` became `^` and selected the
item's own snapshot. `@^` was never stripped, because `@` is not the descendant
delimiter — which is why the two spellings one line of reasoning apart behaved
differently.

**The wrapper, `P3Pmsg_SelectObject`.** A leading `.` means "this component names the
object you are standing on", and the guard against that reading was `lpszObjectPath[1] ==
0`. `.^` is two characters, so it took the matching route: `ParseObjectPath` stopped on
the `^` with an empty name, and the empty name was compared against `BHP`.

**The field arm of `P3Pmsg_SelectObjectRecurse`.** The `@` arm hands the **rest** of the
path to the attribute collection, which is what makes `Item@^` reach the collection's
snapshot. The `.` arm handed the rest back to the **item**, so `Item.^` re-entered the
same arm and took the `^` branch.

All three now ask `P3Pmsg_IsPathDelimiter`, which answers TRUE for the terminator as well
as for the five delimiters — one call covering both halves of "no name after it". A
following **name** still goes back to the item: `Item.Last` wants a descendant by name,
and the field arm's own tail already looks one up through `r_Desc().r_Curs().Goto`, so
both routes land on the same object and the shorter one is left alone.

**Measured as agreement between the two spellings of one question.** A root path
`.Store.BHP<suffix>` and an object path `<suffix>` taken from `BHP` ask the same thing,
and the walk is built to make them agree — it strips a name-carrying `.` off each
component and hands the rest to `P3Pmsg_SelectObject`. Over 29 suffixes:

```
  suffix       root .Store.BHP+       object from BHP        agree
  .^           845 SNAP item          0 (void)               NO      -> both 1220 SNAP desc coll
  .^.          1220 SNAP desc coll    0 (void)               NO      -> both void
  ..Last       681 Last live          0 (void)               NO      -> both 681
  .^.Last      1267 Last SNAP         0 (void)               NO      -> both 1267
  ..Close      1431 Close live        0 (void)               NO      -> both 1431
  .Last        681 Last live          0 (void)               NO      -> unchanged, and §18
  .Close       1431 Close live        0 (void)               NO      -> unchanged, and §18

  22 agree, 7 differ     ->     27 agree, 2 differ
```

`.Store.BHP.^.` is the one spelling that **loses** an answer, and it should. It reached
the snapshot's descendant collection only because `.^` meant the snapshot **item**, so the
trailing `.` was asking an ITEM for its collection. With `.^` naming the collection, the
trailing `.` asks a COLLECTION for one — the same void `..`, `@.`, `@@` and `^^` all
answer, and §15's rule that a collection is not an item. The object is not lost: `.^` and
`^.` both name it, which is the whole point.

## 18. A leading `.` was the root marker everywhere

`P3Pmsg_GetPath` says what a path is rooted at, in its own first comment:

```
//  Process root
//  NOTES: Defined by absence of parent.
//       : May be physical root object ":Rootname", or
//       : Floating item ".item"
```

It emits `.item1.item2@item3` for an object in a tree and `.item` for a floating one, and
the **leading** component names the object the path starts from. `P3Pmsg_SelectObject`
asserted that name at every depth instead. So `.Last` asked `BHP` whether `BHP` is called
`Last`, while `.Store.BHP.Last` descends — `RootPath2Object` strips a name-carrying `.`
off each component before it selects, and the raw object path has no such strip.

The test is `HasParent()`. An object with a parent is not where a rooted path starts, so a
leading `.` there is an ordinary descendant delimiter. That is §17's sweep finished:

```
  .Last        681 Last live          0 (void)        NO    ->   both 681
  .Close       1431 Close live        0 (void)        NO    ->   both 1431

  27 agree, 2 differ     ->     29 agree, 0 differ
```

**The root keeps the assertion**, and `P2PmsgMgr::Path2Object` is what needs it: a path
rooted somewhere else has to be refused rather than hunted for among the root's
descendants. The test for that now puts a descendant carrying the wrong root's name into
the store first, so the refusal cannot be an accident of there being nothing to find.

**Measuring it turned up a second defect in the same function.** A leading component that
**matched**, with nothing after it, fell through to the recurse carrying an empty path —
where `ParseObjectPath` produced an empty name and the `Goto` for it matched nothing:

```
  root  + ".Store"          -> (void)      ->   pos=48, the root
  float + ".Floater"        -> (void)      ->   the floating item
  desc coll + ".Last"       -> (void)      ->   pos=470, as "Last" already answered
  attr coll + ".Currency"   -> (void)      ->   pos=1056, as "Currency" already answered
```

The first of those is §11's subject one level up: the library could not resolve the path
it emits for a root. The last two are a leading `.` on a **collection**, where it was never
the root marker at all — both arms descend by name, which is what `Goto` and `Exists` do —
so only the discarded match was ever wrong there.

The two collection arms also move **above** the item one, for the reason §16 gives: `IsAttr`
and `IsDesc` read the block header, which every block has, while `IsField`, `IsList` and
`IsVect` read a `VBLockItem`'s fields out of whatever block is there.

**What goes with it, and it is a loss.** `.BHP.Last` asked *of* `BHP` used to resolve, by
matching `BHP`'s own name and then descending. That is the assertion reaching where no path
is rooted. It has no caller in this tree and none in Chartboard, and the same object is
`Last`, `.Last` or `..Last` from there — but it did work, and now it does not.

## 19. A floating item was its own storage

§18 took `P3Pmsg_GetPath`'s opening comment at its word about both of the things a path
can be rooted at. The second is a **floating item** — one never linked into a tree — and
asking one for its own identity gave a different answer every time:

```
  oFloat=79034294752  copy=79034285552  copy=79034286304  copy-of-copy=79034287056
```

Not a SYS-heap address, as §19 first recorded it. **The block is inside the object.**
`P3PmsgField::RenderThisSafe` builds a floating item's `VBLock` in
`P3PmsgObject::m_oVBLock` — an array declared in the class — and `Connecta`s it with no
heap at all. So the object is not a handle on the item; it **is** the item, and a copy of
the object is a second item.

The number was the symptom, and it understated what it meant:

| asked of a floating item | was | now |
|---|---|---|
| `GetP2Pos` through any handle | a different number per handle | one number |
| a write through a handle | went to that handle's own copy | reaches the item |
| `SelectObject(&o, GetPath(&o))` | a duplicate of it | it |
| a handle outliving the object | read the dead object's storage | reads the block |

Two paths copy a `P3PmsgObject`, and they did different things with an inline block. The
copy constructor `memcpy`s it into the copy's own array — a duplicate, which is where the
four numbers above come from. `Connect`, which `operator=` delegates to, says in its own
comment what it wanted instead:

```cpp
    // Cannot share heap that does not exist
    // NOTES: Create heap and place data on heap
```

It created the heap and placed nothing on it, leaving `m_aVBLock` addressing the
**source's** array and copying that pointer as though it were an address on the new heap.
A SYS heap resolves an address by returning it — `P2PmsgHeap_Addr2Phys` is the identity
for that type — so the result read correctly and dangled the moment the source went out
of scope. Under it stood `ASSERT(oObject.m_nVBLockSize==0)`, marked *"It's a bug should
this occur"* with a `TODO: Code around this issue`, on a condition the guard above had
already excluded: every path that reached that arm asserted.

`RehomeInlineItem` is the "place data on heap" half. The block moves onto a SYS heap of
its own the first time it is shared — from the copy constructor and from `Connect` — and
both handles then name it the way they already name an item in a tree.

**Only an item.** A standalone block that is not one is a **value**: `P3PmsgData` and
`P3PmsgName` use the same inline storage through `ConnectVBLock`, and duplicating one is
what copying a value means. The discriminator is the block header, which every block has,
for the reason §16 gives. `Connect` now duplicates a value block rather than creating a
heap for it, so the two copy paths agree — which is the principle the copy constructor's
own NOTES had already established for the void case.

**Nothing points at the block yet.** A collection or a push is allocated through
`AllocVBLock`, which creates the heap when there is none. So an item with no heap has no
attributes, no descendants and no stack, and no back-pointer needs fixing up when it
moves; the `m_hVBList` test at the top of the rehome is that invariant, not an
optimisation. A floating item that already HAS a heap was one object under copy before any
of this, which is how §19 could report the number stable when asked twice and unstable
across copies in the same breath.

**It costs nothing.** 20000 insertions of a floating temporary, three runs each side:

```
  before   125 / 141 / 140 ms
  after    125 / 140 / 125 ms
```

Nothing on the insertion path takes a `P3PmsgObject` by value, so nothing rehomes. The
move is lazy by construction — an item that is never shared never leaves its object.

## 20. "Is there anything here?" had four answers

§19 left one thing recorded: a `P3PmsgField` copies as a value, a `P3PmsgObject` copies
as a handle, and nothing at the call site says which you asked for. The question that
follows from it is what a caller CAN ask. Measuring that found the same defect one level
down — **the question has two spellings on every class in this family, and they
disagreed**:

| class | `IsVoid()` | `operator bool` |
|---|---|---|
| `P3PmsgObject` | `m_hVBList==0 && m_aVBLock==0` — denotes nothing | `m_hVBList!=0` — is on a heap |
| `P3PmsgField` | `m_hVBList==0` — is on a heap | has a name or non-null data — is populated |
| `P3PmsgAttr` / `P3PmsgDesc` | — | delegates to the object's |
| `P3PmsgList` / `P3PmsgVect` | the field's | **`IsVoid()` — the answer inverted** |

Four meanings for one question. Asked of a floating item, two said yes and two said no,
and which two depended on which class you happened to be holding:

```
  default-constructed   field: IsVoid=yes bool=no   |  object: IsVoid=no  bool=no
  floating, named       field: IsVoid=yes bool=yes  |  object: IsVoid=no  bool=no
  in a tree             field: IsVoid=no  bool=yes  |  object: IsVoid=no  bool=yes
  a value copy of it    field: IsVoid=yes bool=yes  |  object: IsVoid=no  bool=no
```

Read the second row twice. `oField.IsVoid()` says there is nothing here and `if (oField)`
says there is, in the same breath, about the same item.

**§19 is what made this urgent rather than untidy.** An inline item is now rehomed onto a
heap the first time it is shared, so every answer spelled *"is there a heap"* changes when
somebody takes a copy of the handle:

```
  before sharing : field IsVoid=yes  object bool=no
  after  sharing : field IsVoid=no   object bool=yes
```

Nothing about the item changed; it is still called `Floater`. Asking a question must not
be what decides its answer, and that is the argument the rest of this section rests on.

So: **`IsVoid()` means "denotes no item" on every class, and `operator bool` is exactly
`!IsVoid()` on every class.** `P3PmsgObject::IsVoid` already asked that — it has since
2025-02-18 — and is untouched; the other five now follow it. All four rows above now read
the same across all four columns.

**Nothing that exists changes behaviour.** Every caller of `P3PmsgObject::operator bool`
in this tree and in Chartboard — six of them, found by deleting it and building
everything — reads it as *"did I get anything?"*: `P3PmsgAttr` and `P3PmsgDesc` delegate
to it, `GetParent` walks are guarded by it, and a `RootPath2Object` miss is detected with
it. Each is handed either a tree object or a void one, the two states where the old
answer and the new one agree. `P3PmsgList` and `P3PmsgVect` have no caller at all, which
is why an inverted answer had gone unnoticed.

### The one caller of the field's, and it was a bug

`P3PmsgField::operator bool` had exactly one caller in the solution, in `CListCtrl_Ext`,
which walks a row's columns and looks each one up by its header text:

```cpp
    P3PmsgItem oItem = oItemRow.r_Desc().SelectObject(lpszColumnText);
    if ( !oItemRow )
      continue;
    oCListCtrl.SetItemText ( nItem, nSubItem, oItem.r_data().ToString() );
```

It tests `oItemRow` where it means `oItem` — one token, and the row is the loop invariant
that is never void here — so the guard could not fire and a column the row does not carry
fell through to `r_data()` on a void field.

The guard as written could not have helped either, which is the part worth keeping. The
old `operator bool` answered *"is it populated"* by reaching through to `r_data()`, so
evaluating it on a field over a failed lookup **faults**:

```
  lookup missed: object IsVoid=yes
  field over it: IsVoid=yes   asserts=0
  about to evaluate `if ( oMissField )` ...
                                            <-- exit 3
```

Against the fixed library the same probe prints `... it returned false`. Only the line
after it — `r_data()` on a void field, reached by deliberately ignoring the guard — still
faults, which is what the guard is for.

### The other half of §19's note is stated, not changed

`P3PmsgField`'s copy constructor carried an arm that `AddRef`'d rhs's heap and shared its
block — a HANDLE copy — behind `if ( !OBJ__hVBList )`, testing a member of THIS field that
`RenderThisSafe` had zeroed on the line above. `Connecta(0,...)` returns early when the
handle it is given already matches, so the guard was true on every call and the arm below
it never ran.

It is removed rather than repaired. Reviving it would flip every field copy in this tree
and in Chartboard from a value to an alias, silently, which is a decision and not a bug
fix. What the class does is now what the class says:

```
  P3PmsgField oB = oA;               a copy of the item
  P3PmsgField oB = oA.r_Object();    the item
```

The library writes the second wherever it means to write through — all seven such sites
in the solution do, `P3PmsgRefactor_DataType` and `P2Pmsg_UpgradeMove` among them —
because a collection hands back its own cursor and the next `SelectItem` moves it:

```
  bound to AAA: name='AAA' pos=259
  after selecting BBB, the SAME reference reads 'BBB' pos=423
```

Three meanings, then, for one expression, and the suite now pins all three.

## 21. `==` and `!=` asked two different questions, and neither was identity

§20 left it recorded that a `P3PmsgField` copies as a value, that
`P3PmsgField oB = oA.r_Object()` copies as a handle, and that nothing at the call site
says which you are holding. The first thing a caller would try is to ask the pair. So
this is what `oA == oB` answered.

**It answered whether both of them are non-void.** `operator bool` was an IMPLICIT
conversion, so the built-in `operator ==(int, int)` was a viable candidate and the
expression meant `(int)(bool)oA == (int)(bool)oB`. Against a store holding `AAA=1` and
`BBB=2`:

```
  oTree == oHnd   (a handle on the same item)  -> true    (the item: SAME)
  oTree == oCopy  (a value copy of it)         -> true    (the item: different)
  oTree == oOther (a different item entirely)  -> true    (the item: different)
  oTree == oEmpty (an empty floating item)     -> true    (the item: different)
  oTree == oVoid  (a lookup that found nothing) -> false   (the item: different)
```

Five pairs and three wrong answers. Two of them are right — the handle, and the void —
and both are right by accident: the only `false` this can produce is "exactly one of us
is void", which is not a question anybody asks of a pair.

**`!=` did something else again, and it was not the negation.** Declaring `operator ==`
in `P3PmsgField` hides the base class's `==` — but name hiding is per name, so it does
not hide `P3PmsgData::operator !=`, which was inherited, visible, and a comparison of the
DATA. The same pair therefore answered both ways at once:

```
  oTree == oOther                               true       <- "are we both non-void?"  yes
  oTree != oOther   -- the SAME pair            true       <- "do our values differ?"   yes
```

and on a field over a failed lookup, where there is no block to read, `!=` reached
through to it and **faulted**:

```
  oVoidA == oVoidB                              true ok   asserts=0
  oVoidA == oTree                               false ok  asserts=0
  oVoidA != oTree
                                                          <-- exit 3
```

**And the one comparison this class always meant to offer was the only one that did not
compile.** `P3PmsgName` declares both of its comparisons `const`; `P3PmsgField` hides
them with one that was not. Every expression below was put to the compiler on its own:

| expression | before | after |
|---|---|---|
| `if ( oField )`, `!oField`, `a && b`, `?:`, `static_cast<bool>` | compiles | compiles |
| `if ( oObject )`, `if ( oList )`, `if ( oAttr )`, `if ( oDesc )` | compiles | compiles |
| `bool b = oField` / `return oField` | compiles | **rejected** C2440 |
| `int n = oField` / `int n = oObject` | compiles | **rejected** C2440 |
| `oField + 1` | compiles | **rejected** C2678 |
| `oAttr == oDesc` | compiles | **rejected** C2678 |
| `oFieldA == oFieldB` / `oFieldA != oFieldB` | compiles, wrongly | compiles, correctly |
| `oObjectA == oObjectB` / `!=` | both, `!=` via bool | both, `!=` the negation |
| `oConstField == L"AAA"` | **rejected** C2678 | compiles |

Everything compiled except the one thing the class was for.

So: **`operator bool` is explicit on all six classes, and the question the fall-through
was answering badly is given a real operator.**

- `P3PmsgField::operator ==` and `!=` against another field ask `P3PmsgObject`: the same
  heap handle and the same block address, and therefore the SAME ITEM. A handle is equal
  to the item it was taken from; a value copy of that item is not, however identically it
  reads.
- `P3PmsgObject` gains `!=` for the same reason.
- `P3PmsgField::operator == ( LPCTNAM )` is `const`.
- `if ( o )`, `!o`, `a && b`, `a ? x : y` and `static_cast<bool>(o)` are contextual
  conversions and are untouched. A string literal still selects the name comparison, not
  an identity test against a temporary field carrying that name — the suite pins that.

The same two probes against the fixed library:

```
  oTree == oHnd   (a handle on the same item)  -> true    (the item: SAME)
  oTree == oCopy  (a value copy of it)         -> false   (the item: different)
  oTree == oOther (a different item entirely)  -> false   (the item: different)
  oTree == oEmpty (an empty floating item)     -> false   (the item: different)
  oTree == oVoid  (a lookup that found nothing) -> false   (the item: different)
  oTree != oOther   -- the SAME pair            true ok   asserts=0
  oVoidA != oTree                               true ok   asserts=0
```

**Nothing that exists changes behaviour, and that is measured twice.** Marking every
`operator bool` explicit and compiling Msgcore, MsgcoreMFC, TargetCore, TargetCoreMFC,
MsgcoreUtils, MsgFacade, TargetFacade, MscsUnitTests and Chartboard in both
configurations produced **0 errors**. Declaring `==` and `!=` between two of these
objects as `= delete` and repeating the sweep produced **0 errors**. Not one call site in
the built tree used either — which is why a comparison that was wrong in three ways had
never reported anything.

### The asymmetry that is left on purpose

`oField == oObject` compiles, because a `P3PmsgObject` converts to a `P3PmsgField`;
`oObject == oField` does not, because the conversion back is spelled `r_Object()` and is
a call. Making it symmetric means either a second implicit conversion or an overload
taking one side as an object and the other as a field, and both of those hide which side
is being asked. `oObject == oField.r_Object()` does not, so that is the spelling.

## 22. "Is there a heap" was standing in for "where is the block"

What was left after §21 was recorded like this: a field cannot say, alone, whether a
write through it reaches anyone, and closing that needs a comparand. It needs one for
*whose*. It does not need one for *where* — and the library was already asking *where*, in
three places, in a spelling that answers something else.

**A `P3PmsgObject` can hold a heap handle and still address its own inline array.** It is
not an exotic state. A floating item grows into it, and this is where:

```
      8 wide chars: heap=no  block=INLINE (mine)
     64 wide chars: heap=no  block=INLINE (mine)
    128 wide chars: heap=yes block=INLINE (mine)
   4096 wide chars: heap=yes block=INLINE (mine)
```

`AllocVBLock` creates a private SYS heap for a payload the inline block cannot hold, and
leaves the item block where it is. 128 wide characters — a description, a path, a SQL
fragment — is enough.

**On that state, "is this shareable" answered yes about a block that was nobody's but its
own.** `RehomeInlineItem` declined on `m_hVBList != 0` and called it "already on a heap";
the `P3PmsgObject` copy constructor and `Connect` both rehomed only when
`rhs.m_hVBList == 0`. So nothing moved, the share arm ran, and `m_aVBLock` was copied
verbatim — **an address inside the source object**:

```
     floating, untouched    heap=no  aVBLock=INLINE (mine)  size=340
     floating, grown        heap=yes aVBLock=INLINE (mine)  size=340
  .. copy the object out -- this is what r_Object() hands a caller
     the copy addresses the SOURCE's inline array? YES
  .. source destroyed; the copy is all that is left
     it still addresses that dead stack frame? YES
```

`Connect`'s own NOTES describe this exact failure and say it was fixed: *"left m_aVBLock
pointing into the SOURCE's inline storage … reads correctly and dangles the moment the
source goes out of scope."* It was fixed for the arm the guard reaches. The guard does not
reach this one.

**What it cost.** A factory that builds a floating item and hands back `r_Object()` — the
documented way to return a handle — returns an address in a frame that has already been
popped:

```
  handle returned:   block=elsewhere  name=''  reads 4242
                                                          <-- and then exit 3
```

The name is already gone. The data survives, because the payload is on the AddRef'd heap
and only the ITEM block was in the dead frame — and the next read, once that frame is
written over, faults. Every read that did not fault reported `asserts=0`, because nothing
in the library was looking.

### The fix is where the state is made, not where it is read

The state is created in exactly one place, and that place is the last instant at which
`RehomeInlineItem`'s own stated invariant still holds. Its NOTES say it out loud:

> NOTHING CAN POINT AT THE BLOCK YET. A collection or a push is allocated through
> AllocVBLock, which creates the heap when there is none — so an item with no heap has no
> attributes, no descendants and no stack, and there are no back-pointers to fix up.

That is true, and it is a **window**, not a standing property. It closes the moment
`AllocVBLock` makes the heap. So:

- **`AllocVBLock` rehomes the item at the instant it creates the heap**, before the
  allocation that prompted it. The "a heap and an inline block both" state ceases to
  exist, and an item's identity changes when it GROWS rather than when somebody asks
  after it — which is §19's rule. That is the check that decided where this fix goes:
  correcting only the two copy paths moved an item when somebody asked for it, and
  §19's `a floating item with descendants keeps one identity` failed. Rehoming at
  creation keeps it passing.
- **`RehomeInlineItem` asks where the block is**, which the line beneath the old guard
  was already doing, and allocates on the heap the object has rather than making a
  second one.
- **The copy constructor and `Connect` ask the same question**, and their value-copy arm
  now runs on "the block is still inline" instead of "there is no heap" — which is what
  keeps a standalone `P3PmsgData` that has grown from dangling the same way.

### And the question a field can now be asked

`P3PmsgObject::IsInline` and `P3PmsgField::IsInline`. True means **this object IS the
storage**: a write through it reaches nobody, which is exactly what a caller who meant to
write THROUGH has got wrong. False means the block is on a heap and this object is one
name for it. It agrees with what a write actually does on every shape a field can have:

```
  a handle on a tree item          IsInline=false   seen by the store      YES       agree
  a value copy of a tree item      IsInline=true    seen by the store      no        agree
  the collection's own cursor      IsInline=false   seen by the store      YES       agree
  a plain floating item            IsInline=true    seen by its source     no        agree
  a grown floating item            IsInline=false   seen by its source     YES       agree
  a shared floating item           IsInline=false   seen by its partner    YES       agree
  a copy of a grown floater        IsInline=true    seen by its source     no        agree

  agree=7 differ=0 asserts=0
```

It is **not** `GetP2PmsgHandle() != 0`, which is the heap question and still answers it.
Those two disagreed on precisely the state this section opened with, and that disagreement
was the defect.

### What it does not settle

It says where the item is, not whose it is. A value copy that has since grown has an item
of its own on a heap of its own, and answers `false` while reaching nothing the caller
holds:

```
  a grown copy of a tree item      IsInline=false   seen by the store no
```

For *whose*, there is a comparand and it is §21's `==`. What `IsInline` adds is the answer
that needs none — `true` is a guarantee that a write goes nowhere — and §21 already pins
that `==` and the write-through agree.

### The measurement

Nine solutions in both configurations: **0 errors**. The suite, static and DLL, 192 cases;
`MscsUnitTests` 125; C4; the golden image byte-identical at 4104 bytes; §17's agreement
sweep 29 agree 0 differ; §18 and §19 unchanged; Chartboard 0 errors and its four drivers
17, 13, 24 and 15 checks, none failing.

Against the unfixed library the suite **does not compile** — `IsInline` is what it is
asking for. With every `IsInline` check lifted out, what is left is behaviour, and it
bites:

```
  FAIL [a handle on a grown floater names the item, not the source]
  FAIL [assignment of a grown floater names the item too]
  FAIL [growing an item moves it; asking after it does not]
```

and a fourth case, `a handle outlives the frame that built it`, raises three internal
assertions — `MsgVBHeap.cpp(1396)`, `P2Pmsg.cpp(2853)`, `P2PmsgVBLock.cpp(647)` — and
then **hangs the runner**, so the run does not finish at all.

## 23. A value copy copied the block, not what the block pointed at

§22 left the copy constructor's value arm looking right: the block is duplicated with a
memcpy and the copy addresses ITS OWN array rather than the source's. That is a value copy
only while the whole value fits in the block.

**A value that outgrows its block does not stay in it.** `P2PmsgObject_NewVBLockData` puts
the payload in a SECOND block on the heap and leaves a CHAIN POINTER behind in the first;
`P3PmsgName_ResizeName` does the same for a name. So from the first growth on, what the
memcpy copies is an address:

```
  source chain   0x1e2df912dd0
  copy   chain   0x1e2df912dd0
  one block?     ** YES **
  source payload 0x000001E2DF912DD9
  copy   payload 0x000001E2DF912DD9
  wrote through the copy; the source reads it back? ** YES -- ONE PAYLOAD **
```

**Which is the same mistake §22 fixed one level up, and not the same defect.** §22's copy
addressed the SOURCE OBJECT and dangled the moment the source died. This addresses a heap
that both of them hold open — the handle is AddRef'd before the memcpy — so it stays
readable, and nothing faults. What it does instead is alias: a write through either is
seen by the other, and the first of the two to retype hands the block back to the heap
while the other still chains to it. `P2PmsgObject_NewVBLockData` walks the chain and
`Free()`s what it finds, which is exactly how a value gets retyped.

### The heap is meant to be shared; the block is not

`PrivatiseInlineChain` runs where the two callers memcpy. It walks whichever chain the
inline block carries — `VBLock_Data` or `VBLock_Name`, the only two kinds of inline block
that are not items — and gives this object its own copy of every link.

- **On the heap they already share, not a new one.** The handle was AddRef'd by the caller
  before this runs and outlives either object on its own, so the payload stays where every
  accessor already resolves it. Making a second heap would have been a second answer to a
  question the AddRef had already answered.
- **The whole chain, not its first link.** Chaining is usually a single step —
  `NewVBLockData` collapses what it finds before adding one — but every reader in the file
  loops, so this loops.
- **Items do not come here.** An inline ITEM is rehomed out of the object before either
  caller reaches its value arm (§22), and an object whose block is already on a heap is
  sharing that block deliberately: that is what a handle IS.

### The measurement

```
  -- a grown VALUE, copied --
     the source still chains somewhere            ok
     the copy chains somewhere too                ok
     and it is not the source's block             ok
     the payload is the same payload              ok
     the source reads what it always did          ok

  -- a value grown twice, then copied --          ok / ok / ok
  -- the copy outlives the source --              ok / ok / ok
  -- a grown NAME, copied --                      ok / ok / ok

  failures=0 asserts=0
```

Unshared is only half of what a value copy owes. The other half is the byte-for-byte row
in each of those four sections — `the payload is the same payload`, and its equivalent in
the three the listing abbreviates — and a fix that merely stopped sharing would pass every
`not the source's block` row and fail all four of those.

Nine solutions in both configurations: **0 errors**. The suite 199 cases static and 194
through the DLL; `MscsUnitTests` 125; C4; the golden image byte-identical at 4104 bytes;
§17's agreement sweep 29 agree 0 differ; §18, §19 and §22 unchanged; Chartboard 0 errors and
its four drivers 17, 13, 24 and 15 checks, none failing.

Against the unfixed library the suite builds — nothing new is asked of the library, which
is the difference from §22 — and four cases fail:

```
  FAIL [a grown value copies its payload, not the address of one]
  FAIL [assignment gives the copy its own payload too]
  FAIL [a value grown twice copies what it ended up with]
  FAIL [the copy keeps its payload after the source is gone]
```

### What reaches it

Not much, and that has not changed. `P3PmsgData`'s own copy constructor is a deep one and
does not go through `P3PmsgObject`'s; `P3PmsgName` never copies its object at all. The arm
is reached by copying a value's `P3PmsgObject` directly, which is what `p_Object()` is for
and what the cases above do. The defect was recorded, and recorded as unreached, in the
*what is left* list that followed §22. It is fixed here on the same terms §22's was: the
arm exists, it is the arm that says what a value copy means in this library, and it was
not copying the value.

Five of the seven cases reach below the exported surface — where a value keeps its payload
is a fact about the block, and the block navigation is internal, declared in the library's
headers but not marked `Msgcore_EXT`. Those five build in the static configuration only,
which is the whole of the 199/194 difference above. The alternative was to export three
functions for a test.

## 24. What is left

- **`IsInline` says where an item is, not whose it is.** §22's table above has the one row
  where that matters: a duplicate that has since grown answers `false` and still reaches
  nothing the caller holds. `true` is a guarantee and `false` is not, and the comparand
  that closes it is §21's `==`. A type that carried the distinction in its own right would
  be a different library.

- **A value copy walks the chain; a value copy of a value copy walks it again.** §23
  duplicates every chained block at every copy, which is what a value copy means and also
  what it costs: copying a grown value is now an allocation and a memcpy per link rather
  than a pointer assignment. One link is the usual case and the cases above measure no
  more than that. If a caller is ever found copying grown values in a loop, the answer is
  to hand it a handle — `r_Object()` — and not to make the value copy shallow again.

- **`P2PSafePtr` carries the same shape and was not touched.** It declares
  `operator SafePtrType*()` and `operator bool()` together, so `int n = ptr` compiles
  there too; `ptr == ptr` does not, but for the opposite reason — C2593, the two
  conversions are ambiguous rather than one of them silently winning. A refused
  comparison is not the defect §21 fixed, and it is a raw-pointer template rather than a
  Msgcore object, so it is recorded here and left alone.

Nothing else from this investigation is outstanding. That is not a claim that the grammar
is without defect — only that every case these twenty-three sections measured has an answer,
and that the answer is pinned by a test.

## 25. Reproducing this document

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

The cases are also pinned as regression tests in `tests/MsgcoreSuite.cpp`, run by
`tests\build_run_suite.bat` in both link modes: `Test_Stack` and `Test_RootPath` for §2-§5,
`Test_ListPath` and `Test_StackContainers` for §6-§7, `Test_VectDrop` and `Test_StackDrop`
for §7, and `Test_HeapCoalesce` and `Test_HeapCloserFit` for §8. Note that
`build_run_suite.bat` compiles only the test sources — a change to the library itself does
not reach the suite until `msbuild` has rebuilt it.
