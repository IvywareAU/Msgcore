# The `^` stack path operator

> Status: current as of 2026-09-12. Describes `T_StckDelim` — the third path delimiter —
> what it was for, why no path could use it, and what it resolves to now. Every listing and
> every line of output below was run against this tree; §6, §7 and §40 say how to
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
| `P3PmsgField::IsInline`, read as "is it shared" | where the BLOCK is | **a grown duplicate answered false while reaching nobody — false was never a guarantee (§24)** |
| `P2PSafePtr::operator =`, taking a non-const reference | a temporary fell through to the RAW-POINTER arm | **`sp = MakeSP()` freed the payload and kept addressing it (§25)** |
| `P2PSafePtr`'s copy paths, on an empty source | a count of zero, migrated and then raised to one | **two asserts and a use-after-free for copying an empty one (§25)** |
| `P2PSafePtr`, every member but `operator->` | non-const | **a const safe pointer could not be tested, read, compared or assigned from (§25)** |
| `P3PmsgField::IsSole` | forwarded to the object, which counts holders of the HEAP | **a field's OWN descendants made it answer "somebody else is looking" (§26)** |
| `~P3PmsgObject`, after §23 | closed the heap and freed nothing | **every value copy kept a block on the shared heap — 4841 copies exhausted it (§27)** |
| `P2PSafePtr::operator SafePtrType*()` | an IMPLICIT conversion | **`delete sp` and `sp[0]` compiled, and freed the payload under a live holder (§28)** |
| `P3PmsgField::IsSole`, after §26 | subtracted the two sub-objects a field owns, and stopped there | **a collection that had been WALKED answered "somebody else is looking" (§29)** |
| `VBLockData_Sizeof_uv` | no arm for the chained type byte, so it fell to `ASSERT(0)` | **sizing any link but the last asserted, and a Debug build halts there (§30)** |

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
object, §19) and `c8f1af6` (one question, one answer, §20) and `4f4c334` (one question per operator, §21) and `da22f93` (where the block is, not whether there is a heap, §22) and `f77bb97` (copying the value and not the address of one, §23) and `a42f207` (the guarantee asked of the storage, §24) and `09adc6c` (the safe pointer's own conversions, §25) and `eca8cd1` with `2109c8a` in TargetCore (the three entries What-is-left was carrying, §26-§28) and `f600c52` (the cursor, and a chain of three, §29-§30).

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

## 24. `IsInline`'s TRUE was a guarantee and its FALSE was not

§22 gave a field a question it could answer alone — is the item inside me — and recorded
what that question does not settle. One row:

```
  a grown copy of a tree item      IsInline=false   seen by the store no
```

A duplicate that has since GROWN is on a heap, so `IsInline` answers false; and it is on a
heap of its very own that nothing else holds, so a write through it reaches nobody.
`IsInline`'s false said only *the block is not in here*. It was never a guarantee that
anyone was looking, and that row is where the difference bites.

### The same guarantee, asked of the storage

`IsSole` asks after the STORAGE rather than the block. There are two ways to be the sole
holder of storage, and they are the two ways to own it:

- **The block is INSIDE this object.** Nothing else in the process can address it — §22
  rehomes an item before it is ever shared, and §23 duplicates a value's payload, so an
  inline block is reachable only through the object carrying it. This arm is `IsInline`,
  unchanged and still exactly right about what it claims.
- **The block is on a heap this object is the only holder of.** Every object that names a
  heap holds a reference to it — `Connecta` AddRefs, and so do the copy constructor and
  `Connect` — so a count of one means there is no second object to be looking.
  `P2PmsgHeap_RefCount` reads the count `AddRef` raises and `Close` lowers.

### The measurement

Every shape a field can have, against a named witness. The truth column is what a write
actually reached, never an inference from the predicate being tested:

```
  a handle on a tree item          inline=false refs=6   sole=false | seen by the store    YES  agree
  a value copy of a tree item      inline=true  refs=0   sole=true  | seen by the store    no   agree
  the collection's own cursor      inline=false refs=5   sole=false | seen by the store    YES  agree
  a plain floating item            inline=true  refs=0   sole=true  | seen by its source   no   agree
  a grown floating item            inline=false refs=2   sole=false | seen by its source   YES  agree
  a shared floating item           inline=false refs=2   sole=false | seen by its partner  YES  agree
  a copy of a grown floater        inline=true  refs=0   sole=true  | seen by its source   no   agree
  a grown copy of a tree item      inline=false refs=1   sole=true  | seen by the store    no   agree

  agree=8 differ=0 asserts=0
```

The last row is §22's, and it is the one that changed.

A guarantee is only worth the row that breaks it, so the shapes below were chosen to break
the TRUE rather than to confirm it:

```
  a nested tree item                     refs=9  sole=false seen by a sibling handle YES  agree
  a list and a second name for it        refs=2  sole=false
  a write through the second name        seen by the first YES
  a grown floater alone                  refs=1  sole=true
  ... while a handle is held on it       refs=2  sole=false
  ... once the handle is gone            refs=1  sole=true  reads 44
  a grown duplicate                      refs=1  sole=true
  ... once a second name is taken on it  refs=2  sole=false seen by the duplicate YES  agree
  ... and the store is still not it      refs=2  sole=false seen by the store    no   open (false, unseen)

  agree=2 open=1 GUARANTEE-BROKEN=0 asserts=0
```

Note the middle three. Sole stops the instant a second name exists and comes back when
that name goes away, and the write the second name made is still there — the count is of
holders, and holders come and go. The last row is `open (false, unseen)`, and it is what
the next heading is about: false is not wrong there, it is silent.

### What FALSE still does not say

It is not the opposite guarantee, and the NOTES carry the row that proves it. The count is
of holders of the HEAP, not of names for a BLOCK, so a second holder may be naming
something else entirely — including one of this object's own sub-objects:

```
  a grown floater, no descendants yet    refs=1  sole=true
  ... once it has been given one         refs=2  sole=false
```

A field that has been asked for its descendants keeps a `P3PmsgDesc` that holds the heap,
and answers false from then on while still being the only name for its own item.
Narrowing that needs a count per BLOCK, which the image does not carry. That row is pinned
by a case, so the claim in the NOTES stays true of the code.

So there are three questions now and each has its own answer. Where is the item —
`IsInline`. Can anyone else see a write — `IsSole`, with a guarantee on true. Whose item
is it — §21's `==`, exact in both directions, and the only one of the three that needs
something to compare against.

### The teeth

Different in kind from §22's and §23's, and worth saying plainly. Against the unfixed
library the suite does not compile — `IsSole` is what it is asking for. With the 22
`IsSole` checks lifted out, it passes **207 cases and 1167 checks**: nothing in the library
behaves differently. This section adds a question and changes no behaviour, and that is the
whole of its risk.

Nine solutions in both configurations: **0 errors**. The suite 207 cases static and 202
through the DLL; `MscsUnitTests` 125; C4; the golden image byte-identical at 4104 bytes;
§17's agreement sweep 29 agree 0 differ; §18, §19, §22 and §23 unchanged; Chartboard 0 errors
and its four drivers 17, 13, 24 and 15 checks, none failing.

## 25. `P2PSafePtr` had §21's shape, and two lifetimes turned on it

§21 took an implicit `operator bool` off six Msgcore classes and recorded that
`P2PSafePtr` -- the template in `MsgCollectors.h` that every `...SP` typedef names --
carries the same pair of conversions, `operator SafePtrType*()` and `operator bool()`
declared together with no comparison of its own. It was left alone on the grounds that
what it does with them is REFUSE the comparison rather than answer it wrongly: `sp == sp`
is C2593, the two conversions being equally good ways to reach a built-in `==`.

That reading was right about the comparison and much too narrow about the rest. Two
implicit conversions do more than answer comparisons, and a class that copies by reference
count has lifetimes riding on which overload the compiler picks.

### Every expression, one compile each

Each was put to the compiler on its own, against the same declarations the tree uses:

| expression | before | after |
|---|---|---|
| `if ( sp )` | compiles | compiles |
| `!sp` | compiles | compiles |
| `spA && spB` | compiles | compiles |
| `sp ? 1 : 2` | compiles | compiles |
| `static_cast<bool>(sp)` | compiles | compiles |
| `if ( constSP )` | **rejected** C2451 | compiles |
| `bool b = sp` | compiles | compiles |
| `return sp` | compiles | compiles |
| `int n = sp` | compiles | **rejected** C2440 |
| `sp + 1` | compiles | **rejected** C2666 |
| `spA - spB` | **rejected** C2593 | compiles |
| `sp[0]` | compiles | compiles |
| `delete sp` | compiles | compiles |
| `sp += 1` | **rejected** C2676 | **rejected** C2676 |
| `spA == spB` | **rejected** C2593 | compiles |
| `spA != spB` | **rejected** C2593 | compiles |
| `constSP == spB` | **rejected** C2678 | compiles |
| `sp == pRaw` | compiles | compiles |
| `sp == nullptr` | compiles | compiles |
| `sp == 0` | compiles | **rejected** C2666 |
| `spA < spB` | **rejected** C2593 | compiles |
| `constSP->n` | compiles | compiles |
| `*sp` | compiles | compiles |
| `*constSP` | **rejected** C2678 | compiles |
| `constSP.IsEmpty()` | **rejected** C2662 | compiles |
| `Thing *t = constSP` | **rejected** C2440 | compiles |
| `Thing *t = sp` | compiles | compiles |
| `spA = spB` | compiles | compiles |
| `spA = constSP` | **rejected** C2679 | compiles |
| `spA = MakeSP()` | compiles | compiles |
| `ThingSP c = MakeSP()` | compiles | compiles |
| `ThingSP c = pRaw` | compiles | compiles |
| `ThingSP c = 0` | compiles | compiles |

Thirteen rows moved, and they are four groups. **Six** are a `const` safe pointer becoming
usable at all: only `operator->` was const, so a const one could not be tested,
dereferenced, asked whether it was empty, compared, or assigned from. **Two** are `==` and
`!=`, the comparison this class is for, arriving. **Three** stop compiling -- `int n = sp`,
`sp + 1`, and `sp == 0`, which had been answering "is it non-empty" against zero. And
**two** go the other way: `spA - spB` and `spA < spB` were ambiguous and now resolve
through the pointer arm, because taking the bool candidate away leaves the pointer one
alone. `<` orders two safe pointers the way the pointers order, which is a defensible
thing to be able to do; `-` is the distance between two unrelated payloads, which is not.
Neither is an improvement asked for, and both are recorded rather than defended.

And one row did not move, which is the row that matters most: **`spA = MakeSP()` compiles
in both columns and does not mean the same thing in either.** That is why the rest of this
section is runtime output rather than compiler verdicts.

### What the conversions were doing

`operator = ( P2PSafePtr& )` took its source by NON-const reference, so a temporary could
not bind to it. The only other candidate is `operator = ( SafePtrType* )`, reached through
the implicit pointer conversion -- and that arm stamps a fresh count of ONE, knowing
nothing of the count the temporary is still holding. Counting what happens to the pointee:

```
-- what the conversions do --
  two holders of one Thing                     live=1
  one holder let go                            live=1
  both let go                                  live=0 dtors=1
  assigned from a temporary, still in scope    live=0 dtors=1
  ... and the survivor reads                   FREED STORAGE
  ... its destructor would free it again       SECOND DELETE (suppressed here)
  scope left                                   live=0 dtors=1
  delete sp, with the holder still alive       live=0 dtors=1

asserts=0
```

`live=0` with the assignee still in scope is a read of freed storage, and the assignee's
own destructor is the second delete. The probe takes the pointer back out rather than
crashing on it, because the job here is to report. The same program against the fixed
template:

```
-- what the conversions do --
  two holders of one Thing                     live=1
  one holder let go                            live=1
  both let go                                  live=0 dtors=1
  assigned from a temporary, still in scope    live=1 dtors=0
  ... and the survivor reads                   its own Thing
  scope left                                   live=0 dtors=1
  delete sp, with the holder still alive       live=0 dtors=1

asserts=0
```

One payload, shared, freed once. The LAST row of both listings reads the same, and
that is not an oversight: `delete sp` reaches through the pointer conversion, which
stays, so it frees the payload under a holder that still counts one either way. That
row is §26's entry.

### The empty one

Something else shares these paths, and it had never been put to them. `SwapRef2Shared()`
migrates a count to the heap whenever the source is still holding its own, and neither
copy path asks first whether there is anything to count. An empty source therefore gets a
heap count of ZERO -- which the copy constructor then raises to one, for a pointee that
does not exist:

```
-- the empty safe pointer --
  a copy of an empty one                       empty=yes asserts=0
  ... the copy destructed                      asserts=1
  ... and the source after it                  asserts=2
  assigned from an empty one                   empty=yes asserts=0
  ... both destructed                          asserts=0
  three holders of one Thing                   reads 9/9/9 asserts=0
  ... all three destructed                     asserts=0

asserts=0
```

The copy's destructor asserts, then frees the count; the source's destructor then reads
the freed int. This is reachable only because the copy constructor was repaired earlier in
this same work -- before that it did not compile, so nothing had ever copied one of these.
After:

```
-- the empty safe pointer --
  a copy of an empty one                       empty=yes asserts=0
  ... the copy destructed                      asserts=0
  ... and the source after it                  asserts=0
  assigned from an empty one                   empty=yes asserts=0
  ... both destructed                          asserts=0
  three holders of one Thing                   reads 9/9/9 asserts=0
  ... all three destructed                     asserts=0

asserts=0
```

### The fix

- **`operator bool` is explicit, and const.** The contextual conversions are what anybody
  wants from it and they survive explicit: `if ( sp )`, `!sp`, `sp && x`, `sp ? a : b`,
  `static_cast<bool>`. It does not make this a bool-free type and the table says so -- a
  `SafePtrType*` converts to bool on its own, so `bool b = sp` still compiles and always
  did. Nothing in the built tree used this conversion at all: deleting it outright and
  compiling nine solutions in both configurations gave **0 errors**.
- **`operator SafePtrType*()` stays IMPLICIT, and that was measured rather than assumed.**
  Deleting it named eight call sites -- `PostP2PeerMsg(spMsg)`, `RemoveP2PmsgPump(spPump)`,
  `P2PeerMsg *pMsg = spMsg` and their kind -- so it is the idiom the library is written in.
  It is const now.
- **`operator = ` takes its source by const reference.** A temporary binds here instead of
  falling through, which is the whole of the first defect above.
- **Both copy paths ask whether there is anything to share**, which is the whole of the
  second.
- **`==` and `!=` exist**, against another safe pointer and against a raw one, and both are
  const. They are identity of the POINTEE: two safe pointers over one payload are equal
  however separately they came by it. The raw-pointer arm is not a convenience -- without
  it `sp == pThing` builds a TEMPORARY safe pointer around `pThing` through the implicit
  constructor, and that temporary deletes what it was handed when the comparison ends.
- **`operator*`, `IsEmpty` and the pointer conversion are const**, which is the six rows.

### The teeth

Put the eight new cases to the unfixed template and the run does not finish. Two checks
fail and the CRT reports the double free directly, and the process dies there:

```
      FAIL [assigning from a temporary shares it instead of seizing it]  SafePtrProbe::nLive == 1
      FAIL [assigning from a temporary shares it instead of seizing it]  oPtr->nValue == 1234
      ASSERT ... debug_heap.cpp(904) : Assertion failed: _CrtIsValidHeapPointer(block)
      ASSERT ... debug_heap.cpp(908) : Assertion failed: is_block_type_valid(header->_block_use)
```

Nothing after it runs, so nothing after it is measured. Lifting that case as well -- along
with the two whole cases and sixteen checks the old declarations simply reject -- lets the
rest be put to it, and the empty one bites too:

```
      ASSERT [copying an empty safe pointer leaves both of them empty]  MsgCollectors.h(157) : Assertion failed!
      ASSERT [copying an empty safe pointer leaves both of them empty]  MsgCollectors.h(157) : Assertion failed!
  cases   : 212  (1 with failures)
  checks  : 1208  (2 failed)
  result  : FAIL
```

`MsgCollectors.h(157)` is `ASSERT(m_pSafePtrType != NULL)` inside `Delete()`, reached with
a count of one and nothing to count.

### The gate

Nine solutions in both configurations: **0 errors**. The suite 215 cases static and 210
through the DLL, both PASS; `MscsUnitTests` 125; C4; the golden image byte-identical at
4104 bytes; §17's agreement sweep 29 agree 0 differ; §18, §19, §22, §23 and §24 unchanged;
Chartboard 0 errors and its four drivers 17, 13, 24 and 15 checks, none failing.

## 26. `IsSole`'s FALSE, narrowed to what it is actually about

§24 gave a caller a guarantee on TRUE and recorded that FALSE is not the opposite one,
with the row that proves it: a field that has been asked for its descendants keeps a
`P3PmsgDesc` that holds the heap, so the count is two and the answer is false while the
item is still nobody else's. What would close it, that section said, is a reference count
per BLOCK rather than per heap.

**That remains true, and it remains a different library.** `VBListHANDLE` is a runtime
structure and not part of the image, so a per-block map could be added to it without the
golden gate noticing -- but there is no lock of any kind on that handle, only `nRefCount`
is atomic, and `P2PmsgHeap_Close` says in its own comment that AddRef and Close race
across pump threads. Worse, a registry keyed on the block would have to hook every write
to `m_aVBLock`, and two of those take no reference at all: `Connecta` returns early when
the heap is unchanged, after assigning the new block, and `RehomeInlineItem` rewrites the
address in place. `P3PmsgObject`'s members are public, so nothing could enforce it either.

### But that row was never about a stranger

The second holder is the field's OWN sub-object. `P3PmsgField` made it, holds it, and
destroys it; `r_Attr` and `r_Desc` are where it comes from. A field can see its own parts
even though an object cannot see any, so `P3PmsgField::IsSole` overrides rather than
forwards and subtracts them.

**Undercounting is safe and overcounting is not**, and that asymmetry is the whole design.
A holder missed leaves the answer FALSE, and false promises nothing. A holder subtracted
that was never mine reports TRUE with a stranger looking, and TRUE is a guarantee. So only
the two sub-objects the class owns outright are counted, and each only when its heap is
this heap.

```
-- the row §24 pinned --
  a grown floater, no sub-objects yet      refs=1  sole=true
  ... once it has been given a descendant  refs=2  sole=true
  ... and it still reads what it held      7 / 1
  a grown floater with an attribute        refs=2  sole=true
  ... and a descendant as well             refs=3  sole=true

-- what must not change --
  a handle on a tree item                  refs=6  sole=false seen by the store      YES  agree
  a value copy of a tree item              refs=0  sole=true  seen by the store      no   agree
  a tree item with descendants of its own  refs=7  sole=false seen by the store      YES  agree
  a shared grown floater, with descendants refs=3  sole=false seen by its partner    YES  agree

-- what this still cannot lift --
  a grown floater before a push            refs=1  sole=true
  ... once it has been pushed              refs=1  sole=true
  ... and it still reads what it held      5
  a grown floater with two descendants     refs=2  sole=true
  ... once a cursor has been taken on them refs=3  sole=false

agree=4 open=0 GUARANTEE-BROKEN=0 asserts=0
```

The first block is the row §24 pinned, answered. The second is what had to not move, and
the third is what this still cannot lift -- and it is now ONE thing rather than any
sub-object at all. A cursor lives inside the collection that made it and there is no
accessor to reach it from a field, so it is not subtracted, and that is pinned by a case
so the claim in the NOTES stays true of the code. A push was the other candidate and turns
out not to hold the heap at all: it costs nothing today, though it could not be subtracted
either if it did.

Three questions still, and the middle one is now answered more finely: where the item is
-- `IsInline`; whether anyone ELSE can see a write -- `IsSole`, with the guarantee on true
and my own parts no longer counted against me; whose item it is -- §21's `==`.

## 27. A value copy took a block and never gave it back

§23 gave a value copy its own copy of the chained payload block, and §26's second entry
recorded what that costs: an allocation and a memcpy per link, at every copy. The entry
said one link is the usual case and that a caller found copying grown values in a loop
should be handed a handle instead.

Writing that loop to time it is what found this. It does not run.

```
-- ten copies of one grown value, one at a time --
  the source's own payload block   2343280238320
  copy  0 payload block            2343280240448
  copy  1 payload block            2343280242576   <-- a NEW block
  copy  2 payload block            2343280244704   <-- a NEW block
  copy  3 payload block            2343280247440   <-- a NEW block
  copy  4 payload block            2343280249568   <-- a NEW block
  copy  5 payload block            2343280251696   <-- a NEW block
  copy  6 payload block            2343280253824   <-- a NEW block
  copy  7 payload block            2343280255952   <-- a NEW block
  copy  8 payload block            2343280258080   <-- a NEW block
  copy  9 payload block            2343280260208   <-- a NEW block
  climbing: nothing is given back

-- ten P3PmsgData copies, the ordinary spelling --
  the same address every time: the block is given back

-- copies until something gives --
  threw at copy 4841: 'Attempt to exceed maximum P2PmsgHeap size of 10000000 bytes'

asserts=0
```

**The block is allocated on the heap the two objects SHARE**, which is right -- the AddRef
has already settled the heap's lifetime, and §23 is careful about that. But a shared heap
does not go away when the copy does, and `~P3PmsgObject` closes the heap and frees
nothing. So the copy's block stayed allocated on a heap that was still open, and a source
that outlives its copies accumulates one block per copy until the heap refuses. Note the
second listing above: the ordinary `P3PmsgData` copy does give it back, because that one
gets its OWN private heap and the heap is destroyed whole. Only the shared-heap arm --
§23's new one -- leaks.

`ReleaseInlineChain()` is the other half of `PrivatiseInlineChain`, and it was missing. It
walks whichever chain the inline block carries and frees each link, and it is called from
the four places an object stops naming its inline block -- the destructor, `Nullify`,
`Connect`'s share arm and `Connecta` -- always before the heap the chain is on is closed.
An item on a heap belongs to the message and is excluded by the same address test §23
uses; an externally managed block is excluded by `m_xVBLock`.

```
-- ten copies of one grown value, one at a time --
  the source's own payload block   2831058123136
  copy  0 payload block            2831058125264
  copy  1 payload block            2831058125264
  copy  2 payload block            2831058125264
  copy  3 payload block            2831058125264
  copy  4 payload block            2831058125264
  copy  5 payload block            2831058125264
  copy  6 payload block            2831058125264
  copy  7 payload block            2831058125264
  copy  8 payload block            2831058125264
  copy  9 payload block            2831058125264
  the same address every time: the block is given back

-- ten P3PmsgData copies, the ordinary spelling --
  the same address every time: the block is given back

-- copies until something gives --
  20000 copies, no failure

asserts=0
```

### How long the chain is, and what a copy of it costs

```
-- chain length by payload size --
      wchars   links     chained
           1       0           0  fits in the inline block
           4       0           0  fits in the inline block
           8       0           0  fits in the inline block
          16       0           0  fits in the inline block
          24       0           0  fits in the inline block
          32       0           0  fits in the inline block
          64       0           0  fits in the inline block
         128       0           0  fits in the inline block
         256       0           0  fits in the inline block
         384       1         785
         500       1        1017
         508       1        1033
         512       1        1041
         600       1        1217
        1024       1        2065
        4096       1        8209
       16384       1       32785
       32000       1       64017
       32700       1       65417
       32768       -           -  THREW 'Buffer overrun (65536 vs 65535) blocked'
       65536       -           -  THREW 'Buffer overrun (131072 vs 65535) blocked'
      262144       -           -  THREW 'Buffer overrun (524288 vs 65535) blocked'
     1048576       -           -  THREW 'Buffer overrun (2097152 vs 65535) blocked'

-- the same value, grown again and again --
  grown to    512 wchars   links=1  chained=1041
  grown to   1024 wchars   links=1  chained=2065
  grown to   1536 wchars   links=1  chained=3089
  grown to   2048 wchars   links=1  chained=4113
  grown to   2560 wchars   links=1  chained=5137
  grown to   3072 wchars   links=1  chained=6161
  grown to   3584 wchars   links=1  chained=7185
  grown to   4096 wchars   links=1  chained=8209

-- a grown NAME --
  a name resized to 50   links=1

-- what a copy costs --
  the grown one chains 1 link(s); the small one 0
  20000 copies of a value that fits    0 ms
  20000 copies of a value that grew    16 ms

longest chain seen=1 asserts=0
```

**A chain is one link long, and cannot be longer from this side.** Both
`P2PmsgObject_NewVBLockData` and `P3PmsgName_ResizeName` REPLACE the chained block rather
than appending to it -- growing a value eight times running leaves one link, not eight --
and a payload past 65535 bytes is refused rather than split across blocks. A longer chain
can only arrive already built, in an image, which is why the loops tolerate one. So the
per-link cost the entry worried about is one allocation and one memcpy, permanently, and
it measures at 16 ms for twenty thousand copies against 0 ms for twenty thousand copies of
a value that fits. That is the whole of it, and it is not the reason to prefer a handle;
the reason to prefer a handle is that a handle is a different thing.

## 28. `delete sp` does not compile any more

§25 made `P2PSafePtr`'s `operator bool` explicit and left `operator SafePtrType*()`
implicit, having measured that call sites depend on it, and recorded what that left
standing: `delete sp` and `sp[0]` compile, because a conversion that hands out the raw
pointer hands out everything a raw pointer can do with it. Deleting through it frees the
payload under a holder that still counts one, and the holder's own destructor is then the
second free.

Nobody had written either. That is not the same as nobody being able to.

The count was nine, not eight -- the ninth is §25's own test, which reads the pointer out
of a const safe pointer. Nine sites is a morning's work, so the conversion is explicit and
they say `p_SafePtr()`: it hands over the pointer and KEEPS the ownership, which is the
whole difference from `Dereference()` three lines below it in most of them.

```
  P2PeerCon.cpp  3842, 3907, 4147   PostP2PeerMsg ( spMsg )
  P2PeerMsg.cpp  370, 376           ASSERT(!P2PeerMsg_IsPosted(spMsg))
  P2Pwin32.cpp   1495               RemoveP2Pexplorer ( spP2PmsgPump )
  P2Pwin32.cpp   3535               RemoveP2PmsgPump ( spP2PmsgPump )
  P2Pwin32.cpp   5732               P2PeerMsg *pMsg = spMsg
  MsgcoreSuite.cpp 4231             SafePtrProbe *pRaw = roHeld
```

Every expression again, against §25's declarations and against these:

| expression | after §25 | after §28 |
|---|---|---|
| `if ( sp )` | compiles | compiles |
| `!sp` | compiles | compiles |
| `spA && spB` | compiles | compiles |
| `sp ? 1 : 2` | compiles | compiles |
| `static_cast<bool>(sp)` | compiles | compiles |
| `if ( constSP )` | compiles | compiles |
| `bool b = sp` | compiles | **rejected** C2440 |
| `return sp` | compiles | **rejected** C2440 |
| `int n = sp` | **rejected** C2440 | **rejected** C2440 |
| `sp + 1` | **rejected** C2666 | **rejected** C2678 |
| `spA - spB` | compiles | **rejected** C2678 |
| `sp[0]` | compiles | **rejected** C2678 |
| `delete sp` | compiles | **rejected** C2440 |
| `sp += 1` | **rejected** C2676 | **rejected** C2676 |
| `spA == spB` | compiles | compiles |
| `spA != spB` | compiles | compiles |
| `constSP == spB` | compiles | compiles |
| `sp == pRaw` | compiles | compiles |
| `sp == nullptr` | compiles | compiles |
| `sp == 0` | **rejected** C2666 | compiles |
| `spA < spB` | compiles | **rejected** C2678 |
| `constSP->n` | compiles | compiles |
| `*sp` | compiles | compiles |
| `*constSP` | compiles | compiles |
| `constSP.IsEmpty()` | compiles | compiles |
| `Thing *t = constSP` | compiles | **rejected** C2440 |
| `Thing *t = sp` | compiles | **rejected** C2440 |
| `spA = spB` | compiles | compiles |
| `spA = constSP` | compiles | compiles |
| `spA = MakeSP()` | compiles | compiles |
| `ThingSP c = MakeSP()` | compiles | compiles |
| `ThingSP c = pRaw` | compiles | compiles |
| `ThingSP c = 0` | compiles | compiles |

Ten rows moved and they are three groups. **Four** are the ones this section is for:
`delete sp`, `sp[0]`, `sp + 1` and `spA - spB` no longer compile. **Two** are the
deliberate spelling changing: `Thing *t = sp` and `Thing *t = constSP` are now
`sp.p_SafePtr()`, which is the trade. **Two** more are `bool b = sp` and `return sp`,
which §25 noted still compiled because a `SafePtrType*` converts to bool on its own --
take the pointer conversion out of the implicit set and that goes with it, so the type is
bool-free after all, contextual conversions excepted. And **one** goes the other way:
`sp == 0` compiles again, the bool candidate that made it ambiguous having gone. `spA <
spB` follows `spA - spB`; ordering two safe pointers now needs `p_SafePtr()` on both,
which is a fair thing to have to say.

### The teeth for all three

Different for each, and worth separating. §26's cases fail outright against the unfixed
object -- five checks across four cases, and the run then stops in §27's loop case and
does not finish, because that case is §27's tooth and it exhausts the heap. §27's tooth on
its own is the probe above: 4841 copies and a refusal. §28 has no behavioural tooth at all,
the way §24 had none -- against the unfixed template the cases do not compile, `p_SafePtr`
not existing, and the table above is the measurement instead.

### The gate

Nine solutions in both configurations: **0 errors**. The suite 225 cases static and 220
through the DLL, both PASS; `MscsUnitTests` 125; C4; the golden image byte-identical at
4104 bytes; §17's agreement sweep 29 agree 0 differ; §18, §19, §22, §23, §24 and §25
unchanged; Chartboard 0 errors and its four drivers 17, 13, 24 and 15 checks, none
failing.

## 29. The cursor `IsSole` could not discount

§26 gave `P3PmsgField::IsSole` an override, because the object's version counts holders
of the HEAP and cannot tell a stranger from one of this field's own parts. It subtracted
`m_pP3PmsgAttr` and `m_pP3PmsgDesc` and stopped there, and that left a row: a cursor
lives one level further in, inside the collection that made it, and a cursor holds the
heap. So a collection that had been WALKED went back to answering "somebody else is
looking" while its item was still nobody else's.

**The obvious fix is the wrong one.** The entry said this needed "an accessor on both
collection classes", and there is no const accessor to add cheaply:

- `m_pCurs` is a protected raw owning pointer on `P3PmsgDesc` (`MsgDesc.h:217`) and on
  `P3PmsgAttr` (`MsgAttr.h:188`), and it is **not** `mutable`. The two classes share no
  base; each declares its own.
- `r_Curs()` is a **creating** accessor -- `if ( m_pCurs == nullptr ) m_pCurs = new
  P3PmsgCurs ( *this );` (`MsgDesc.cpp:463`, `MsgAttr.cpp:429`) -- and a fresh cursor
  calls `Goto(0)`, which `Connect`s one of its members and so takes a reference on the
  heap. A const question about who is holding this heap, answered by taking another
  reference on it, is its own wrong answer.
- A cursor is also stateful and SHARED: one per collection, repositioned by every
  `operator[]`, `Select*`, `Exists` and `Delete`. Handing one out from a const method
  moves a position another caller is standing on.

**So the cursor is not handed out. The collection is asked what it is holding.** One
question, asked down the ownership tree, and `IsSole` becomes the comparison it always
wanted to be:

```cpp
bool
P3PmsgField::IsSole ( ) const
{
    if ( OBJ__.m_aVBLock == 0 )
      return false;                    // Void: no storage to be sole holder of
    if ( OBJ__.IsInline ( ) )
      return true;                     // The block is in here, so nowhere else

    const P2PmsgHANDLE hVBList = OBJ__.m_hVBList;
    if ( hVBList == 0 )
      return false;                    // A block on no heap is nobody's to count

    return P2PmsgHeap_RefCount ( hVBList ) == HeapHolders ( hVBList );
}
```

`HeapHolders` rests on one rule, and the rule is the heap's own rather than a
convention: **every path that gives a `P3PmsgObject` a non-zero `m_hVBList` AddRefs it.**
They are the copy constructor (`P2Pmsg.cpp:2485`), `Connect` (`:2612`), `Connecta`
(`:2636`) and the two `CreateSYS` sites (`:2780`, `:2868`), which is every assignment to
that member in the library -- and `~P3PmsgObject` closes it. So a live object whose handle is this handle IS one
reference, and this counts references rather than estimating them:

```cpp
int
P3PmsgField::HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept
{
    if ( hVBList == 0 )
      return 0;

    int nHolders = OBJ__.m_hVBList == hVBList ? 1 : 0;
    if ( m_pP3PmsgAttr != nullptr )
      nHolders += m_pP3PmsgAttr -> HeapHolders ( hVBList );
    if ( m_pP3PmsgDesc != nullptr )
      nHolders += m_pP3PmsgDesc -> HeapHolders ( hVBList );
    if ( m_pMsgStck != nullptr )
      nHolders += m_pMsgStck    -> HeapHolders ( hVBList );
    return nHolders;
}
```

with `P3PmsgDesc` and `P3PmsgAttr` adding their own object and their cursor, `MsgStck`
adding the three stacked objects `r_item`/`r_list`/`r_vect` create, and `P3PmsgCurs`
adding its `m_pItemParent` and all three of its by-value members.

**What is followed is what is owned, and nothing else.** A collection's
`m_pP3PmsgField`, a stack's `m_pP3PmsgField` and a cursor's `m_pP3PmsgAttr` /
`m_pP3PmsgDesc` all point back UP at the owner. Following one would count this field a
second time and report TRUE with a stranger looking, which is the only direction that
breaks anything -- §26's asymmetry, unchanged: a holder missed leaves FALSE and false
promises nothing; a holder subtracted that was never mine reports a guarantee that is
not true.

**It is a count and not a flag, and that was not a guess.** `P3PmsgCurs::Goto` connects
whichever of `m_oP3PmsgField`, `m_oP3PmsgList` and `m_oP3PmsgVect` matches the item it
landed on (`MsgCurs.cpp:516-531`) and does **not** disconnect the other two. So one
cursor that has walked past a list and then a field is holding this heap TWICE. A
subtraction of one per cursor would have undercounted there -- safe, but only by luck,
and nobody had looked:

```
-- the rows §29 was carrying --
  a grown floater with two descendants         refs=2  mine=2  sole=true
  ... once a cursor has been taken on them     refs=3  mine=3  sole=true
  ... and it still reads what it held          7 / 2
  a grown floater with a list and a field      refs=2  mine=2  sole=true
  ... cursor on the list                       refs=3  mine=3  sole=true
  ... and then on the field: TWO members live  refs=4  mine=4  sole=true
  both collections walked, both cursors live   refs=5  mine=5  sole=true
  a grown floater before a push                refs=1  mine=1  sole=true
  ... once it has been pushed (push holds nothing) refs=1  mine=1  sole=true
  ... and once the snapshot has been READ      refs=2  mine=2  sole=true
  ... and it still reads what it held          5

-- what must not change --
  a handle on a tree item                      refs=6  mine=1  sole=false seen by the store    YES  agree
  a tree item, descendants walked              refs=8  mine=3  sole=false seen by the store    YES  agree
  a shared floater, descendants walked         refs=4  mine=3  sole=false seen by its partner  YES  agree
  a shared floater, the STRANGER walked        refs=5  mine=2  sole=false seen by its partner  YES  agree

agree=4 open=0 GUARANTEE-BROKEN=0 asserts=0
```

The `mine=` column is `HeapHolders`, and it tracks `refs=` exactly on every row that is
sole. The fourth row is the one that settles the design: `refs=4 mine=4`, one field, one
`P3PmsgDesc`, and one cursor holding two.

**§26's other entry closed on the way past.** That section recorded the `MsgStck` as
unreachable and measured a push as adding no holder of the heap at all. Both readings
were right and neither was the whole of it: a push holds nothing, and READING the
snapshot -- `r_Stck().r_item()` -- news a `P3PmsgField` on this heap that does. The same
walk reaches it, so it is subtracted for the same reason and by the same rule.

**Undercounting remains, deliberately.** `P3PmsgList` and `P3PmsgVect` cache
`P3PmsgData` cursors in `m_pP3PmsgData[]`, and a vect carries a `m_pP3PmsgType`; none of
those is descended into. Whatever they hold leaves the answer FALSE, which is the safe
side, and What-is-left records it rather than this claiming to have closed it. §31
later measured all three and closed it.

**The exported surface moved, and it had already moved.** These classes are whole-class
MFC extension exports, so five new public members are five new exports.
`tools/ci/check_exports.ps1` reported **nineteen** additions and one removal against
`exports-cxx-x64.manifest`: five are `HeapHolders`, and the other fourteen are §11, §15,
§20, §21, §22, §23, §24, §26 and §27 arriving at a manifest nobody had re-measured since
`4d39d0d`. Every one of them is a member those sections meant to add. The manifest is
re-measured here, which is what `c50df62` did the last time this happened. The **flat C
ABI manifest is byte-identical** -- 282 symbols, unchanged -- so the supported surface
did not move and no version bump is due. `tools/ci/api-drift.allow` gains the first five
TRIAGED lines it has ever had, with the reason the file's own header asks for.

**Teeth.** Restore the pre-§29 library, force a rebuild, and the cases fail rather than
passing quietly. Five of the six are `oF.IsSole()` reading false where it now reads true;
the sixth is the one that must NOT move, and it fails on its opening `TF_CHECK` for the
same reason.

## 30. A chain of three, which only an image could deliver

§27 measured every in-process path as producing exactly ONE link, and that measurement
stands: `P2PmsgObject_NewVBLockData` and `P3PmsgName_ResizeName` both REPLACE the chained
block rather than appending to it, and a payload past 65535 bytes is refused rather than
split. The walks in `PrivatiseInlineChain`, `ReleaseInlineChain`, `NewVBLockData` and
every reader loop anyway, because an image can carry a longer chain. **Not one of them
had ever been put to one**, and the entry said so: "for want of such an image to put them
to."

**The image is the arena, which is what makes the want answerable.** `P2PmsgMgr::Save`
takes `P2PmsgHeap_pImage` and `P2PmsgHeap_Sizeof` and writes the heap whole with a single
`WriteFile` (`P2PmsgMgr.cpp:431-478`); `Load` reads it back whole and casts it
(`P2PmsgMgr.cpp:210-262`). A `VBLaddr` is a plain offset resolved as `base + aVBLaddr`
(`MsgVBHeap.cpp:4096`), relocated by nothing on load and validated for LENGTH by nothing
anywhere. So a chain pointer round-trips untouched, and the state an image delivers is
exactly the state the heap's own allocator builds:

```cpp
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
```

**The three walks the entry named all held.** `PrivatiseInlineChain` gave the copy three
links of its own -- every one of them, checked address by address down both chains, not
just the first -- `ReleaseInlineChain` gave all three back five hundred times over, and
`NewVBLockData` collapsed three to one on a retype. A saved image carried three links and
read the value back out of them.

**One thing did not hold, and it is a fourth walker the entry did not name.**
`P3PmsgData::VerifyContainment` sizes every block it steps to, *before* the loop asks
whether that block is itself a link (`P2Pmsg.cpp:1596`). `VBLockData_Sizeof_uv` has no
arm for the chained type byte -- `0xFF` is claimed by none of them -- so it falls through
every one to the `ASSERT(0)` at the bottom:

```
-- a chain no in-process path can build --
  a grown value chains exactly one link                  ok
  a second link can be appended                          ok
  the chain is now two links                             ok
  and the value still reads back through it              ok
  a third link can be appended                           ok
  the chain is now three links                           ok
  and the value still reads back through THAT            ok
    [assert] C:\_Dev\_ClaudeCode\MSCS\Msgcore\P2PmsgVBLock.cpp(1553) : Assertion failed!
    [assert] C:\_Dev\_ClaudeCode\MSCS\Msgcore\P2PmsgVBLock.cpp(1553) : Assertion failed!
  VerifyContainment accepts a three-link chain           ok
  ... and does it without asserting                      ** FAILED **
```

**Say the size of it accurately.** `VBLockData_Sizeof` floors its result at
`VBLockData_Sizeof_Min`, the header plus exactly one chain address, so the NUMBER it
handed back for a link was already right. The defect is the assertion and only the
assertion, and that is the whole of the claim being made here.

It is not nothing. Under the default report mode a `_CRT_ASSERT` raises the assertion
dialog and a Debug build stops in it, so a Debug consumer that loads such an image and
validates it halts on a structure the library then goes on to handle correctly. The
listing above continues past it only because the probe installs a report hook that counts
and returns, exactly as `TestFramework` does -- which is why this is measured as a count
of two rather than seen as a hang. `VBLockData_Sizeof_uv` now knows the chained form,
which changes no computed size and removes the assertion. The refusal below it still
stands for a type byte that names nothing at all, which is what it was for.

```
-- a chain no in-process path can build --
  a grown value chains exactly one link                  ok
  a second link can be appended                          ok
  the chain is now two links                             ok
  and the value still reads back through it              ok
  a third link can be appended                           ok
  the chain is now three links                           ok
  and the value still reads back through THAT            ok
  VerifyContainment accepts a three-link chain           ok
  ... and does it without asserting                      ok
  AssertValid walks three links without asserting        ok
  c_size still answers for the payload at the end        ok
  Sizeof_VBLockData resolves through all three           ok

-- PrivatiseInlineChain, put to three links --
  source is three links                                  ok
  copy chain length                                      3
  the copy carries a chain of the same length            ok
  and its LAST link is its own, not the source's         ok
  EVERY link of the copy is its own                      ok
  and holds the same payload, byte for byte              ok

-- ReleaseInlineChain, put to three links --
  tail block of copy 1 / copy 500                        2322210936160 / 2322210936160
  every one of 500 copies was three links                ok
  and every one gave all three links back                ok

-- NewVBLockData's collapse, put to three links --
  three links before the retype                          ok
  chain length after the retype                          1
  the retype collapsed it back to one link               ok
  and the new value reads back                           ok

-- and through an image, which is where one would come from --
  a stored item's value chains one link                  ok
  three links before the save                            ok
  saved                                                  ok
  loaded                                                 ok
  chain length after the round trip                      3
  the image carried all three links                      ok
  and the value reads back out of the image              ok

failures=0 asserts=0
```

**Teeth.** `VerifyContainment` asserts twice on a three-link chain and `AssertValid`
reaches it again for two more; `TestFramework`'s assert trap folds each into a failure of
the case, so the case is the check.

```
      FAIL [a cursor on its own descendants does not stop it being sole]  oF.IsSole()
      FAIL [a cursor that has walked two kinds of item holds it twice]  oF.IsSole()
      FAIL [a cursor that has walked two kinds of item holds it twice]  oF.IsSole()
      FAIL [an attribute collection's cursor counts the same way]  oF.IsSole()
      FAIL [a stacked snapshot that has been read does not stop it either]  oF.IsSole()
      FAIL [a shared floater whose descendants were walked is still not sole]  oF.IsSole()
      ASSERT [every link of a long chain can be sized and contained]  C:\_Dev\_ClaudeCode\MSCS\Msgcore\P2PmsgVBLock.cpp(1553) : Assertion failed!
      ASSERT [every link of a long chain can be sized and contained]  C:\_Dev\_ClaudeCode\MSCS\Msgcore\P2PmsgVBLock.cpp(1553) : Assertion failed!
      ASSERT [every link of a long chain can be sized and contained]  C:\_Dev\_ClaudeCode\MSCS\Msgcore\P2PmsgVBLock.cpp(1553) : Assertion failed!
      ASSERT [every link of a long chain can be sized and contained]  C:\_Dev\_ClaudeCode\MSCS\Msgcore\P2PmsgVBLock.cpp(1553) : Assertion failed!
  cases   : 236  (6 with failures)
  checks  : 1302  (10 failed)
  result  : FAIL
```

## 31. The cursors a collection keeps for itself

§29 walked what a field owns -- its attributes, its descendants, its stack, and the
cursors those carry -- and stopped at three members: `P3PmsgList::m_pP3PmsgData[]`,
`P3PmsgVect::m_pP3PmsgData[]` and `P3PmsgVect::m_pP3PmsgType`. What-is-left recorded the stop as
deliberate rather than an oversight and said what would lift it: each would have to be
shown to hold this heap AND to be owned outright before it could be subtracted, and
neither had been measured.

Both are measured here. Two of the three hold and are owned; the third is owned and holds
nothing, which is a different answer and is worth having.

### What the code says about each

**`P3PmsgList::m_pP3PmsgData[]` is the list's own and it holds this heap.** Every element
is `new P3PmsgData()` at one of the three read sites and nowhere else -- `GetNext`
(`MsgList.cpp:183`), `GetTail` (`:232`), `GetPrev` (`:255`) -- the array is zeroed at
construction (`:106`, `:132`) and deleted by `~P3PmsgList` (`:122-123`) and by `Truncate`
(`:448-450`). Nothing anywhere assigns it a pointer that came from outside, and there is
no back-pointer up at an owner. It holds the heap by §29's rule: each wrapper is
`Connect`ed on `OBJ__hVBList` (`:184`, `:233`, `:256`), and `P3PmsgData::Connect`
(`P2Pmsg.cpp:300-306`) reaches `P3PmsgObject::Connecta`, which AddRefs when the handle
changes and returns early when it is already this one.

**It is one per SLOT USED, not one per list.** `m_nCurs` advances modulo
`MAX_P3PmsgData_Curs` -- three (`P2PmsgVBLock.h:323`) -- and a read connects the wrapper in
the new slot without disconnecting the others. So a list walked three deep is holding this
heap three times, for the same reason §29 found a `P3PmsgCurs` holding it twice, and the
count has to count rather than flag.

**`P3PmsgVect::m_pP3PmsgType` is the vect's own and it holds this heap.** `Goto` deletes
the previous wrapper and news one of the element's own kind -- `new P3PmsgList`, `new
P3PmsgVect` or `new P3PmsgField` (`MsgVect.cpp:789-793`) -- then `Connect`s it on
`OBJ__hVBList` (`:794`); `~P3PmsgVect` (`:123-124`), `Delete` (`:535`), `Truncate` (`:543`)
and `Goto` itself on every failure and re-entry (`:774`, `:780`, `:787`) delete it. Because
`InsertAt` ends in a `Goto` (`:757-758`), a vect that has had anything inserted is already
holding one -- which is why the "before" run below shows a two-element vect at `refs=2
mine=1` before anybody has read it.

**`P3PmsgVect::m_pP3PmsgData[]` is owned and holds nothing, and that is measured rather
than assumed.** It is zeroed at construction (`:107`, `:140`) and deleted (`:125-126`,
`:544-545`), so ownership is not in question. But every site that would put a pointer in it
is inside a comment block -- the vect's own `GetNext` (`:190-207`) and a copy of
`P3PmsgList::GetTail` (`:237-249`), which is still spelled `P3PmsgList::` inside a vect
source file and so was probably never a live vect reader. There is no code path that fills
that array. A vect's element cursor is `m_pP3PmsgType`; a vect data cursor is not a thing
that exists. It is walked anyway, because a null entry contributes nothing and the count
stays exact either way, and because a vect that ever grows those readers back should not
go quietly wrong in the unsafe direction.

### The trap, which is what makes this an override

`P3PmsgField` derives from `P3PmsgData`, so the obvious shape -- a `P3PmsgData::HeapHolders`
that everything chains up into -- counts one object twice. Every `P3PmsgField` constructor
aliases the inherited cell onto its own object, `P3PmsgData::m_pObject = &m_oObject`
(`P2Pmsg.cpp:3563`, `:3574`, `:3601`, `:3646`), and `~P3PmsgField` clears it (`:3583-3584`)
so that `~P3PmsgData` does not delete a member. `OBJ__` and `m_pObject` are the same
object. A field that counted itself and then called the base version would report one
holder too many, and **overcounting is the direction that reports a guarantee that is not
true** -- §26's asymmetry, in the direction that matters. So `P3PmsgField::HeapHolders`
overrides `P3PmsgData::HeapHolders` rather than adding to it, counts `OBJ__` itself, and
never calls the base version. Its existing count is unchanged to the reference, which is
what keeps §26's and §29's rows where they were.

```cpp
int
P3PmsgData::HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept
{
    if ( hVBList == 0 ||
         m_pObject == nullptr )
      return 0;

    return m_pObject -> m_hVBList == hVBList ? 1 : 0;
}
```

**`HeapHolders` is virtual now, and two independent things force it.** First,
`P3PmsgField::IsSole` is inherited unchanged by both collections and calls
`HeapHolders ( hVBList )` on itself; resolved at compile time that is
`P3PmsgField::HeapHolders`, and a list would never reach its own override no matter what
was declared on it. Second, `m_pP3PmsgType` is declared `P3PmsgField*` and `Goto` news a
`P3PmsgList` or a `P3PmsgVect` into it for a nested container, whose own cursors are a
level further in again. The "a vect holding a vect" row exists only because of the
dispatch.

That makes the virtualness visible on the exported surface in a way worth naming, because
it is the one export removal in this section: MSVC encodes virtualness in the mangling --
`Q` for public non-virtual, `U` for public virtual -- so
`?HeapHolders@P3PmsgField@@QEBAHPEAX@Z` retires and `...@UEBAHPEAX@Z` is minted. Four
additions and one removal, and the removal is the same member.

### Measured

Seven cases and twenty-eight checks. Before, against the unchanged library, ten of those
checks fail:

```
  - a list's own data cursors do not stop it being sole
      a list nobody has read                       refs=1  mine=1  sole=true
      ... once one element has been read           refs=2  mine=1  sole=false
      FAIL [a list's own data cursors do not stop it being sole]  oList.IsSole()
      ... and the tail, in a second slot           refs=3  mine=1  sole=false
      FAIL [a list's own data cursors do not stop it being sole]  oList.IsSole()
  - a list walked end to end holds its heap once per cursor slot
      a list walked end to end                     refs=4  mine=1  sole=false
      FAIL ...
  - a vect's own element cursor does not stop it being sole
      a vect with two elements                     refs=2  mine=1  sole=false
      FAIL ...
      ... once an element has been read            refs=2  mine=1  sole=false
      FAIL ...
  - a nested collection's cursors are reached through the element cursor
      a vect holding a vect                        refs=2  mine=1  sole=false
      FAIL ...
      ... and the inner element read through it    refs=3  mine=1  sole=false
      FAIL ...
  - a list element's data cursor is reached through the element cursor
      a vect holding a list                        refs=2  mine=1  sole=false
      FAIL ...
      ... with that list read through it           refs=3  mine=1  sole=false
      FAIL ...
  - a shared list whose cursors were used is still not sole
      a shared list, walked by me                  refs=4  mine=1  sole=false
  - and a vect the STRANGER is walking is still the stranger's
      FAIL [and a vect the STRANGER is walking is still the stranger's]  oVect.IsSole()
      a shared vect, walked by the partner         refs=4  mine=1  sole=false
```

After:

```
  - a list's own data cursors do not stop it being sole
      a list nobody has read                       refs=1  mine=1  sole=true
      ... once one element has been read           refs=2  mine=2  sole=true
      ... and the tail, in a second slot           refs=3  mine=3  sole=true
  - a list walked end to end holds its heap once per cursor slot
      a list walked end to end                     refs=4  mine=4  sole=true
  - a vect's own element cursor does not stop it being sole
      a vect with two elements                     refs=2  mine=2  sole=true
      ... once an element has been read            refs=2  mine=2  sole=true
  - a nested collection's cursors are reached through the element cursor
      a vect holding a vect                        refs=2  mine=2  sole=true
      ... and the inner element read through it    refs=3  mine=3  sole=true
  - a list element's data cursor is reached through the element cursor
      a vect holding a list                        refs=2  mine=2  sole=true
      ... with that list read through it           refs=3  mine=3  sole=true
  - a shared list whose cursors were used is still not sole
      a shared list, walked by me                  refs=4  mine=3  sole=false
  - and a vect the STRANGER is walking is still the stranger's
      a shared vect, walked by the partner         refs=4  mine=2  sole=false
```

`mine=` is `HeapHolders` and it tracks `refs=` exactly on every row that is sole. The last
two rows are the ones that must NOT move, and they are the reason this is a subtraction
rather than a suppression: on the shared list `mine=3` of `refs=4` -- the list and its two
cursors are mine, the fourth is the stranger's own object -- and on the shared vect
`mine=2` of `refs=4`, the vect and its element cursor against the partner's same pair. The
count falls short by precisely the stranger's holdings, which is what leaves the answer
false.

The `refs=` column exists only in static link mode: `P2PmsgHeap_RefCount` is not exported,
and exporting it to make a test's listing prettier would widen a surface frozen through
1.x. The row is guarded the way `Test_ImageAddressBounds` is, and the DLL run fails and
passes the identical ten checks without it.

**Teeth.** Restore the pre-change library, force a rebuild, and six of the seven cases
fail -- ten checks, the same ten in both link modes. The seventh, "a shared list whose
cursors were used is still not sole", passes before and after and is meant to: it is one
of the two rows that must not move, and a row that cannot fail either way is the only
honest way to pin one. The OTHER must-not-move row is among the six, and it fails on its
opening `TF_CHECK ( oVect.IsSole ( ) )` rather than on the shared assertion it exists for
-- exactly as §29's sixth case did. Before the walk reaches the element cursor, a vect
nobody shares has already read false, so the case cannot get as far as the stranger.

## 32. Every image in hand, asked whether it carries one

§30 built a three-link chain with the heap's own allocator and put the four walks to it,
and What-is-left recorded what that left standing: the chain was built, not found. The format
argument says an image CAN carry one -- the arena is written and read whole, a `VBLaddr` is
relocated by nothing and validated for length by nothing -- but no file in hand had been
asked. "Whether any image in hand -- a fuzz seed, a sample workspace -- carries a longer
chain has not been measured either way."

It is measured now, and the answer is no.

### What there is to ask

Nineteen saved arenas in this library's format: the six fuzz seeds in
`tests/fuzz/corpus/`, the twelve stores `c4test.exe` leaves in `tests/out/`, and the golden
image itself -- `MscsUnitTests/golden_ref.p2p`, 4104 bytes, the one §25's and §28's gates
keep reporting byte-identical. No in-repo test leaves a sample workspace behind:
`Test_ChainLongerThanOne` (`tests/MsgcoreSuite.cpp:3695`) writes `%TEMP%\mscs_chain29.p2p`
and `_wremove`s it, which is §30's own image and not one found lying about.

**Not one of them carries a chained block.** Every file reports zero standalone `VBLockData`
blocks flagged `uDataType == 0xFF`, so the longest chain in any image in hand is one link --
the head inline in the owning block, one payload hanging off it -- or none at all:

```
   golden_ref.p2p          blocks=11  dataBlocks=0  chainedHeads=0  MAX chain links=0
   valid_store.dat         blocks=6   dataBlocks=0  chainedHeads=0  MAX chain links=0
   c4_roundtrip.dat        blocks=6   dataBlocks=0  chainedHeads=0  MAX chain links=0
   ...
ANSWER: No image in hand carries a VBLockData chain longer than one link.
```

**The measurement is worth nothing without a positive control, so there is one.** §30's
own path was reproduced -- grow a value, append two links with `Lengthen29`, save, reload --
and the library read the chain back at three. The same file, put to the same two scanners:

```
   library probe:  Data block @ 423  : CHAINED, 2 link(s)
                   Data block @ 2484 : CHAINED, 1 link(s)
                   blocks=7  dataBlocks=3  chainedHeads=2
                   RESULT: >>> chain of MORE THAN ONE link present
   byte scanner:   @423 0xFF -> 2484;  @2484 0xFF -> 4545;  @4545 payload type=26
                   RESULT: a chain of MORE THAN ONE link EXISTS.  maxlen=3
```

So zero on nineteen files is a reading and not a blind spot. This closes the entry in the
direction that leaves §30 exactly where it stood: the format argument is all there is, and
no file contradicts it.

### Eleven of the nineteen had to be read without the library, which is the point of them

The honest way to ask is with the library's own walk, and it answered for eight files.
The other eleven are refused by the heap-open before any walk can start, each with its own
message -- which is these files working as designed, since most of the corpus is
deliberately malformed:

```
  c4_evil_oversize.iom  IOMAGE declared size (16777215) exceeds buffer (4096)
  f11_collate_nogrow.dat  BSTRio block structure is corrupt
  f1_bstrio_oob.dat     BSTRio declared size (2032) exceeds buffer (49)
  c4_bstrio_oversize.dat  BSTRio declared size (131072) exceeds buffer (4096)
  c4_addr16_oversize.dat  Image declares a 0x10001-byte arena, past the 0xffff
                          addressable by the Addr16 width it also declares
  c4_f3_iomage_walk.iom   IOMAGE block structure is corrupt
```

A byte-level scanner parses the arena directly, bounds every oversize declaration to the
file length and walks what is actually there. It measured all eleven, and all eleven are
zero as well. A file the library will not open is still a file that can be asked this
question, and the answer is not allowed to be "unmeasurable" just because the loader was
right to refuse it.

### What that walk turned up, which is not about chains at all

Four of those refusals assert on the way to being refused, and all four are the same line:

```
  f11_collate_nogrow.dat   [assert #1] MsgVBHeap.cpp(3609) : Assertion failed!
  f2_walk_oob.dat          [assert #2] MsgVBHeap.cpp(3609) : Assertion failed!
  c4_bstrio_honest.dat     [assert #3] MsgVBHeap.cpp(3609) : Assertion failed!
  c4_f2_walk_oob.dat       [assert #4] MsgVBHeap.cpp(3609) : Assertion failed!
```

`MsgVBHeap.cpp:3609` is `ASSERT(P2PmsgHeap_AssertVBlocksBSTRio(pHandle))`, the last line of
`P2PmsgHeap_CreateBSTRio`. The untrusted overload above it CALLS that function and then
runs the very same walk again as a real gate, and the comment it carries says why:

> Walk the block chain FOR REAL, in every build (item 19). The delegate above ends with
> `ASSERT(P2PmsgHeap_AssertVBlocksBSTRio(pHandle))` -- the whole validation call sits
> inside the assertion, so Release does not run a reduced version of it, it does not run
> it at all. [...] Only this overload is changed. The single-argument one keeps its
> ASSERT, because it is for images this process just built, where the walk is a developer
> aid and not a gate.

The reasoning is right and the call graph defeats it: the untrusted path reaches the
developer aid on its way to the gate. So on an untrusted image the walk runs **twice**, and
the first run is an assertion that fires. Under the default report mode a `_CRT_ASSERT`
raises the dialog and a Debug build stops in it -- so a Debug consumer handed a corrupt
image halts on a structure the library was about to refuse cleanly and by name, which is
§30's shape exactly, on the load path rather than the sizing one.

**Nobody had met it because the corpus is never fed to a build that has assertions.**
`tests/fuzz/build_run_fuzz.ps1` builds `ReleaseLib` with `/DNDEBUG` (`:153`, `:167`), where
`ASSERT` compiles to nothing. That is the same lesson item 19 wrote into the comment above
-- "the structure is checked in the build nobody ships" -- holding one layer further up and
still unlearned: the assertion is exercised only in the build the fuzzer does not run.

It is recorded rather than fixed. Suppressing an assertion on the untrusted load path is a
change to a gate, with `P2PmsgHeap_UntrustedGate` standing in the middle of it, and it
wants its own teeth and its own section rather than arriving as a side effect of counting
chain links.

## 33. The platform the solution spells `x86`

§29 re-measured `exports-cxx-x64.manifest` against a built DLL and could not do the same
for Win32. The entry recorded why: "`Msgcore(2026).sln` carries no Win32 configuration at
all -- `Debug|Win32` is rejected by MSBuild as an invalid solution configuration". The
first half of that sentence does not follow from the second, and it is false.

```
Msgcore(2026).sln.metaproj : error MSB4126: The specified solution configuration
"Debug|Win32" is invalid. Please specify a valid solution configuration using the
Configuration and Platform properties (e.g. MSBuild.exe Solution.sln
/p:Configuration=Debug /p:Platform="Any CPU") or leave those properties blank to use
the default solution configuration. [Msgcore(2026).sln]
```

That is the SOLUTION refusing a solution platform name, 0.21 seconds in, having never
reached the project. `GlobalSection(SolutionConfigurationPlatforms)` lists `Debug|x86`,
`DebugLib|x86`, `Release|x86` and `ReleaseLib|x86`; `GlobalSection(ProjectConfigurationPlatforms)`
maps each onto the project's own `|Win32` -- `{C1E33B10-...}.Debug|x86.ActiveCfg =
Debug|Win32` -- and `Msgcore(2026).vcxproj` declares all four `|Win32` configurations
itself, at lines 4-34. Solution platform `x86`, project platform `Win32`, is MSBuild's
ordinary convention and not a quirk of this tree. Both of these build:

```
msbuild "Msgcore(2026).vcxproj" /t:Rebuild /p:Configuration=Debug /p:Platform=Win32 /m
msbuild "Msgcore(2026).sln"                /p:Configuration=Debug /p:Platform=x86
```

A full `Debug|Win32` rebuild is nineteen sources through `HostX86\x86\CL.exe` at `/W4
/sdl /std:c++17 /MDd`, **61 warnings and 0 errors** in 9.32 seconds; `Release|Win32` is
**44 warnings and 0 errors** in 5.37. `out\Win32\Debug\Msgcore.dll` was already on disk
from an earlier build. Nothing was ever stopping this except the name.

### The drift, which is the same drift

```
out\Win32\Debug\Msgcore.dll (Win32): 1048 exports -- 282 flat msgcore_*, 766 mangled C++

  OK -- flat C ABI matches tools/ci/exports-flat.manifest (282 symbols).

  mangled C++ (Win32) drifted from tools/ci/exports-cxx-win32.manifest:
  EXPORT ADDED, undeclared: ??8P3PmsgField@@QBE_NABV0@@Z
  EXPORT ADDED, undeclared: ??8P3PmsgField@@QBE_NPB_W@Z
  EXPORT ADDED, undeclared: ??9P3PmsgField@@QBE_NABV0@@Z
  EXPORT ADDED, undeclared: ??9P3PmsgObject@@QBE_NABV0@@Z
  EXPORT ADDED, undeclared: ?Drop@P3PmsgVect@@UAEXXZ
  EXPORT ADDED, undeclared: ?HeapHolders@MsgStck@@QBEHPAX@Z
  EXPORT ADDED, undeclared: ?HeapHolders@P3PmsgAttr@@QBEHPAX@Z
  EXPORT ADDED, undeclared: ?HeapHolders@P3PmsgCurs@@QBEHPAX@Z
  EXPORT ADDED, undeclared: ?HeapHolders@P3PmsgDesc@@QBEHPAX@Z
  EXPORT ADDED, undeclared: ?HeapHolders@P3PmsgField@@QBEHPAX@Z
  EXPORT ADDED, undeclared: ?IsInline@P3PmsgField@@UBE_NXZ
  EXPORT ADDED, undeclared: ?IsInline@P3PmsgObject@@QBE_NXZ
  EXPORT ADDED, undeclared: ?IsSole@P3PmsgField@@UBE_NXZ
  EXPORT ADDED, undeclared: ?IsSole@P3PmsgObject@@QBE_NXZ
  EXPORT ADDED, undeclared: ?P3Pmsg_GetPath@@YA?AV?$CStringT@...@ATL@@PBVP3PmsgDesc@@@Z
  EXPORT ADDED, undeclared: ?P3Pmsg_GetStckDepth@@YAIPBVP3PmsgField@@I@Z
  EXPORT ADDED, undeclared: ?PrivatiseInlineChain@P3PmsgObject@@QAEXXZ
  EXPORT ADDED, undeclared: ?RehomeInlineItem@P3PmsgObject@@QAEIXZ
  EXPORT ADDED, undeclared: ?ReleaseInlineChain@P3PmsgObject@@QAEXXZ
  EXPORT REMOVED, still declared: ??8P3PmsgField@@QAE_NPB_W@Z
```

**Nineteen and one, which is §29's shape exactly.** The entry said Win32 "carries the
same fourteen pre-§29 omissions the x64 one did", and that is true as far as it goes --
but the drift a reader actually meets is nineteen, because the five `HeapHolders` §29
added to the x64 manifest were never added to this one either. Normalise the x64 names
for pointer width -- `QEAA` to `QAE`, `AEBV` to `ABV`, `PEB_W` to `PB_W` -- and the two
nineteen-name sets reduce to the same member set, with `P3Pmsg_GetStckDepth` and
`RehomeInlineItem` differing in one letter apiece, `_K` against `I`, which is `size_t`
and the ABI difference showing through. The single removal is the twin of x64's: not a
deletion but §21's constification, `QAE` becoming `QBE`, the same member re-mangled and
re-added two lines up.

The manifest is regenerated with the script's own `-Regenerate` rather than hand-edited,
and the diff is exactly +19 / -1 and nothing else. That brought Win32 to **766** mangled
names, which is what x64 declared at that moment, so the two platforms described the same
set of declarations for the first time since `4d39d0d`.

**Then §31 landed, and the point is that it landed on both.** Its four members -- three new
`HeapHolders` and one re-mangled by becoming virtual -- move x64 by +4/-1 and Win32 by the
Win32 spelling of the same five. Both manifests are re-measured against freshly built DLLs
and regenerated a second time, and both finish at **769 mangled and 1051 exports**:

```
out\x64\Debug\Msgcore.dll   (x64): 1051 exports -- 282 flat msgcore_*, 769 mangled C++
out\Win32\Debug\Msgcore.dll (Win32): 1051 exports -- 282 flat msgcore_*, 769 mangled C++
  OK -- exported surface matches the manifests: 282 flat, 769 mangled.
```

Against `4d39d0d` that is +22/-1 on Win32 and +4/-1 on x64, the difference being only that
x64 had already banked the nineteen. A section that adds a member now moves two manifests
instead of silently moving one and leaving the other to be discovered, which is the whole
of what this section is for.

**The flat ABI is byte-identical, and the regeneration is what proves it.** `-Regenerate`
rewrites `exports-flat.manifest` from whichever DLL it measured, so pointing it at the
Win32 DLL was a test of the script header's claim that the flat surface is
platform-independent -- a claim that had never been put to anything, since only x64 had
ever been regenerated. SHA-256 before and after: `3E494A36...DE3B` both times. In the
second round the file is rewritten twice more, once from each platform's DLL, and `git
status` reports it unmodified through all four rewrites. 282 flat names either way. No
version bump is due for any of this.

### What the /sdl paragraph still says, and the one number in it that had moved

`check_exports.ps1`'s header carries a long account of item 10: that `Debug|Win32` once
exported 1006 names where `Release|Win32` exported 984, that the 22 extra were
`?__autoclassinit2@<class>@@QAEXI@Z` emitted by `/sdl`, and that turning `/sdl` on
everywhere moved them into the base manifests and retired the supplement. The check does
not need that story to run -- `-Configuration` selects no expectation any more -- but the
header says outright that the claim is "re-earned on every push", so it was earned rather
than quoted. `/sdl` is on the `CL.exe` line for both `Debug|Win32` and `Release|Win32`;
both DLLs export the same count; `Compare-Object` over the two sorted sets returns **zero**
differences, identical sets and not merely identical counts. The split is genuinely
closed.

**The count is 23, not 22.** One more class is exported than on the day that paragraph was
written -- `MsgStck`, by §26 and §29 -- and the synthesised member tracked it. Every
figure in that header had moved the same way: 700 mangled names where the file now
declares 769, and `982 exports differ in 1340 names` between the platforms where the same
comparison now gives 1051 and 1470. The mechanism the paragraph describes is intact and
the arithmetic in front of it was stale, which is the failure §29 met in the manifest
itself, one file over. The present-tense figures are brought current; item 10's own
numbers are left exactly as written, with a note saying so, because rewriting them would
destroy the measurement they are.

## 34. The two members the drift check was failing on, and the one it was not

§29 left `check_api_drift.ps1` red and said why: `P3PmsgVect::Drop` and
`P3PmsgField::operator!=` reach no binding and have no allowlist line, and deciding
whether the flat C surface should carry them "is a question about the C API rather than
about `^`". It is, and it is a question with an answer. Both are answered here, and
answering them found a third thing that neither of them is about.

```
Scanned 286 public members over 1 pair(s); 116 bound, 168 allowlisted.
  163 of the allowlist entries are UNTRIAGED -- banked, not decided.

2 upstream members reach no binding:

  P2Pmsg.h:768
      P3PmsgField::operator!=   (operator)
      looked for : msgcore_field_*
  MsgVect.h:95
      P3PmsgVect::Drop   (method)
      looked for : msgcore_vect_*
```

### `Drop` is not the drop the surface already has

**The precedent that would have settled this does not exist.** `P3PmsgList::Drop`
(`MsgList.h:122`) is the same member on the sibling class, and the check scores it
`bound -> msgcore_list_drop_head`. It is not bound. `msgcore_list_drop_head` and
`msgcore_list_drop_tail` bind `P3PmsgList::DropHead` and `::DropTail` (`MsgList.h:78,88`),
which take one element off an end. The whole-object drop reaches no binding on either
class; on the list it is merely invisible, for a reason the last part of this section is
about.

So the reason had to be written rather than copied, and "internal" was not available.
**`Drop` frees the item in the store; `destroy` releases a HANDLE to it.** The flat
surface already spells the second once per family -- `msgcore_field_destroy`,
`msgcore_attr_destroy`, `msgcore_desc_destroy`, `msgcore_list_destroy`,
`msgcore_vect_destroy` -- so `msgcore_vect_drop` beside `msgcore_vect_destroy` would be
two exports that read the same and do the opposite. Underneath the naming there is an ABI
constraint that is not cosmetic. The surface removes things by asking the PARENT and
naming the child -- `msgcore_field_delete_item`, `msgcore_attr_delete`,
`msgcore_desc_delete`, `msgcore_list_delete_at`, `msgcore_vect_delete` -- or by emptying
in place, `msgcore_*_truncate`. Both leave every handle the caller holds valid. `Drop`
asked of the item ITSELF cannot: it frees the block other live handles alias, and the
durable re-resolution route rule 2 of `Msgcore_c.h` offers against relocation -- hold the
p2pos, come back through `msgcore_mgr_p2pos2field` -- resolves to nothing once the item is
gone. **The flat ABI has no way to tell a handle that it died, so it does not hand out the
call that kills one.**

Two things that look like counter-examples and are not. `d2763ce` wrote
`P3PmsgVect::Drop` for exactly one caller -- `MsgStck::Pop` freeing the generation it had
just unlinked, "no vect had ever been a pushed stack generation" (`MsgVect.cpp:585`) --
and that caller IS on the flat surface, as `msgcore_stck_pop`, which reaches `Drop` from
the inside. And `msgcore_stck_drop` frees no item the caller holds a handle on and unlinks
nothing from the tree: its own comment is "Release the stacked position without restoring
it" (`Msgcore_c.h:886`).

The same argument covers `P3PmsgField::Drop`, `P3PmsgAttr::Drop` and `P3PmsgDesc::Drop`,
all three of which were UNTRIAGED, so all three are triaged with it.

### `!=` was untriaged and `==` was invisible, for no reason but timing

§21 settled what these two ask: identity -- the same heap handle and the same block
address, therefore the SAME ITEM -- and not value equality, which is `r_data()` and
`r_name()`. **The flat surface already carries that question, as a value rather than as a
predicate.** `msgcore_field_get_p2pos` is "the natural inode number (st_ino)"
(`Msgcore_c.h:547`), and two handles denote the same item exactly when their p2pos are
equal and non-zero, compared with the caller's own `==`.

What makes this a decision rather than an omission is that the operator is the WEAKER
test. It compares a block address, and rule 2 at the top of `Msgcore_c.h` says any
allocating call may move every block, so a `msgcore_field_equals` would be answerable only
between two handles re-resolved since the last mutation -- where the p2pos comparison
survives one. Exporting it would put a weaker identity test beside a stronger one already
there. The residue is small and is named in the line rather than left out of it: p2pos
returns 0 for an unaddressable standalone field, so two distinct FLOATING fields compare
equal on the flat surface where the operator would separate them.

**`==` was not on the check's list and should have been.** `4f4c334` added both identity
overloads on the same day. `P3PmsgField::operator ==` was already a banked NAME, because
`operator == ( LPCTNAM )` predates it -- `P2Pmsg.h:720` at `4f4c334^` -- and the check asks
about each distinct name once, so the new `==` collapsed into the line that already existed
and never resurfaced, while `!=` was a new name and surfaced immediately. They are one
decision and are triaged as one.

```
Scanned 288 public members over 1 pair(s); 116 bound, 172 allowlisted.
  159 of the allowlist entries are UNTRIAGED -- banked, not decided.
  43 of the 116 bound matched by prefix fallback, 28 of those at a bare accessor root.

OK -- every upstream member has a binding, or an allowlisted reason not to.
```

163 was right when §29 counted it. It is 159 now: four lines moved out of UNTRIAGED and
two were new. The scanned total is 288 rather than 286 and the allowlist 172 rather than
170 because §31's two collection `HeapHolders` arrived while this was being written, and
the check caught them -- which is the check working. `P3PmsgData::HeapHolders` needs no
line: that class has no family in `api-drift.config.psd1` and is never scanned, for the
reason the config states in its own comment.

### The third thing: `bound` is not measuring what it says

`P3PmsgList::Drop` being scored `bound` is not a one-off. The matcher tries the exact
snake-cased name first and then falls back to a prefix root, and the root is the FIRST
segment only:

```powershell
$root = $target + ($snake -split '_')[0]
foreach ($n in $surfaceNames) {
    if ($n -eq $root -or $n.StartsWith($root + '_')) { $hit = $n; break }
}
```

The comment above it says what the fallback is for, and for that case it is right:
`DeclareItem` becomes `declare_item`, roots at `declare`, and matches
`msgcore_..._declare_double` -- one C++ name spread across overloads the surface spells
out. For a ONE-WORD member the first segment is the whole name and nothing is lost. For a
multi-word member whose first segment is an accessor verb, the root is `get`, `is` or
`set`, and it matches whatever comes first in the surface list:

```
P3PmsgList::GetHeadPos    -> msgcore_list_get_count
P3PmsgList::GetTail       -> msgcore_list_get_count
P2PmsgMgr::IsField        -> msgcore_mgr_is_dirty
P3PmsgField::IsInline     -> msgcore_field_is_null
P3PmsgField::GetPath      -> msgcore_field_get_name
```

None of those is a binding. **43 of the 116 "bound" match by the fallback, and 28 of those
collapse to a bare `get` / `is` / `set` root.** The honest bound count is nearer 88 than
116, and the backlog this check exists to work down is correspondingly LARGER than the 159
it prints, not smaller.

Those two numbers are printed by the check now rather than asserted here, for the reason
its own allowlist header gives about the UNTRIAGED count -- a number nobody prints is a
number nobody reads -- and `-ShowBound` names every one of the 28:

```
  43 of the 116 bound matched by prefix fallback, 28 of those at a bare accessor root.
```

**One of those false positives is load-bearing, and it is §29's own.** There is no
`msgcore_field_is_sole` in `Msgcore_c.h`; there are nine `msgcore_field_is_*` predicates
and not one of them answers this question. `IsSole` scores bound against
`msgcore_field_is_null` by the `is` root. So the `HeapHolders` triage reason §29 wrote --
"a caller on the flat surface wants the answer and has it -- `IsSole` is the question the
C API would carry" -- says something that is not true. The argument around it survives
whole: the working of a question is not the question, `HeapHolders` is only meaningful next
to `P2PmsgHeap_RefCount` which the surface does not expose either, and it takes a
`P2PmsgHANDLE` which is not a type the surface has. The sentence is corrected in place and
the premise it was resting on becomes an open entry. `IsInline` is in the same position, by
the same root.

**Nothing is bound here and the matcher is not tightened here**, and both of those are
deliberate. Tightening it turns 28 silent passes into 28 new failures in one commit, every
one of them a separate question about the C API -- which is the work §29 declined for two
members and this section has done for six. Doing it as a side effect of a `^`
investigation would be the same mistake in the other direction. What this section owes the
next reader is the measurement, and the measurement is above.

## 35. The gate did not fail to silence the assertion -- it caused it

**§32 recorded four images that assert on their way to being refused, and read the defect
one way round.** It looked like an assertion the gate had failed to suppress: a developer aid
on the trusted path, reached by accident from the untrusted one, firing about a structure the
loader was a moment from rejecting by name. It is the other way round. Every check INSIDE
`P2PmsgHeap_AssertVBlocksBSTRio` is already gate-aware -- `VBHEAP_ASSERT0` and `VBHEAP_DIAG`
(`MsgVBHeap.cpp:1687`, `:1690`) stay silent inside a gate and feed `bResult` instead -- so
within the gate the walk said nothing, exactly as designed, and handed its verdict to one
ungated `ASSERT` on its return value. Outside a gate that same walk REPAIRS the image's key
block, answers true, and the assertion never fires. The gate is what made it fire: it did not
leave noise in place, it created noise, on precisely the images it was built to reject.

### The four entry points, measured rather than recalled

Two arms, three overloads each, and only one of the six had the defect. Each arm has a
builder that takes sizes (`MsgVBHeap.cpp:3018`, `:3368`), a length-validated entry point for
untrusted bytes (`:3078`, `:3443`) and a single-argument delegate the second calls (`:3177`,
`:3553`). On both arms the untrusted overload opens a `P2PmsgHeap_UntrustedGate` (`:3116`,
`:3517`), calls the delegate, and then runs the walk itself as the real gate, throwing on a
false answer (`:3155`, `:3535`). Five of the six ASSERT sites were already gate-aware or
unreachable from a gate; the sixth, the last line of `P2PmsgHeap_CreateBSTRio(VBListBSTRio*)`
at `:3609`, was not.

The gate opens BEFORE the delegate on both arms, not around the walks that follow it, and
that position is what put the delegate's assertion inside it. §39 named the IOMAGE twin as
`MsgVBHeap.cpp:3330` and `:3438` and neither is it: `:3330` sits in the commented-out block
between `CreateIOMAGE(pIOmage)` and `CreateSYS`, and `:3438` is the BSTRio BUILDER, already
guarded. The twin's real call sites are `:3287` and `:3288` and have carried the guard all
along -- the entry was right that the arm was unmeasured and wrong about where to look.

### Measuring the twin, with a control for the guard itself

Reading a guard does not establish that it does anything, so §40's probe grew a second
phase: every image that gets past the untrusted entry point's length and classification
gates is opened AGAIN through the trusted single-argument overload, with no gate open, and
the assertions counted separately. That turns "the guard is present" into "the guard
suppresses N assertions on this file".

```
TRUSTED-DELEGATE: tests\out\c4_f3_iomage_walk.iom
  asserts trapped by the trusted delegate: 11
  AssertValid*(handle)=true  AssertVBlocks*(handle)=false  (+10 asserts)
```

Eleven. Through the untrusted entry point the same file reports `asserts on this file: 0`
and `IOMAGE block structure is corrupt`. The IOMAGE guard is load-bearing on exactly the
image that would otherwise exercise it: that arm does not have the defect, it has the fix.
The BSTRio arm is the same shape with the guard missing -- the F3 lesson running the other
way, the pattern fixed on one branch and the other left.

### The fix, and the call sites that decide how far it goes

One line, matched to the twin (`MsgVBHeap.cpp:3634`):

```cpp
    if ( !P2PmsgHeap_InUntrustedGate() )
      ASSERT(P2PmsgHeap_AssertVBlocksBSTRio(pHandle));
```

The assertion is KEPT, and the distinction the entry asked to be shown rather than asserted
is real, though not for the reason it sounded like. There is no live trusted caller of
`P2PmsgHeap_CreateBSTRio(VBListBSTRio*)` in this repository at all: the only in-tree call is
`MsgVBHeap.cpp:3519`, the untrusted overload's own delegation, and the one in
`P2PmsgMgr.cpp:340` sits inside the `Load` that lines 290-371 comment out. The overload is
on the header surface (`MsgVBHeap.h:132`), so external callers may hold one, and for them
the walk is what it always was -- a bug in the caller, worth stopping on. Removing the
assertion would take that away from every consumer to fix a defect that exists only inside
the gate. Gating it takes it away from nobody.

**And the doubled walk goes with it.** Inside a gate the delegate no longer walks at all, so
the load walks the image once instead of twice, and the surviving walk at `:3535` is the
only STRUCTURAL gate on that path -- worth saying plainly, because `:3564` reads like a
second one and is not. `P2PmsgHeap_AssertValidBSTRio(pBSTRio)` there passes the raw image
buffer through a parameter typed `P2PmsgHANDLE`; the function casts it to `VBListHANDLE*`
and early-returns `true` when the byte at the `uVBListType` offset is not
`P2PmsgHeap_BSTRio`. That is the type confusion the IOMAGE twin had removed for the same
reason at `MsgVBHeap.cpp:4983`, still here. Measured rather than inferred: it answered `true`
for all twelve BSTRio images in hand, the four corrupt ones included.

### Before and after, over the same nineteen images

The corpus of §32, one pristine `git worktree` of `46a63f8` against one carrying only this
change, so that nothing else in the tree could be the difference. The whole diff of the
per-file outcomes is five hunks, of which the first four are this hunk four times over, at
lines 9, 12, 30 and 39 -- `f11_collate_nogrow.dat`, `f2_walk_oob.dat`,
`c4_bstrio_honest.dat`, `c4_f2_walk_oob.dat`, which is §39's four:

```
9c9
<   asserts on this file: 1
---
>   asserts on this file: 0
[12c12, 30c30, 39c39 identical]
66c66
< PHASE 1 (untrusted entry points) asserts trapped = 4
---
> PHASE 1 (untrusted entry points) asserts trapped = 0
```

Nothing else changed at all. Every refusal message, every block count, every chain verdict,
every one of the eleven files refused before a walk can start: identical. All four are still
refused, still by name, still with `BSTRio block structure is corrupt`. The trusted phase is
identical too, at 115 assertions either way; the only line that moves in it is the
assertion's own, `3609` to `3635`, because the note now above it is longer than the line it
explains.

### The mechanism, in six lines

The sweep proves the outcome; it cannot show WHY the gate was the cause, because the corpus
files are corrupt several ways at once. One image and one counter isolate it -- save a real
store, add one to `oKeys.nAllocEntries`, leave the header, the declared size and all three
root offsets exactly as the writer left them, so every gate ahead of the block walk passes
and the walk is what decides:

```
############ BEFORE (pristine 46a63f8) ############
[1] honest image      : accepted=1  why=''  asserts=0
[2] nAllocEntries+1   : accepted=0  why='BSTRio block structure is corrupt'  asserts=1
    first: ...\MsgVBHeap.cpp(3609) : Assertion failed!
[3] same, TRUSTED 1-arg: accepted=1  why=''  asserts=1
    first: ...\MsgVBHeap.cpp(2087) : Assertion failed!

############ AFTER (46a63f8 + the fix) ############
[1] honest image      : accepted=1  why=''  asserts=0
[2] nAllocEntries+1   : accepted=0  why='BSTRio block structure is corrupt'  asserts=0
[3] same, TRUSTED 1-arg: accepted=1  why=''  asserts=1
    first: ...\MsgVBHeap.cpp(2087) : Assertion failed!
```

Line [3] is the whole argument. The SAME image through the trusted overload is ACCEPTED, in
both builds, and asserts at `:2087` -- `VBHEAP_DIAG(...nAllocEntries==nAllocEntries)`, the
diagnostic that then repairs the counter and lets the walk answer true, so `:3609`'s
assertion passes and never fires. Open a gate and that repair is refused, `bResult` goes
false, and the ungated assertion on the return value fires in place of the diagnostics the
gate correctly silenced. A corrupt image was louder inside the gate than outside it. And [3]
is unchanged across the fix, which is the other half: the developer aid the trusted path is
entitled to is exactly as it was.

### Pinned

`Test_UntrustedBSTRioGate` (`tests/MsgcoreSuite.cpp:5692`) is that experiment as two cases:
the honest image must still load -- the way a fix of this shape goes wrong is by becoming a
blanket refusal -- and the corrupt one must be refused, by that message, with zero
assertions. It counts them through a `_CrtSetReportHook2` pushed in front of
`TestFramework`'s own trap and returns FALSE, so the framework still fails the case by
itself; the counting hook is there to NAME the line, not to catch what is already caught.
Static link only, for the reason `Test_ImageAddressBounds` gives beside it: no
`P2PmsgHeap_*` symbol is on the exported surface.

It fails without the fix. The same suite sources, linked against the pristine `46a63f8`
archive:

```
  - a corrupt image is refused BY NAME and without a single assertion
      ASSERT [a corrupt image ...]  ...\wt-46a63f8\MsgVBHeap.cpp(3609) : Assertion failed!
      FAIL [a corrupt image ...]  s_nGateAsserts == 0
  cases   : 251  (1 with failures)
  checks  : 1377  (2 failed)
  result  : FAIL
```

One case, two checks, and the failure names the line. Against the fixed archive both link
modes are clean -- `static` 251 cases / 1377 checks, `dll` 240 / 1308, both PASS, the DLL
run reporting the case as static-link-only rather than silently skipping it.

### What is not closed

The `:3564` type confusion. It is measured and it is a no-op, so the "Corrupted BSTRio heap"
refusal it appears to provide has never once been the thing that refused an image; the
structural verdict on this path comes entirely from `:3535`. Repairing it is not a tidy-up
-- it would introduce a refusal where there is none today, ahead of the walk, and change
which message names a bad image. That is a question about `P2PmsgHeap_AssertValidBSTRio`'s
contract and wants the argument §29 and §34 wanted for members like it.

## 36. The one reference a store holds on itself

§29 walked what a field owns and §31 closed the three members that walk stopped at.
What-is-left carried one more, and it was the only one phrased as an absence rather than a
member: `P2PmsgMgr` is not descended into. It derives from `P3PmsgItem` and inherits
`P3PmsgField::HeapHolders` unchanged, so any sub-objects of its own are uncounted -- an
undercount, which leaves FALSE, which promises nothing. Nobody had measured what a manager
owns.

**Measured here, and the answer comes in two halves: a manager owns no sub-object a field
does not, and the one reference it adds on top is not a view of anything.** The first half
closes the entry as written. The second is what the entry was actually standing on, and it
is a decision rather than a gap.

### Every member, and what it holds

`P3PmsgItem` is a typedef for `P3PmsgField` (`P2Pmsg.h:893`), so a manager IS a field. It
arrives with `m_oObject`, `m_pP3PmsgAttr`, `m_pP3PmsgDesc` and `m_pMsgStck`, all four of
which §29's override already walks and all four of which the rows below show it reaching
through a manager. What `P2PmsgMgr` declares of its own is nineteen members
(`P2PmsgMgr.h:336-355`), and not one of them is a `P3PmsgObject` or holds one:

- `m_hFile`, a Win32 file handle; `m_strFilename`, a `CString`; `m_oGUID`; `m_dwSharedMode`;
  and `m_oCSectionMgr`, a `CRITICAL_SECTION`. None of them names a block, and none of them
  is on a heap.
- `m_uiWM_APP_TrigINSERT`, `...UPDATE` and `...DELETE`, three window message numbers. **The
  trigger registry is not in the manager at all**, which is worth saying because the class
  comment advertises a trigger system and a registry of armed nodes would be exactly the
  kind of member this section is looking for. It lives on the heap handle:
  `CreateTrigger` forwards straight to `P2PmsgHeap_CreateTrigger ( m_hMgr, ... )`
  (`P2PmsgMgr.cpp:1083-1098`), `DropTriggers` to its twin (`:1269-1280`), and every other
  arm of it is commented out. An armed trigger adds no holder here.
- six callback function pointers and four `PINT_PTR` keys, which is the paging registration
  and its one-deep push and pop.
- and `m_hMgr`, which is the rest of this section.

Nothing in `P2PmsgMgr.cpp` news a `P3Pmsg` sub-object and caches it. The only `new` in the
file is `Factory`'s `new P2PmsgMgr` (`:569`, `:597`), which is a manager rather than a
member, and `SafeDSetPaging` holds a `P3PmsgItem*` (`:1524-1527`) that is somebody else's
item borrowed for the life of a stack guard. So the inherited walk is already complete over
every view a manager owns. That is a negative answer of the kind §31 found for
`P3PmsgVect::m_pP3PmsgData[]`, and it is worth having for the same reason: the entry
recorded a hole in the walk, and the walk has none.

### The reference that is left over

Every one of the four constructors ends the same way -- `m_hMgr = P2PmsgHeap_CreateBSTRio(...)`
and then `Attacheap`, which allocates the root item and `Connect`s the inherited object on
that handle (`P2PmsgMgr.cpp:38-39`, `:47-48`, `:56-57`, `:66-67`, `:150`). The create mints
the handle at `nRefCount = 1` (`MsgVBHeap.cpp:3400`); the `Connect` AddRefs it a second time
by §29's rule. `~P2PmsgMgr` closes the first (`:74-76`) and `~P3PmsgObject` closes the
second. **So a manager holds its own heap exactly twice and `HeapHolders` accounts for one
of them**, which is why `refs` is exactly `mine + 1` on every row of a manager nobody else
is holding, whatever else the manager has grown.

It is owned outright, too, by §26's test. No constructor and no setter takes a handle from
outside; `Load` creates its own from the file image (`:235`, `:261`); and the defragmenting
arm of `Save`, the one place a manager adopts a handle another manager made, AddRefs it on
the way in (`:416-419`) so the donor's `~P2PmsgMgr` closes its own reference and not this
one. Both of §29's questions therefore answer yes: it holds this heap, and it is mine.

### Why it is not subtracted anyway

**Because `HeapHolders` counts views, and this is not one.** The rule the walk rests on is
an object rule: every path that gives a `P3PmsgObject` a non-zero `m_hVBList` AddRefs it, so
a live object whose handle is this handle IS one reference, and `refs - mine` is the number
of OTHER VIEWS -- other names for blocks, each of which a write could be made through.
`m_hMgr` is not a `P3PmsgObject`, was never `Connect`ed and names no block. Counting it does
not overcount the arithmetic; it changes what the equality means, from "every view of this
heap is one of mine" to "every reference on this heap is one of mine, view or not".

**And the block it would report the guarantee for is the one block on the heap whose
address is a function of the handle.** `P2PmsgHeap_Connect` reads the store root straight
off the handle (`MsgVBHeap.cpp:5040-5049`, `:5031-5037`), and that root is the manager's own
item -- measured, not assumed: a bare manager's `GetP2Pos()` is 48, `P2PmsgHeap_Connect(h)`
is 48, and `P2PmsgHeap_IsRoot` agrees. Everywhere else on the heap the count's premise
holds, that an outsider cannot name a block without a counted reference to reach it
through. For the store root it does not, and a raw handle held without an AddRef is already
outside the count by `P3PmsgObject::IsSole`'s own NOTES (`P2Pmsg.cpp:3370-3373`).

So the undercount stays, deliberately, and `!mgr.IsSole()` still reads as it always has --
including the row this investigation pinned in §26, `tests/MsgcoreSuite.cpp:3207`. What
changes is that the shortfall is now known to be exactly one, known to be the handle, and
pinned by cases rather than left as an unmeasured absence. The NOTES block on `m_hMgr`
records the argument where the member is declared.

### Measured

Six cases and thirty-nine checks, twenty-five of which survive into DLL mode. The `refs=`
column is static-only for §31's reason -- `P2PmsgHeap_RefCount` is not exported -- but
`mine=` is not, because `HeapHolders` is a public member of a whole-class exported class, so
the numbers that carry the argument are checked either way:

```
  - a manager's walk reaches every view it owns
      a manager nobody has touched                 refs=2  mine=1  sole=false
      ... with a descendant collection             refs=3  mine=2  sole=false
      ... and that collection walked               refs=4  mine=3  sole=false
  - the reference a manager's walk does not make is its own handle
  - a manager's attributes, stack and snapshot are walked like a field's
      a manager with an attribute collection       refs=3  mine=2  sole=false
      ... and a push                               refs=4  mine=3  sole=false
      ... and the snapshot read                    refs=5  mine=4  sole=false
  - a manager whose root a stranger names is still not sole
      a manager whose root a stranger names        refs=3  mine=1  sole=false
  - a manager whose child a stranger holds is still not sole
      a manager whose child a stranger holds       refs=5  mine=3  sole=false
  - a copy of a manager is a second store, not a second view
      the original                                 refs=4  mine=3  sole=false
      the copy                                     refs=3  mine=2  sole=false
```

The first and third cases are the walk being complete: every step adds one holder and
`mine` follows it -- the `P3PmsgDesc` the manager made, the cursor that collection keeps for
itself, the `P3PmsgAttr`, and the field a read of the stack news on this heap, which is
§29's last row reached through a manager. The shortfall never grows past one.

The two stranger rows are the ones that must not move, and they are where the shortfall
doubles: `refs=3 mine=1` for a second name on the manager's own root, `refs=5 mine=3` for a
handle on a child. The count falls short by precisely the stranger's holding on top of the
handle, which is what leaves the answer false. The last case is the copy constructor, which
makes its own heap (`P2PmsgMgr.cpp:56`) and copies the tree into it: two stores, two
handles, and neither manager a holder of the other's heap.

### Teeth, and they point the other way

There is no behaviour change here to break, so the case that pins this decision has to fail
against the decision being reversed rather than against the code being restored. Add the
four-line `P2PmsgMgr::HeapHolders` override that subtracts `m_hMgr`, rebuild, and **seven
cases fail on twenty-seven checks** -- the six above on twenty-six, and one that was already
in the suite:

```
  - nothing in a tree is sole
      FAIL [nothing in a tree is sole]  !mgr.IsSole()   MsgcoreSuite.cpp:3207
  - a manager's walk reaches every view it owns
      a manager nobody has touched                 refs=2  mine=2  sole=true
      ... with a descendant collection             refs=3  mine=3  sole=true
      ... and that collection walked               refs=4  mine=4  sole=true
  - a manager whose root a stranger names is still not sole
      a manager whose root a stranger names        refs=3  mine=2  sole=false
  - a manager whose child a stranger holds is still not sole
      a manager whose child a stranger holds       refs=5  mine=4  sole=false
```

That listing is the whole argument in four rows. A store with a tree under it reads
`sole=true`, and the row §26 pinned -- the one whose title is "nothing in a tree is sole" --
goes red on `!mgr.IsSole()`. The two stranger rows still read false, because a stranger
still defeats the count either way; they fail on the shortfall number rather than on the
guarantee, exactly as §31's must-not-move vect row failed on its opening `TF_CHECK`. The
guarantee is not what a subtraction here would break. What it would break is the claim that
`IsSole`'s TRUE is about views, and it would make the store the one object that answers a
different question from everything inside it.

**The exported surface does not move.** No member is added and none is removed: the library
change is a NOTES block above `m_hMgr` in `P2PmsgMgr.h` and nothing else, so
`exports-cxx-x64.manifest`, `exports-cxx-win32.manifest` and the flat C ABI manifest are all
byte-identical, and `api-drift.allow` gains no line. That is the shape of this entry -- a
measurement that found the existing behaviour already right, which is a close and not a fix.
What-is-left keeps one sentence in place of the entry: a manager holds its heap once more
than its walk claims, on purpose, and the reason is written where the member is.

## 37. Twenty-eight silent passes, and the two questions underneath them

§34 measured the drift check's prefix fallback and declined to fix it, in as many words:
tightening it "turns 28 silent passes into 28 new failures in one commit, every one of them
a separate question about the C API", and doing that inside a `^` investigation would be
the wrong session. This is the right session. The matcher is tightened, the check went red
with thirty-five members on it, every one now carries a written reason, and the three
entries §39 was holding -- the matcher, `P3PmsgList::Drop`, and `IsSole` with `IsInline` --
close together, because they were always one entry. **The honest bound count is 81, not
116, and not the 88 §39 guessed.**

### The fallback needed a rule and had a heuristic

The old fallback is three lines, quoted in §34. It roots a C++ member at the first
snake-cased segment and takes whatever the surface lists first:

```powershell
$root = $target + ($snake -split '_')[0]
foreach ($n in $surfaceNames) {
    if ($n -eq $root -or $n.StartsWith($root + '_')) { $hit = $n; break }
}
```

The shape it exists for is real and no exact comparison can find it: `DeclareItem` is one
C++ name (`P2Pmsg.h:814`) that the flat surface spells out as **twenty** functions --
`msgcore_field_declare_int`, `_declare_double`, `_declare_blob`, on through the type list
and their `_u8` twins -- because C has no overloads. What went wrong is that three other
shapes are indistinguishable from that one through a first-segment prefix, and `break` on
the first hit made all three look like it. The tightening is `Test-RootCandidates`
(`tools/ci/check_api_drift.ps1:313`): three tests, applied to the candidate SET rather
than to the first name that matched.

**Test one: a surface name equal to the root is a truncation, not an overload.** By the
time the fallback runs the exact test has failed, so the C++ name carries segments the root
does not -- and a function named exactly the root carries none of them either.
`P2PmsgMgr::RootPath2Object` rooted at `root` and matched `msgcore_mgr_root`
(`Msgcore_c.h:541`), which returns the manager's root field and resolves no path at all.
That arm could only ever fire for a multi-segment name, where it is always this mistake.

**Test two: an accessor verb is the family's verb, not this member's name.** A root of
`get` / `set` / `is` / `put` / `has` with more name behind it matches every accessor in the
family and distinguishes nothing. This is §34's 28, and the old code already knew about it
-- it computed `$bareVerb`, printed the count, and scored the match anyway. `IsInline`,
`IsSole` and `IsDirty` all rooted at `is` and all scored against `msgcore_field_is_null`.

**Test three: a function that is already somebody's is not evidence for somebody else.**
Two ways a candidate is spoken for. It is the exact counterpart of a different scanned
member -- `msgcore_list_drop_head` is `DropHead`'s (`MsgList.h:78`), so it cannot also be
whole-object `Drop`'s. Or it is the UTF-8 twin of the function beside it:
`msgcore_attr_select_item_u8` is `msgcore_attr_select_item` spelled for `char*`. Without the
second half, `P3PmsgAttr::Select` would score against a `_u8` name once `SelectItem`,
`SelectList` and `SelectVect` had taken the three real `select_*` functions.

The order matters, because it makes the refusal count mean something: a member with nothing
under its root is an ordinary miss rather than a refusal, and running test two first would
put every `P3PmsgBSTR` member in the tally. Survivors are then ranked by how many of the
C++ name's own segments each spells, so `AddListTail` now reports
`msgcore_list_add_tail_int` rather than `_add_head_int` -- no verdict moves, but what the
run says the evidence WAS does.

### The numbers, before and after

Before, against a clean tree at `4e897f7` with §34's two lines in place:

```
Scanned 288 public members over 1 pair(s); 116 bound, 172 allowlisted.
  159 of the allowlist entries are UNTRIAGED -- banked, not decided.
  43 of the 116 bound matched by prefix fallback, 28 of those at a bare accessor root.

OK -- every upstream member has a binding, or an allowlisted reason not to.
```

After the matcher change and before any triage -- red on thirty-five, with the refusals
splitting exactly as the three tests predict:

```
Scanned 288 public members over 1 pair(s); 81 bound, 172 allowlisted.
  8 of the 81 bound matched by prefix fallback; 35 more were refused by it.

35 upstream members reach no binding:

   28 root is a bare accessor verb
    6 every function under the root already binds another member
    1 only a truncation of the name matches the root
```

The 28 is §34's 28, member for member, which is the closest thing to a control this change
has: the rewrite was not steered at that list and lands on it exactly. The eight that
survive are the shape the fallback was written for -- `DeclareItem` three times,
`AddListHead`, `AddListTail`, `P3PmsgCurs::Goto`, `P3PmsgCurs::Item` -- and one that is
not, which the end of this section is about.

### Thirty-five members, four arguments, and three that are bound after all

The reasons are in `tools/ci/api-drift.allow:300` onwards. What is worth recording is that
thirty-five members did not need thirty-five arguments -- grouping them is what §34 did for
the four `Drop`s and the two identity operators.

**A list position is a raw arena offset, and the ABI cannot hand one out.** `GetHeadPos`,
`GetTailPos`, `GetNext`, `GetPrev`, `GetTail`. `VBLaddr` is `#define VBLaddr UINT_PTR`
(`Msgcore.h:49`), so it fails twice: rule 2 of `Msgcore_c.h` invalidates the value on the
next allocation, with none of the re-resolution route a p2pos has, and the type would make
the flat ABI pointer-width-dependent -- §33's `x86` split arriving in the supported surface.
The surface walks by ordinal and by cursor, both re-validated per call.

**The arena's internals are not the ABI, in three different ways.** `GetVBLockType` takes
an MFC `POSITION` (`MsgList.h:125`, `MsgVect.h:101`) and answers with the block type code
§40's image scanner reads out of `uVBLockDefs`, while `GetVBLockListSize` and
`GetVBLockVectSize` answer with the block's byte size -- where the surface asks about
CONTENT instead, `msgcore_list_get_type_at` and `msgcore_vect_get_type`. The four
`GetP2Pmsg*Hdl` and `GetP2PmsgHandle` getters hand out `P2PmsgFieldHdl`, three bare
`UINT_PTR`s (`P2PmsgVBLock.h:276-281`), beside an opaque `MsgFieldHandle`: two handle
vocabularies in an ABI with one set of rules for handles. And `GetAccess`, `SetAccess` and
`GetPermissions` return bits nobody on that side can name -- the access byte's vocabulary
(`AttrField_HIDDEN`, `_NOCOPY`, `_SORT`, `_TOUCH`, `_EXPAND`, `P2PmsgVBLock.h:283-289`)
sits in a header `Msgcore_c.h` does not include, and the permission byte has none at all.

**The dirty flag belongs to the C++ object, not to the item.** `P3PmsgField::IsDirty` is
`m_bFieldDirty || P3PmsgData::IsDirty() || P3PmsgName::IsDirty()` (`P2Pmsg.cpp:4293-4300`)
and the vect's adds `m_bVectDirty` -- every term an in-memory flag on the wrapper, set by
writes made through THAT wrapper, so two flat handles onto one item are two objects with
two flags. `msgcore_mgr_is_dirty` is the question that outlives a handle.

**Two calls, not one, is the shape the whole handle group is written in.** That covers
`P2PmsgMgr::IsField` and `IsAttributed`, keyed on a `P2Pos` and answered by
`msgcore_mgr_p2pos2field` then a field predicate; `GetPath`, answered by
`msgcore_field_get_p2pos` plus `msgcore_mgr_p2pos2path`; `P3PmsgDesc::GetField` and its attr
twin, whose answer the caller already holds, since `msgcore_desc_from_field` is the only way
in; and `Select` and `SelectObject` on both collections, where the by-value/by-reference
distinction separating `Select` from `SelectItem` has no counterpart a C caller can observe.

**Three of the thirty-five are bound, and the matcher cannot see it.** `GetP2Pos` is
`msgcore_field_get_p2pos`, quoted by rule 2 itself at `Msgcore_c.h:108-109`; it misses
because `ConvertTo-Snake` splits `GetP2Pos` into `get_p2_pos` where the surface spells
`get_p2pos`. `PageRegistration`'s two overloads (`P2PmsgMgr.h:211-215`) are
`msgcore_mgr_set_paging_sinks` and `msgcore_mgr_set_populate_sink`
(`Msgcore_c.h:1027-1032`), a deliberate rename the C header spends fifteen lines
justifying. Both are the third of the three ways out the check prints, and their lines
exist so the next reader is not told a bound member is unbound. The third case is
`P3PmsgVect::IsName`, declared at `MsgVect.h:145` beside `IsData`, `IsField`, `IsList` and
`IsVect`, which are defined at `MsgVect.cpp:1165`, `:1187`, `:1198` and `:1208` and bind
exactly. **`IsName` has no body anywhere in the repository** -- a defect in `MsgVect.cpp`,
recorded as one.

### `P3PmsgList::Drop` can be seen now, and carries its line

§39's third entry was that `Drop` on the list is decided nowhere and cannot be seen: the
same member as `P3PmsgVect::Drop` under the reason §34 wrote, unable to carry an allowlist
line, because the fallback scored it bound against `msgcore_list_drop_head` and the check
would have reported the line as redundant and told the next reader to delete it. Test three
removes exactly that. `msgcore_list_drop_head` and `_drop_tail` are `DropHead`'s and
`DropTail`'s (`MsgList.h:78,88`); they are spoken for, and the whole-object drop now says
it reaches no binding. §34's parked comment is a rule line under the `Drop` group.

### `IsSole` and `IsInline`: not carried, and deliberately not closed

These two are why §29's `HeapHolders` reason said a flat caller "wants the answer and has
it". It does not. There is no `msgcore_field_is_sole` and no `msgcore_field_is_inline`, and
none of the nine `msgcore_field_is_*` predicates answers either question; both scored bound
through the `is` root and both are among §34's 28. The premise §34 reopened stays open ON
PURPOSE, with the argument written out both ways in `api-drift.allow`.

**For.** `msgcore_field_is_sole ( MsgFieldHandle )` would be signature-identical to the
nine predicates already there, it needs no type the surface lacks, and its TRUE is the one
fact a flat caller cannot reconstruct from anything else on the surface: p2pos answers
identity (§21), not exclusivity, and there is no flat route to `P2PmsgHeap_RefCount`. A
host deciding whether it may write in place has exactly this question and no way to ask it.

**Against.** It is an addition to the supported ABI: `Msgcore_c.h` is the 282-function
surface the README calls supported, a new export bumps `Msgcore_version.h` in the same
commit, and that is a release decision and not a matcher's. The second half matters more.
`IsSole`'s FALSE still promises nothing -- §26's asymmetry, unmoved by §29 and §31 -- so
the export would be load-bearing in one direction and advisory in the other, and the header
would have to say so at length. `IsInline` is the same shape and weaker: its FALSE is
explicitly not a guarantee (§24), and it describes where storage sits rather than what the
caller may do. Both are triaged as not-carried, which records today's state.

### What the tightened matcher still cannot do

**One of the eight surviving fallback matches is still not a binding.**
`P3PmsgField::SelectObject` scores against `msgcore_field_select_list`: the root is
`select`, `msgcore_field_select_item` is `SelectItem`'s, and `select_list` and `select_vect`
have no exact C++ counterpart on `P3PmsgField`, so they are free and the rule takes one.
Telling two nouns apart under one verb is beyond a name comparison and the three tests do
not pretend otherwise. The split stays printed so the residue stays countable.

**And `Sizeof` has never been scanned, on any class.** The extractor's keyword guard
(`check_api_drift.ps1:176-179`) drops a candidate named in a C++ keyword list, and
PowerShell's `-notcontains` is case-insensitive, so `Sizeof` goes out with `sizeof`. Eight
`Sizeof` declarations across the nine scanned headers have never been asked about, and one
has an exact binding waiting -- `P2PmsgMgr::Sizeof` (`P2PmsgMgr.h:298`) against
`msgcore_mgr_sizeof` -- so the honest count is really 82. That is the same KIND of error as
the one above, which is why it is named: the check's own extractor is the last place
anybody looks.

Where the count stands: 172 allowlist entries became 207, all thirty-five new ones TRIAGED,
and UNTRIAGED is unchanged at 159 because nothing was banked here. What moved is the
denominator it should be read against, which is what §39 predicted.

```
Scanned 288 public members over 1 pair(s); 81 bound, 207 allowlisted.
  159 of the allowlist entries are UNTRIAGED -- banked, not decided.
  8 of the 81 bound matched by prefix fallback; 35 more were refused by it.

OK -- every upstream member has a binding, or an allowlisted reason not to.
```

## 38. The ceiling that stopped nothing, and the six in it that are not assertions

**`check_asserts.ps1` has been failing since `d2763ce` on 2026-09-11, and the form it
failed on is not the form that matters here.** The baseline was banked at `4d39d0d`, the
same commit §33 found `exports-cxx-win32.manifest` frozen at; §29 re-measured the first of
that day's three ceilings and §33 the second, and this is the third. It finds the file's own
header right about which half is worse: `callwrap` rose 245 to 248 and `predicate` 192 to
195, both of which CI would have named had anyone run it, while `marker` FELL 216 to 208 --
and in that same range five genuinely new `ASSERT(0)` sites went into the tree without one
line of output, because the ceiling above them was never lowered. The fall is not the
notice. The fall is the hole.

### The baseline is the tree it was banked from, exactly

`tools/ci/assert-baseline.txt` has two commits in its whole history: the initial import, and
`4d39d0d` on 2026-09-08. A pristine worktree of that commit answers the script with the
file's own three figures and nothing else, which makes this a stale-baseline finding and not
a disagreement about what to count:

```
$ pwsh tools/ci/check_asserts.ps1          # git worktree of 4d39d0d
  callwrap   245          $ cat tools/ci/assert-baseline.txt
  marker     216          callwrap 245
  predicate  192          marker 216
  TOTAL      653          predicate 192
```

Thirty-seven commits have touched a non-test source since -- thirty-six when §39 wrote
that sentence, `34fb92c` having landed in between without moving a count. Against a
pristine worktree of `46a63f8` the script answers 248 / 208 / 195, bit for bit what the
working tree answered before this session touched it, which is how the drift was told
apart from our own: nothing in §31 to §34 adds an ASSERT anywhere.

### When the gate went red, and what it never said

The counts replay at every commit in the range, the script's own classifier run over
`git grep` at each revision:

```
Commit  callwrap marker      Commit  callwrap marker
4d39d0d      245    216      0888659      246    207
b6ae7c7      245    213      48ff002      247    207
72e8f88      245    212      f77bb97      248    207
d2763ce      246    211      f600c52      248    208
d31f2c4      246    210      46a63f8      248    208
5d48e5a      246    209
2a03161      246    208      max marker across the range: 216
```

`callwrap` crossed at `d2763ce` -- §7's commit, push and pop a list or a vector -- and has
never come back under, so the gate has failed on every commit since and nobody ran it. But
the `marker` column is the one to read. It never once rises above 216. It falls to 207 by
`0888659` and climbs back to 208 at `f600c52`, and that climb is a real new `ASSERT(0)` at
`P2PmsgVBLock.cpp:1498` -- a rise, of exactly the kind this script exists to report,
reported nowhere, because the ceiling above it was nine sites away. Four more went the same
way: `MsgStck.cpp:244` and `:345` at `d2763ce`, a new type-rejection guard and not a
respelling of the four stubs that commit deleted; `MsgVect.cpp:620`, the `else` arm of
`P3PmsgVect::Drop`'s parent isolation; and `P2Pmsg.cpp:7561`, a new `else` arm in the
`P3Pmsg_GetPath` block `2a03161` added. Five new sites of the form item 19 names, and the
gate's entire output on the subject was `marker fell from 216 to 208 -- bank it`. That is
what "the stale ceiling silently re-admits everything between the two numbers" means when it
stops being an abstraction: the gap is eight wide, and five have gone through it.

### Where the eight went

Not suppressed -- deleted, by the path-grammar work of §13 to §18, because each was a claim
of unreachability the grammar fixes made false and then answered properly. `b6ae7c7` took
three out of `P2Pmsg.cpp` when `^` started being followed, `72e8f88` one when a list became
a resolvable component, `d31f2c4` and `5d48e5a` one each, and `2a03161` two -- its own
message saying why: all four of `@.`, `.@`, `..` and `@@` are well-formed questions naming
nothing, "void, where three of them used to ASSERT(0) first". `d2763ce` took four out of
`MsgStck::Push` and `Pop` by implementing the list and vect arms that had been stubs, and
`0888659` the `else ASSERT(0);` off `P2PmsgMgr.cpp`. Fourteen removals against six added
lines, five of them new sites: net eight, every removal a marker that was a lie.

### The six rises, attributed and decided

Three `callwrap` and three `predicate` net -- five predicates added, two retired -- and
every line below is a `git blame` at `46a63f8`, not an inference:

```
48ff0029 P2Pmsg.cpp:2902    ASSERT(nSizeofHdr>0&&nVBLockSize>nSizeofHdr);
48ff0029 P2Pmsg.cpp:2911    ASSERT(P2PmsgHeap_AssertValidAlloc(m_hVBList,m_aVBLock));
f77bb970 P2Pmsg.cpp:2930    ASSERT(nSizeofHdr>0&&nVBLockSize>nSizeofHdr);
f77bb970 P2Pmsg.cpp:2936    ASSERT(P2PmsgHeap_AssertValidAlloc(hVBList,aCopy));
c8f1af6a P2Pmsg.cpp:3563    ASSERT(OBJ__hVBList==nullptr);
dbfa789e MsgVBHeap.cpp:4418 ASSERT(!bFirstFit);
dbfa789e MsgVBHeap.cpp:4593 ASSERT(!bFirstFit);
d2763cee MsgVect.cpp:593    ASSERT(r_Object().IsVect());
```

`P2Pmsg.cpp:2902` and `:2930` are the same line in two functions and the argument's easy
end. `VBLsize` is `#define VBLsize UINT_PTR` (`Msgcore.h:53`), so `nVBLockSize -
nSizeofHdr` on the next line does not go negative when the test fails, it goes enormous,
and goes straight to `P2PmsgHeap_Alloc` and then a `memcpy` of `nVBLockSize` bytes. In
`P2PmsgObject_CopyHeapVBLock` the size is read out of the block's own header, so on a heap
opened from an image it is whatever the image said. Item 19's first half applies: both
throw now, the `EVERR->MODULE->...->Throw()` idiom already at `P2Pmsg.cpp:2715`.

`MsgVect.cpp:593` is item 19 stated by its own neighbours. The comment above
`P3PmsgVect::Drop` explains that a vect used to fall through to `P3PmsgField::Drop`, "which
opens with ASSERT(OBJ__IsField()) -- false for a vect", and leaked every element block
anyway. The assertion did not stop it, because an assertion is not in the binary that did
the dropping -- and the new function answered that by writing the same assertion one level
down. Nor is it decoration: `Truncate()` below reads the element slot array through
`VBLock_pVect` and frees what it finds, arbitrary bytes on a non-vect block. It throws now.

`P2Pmsg.cpp:3563` is item 19's second half. `ASSERT(OBJ__hVBList==nullptr)` sat in one of six
`P3PmsgField` constructors that all call `RenderThisSafe` and none of which asserts it, and
nothing in either build branches on the answer -- `*this = rhs` runs regardless. It was a
note from §21's session, asserting what the comment above it already says; it is prose now.

The two `ASSERT(P2PmsgHeap_AssertValidAlloc(...))` are NOT changed, and the reason is
worth more than the change. That function is not a predicate: its SYS arm repairs the block
it judges -- `pVBLock->oHdr.uVBLockDefs|=VBLock_Linked` at `MsgVBHeap.cpp:1390` -- and so
does its BSTRio arm, whose comment says "The repair below sets a flag bit IN THE IMAGE" and
skips it inside `P2PmsgHeap_UntrustedGate`. `VBLock_Init` (`P2PmsgVBLock.cpp:327`) sets
`VBLock_Alloc` and not `VBLock_Linked`, so that repair is live code on a fresh block, and
wrapped in `ASSERT(...)` it is not a lost check but a lost WRITE -- the two builds leave
different bytes behind. Promoting the call out of the assertion fixes that by running the
repair in Release, an image write and a decision about the untrusted gate rather than a
tidy-up in passing. What settles it is splitting a pure `P2PmsgHeap_IsValidAlloc` out of
the repairing one, in `MsgVBHeap.cpp`.

The two `ASSERT(!bFirstFit)` are `MsgVBHeap.cpp` as well, and the clearest of the eight.
The condition already throws, four lines below each:
`EVERR->MODULE->AFP(aVBLockFree)->Message(L"...the chosen free block did not...")`. The
assertion adds a Debug dialog immediately before a correct, named refusal -- §35's shape
exactly, asserting on the way to refusing, and reached only on the second pass, which is
the pass the comment above it calls unreachable by any heap this allocator built. **Both
are deleted rather than converted**, because the throw is already there and converting
would have written the same refusal twice. The comment block at each site now says why
there is no assertion on the retry flag, so that restoring one is a decision rather than an
oversight (`MsgVBHeap.cpp:4430-4440`, `:4605-4615`).

### Six of the `predicate` count are not assertions at all

One thing the re-measure turned up has nothing to do with staleness. PowerShell's `-match`
is case-insensitive unless spelled `-cmatch`, so `\b(?:ASSERT|P2PASSERT)\s*\(` also
matches `->Assert()`:

```
MsgVBHeap.cpp:2525  ->Message("Attempt to collate non-free entry")->Assert()->Throw();
MsgVBHeap.cpp:2545  ->Message("Internal address corruption")->Assert()->Cancel();
MsgVBHeap.cpp:2625  ->Message("Attempt to collate non-free entry")->Assert()->Throw();
MsgVBHeap.cpp:2644  ->Message("Internal address corruption")->Assert()->Cancel();
MsgVBHeap.cpp:3916  ->Message("Attempt to free non-allocated entry")->Assert()->Throw();
MsgVBHeap.cpp:3973  ->Message("Attempt to free non-allocated entry")->Assert()->Throw();
```

The text after the paren is `)->Throw();`, neither `0)` nor an identifier, so all six land
in `predicate`. They are the opposite of what the script is against: every one is on a
branch ending in `->Throw()` or `->Cancel()`, which is item 19's own prescription. And
`P2Pevent::Assert` is `{ ASSERT(0); return this; }` (`Msgexception.h:353`), already counted
once as the `marker` it is -- so the six are a second count of one site. Not fixed here,
for §34's reason and the script header's, which now says so: `-cmatch` would drop the
number by six in the same breath as the re-bank, and two corrections landing in one figure
is how a baseline stops meaning one thing.

### What the figures should be

Three changes, built clean on `Debug|x64`, suite passing in both link modes -- 251 cases
and 1377 checks static, 240 and 1308 dll -- and measured live at 03:52 on 2026-09-12 in a
tree also carrying other work in `MsgVBHeap.cpp`, `P2PmsgMgr.h` and `tests/`:

```
  callwrap   247        marker   208        predicate  190
```

`callwrap` 248 to 247 by the `MsgVect.cpp` conversion; `predicate` 195 down through its
banked 192 to **190**, because deleting the two `ASSERT(!bFirstFit)` carried it two BELOW
the figure banked at `4d39d0d`; `marker` untouched at 208. `predicate 190` and not 184,
because the six `->Assert()` false positives are a separate correction with its own reason
and its own commit.

**Two of the three numbers are banked downward and the third is banked UP, which wants
saying out loud.** `callwrap` sits two above `4d39d0d`, and those two are named:
`P2Pmsg.cpp:2911` and `:2936`, the `AssertValidAlloc` pair. A rise banked without an
argument is the thing this file exists to prevent, which is why What-is-left refused to
re-bank at all while the argument was missing. It is not missing now -- the pair is the one
case of the eight where promoting the call OUT of the assertion is the change that needs
justifying, not leaving it in -- so the ceiling moves, and the two sites it moved for are
written down here and in the commit that moved it.

The gate goes green with that bank, for the first time since `d2763ce` made it red on
2026-09-11:

```
  callwrap at baseline (247).
  marker at baseline (208).
  predicate at baseline (190).

OK -- no ASSERT-family form has grown.
```

Two of the eight sites remain open, both in `MsgVBHeap.cpp`, and both are the same one
question: `P2Pmsg.cpp:2911` and `:2936` want a pure `P2PmsgHeap_IsValidAlloc` split out of
the repairing one before anything can be promoted out of an `ASSERT`.

**Both of the numbers this section argued for were superseded within one session, and by
the two things it named.** §39 split the pure validator out and promoted the pair, which took
`callwrap` back to 245 -- the `4d39d0d` figure exactly, so the rise banked above is repaid
rather than carried, and the argument for banking it upward turned out to be an argument
for a ceiling that lasted one commit. §40 then found that the `predicate` figure was seven
high and not six, and that `marker` was one LOW, for a reason this section could not have
guessed because it had reasoned about the classifier rather than run it. The bank is now
245 / 209 / 183. What survives whole from here is the shape of the argument and the account
of the eight-wide gap; the arithmetic in front of it moved, which is the failure this
section is about, arriving one section later in its own text.

## 39. Two validators whose names said they tested

**§39 left two entries about the same misnaming, and they are opposite halves of
it.** `P2PmsgHeap_AssertValidBSTRio` was called on something that is not the
thing it validates, so it tested NOTHING; `P2PmsgHeap_AssertValidAlloc` was
called on exactly the thing it validates and did rather MORE than test it. A
refusal that could not fire, and a repair that fires on every ordinary block.
Both close here, and the result worth leading with is that §39's prediction about
the first is wrong: fixing it moves no refusal message on any image in the
corpus, because the fix is a deletion.

### The early return was not usually taken, it was always taken

`P2PmsgHANDLE` is `typedef void *` (`Msgcore.h:35`), so
`P2PmsgHeap_AssertValidBSTRio(pBSTRio)` at the head of
`P2PmsgHeap_CreateBSTRio(VBListBSTRio*)` compiled without a word while handing an
image buffer to a function whose parameter is a heap handle. The validator
`static_cast`s to `VBListHANDLE *` and returns `true` when the byte at the
`uVBListType` offset is not `P2PmsgHeap_BSTRio`. §35 read the resulting silence
as "usually a no-op", the IOMAGE twin's own removal note being the source of the
phrasing -- "it usually early-returned ... but ~1-2% of the time that byte was
0x01". It is not usually. It is always, and the reason is two lines above.
`uVBListType` sits at offset 4 of a `VBListHANDLE`, after the four-byte atomic
`nRefCount` (`MsgVBHeap.cpp:806-807`); offset 4 of a `VBListBSTRio` is the low
byte of `oDefs.uComp2`, which `P2PmsgHeap_IsBSTRio` (`MsgVBHeap.cpp:5124`) has
just insisted is the exact complement of `uDefs1`, whose low byte is the type
code. The byte read as `uVBListType` is therefore `~P2PmsgHeap_BSTRio`, and no
eight-bit value is its own complement: the early return was certain for every
image that could reach the line and the `"Corrupted BSTRio heap"` refusal under
it unreachable in every build ever shipped. §39 said the throw "has never fired
on anything"; the stronger statement is that it could not. The IOMAGE arm has no
complement relation at that offset, which is why its note can report a 1-2% hit
rate and a real assertion on Linux.

### Measured both ways, on nineteen images

§40's probe with two phases added. Phase 0 hands the raw buffer straight to the
validator, which is what the call site did; phase 2b hands it the HANDLE it is
contracted to take, which is what the entry was really asking about.

```
=== valid_store.dat  (2032 bytes)
  PHASE0 raw-image       AssertValidBSTRio(image)=true  asserts=0
  PHASE0 image bytes     unchanged
=== f11_collate_nogrow.dat  (3487 bytes)
  PHASE0 raw-image       AssertValidBSTRio(image)=true  asserts=0
  PHASE1 untrusted       accepted=0  why='BSTRio block structure is corrupt'  asserts=0
  PHASE2b on the HANDLE   AssertValidBSTRio(handle)=true  asserts=4  image CHANGED
```

Fifteen of the nineteen classify as BSTRio -- the corpus has grown since §32's
twelve -- and phase 0 is the same on all fifteen: `true`, no assertion, not one
byte moved, the four corrupt ones included.

Phase 2b is the half that decides the shape of the fix. `true` as well, on all
thirteen images it can be run against, corrupt and honest alike -- and on
`f11_collate_nogrow.dat` it rewrote the image's free-list keys on the way to
saying so. Outside a gate its `VBHEAP_DIAG`s assert and then REPAIR rather than
setting `bResult` false -- the mechanism §35 traced at its `:2087`, now `:2138`
as the notes around it have grown. The correctly-typed
call is not a refusal this path was missing, it is a WRITE this path does not
want, and fitting the right argument would have imported the second entry's
defect into the first one's site.

The twin says what to do instead and says it in the code: `InitIOMAGE` did not
retype its call, it DELETED it, because "the REAL handle is validated by the
caller". Both BSTRio copies go the same way (`MsgVBHeap.cpp:3639`, `:5058`). The
second is the one §39 pointed at: the entry cites `:4983` as the IOMAGE twin and
at `1e5c442` that line is the BSTRio call inside `P2PmsgHeap_InitBSTRio`, sampled
one time in a hundred and doing nothing on each. The citation had drifted by one
function, as §35 found for `:3330` and `:3438`.

### Nineteen images, before and after, and nothing moved

One pristine `git worktree` of `1e5c442` against one carrying only this change,
each built with `/p:WDMSCS_LIB=<that worktree>\lib` for the reason §40 gives. The
entry warned that fixing this "introduces a refusal where there is none today,
ahead of the walk, and changes which message names a bad image". It does not:

```
$ diff before.txt after.txt          # assertion LINE NUMBERS normalised
$ echo $?
0
```

Eighteen raw lines differ and all eighteen are `MsgVBHeap.cpp(2049)` becoming
`MsgVBHeap.cpp(2100)` and its neighbours, the notes now standing where the calls
were being longer than the calls. Eight accepted, eleven refused, four of them by
`BSTRio block structure is corrupt` from the walk at `:3610` -- which §35 already
called the only structural gate on that path and which is now the only thing on
it that reads like one.

### A repair with a predicate's name, and the split that fixes it

`P2PmsgHeap_AssertValidAlloc` sets `VBLock_Linked` on the block it is judging, on
both arms (`MsgVBHeap.cpp:1411`, `:1967`), and those two writes are the only
writes in either body -- everything else is `VBLock_Is*` reads and `Addr2Phys`
translations. §39's case for why that matters rests on `VBLock_Init` setting
`Alloc` and not `Linked` (`P2PmsgVBLock.cpp:327`). It is worse than live code on
a fresh block:

```
a fresh block from P2PmsgHeap_Alloc
  uVBLockDefs = 0x47   Linked=0  Alloc=1
```

Every ordinary allocation arrives without the bit, so the repair arm is what an
unadorned `P2PmsgHeap_Alloc` result meets on its way through, and under
`ASSERT(...)` that write is in one of the two builds and not the other.

`P2PmsgHeap_IsValidAlloc` (`MsgVBHeap.h:298`, `MsgVBHeap.cpp:2989`) is the same
question with no write and no assertion on any path. It is not a second copy of
the checks: each arm is one body taking a `bRepair` flag (`MsgVBHeap.cpp:1403`,
`:1953`) with two thin entry points over it, so the pure form cannot drift from
the repairing one and answer differently. Every write and every assertion sits
inside `if (bRepair)`, which is what makes "no write remains on this path" a
property of the text rather than a claim about it.

```
with VBLock_Linked cleared: uVBLockDefs = 0x47
  P2PmsgHeap_IsValidAlloc     -> false  uVBLockDefs = 0x47  UNCHANGED  asserts=0
  P2PmsgHeap_AssertValidAlloc -> false  uVBLockDefs = 0xc7  WROTE  asserts=1
  P2PmsgHeap_IsValidAlloc     -> true   (the repair is why)  asserts=0
```

Same answer, one byte apart, and the third line is the repairing form's whole
character: the block it just called invalid now passes, because judging it
changed it. With that, `P2Pmsg.cpp:2934` and `:2981` throw instead of asserting,
in the `EVERR->MODULE->AFP(...)->Message(...)->Throw()` idiom the size guards
four lines above each already use. Both are ordinary rehome and duplicate paths
the suite walks constantly, so the throw being live is itself the measurement
that blocks arriving there ARE Linked -- one that is not would have named itself
across the whole suite.

**Only one half of the Debug/Release claim is measurable in one build**, and the
half above is it: that `ASSERT(x)` is nothing under `NDEBUG` is a property of the
macro rather than an experiment, and is not offered as one. What closes the pair
is that neither build now calls a writing validator on those two paths at all --
gone by construction rather than by comparison, and worth saying which.

The repairing form is kept for the five callers that want it
(`MsgVBHeap.cpp:2112`, `:2213`, `:3232`, `:4037`, `:4094` -- the heap's own walks
over heaps this process owns). **Three sites of the same shape are deliberately
NOT converted**: `P2Pmsg.cpp:3170`, `:3176` and `:3186` lose the same write in
Release, but the classifier reads
`ASSERT(m_aVBLock==0||m_hVBList==0||P2PmsgHeap_AssertValidAlloc(...))` as
`predicate` rather than `callwrap`, so converting them would move a second figure
for a reason unrelated to this one.

### The ceiling, repaid exactly

The two promoted sites are the two `callwrap` §38 banked a ceiling UPWARD for,
and they were the only ones:

```
=========== BEFORE (pristine 1e5c442) ===========    =========== AFTER ===========
  callwrap   247                                       callwrap   245
  marker     208                                       marker     208
  predicate  190                                       predicate  190
```

245 is the figure banked at `4d39d0d`: the rise §38 could not avoid is repaid
rather than carried. `marker` and `predicate` do not move -- the `bRepair` guards
wrap the existing assertions rather than adding any, and neither deleted BSTRio
call was ever inside an `ASSERT`. The new dispatcher answers `false` on an
unrecognised heap type where the repairing one has `ASSERT(0)`: its caller is
about to say so by name.

### Pinned

`Test_IsValidAllocIsPure` (`tests/MsgcoreSuite.cpp:5848`) is the alloc probe as
four cases: a fresh block is Alloc and not Linked, the pure form answers without
writing or asserting, the repairing form still repairs, and the pure form is
quiet on a block that is fine. Its third case pushes a hook in front of
`TestFramework`'s that SWALLOWS rather than counts -- the opposite of
`Test_UntrustedBSTRioGate` beside it, because its subject asserts BY DESIGN and
letting that through would fail a case for doing the thing it exists to show. It
checks the count is one instead.

`Test_BSTRioValidatorContract` (`:5949`) pins why the deletion removed nothing:
byte 4 of a BSTRio image is the complement of its type byte and so never equal to
it; the validator handed the image answers `true`, silently, without moving a
byte, honest image and bumped `nAllocEntries` alike; and the single-argument
overload still accepts an image this process built while still refusing a buffer
that is no BSTRio image at all, by `Not BSTRio heap type`. Static link only, for
`Test_ImageAddressBounds`'s reason. Both modes clean -- `static` 263 cases / 1423
checks, `dll` 247 / 1330, both PASS, measured in a tree also carrying other work
in `Msgcore_c.cpp` and `tests/`.

## 40. Seven, not six: the case-insensitive letter of the classifier

**§38 deferred one correction to its own commit and stated it slightly wrong, which is the
best argument for having separated it.** `check_asserts.ps1` found its sites with `-match`,
case-insensitive in PowerShell unless spelled `-cmatch`, so `\b(?:ASSERT|P2PASSERT)\s*\(`
also matched `->Assert()`. §38 named six such sites and read the banked `predicate` figure
as six high. It is seven high. The seventh is `P2Pevent::Assert` itself -- the site §38
cited as the one being double-counted -- and it was not counted as the `marker` §38 said it
was.

### The six are what they were said to be

Every one is `P2Pevent`'s fluent builder on a branch that already refuses, item 19's
prescription rather than a violation of it, and they are the only lines in the counted
sources matching the pattern case-insensitively but not case-sensitively:

```
$ git grep -nE '\b([Aa][Ss][Ss][Ee][Rr][Tt])\s*\(' -- '*.cpp' '*.h' \
    | grep -v '^Platform/' | grep -v '^tests/' | grep -vE '\b(ASSERT|P2PASSERT)\s*\('
  MsgVBHeap.cpp:2525:  ->Message("Attempt to collate non-free entry")->Assert()->Throw();
  MsgVBHeap.cpp:2545:  ->Message("Internal address corruption")->Assert()->Cancel();
  MsgVBHeap.cpp:2625:  ->Message("Attempt to collate non-free entry")->Assert()->Throw();
  MsgVBHeap.cpp:2644:  ->Message("Internal address corruption")->Assert()->Cancel();
  MsgVBHeap.cpp:3942:  ->Message("Attempt to free non-allocated entry")->Assert()->Throw();
  MsgVBHeap.cpp:3999:  ->Message("Attempt to free non-allocated entry")->Assert()->Throw();
```

The last two are §38's `3916` and `3973` moved 26 lines; these are the numbers at
`1e5c442`. The two `->Cancel()` arms are the budgeted emitters -- `if (
++pHandle->nCollateCorrupt == 1 )` -- whose own comment argues the `->Assert()` belongs
inside the budget. Nothing here is a lost check.

### The seventh, which is the definition line

```
$ sed -n '352,353p' Msgexception.h
      virtual P2Pevent*
        Assert ( ) { ASSERT(0); return this; }
```

One real `ASSERT(0)`, and it is the body all six call sites reach. But the classifier does
not stop at the first `ASSERT` in the line, it stops at the first match of a
case-insensitive pattern, and on this line that is the method's own name. What the two
spellings capture as `$inner` is the whole finding:

```
line     : Assert ( ) { ASSERT(0); return this; }
-match  inner: [ ) { ASSERT(0); return this; }]
-cmatch inner: [0); return this; }]
```

Under `-match`, `$inner` opens with `)` -- neither `^\s*0\s*\)` nor an identifier and a
paren -- so it fell through both arms into `predicate`. The site that defines the entire
`ASSERT(0)`-as-marker idiom was itself banked as a predicate. §38 wrote that it was
"already counted once as the `marker` it is", and the script header repeated it; neither
had run the classifier over that one line. The claim was inference, and the shape of this
bug is that it eats its own witness. `Msgexception.h:353` is the only declaration --
`git grep -n '::Assert\s*(\s*)'` over non-test sources returns nothing, no override
anywhere -- so the six do resolve to this one body.

### What moved, in all three categories

Measured at 04:10 on 2026-09-12, `git status --short` showing only `M
tools/ci/check_asserts.ps1`, so `callwrap` is the figure before §38's two open
`AssertValidAlloc` promotions land:

```
                 -match     -cmatch
  callwrap          247         247
  marker            208         209
  predicate         190         183
  TOTAL             645         639
```

`callwrap` not moving had to be measured, not assumed: the six could as easily have landed
there. Both classifier regexes are case-neutral by construction -- `^\s*0\s*\)` has no
letters, `^\s*[A-Za-z_][A-Za-z0-9_:]*\s*\(` spells both -- and running the whole classifier
case-sensitively returns the same 247 / 209 / 183. The defect was confined to the two lines
that find the macro.

The net on TOTAL is the minus six §38 predicted; the split is not. `predicate` falls seven
and `marker` rises one, and the rise is what needs saying out loud, because §38's lesson is
that a moving ceiling must be readable as a regression or not. This one is not: no
assertion was written, the site is as old as `Msgexception.h`, and the ceiling moves only
because the site is finally filed under the form it always had. The gate is red for exactly
that and nothing else:

```
$ pwsh tools/ci/check_asserts.ps1
  callwrap at baseline (247).
  marker ASSERT sites rose from 208 to 209
  predicate fell from 190 to 183 -- bank it: -Regenerate in this commit.
```

### What stays case-insensitive, and why that is a different question

Auditing the one `-match` §38 named turned up four more case-insensitive comparisons, and
one of them is load-bearing. `-Detail` carries
`[ValidateSet('marker','predicate','callwrap')]`, and ValidateSet is itself
case-insensitive and passes the caller's spelling through unchanged: `-Detail CALLWRAP`
validates, `$Detail` holds `CALLWRAP`, and the `$_.Kind -eq $Detail` filtering the listing
is the only thing making the two meet. Spelling it `-ceq` for consistency would have
produced a switch that validates its argument and then prints nothing -- a silent empty
listing, worse than the miscount being fixed here.

The other three are latent and left alone. `-notlike 'Platform/*'` and `-notlike 'tests/*'`
exclude 23 of 63 tracked sources and exclude the same 23 under `-clike`, those being the
only two directories in the tree and both spelled as written; the baseline parses into a
plain `@{}` and the counts into `[ordered]@{}`, both comparing keys case-insensitively, so
a baseline saying `CALLWRAP 247` would still be read. Narrowing a file filter is how
sources quietly stop being counted, which is a failure this script has already had once.

## 41. An export whose FALSE means nothing, and the header that stops it being read

§37 closed the `IsSole` entry as "not carried, and deliberately not closed", with the argument
written out both ways and the note that the decision belonged to whoever owns the release. It has
been taken: `msgcore_field_is_sole` is on the supported surface at `Msgcore_c.h:296-345`, the flat
ABI is 283 functions rather than 282, and the version is 3.1.0.

**The code is four lines and the contract is fifty, and that ratio is the section.** The signature
is character-for-character the nine `msgcore_field_is_*` predicates already there, over a
`MsgFieldHandle` that already exists; what is published is a promise load-bearing in one direction
and empty in the other, on a surface the README calls supported.

### Why publish an answer that is half advisory

**Because the load-bearing half is the one fact a flat caller cannot reconstruct from anything
else on the surface.** `msgcore_field_get_p2pos` answers identity and not exclusivity -- §21's
point, that `==` needs the other object to compare against, carried onto a surface where the
other object is a second handle the caller may not have. There is no flat route to
`P2PmsgHeap_RefCount` either, and §31 recorded why there will not be one: its `refs=` column is
static-mode only, because exporting the refcount to prettify a listing would widen a surface
frozen through 1.x. So a host could neither compute the answer nor ask for it, and the
alternative is copying defensively on every write.

**What made it awkward was never that half.** The answer compares a count of holders of the HEAP
against the holders the field can prove are its own, so it reads FALSE whenever the proof is
merely short -- and §26 stated the asymmetry as a design rule: undercounting is safe and
overcounting is not, a holder missed leaves FALSE and false promises nothing, a holder
subtracted that was never mine reports TRUE with a stranger looking. §29 and §31 narrowed FALSE
without touching that rule; §36 measured the last shortfall and declined to remove it, because a
manager's reference on its own heap is not a view of any item. The residue is small, named and
permanent -- a cursor a collection keeps for itself (§26's last unlifted row) and the store's own
handle (§36) -- and dangerous to publish flat, because a caller reading `is_sole == 0` concludes
"someone else is looking", and on both residual shapes nobody is.

### The header, which is the deliverable

```
//   TRUE IS A GUARANTEE. Nothing else in this process holds the storage under
//   hField ... A write made through hField cannot be seen by anyone else ...
//
//   FALSE IS NOT THE OPPOSITE OF IT, AND DOES NOT MEAN "SOMEONE ELSE IS
//   LOOKING". ... it reads FALSE whenever that proof is merely SHORT -- and two
//   entirely unshared shapes make it short. A cursor taken on a field's own
//   descendants holds the heap and cannot be attributed back to the field that
//   owns it. A store manager holds one reference on its own heap that is not a
//   view of any item at all ... In both, the item is nobody's but the caller's
//   and the answer is still 0.
//
//   So FALSE is "unknown", and its only sound use is to decline the in-place
//   write. It is NOT evidence of a second holder. Do not use it to detect
//   sharing, to infer a reference count, or to decide that a copy is owed to
//   somebody else -- there may be no somebody else.
```

Three things there do work beyond restating §26. The residual shapes are NAMED, so a caller who
hits one has somewhere to look instead of a bug to file. The prohibition is a list of things not
to do with FALSE, because "not a guarantee" is a phrase a reader agrees with and then ignores.
And the block says the asymmetry is deliberate and that a future version may narrow FALSE but will
never promote it, which makes shrinking the residue later a non-breaking change. The rest points
at `msgcore_field_get_p2pos` for whose-item-is-it, as `P3PmsgObject::IsSole`'s own NOTES do
(`P2Pmsg.cpp:3412-3417`).

### The conventions it matched rather than invented

**Every mechanical decision was already made somewhere on the surface, and the only one worth
arguing was which `IsSole` to call.** The guard is `if (!toField(hField)) return 0;`, the
one-line form the registry comment at `Msgcore_c.cpp:37-76` exists to keep uniform; the body is
`toField(hField)->IsSole() ? 1 : 0`, the shape of all nine siblings; there is no `_u8` twin
because it takes no string; the declaration sits at the end of the `// Properties` group. The
bad-handle answer is 0, which is not a coin toss: the `is_list` family returns 0 for a refused
handle and `is_null` / `is_void` return 1, both answering the pessimistic half -- and here 0 IS
it, so a bad handle can never become a licence to write in place.

And it forwards to `P3PmsgField::IsSole` (`P2Pmsg.cpp:4375`), not `P3PmsgObject::IsSole`
(`:3397`), whose version cannot tell a field's own sub-objects from a stranger's view and
answers FALSE from the moment a field is asked for its descendants -- §26's headline row.
Forwarding there would have narrowed the TRUE half, the whole value of the export, silently.

### Measured

Five cases in `tests/MsgcoreCApiSuite.cpp`, which speaks only in handles -- no `refs=` or
`mine=` column, because a flat caller has neither:

```
  - TRUE is a guarantee: a detached copy nobody else can reach
      a detached copy of a leaf                          is_sole=1
  - the FIELD's answer, not the object's: a floater with descendants
      a detached store copy, untouched                   is_sole=1
      ... once it has been given a descendant            is_sole=1
  - FALSE where it is earned: a second live view of one item
      one of two live handles on the same item           is_sole=0
  - FALSE IS NOT A GUARANTEE: the only handle on a store reads FALSE
      the sole flat handle on an untouched store root    is_sole=0
  - an invalid handle answers 0, which is the answer that promises nothing
```

The first case spends the guarantee rather than reading it -- the detached copy is written in place
and the live tree checked to be unmoved -- and the third is FALSE being right and proving it, a
write through one of two handles read back through the other. **The fourth is the row the whole
section is about, and it asserts a FALSE.** `hRoot` is the only field handle the API has issued
against that manager; no second flat view of the item exists and the caller cannot make one
without asking. The answer is 0 anyway, for §36's reason, so that a later reader who teaches
FALSE to mean "a second view exists" breaks a test rather than a consumer.

### Teeth

Point the body at `toField(hField)->r_Object().IsSole()` instead, rebuild, and run static:

```
  - the FIELD's answer, not the object's: a floater with descendants
      a detached store copy, untouched                   is_sole=1
      ... once it has been given a descendant            is_sole=0
      FAIL [the FIELD's answer, not the object's: a floater with descendants]  msgcore_field_is_sole(hCopy) == 1
  cases   : 263  (1 with failures)
  checks  : 1423  (1 failed)
```

One check, and exactly the intended one. The narrowing is otherwise invisible -- same signature,
same manifest, same header -- and the other four cases stay green, a leaf copy being inline.

### What moves on the exported surface

The flat ABI goes 282 to 283 and `check_exports.ps1` goes red on both platforms, as intended
until the manifests are regenerated:

```
out\x64\Debug\Msgcore.dll (x64): 1052 exports -- 283 flat msgcore_*, 769 mangled C++
  flat C ABI drifted from tools/ci/exports-flat.manifest:
  EXPORT ADDED, undeclared: msgcore_field_is_sole  --  msgcore_field_is_sole
  This is the supported surface. If the change is intended, regenerate the manifest and bump Msgcore_version.h in the same commit.
  OK -- mangled C++ (x64) matches tools/ci/exports-cxx-x64.manifest (769 symbols).
```

Win32 prints the same with `_msgcore_field_is_sole` decorated, and both mangled C++ manifests are
byte-identical at 769 -- nothing was added to the class half, which is what "one export" means.

`Msgcore_version.h` goes to 3.1.0 in the four places aa0366b established -- `MINOR`, the comma
form, the string form and the packed hex -- and the built DLL reports FileVersion and
ProductVersion `3.1.0.0`. MINOR because the supported surface GREW and nothing on it moved: a
consumer built against 3.0.0 links against 3.1.0 unchanged, and one built against 3.1.0 that
calls the new export cannot link against 3.0.0, which is PATCH's lie exactly. The bump is
load-bearing rather than ceremonial, because `MSGCORE_VERSION_AT_LEAST(3,1,0)` is the only way a
portable consumer can guard the call. The number was free: aa0366b claimed 3.1.0 for a release
whose surface had not moved, and 355cff5 took it and the tag back.

**And one allowlist line retires.** `P3PmsgField::IsSole` was carried in
`tools/ci/api-drift.allow` with §37's argument written out both ways; it is genuinely bound now,
so the check reports the line redundant and it goes. `P3PmsgField::IsInline` keeps its line for the
reason that paragraph gives: its FALSE is explicitly not a guarantee (§24) and it describes where
storage sits rather than what a caller may do. Writing the case FOR down in full is what made this
one a decision somebody could take, rather than a thing the matcher had scored green since it was
seeded.

## 42. A hundred and fifty-nine, under fifteen arguments -- and the three that were bound all along

§39 has carried the same entry since the check was seeded: *159 allowlist entries are still
UNTRIAGED, and now they are the real 159*. It is the largest thing outstanding and it is a
backlog, so the honest measure of a session against it is how many lines stopped saying
"nobody has looked" and started saying something a reader can disagree with. **157 did. Two
more left the file for a better reason than a decision -- the matcher learned to read them,
and they had been bound since the day they were banked.** UNTRIAGED is 0.

### The snake-caser ended a word at a digit

§37 named the cost of leaving `ConvertTo-Snake` alone: `GetP2Pos` "misses because
`ConvertTo-Snake` splits `GetP2Pos` into `get_p2_pos` where the surface spells
`get_p2pos`". The allowlist line it wrote went further and called the miss irreducible --
"no sequence of general rules turns one into the other without inventing a dictionary".
That was wrong, and cheaply so.

The old first rule was `(?<=[a-z0-9])(?=[A-Z])`, which ended a word at a digit as readily
as at a letter. **A digit belongs to the word it sits in.** Every name in this library that
carries one says so: `P2Pos`, `P2Pmsgnn`, `P3PmsgBSTR`, `VBLockBSTR` are each one word with
a generation number inside it, not two words with a number between them. Drop the `0-9`
from the lookbehind and the rule becomes what it always meant -- a word boundary is a CASE
change -- so `GetP2Pos` is `get_p2pos` and `P2Pos2Field` is `p2pos2field`, which is how
`Msgcore_c.h` spells both. A digit at the END of a word (`Int64`, and every
`msgcore_*_int64` on the surface) was never touched by either rule and is not touched now.
The fix is one character class (`tools/ci/check_api_drift.ps1:289`), measured before it was
applied, in both directions, over all 288 scanned members:

```
Members whose spelling changes: 44

GAINED an exact binding:
    P2PmsgMgr::P2Pos2Field  ->  msgcore_mgr_p2pos2field
    P2PmsgMgr::P2Pos2Path  ->  msgcore_mgr_p2pos2path
    P3PmsgField::GetP2Pos  ->  msgcore_field_get_p2pos
LOST an exact binding:
    (none)
```

Forty-four names change and three land on a function; the other forty-one go from a spelling
the surface does not have to another it does not have either, which is why the change is
safe rather than merely lucky. The next run agreed, in the voice the check keeps for an
exemption that has outlived its gap: "3 allowlist entries are no longer needed -- the member
has a binding now".

**Two of those three had sat in the UNTRIAGED block since the seed run, counted for eleven
sections as work somebody still owed.** §34 and §37 were about the fallback's false
POSITIVES -- 28 members scored green that were not bound -- and fixing those made the check
redder and more honest at once. The exact test fails the other way and it is quieter: a
false NEGATIVE does not flatter the bound count, it inflates the BACKLOG, and a backlog
that reads too long looks like diligence. Nobody audits a number that makes them look worse
than they are.

§39 also asked whether `PageRegistration`'s overloads could be made visible by a rule. They
cannot. They are `msgcore_mgr_set_paging_sinks` and `msgcore_mgr_set_populate_sink`
(`Msgcore_c.h:1027`), a rename the C header spends fifteen lines justifying, sharing no
segment with the C++ name after `mgr`; a rule reaching from `page_registration` to
`set_paging_sinks` would have to stem `paging` to `page` and then accept a candidate with
`registration` nowhere in it, which matches most of the surface most of the time. A second
rule was plausible enough to test, and testing it is the point: the library spells a
reference exposure `r_x()` where the flat surface spells it `get_x()`, and mapping one to
the other binds nine members correctly. **It also binds three wrongly, in the shape §37
deleted twenty-eight of.** `msgcore_curs_get_name` is not `P3PmsgCurs::r_name`'s -- the
wrapper implements it as `toCurs(hCurs)->c_wstr()` (`Msgcore_c.cpp:1152`) --
`msgcore_recurs_get_name` is `c_wstr`'s (`:2252`), and `msgcore_field_get_name` is
`c_name`'s (`:510`). Taking a function from the member that implements it, to save nine
lines in a file whose product is lines, is the wrong trade.

### The reason that expired

§37 left two `SetPermissions` lines UNTRIAGED under an argument they plainly belonged to,
saying so in the file: "moving lines nobody asked about would move the UNTRIAGED count out
from under the sections that quote it". That was procedurally right and it expired the
moment §38 was written. Both now sit under `GetAccess`'s argument, and pushing them there
found the half nobody had written down. `GetPermissions` reads a byte whose bits no header
in this repository names. **The setters are worse, and not by symmetry.** Both push an ADD
mask and a REMOVE mask straight into the stored byte (`MsgDesc.cpp:763-770`,
`MsgAttr.cpp:711-718`), so a caller who cannot name a bit cannot name it in either argument,
and the failure is silent: a wrong bit does not fail the call, it changes what the stored
object permits. And the setter ALLOCATES, calling `Create()` when the collection does not
exist yet -- a heap mutation, which under rule 2 of `Msgcore_c.h` invalidates every live
handle the caller holds. An `msgcore_desc_set_permissions` would be a settings call with the
blast radius of a declare, for a vocabulary the surface cannot publish.

### One hundred and fifty-seven members, fifteen arguments

The arguments are at `tools/ci/api-drift.allow:431` onward and they are the deliverable.
What belongs here is the shape of the ones that did the most work, because §34 covered four
`Drop`s with one argument and §37 covered 32 of 35 with four: a backlog needing one argument
per line is a backlog nobody has read.

**A constructor is a name C cannot spell, and a destructor is one call per family.** Twenty
of the 159 at a stroke. The surface spells construction as a named factory per shape --
`msgcore_mgr_create` / `_create_nn` / `_open_file`, `msgcore_field_create` / `_clone` /
`_child`, a `_from_field` for every collection -- and destruction as one `_destroy`, which
literally runs the C++ destructor (`msgcore_field_destroy` is `delete p`,
`Msgcore_c.cpp:369-376`). What it declines is not construction but the argument lists: the
check asks about each NAME once, so `P3PmsgField`'s seven constructors arrive as one line,
three carried and four taking `P2PmsgFieldHdl`, `P2PmsgHANDLE` + `VBLaddr` + `VBLsize`,
`P3PmsgData&` or `P3PmsgObject&` -- every one a type this file had already refused.

**`AssertValid` throws, and nothing thrown crosses this boundary.** Twenty-three more, as
`AssertValid`, `Print` and `VerifyContainment`. `P3PmsgField::AssertValid` runs
`VerifyContainment` and then `EVERR -> Message("Failed Containment") -> Throw()`
(`P2Pmsg.cpp:4119-4125`), the one behaviour a flat entry point may not have: `Msgcore_c.cpp`
catches 111 times, turning a throw into a return code or a NULL handle. The surface asks the
question safely instead -- `msgcore_mgr_is_valid` binds `P2PmsgMgr::IsValid` exactly, `int`
instead of a throw -- which is §35 from the other side: there a gate caused the assertion it
was meant to watch for, here an ABI declines to carry one at all.

**A flat handle has no disconnected state to move to -- except on the one family where it
does.** `Nullify` drops the wrapper's parent pointer and deletes its cached cursor
(`MsgAttr.cpp:109-116`); `Connect` is `Nullify` then re-pointing. On the flat side a
collection handle comes into existence connected -- `msgcore_attr_from_field` is the only way
to get one, and `Msgcore_c.h:378` says so in exactly these words, "Connect to the attribute
collection of hField (Create=true allocates if absent)" -- and leaves existence through
`_destroy`. There is no third state for a connect call to reach. **`MsgStck` is the exception
that makes the argument checkable rather than merely plausible**: `msgcore_stck_create` makes
an UNCONNECTED stack, so that family needs both `msgcore_stck_connect` and
`msgcore_stck_nullify`, and `MsgStck::Connect` and `::Nullify` bind exactly and appear
nowhere in the allowlist. That header line settles `Create` too: it is not a function on the
flat side but the `bCreate` ARGUMENT, and the wrapper calls `pAttr->Create()` when it is set
(`Msgcore_c.cpp:830`).

**The surface builds in place and never adopts a caller-built object**, which covers
`operator+=` and `PushBack` (one member with two spellings, `MsgAttr.cpp:196-212`): the only
thing it grafts is a node already in the tree, between two live handles, and rule 2's own
exemption note (`Msgcore_c.h:112-116`) says why -- a handle a C caller holds may be a
DETACHED deep copy whose block is not in the heap at all, so an `msgcore_attr_push_back`
would have to copy that block into the arena, which is the value copy §23 and §27 are about.

**The trigger family was deliberately re-keyed, and the C header says so.**
`Msgcore_c.h:660-663` opens the group with "The legacy trigger facility posts a Windows
message to an HWND when an armed node changes. These wrappers add the windowless path a
FUSE/daemon host needs", so `msgcore_mgr_create_trigger` takes `(mask, p2pos)` where the C++
takes `(mask, HWND, p2pos)`. `SelectTrigger` asks which mask is registered for a node AND A
WINDOW, and the flat vocabulary has no HWND for its second argument to be. `TriggerINSERT`,
`TriggerUPDATE` and `TriggerDELETE` are all three `msgcore_mgr_fire_trigger(hMgr,
MSGCORE_TRIGGER_*, p2pos)`, which exists *precisely* because the C++ form needs a
`P3PmsgItem` in hand that a filesystem writer does not have (`Msgcore_c.h:684-694`).

### What the wrapper source settles that a name comparison cannot

The most useful hour went into `Msgcore_c.cpp` rather than `Msgcore_c.h`, because the
question an allowlist line answers is not "is there a function spelled like this" but "does
the flat surface carry this member", and those differ wherever somebody renamed something on
the way through. Fourteen members turned out to be carried under `get_`, and every line cites
the call: `MsgStck::r_item` at `Msgcore_c.cpp:2142`, `MsgStck::r_name` at `:2120`,
`P2PmsgRecurs::c_wstr` at `:2252`, `P3PmsgVect::r_data` at `:783`, and ten more. Two more are
carried in a way no reading of the header would have suggested: `msgcore_curs_get_field` is
`*pResult = (P3PmsgField&)(*toCurs(hCurs))` (`:1141-1145`), where the cast IS
`P3PmsgCurs::operator P3PmsgField&` and the assignment beside it is `P3PmsgField::operator=`.
Both sat in the backlog as members nobody had looked at, and the flat surface calls both on
every cursor read -- §34's `Drop` precedent exactly, where `msgcore_stck_pop` reaches `Drop`
from the inside. `P2PmsgMgr::Attacheap` is a third, its only callers anywhere being
`P2PmsgMgr`'s four constructors (`P2PmsgMgr.cpp:39, 48, 57, 67`).

`Msgcore_c.h` also names its own C++ counterpart here and there, which is the cheapest
evidence in the repository: `msgcore_recurs_next` is documented as "Advance (operator++)" at
`Msgcore_c.h:942`, settling `P2PmsgRecurs::operator++` in a line. Its neighbour does not
settle so easily. No `msgcore_curs_prev` exists, so `P3PmsgCurs::operator--` is genuinely not
carried, and the flat step back is `msgcore_curs_goto_index(msgcore_curs_item_index(c) - 1)`:
O(n) where the operator is O(1), which `Msgcore_c.h:716-720` says plainly about the list's
indexed access. That cost is recorded in the line rather than argued away.

### Three more members with no body

§37 found `P3PmsgVect::IsName` declared at `MsgVect.h:145` and defined nowhere, and recorded
it as a defect rather than a decision. Asking the same question of the whole backlog -- does
a definition exist in any `.cpp` -- returns three more, all on `P3PmsgBSTR`:

```
members with no definition in any .cpp:  3
    P3PmsgBSTR::IsFragmented
    P3PmsgBSTR::SetDefaultSizeof
    P3PmsgBSTR::VBLockBSTR_vp
```

The repository knew about one and says so next to the hole: "IsFragmented, declared next to
it, still has no definition: unlike dirtiness there is no P2PmsgHeap primitive behind it, so
supplying one would be inventing a policy rather than wiring an existing one"
(`P2PmsgBSTR.cpp:450-456`). The other two are unrecorded. All three triage as `IsName` did: a
binding would not link. **Four declared-and-undefined members is the count now, and every one
was found by asking a question about the C ABI rather than about C++.**

`P3PmsgBSTR` is a case of its own besides: `api-drift.config.psd1` maps it to
`msgcore_bstrio_` and `Msgcore_c.h` spells no function with that prefix at all, which is why
none of its nineteen remaining members is even a REFUSAL in the tally. The class wraps a
`VBListIOmage`, typedef'd `P2Piomage` (`P2PmsgBSTR.h:58-69`), a `#pragma pack` struct whose
sync word carries size, addressing bits and a layout generation in named bit ranges -- the
wire form of a `P2Pmsg`, and what §32 and §36 read with a scanner. Exporting any of it
publishes that layout as supported ABI, the commitment the block-geometry argument already
declines for the arena's own blocks.

### Two the surface should carry, and one that arrived while this was written

An addition to the flat surface is a version bump and the release owner's decision, not a
matcher's, so both are triaged as not-carried-today with the argument written out in the file
rather than argued away. **A store can only be serialised to a named file.**
`msgcore_mgr_save`, `_load` and `_open_file` are the whole of it, so a host putting a message
on a socket goes through a temporary file. A pair of buffer calls taking `(void*, size_t)`
would need no `P2Piomage` in the signature and would publish no layout -- the objection that
refuses the rest of `P3PmsgBSTR` does not reach them. That is the largest real gap this
triage found; the second is `operator--` above.

And the third: **`msgcore_field_is_sole` is on the surface now.** §37 wrote the case FOR
carrying it out in full, deliberately, beside the case against, closing with "Adding
msgcore_field_is_sole remains a live option and the argument for it is above, in full, on
purpose". It landed at `Msgcore_c.h:293-345` while this section was being written, with fifty
lines of contract saying exactly what the case against had demanded -- TRUE is a guarantee,
FALSE is "unknown" and is not evidence of a second holder -- and a bump to 3.1.0 beside it.
The check reported the allowlist line redundant on the next run and the line is gone.
**Writing the case FOR down in full is what turned it into a decision somebody could take**,
rather than a member a matcher had been scoring silently green since the seed run.
`IsInline` keeps its line for the reason §37 gave: its FALSE is explicitly not a guarantee
(§24), and it describes where storage sits rather than what a caller may do.

### Where the count stands

Before, against the tree as §38 left it, and after:

```
Scanned 288 public members over 1 pair(s); 81 bound, 207 allowlisted.
  159 of the allowlist entries are UNTRIAGED -- banked, not decided.
  8 of the 81 bound matched by prefix fallback; 35 more were refused by it.

Scanned 288 public members over 1 pair(s); 85 bound, 203 allowlisted.
  0 UNTRIAGED -- every allowlist entry carries a reason.
  8 of the 85 bound matched by prefix fallback; 33 more were refused by it.

OK -- every upstream member has a binding, or an allowlisted reason not to.
```

157 lines were argued; 2 left because the snake-caser fix made them bound; 2 further entries
went with them -- `GetP2Pos`, whose line said the matcher could never see it, and `IsSole`,
which stopped being a question. Bound moved 81 to 85 and all four are members that were
already carried before today. **Nothing here widened the C ABI and nothing here was
silenced**: the only count that fell without an argument replacing it is the three that were
never gaps.

The zero is printed on every run, including when it is zero
(`tools/ci/check_api_drift.ps1:561`). The count sat at 159 for eleven sections and only ever
moved because it was in front of somebody; a line that vanishes when the backlog empties
cannot show the backlog coming back, and `-Seed` can put a hundred and fifty of them in the
file with one switch.

## 43. The store root is at 48, and that is why a store is never sole

§36 measured that a manager holds its heap one time more than its walk claims, declined to
subtract the difference, and What-is-left reopened the decline as a question. **It is a
question, and the answer is still no -- but not for the reason §36 put first.** What decides
it is §36's second reason, which that section asserted rather than measured: the store root
is the one block in this library an outsider can name without a counted reference to reach
it through. It is not merely "a function of the handle" -- it sits at offset 48 on every
store this tree can build, whatever the addressing width and whatever is in the tree, and a
`P3PmsgField` built on a literal 48 writes through the whole tree. Subtracting `m_hMgr`
makes `IsSole` answer TRUE in exactly the state where that name is waiting, and §26 says
TRUE is the answer that must never be wrong.

### What `IsSole` is for, asked of its callers rather than of its name

**The library never calls it.** Stripped of comment lines, `IsSole` appears twice in the
whole of `*.cpp` and both are its own definitions -- `P2Pmsg.cpp:3397` and `:4375`. Nothing
in `P2PmsgMgr.cpp`, `MsgDesc.cpp`, `MsgList.cpp`, `MsgVect.cpp`, `MsgCurs.cpp` or
`MsgVBHeap.cpp` asks it, and no `P2PmsgHeap_*` path is gated on it. So the predicate's
meaning is not pinned by code that would go wrong; it is pinned by what it PROMISES, and the
promise is read by people this repository cannot see.

**Its callers are 65 assertions and an export.** `tests/MsgcoreSuite.cpp` asks it 65 times
under `TF_CHECK`, 36 asserting TRUE and 29 asserting FALSE. The TRUE rows rely on one thing
and several prove it in the next line: a write through this object is invisible to the other
object the case holds -- `:3144` grows a value copy, writes it, and checks the store's copy
still reads 1. The FALSE rows rely on the opposite being POSSIBLE rather than certain and
say so in their comments -- `:3195` "taking a NAME did", `:3291` "the STORE holds this
heap", `:3403` "a stranger, not one of mine". **A predicate means what its callers rely on,
and every caller relies on TRUE meaning a write reaches nobody.**

**And it is an export on two surfaces, one of them arriving now.**
`?IsSole@P3PmsgField@@UEBA_NXZ` is in `tools/ci/exports-cxx-x64.manifest:523`, beside the
`(handle, address, size)` constructor a consumer builds a view with (`:129`), and §39's
first entry has since been decided in favour -- `msgcore_field_is_sole` joins the flat C
surface with a version bump. Every caller this predicate will ever have is outside this
tree, reading a header, and a promise published in `Msgcore_c.h` that one class quietly
answers under a different rule is worse after the export than before it.

### §36's finding, reproduced rather than taken on trust

`tests\build_run_suite.bat static`, 251 cases and 1377 checks, PASS. `refs == mine + 1` on
every manager row nobody else is holding, `mine + 2` on the two stranger rows:

```
  - a manager's walk reaches every view it owns
      a manager nobody has touched                 refs=2  mine=1  sole=false
      ... with a descendant collection             refs=3  mine=2  sole=false
      ... and that collection walked               refs=4  mine=3  sole=false
  - a manager's attributes, stack and snapshot are walked like a field's
      ... and the snapshot read                    refs=5  mine=4  sole=false
      a manager whose root a stranger names        refs=3  mine=1  sole=false
      a manager whose child a stranger holds       refs=5  mine=3  sole=false
```

**The other half can be measured without touching the library, and was.** §31 made
`HeapHolders` virtual and `P3PmsgField::IsSole` calls it virtually, so a class derived from
`P2PmsgMgr` that overrides `HeapHolders` and adds one for `m_hMgr` IS the four lines. A
scratchpad probe does that and asks the same questions:

```
== B. the same three, with the subtraction made ==
      a manager nobody has touched                 refs=2  mine=2  sole=true
      ... with a descendant collection             refs=3  mine=3  sole=true
      ... and that collection walked               refs=4  mine=4  sole=true
      MsgcoreSuite.cpp:3207  !mgr.IsSole() == FALSE (the row goes red)
== C. the two stranger rows, with the subtraction made ==
      a manager whose root a stranger names        refs=3  mine=2  sole=false
      a manager whose child a stranger holds       refs=5  mine=4  sole=false
```

§36's "teeth" listing, independently produced. Neither side disputes the arithmetic.

### The case for subtracting, at its strongest

**The count is exact, not approximate.** `m_hMgr` is minted at `nRefCount = 1` in all four
constructors (`P2PmsgMgr.cpp:38`, `:47`, `:56`, `:66`), closed by `~P2PmsgMgr` (`:74-76`),
taken from nowhere outside, and AddRef'd on the one path that adopts another manager's
handle (`:416-419`). §26's two tests -- does it hold this heap, is it owned outright -- both
answer yes, and §29 subtracted `m_pP3PmsgAttr` and `m_pP3PmsgDesc` on no stronger a showing.
**And the predicate is a constant on this class** -- `bare, unnamed sole=false`, `named
sole=false`, `built wide sole=false`, `a copy sole=false`, and it cannot be otherwise, since
`refs = mine + 1` is invariant while the manager's object is on its own handle, which every
constructor asserts (`P2PmsgMgr.cpp:40`, `:49`, `:59`, `:68`). A public inherited predicate
that can only ever answer one value looks like a defect, and the case FOR is that it is one:
a freshly built standalone store, handed to nobody, is the plainest example in the library
of storage a write cannot escape, and the one object the predicate refuses to say so about.

### The case against, and the measurement that settles it

§36's first reason -- that `HeapHolders` counts VIEWS and `m_hMgr` is not one -- is true and
a real cost, but on its own it is a definition being defended, and a definition can be
changed by whoever owns it. The second cannot be argued away, and it is stronger than §36
knew:

```
== D. is the root-from-the-handle property special to a store? ==
      a store's own root      GetP2Pos=48  Connect(h)=48  IsRoot=1  sole=false
MsgVBHeap.cpp(5109) : Assertion failed!
MsgVBHeap.cpp(5087) : Assertion failed!
      a sole floater's block  GetP2Pos=1109726882032  Connect(h)=4294967295  IsRoot=0  sole=true

== E2. is the store root's address even handle-dependent? ==
      bare Addr32 store  root=48      Addr64 store             root=48
      Addr32 with a tree root=48      a copy of the tree store root=48

== E3. the root named by a literal, off the exported surface alone ==
      the store, asked before anyone names it  sole=true
      ... and the write the guarantee denied   AAA=777

== G. and with the subtraction, what does a store guarantee? ==
      a store with a tree under it          sole=true
      ... and its child, asked the same     sole=false
```

**Those two assertions are the evidence.** A floater lives on a SYSTEM heap --
`P2PmsgHeap_CreateSYS` (`P2Pmsg.cpp:2810`, `:2898`) -- and `P2PmsgHeap_Connect` handles only
`P2PmsgHeap_IOMAGE` and `P2PmsgHeap_BSTRio`, falling through to `ASSERT(0); return
(VBLaddr)~0u` at `MsgVBHeap.cpp:5087`, with `P2PmsgHeap_IsRoot` doing the same at `:5109`.
A SYS handle names nothing at all, and every sole object in this library that is not a store
is on one. For all of them the count's premise holds without exception -- a block cannot be
named without a counted reference to reach it through, so TRUE has no reachable
counterexample. For a store it does not hold, and the gap is not narrow. **`48` is not a
function of the handle; it is a constant**, across addressing widths and tree shapes, and
the constructor that turns a handle and an address into a view is exported. A consumer
holding nothing but a store handle -- which `GetP2PmsgHandle` hands out
(`exports-cxx-x64.manifest:438`) and every `P2PmsgHeap_*` entry point takes -- builds a name
on the root and writes a grandchild one statement after the store answered `sole=true`.

**And G is the incoherence in two rows** -- one heap, two answers, the child's being the one
the rest of the document means.

### Which way the error falls, which is §26's question

§26 fixed the rule and §29, §31 and §36 all turn on it: a holder missed leaves the answer
FALSE and false promises nothing; a holder subtracted that was never mine reports TRUE with
a stranger looking. **Declining the subtraction is an undercount.** The store reads FALSE
forever, and E3's write is then a write the predicate never denied -- the constant FALSE is
the correct permanent answer rather than a degenerate one, because FALSE means "the question
is open" (`P2Pmsg.cpp:3357-3358`) and for a store it is open for as long as the handle
exists. **Making the subtraction is an overcount in effect if not in arithmetic.** The
number is right and the promise is wrong, which is exactly the failure §26 named: the count
would be exact about references and false about safety, and `IsSole` is read for safety. The
error falls on the forbidden side under one option and the safe side under the other.

### The row at `MsgcoreSuite.cpp:3207`, on its own terms

What-is-left is right that it was handed to §36 as a constraint and should not have been.
Read whole, `TF_CASE("nothing in a tree is sole")` makes three checks -- the root (`:3207`),
the descendant collection (`:3208`), the named child (`:3209`) -- then proves itself with a
write through a handle to `AAA` (`:3212-3214`). The teeth are on the third check; the first
has no write behind it, and the two checks that carry the case's title do not move under the
subtraction at all. **It is weak evidence and it decides nothing.** The decision above does
not rest on it and would be the same if the row were deleted tomorrow. What it is good for
is what it has always been good for: it is the tripwire that goes red if anybody makes this
change without reading this section, which is why it stays where it is.

### The recommendation

**Do not make the subtraction, and close the entry as decided rather than replacing it.**
`P2PmsgMgr` keeps inheriting `HeapHolders` and `IsSole` unchanged, a store keeps answering
FALSE always, and the four lines are not written. One sentence replaces the entry: a store
is never sole because the block it would vouch for is at a constant offset an outsider can
name from the handle alone, and the undercount is what keeps TRUE honest. Two things would
reopen it and nothing less -- a reference count per BLOCK rather than per heap, which §26
and §39 both price as a different library, or a store root that is not at a fixed offset,
which is an image-format change. The one correction left behind is in the NOTES at
`P2PmsgMgr.h:305-332` and in §36 itself, which both call the root's address "a function of
the handle alone". It is less than that and worse: it is a constant, 48, needing no handle
to be guessed. Sharpening that sentence is a NOTES edit and the owner's to make.

## 44. The pure form reaches every site, `SharedMode` assigns, and two names get bodies

`10c7004` closed "What is left" to three entries: a repairing validator five call sites
lean on, a setter that assigns nothing, and three `P3PmsgBSTR` members with no body. All
three are now fixed, in one pass, because they were one pass's worth of reading.

### The pure form reaches every site that wanted it

`P2PmsgHeap_IsValidAlloc` already existed -- §39 split it out of the repairing
`P2PmsgHeap_AssertValidAlloc` and converted two call sites with it (`P2Pmsg.cpp:2934`,
`:2981`). The five sites `10c7004` named get the same swap. `P3PmsgObject::GetP2Pos`,
`GetVBLock` and `GetVBLocknn` (`P2Pmsg.cpp:3170`, `:3176`, `:3186`) each had
`ASSERT(...||P2PmsgHeap_AssertValidAlloc(...))`; the callee changed and nothing else did,
because the classifier reads all three as `predicate` and always has -- the condition is
`a==0||b==0||f(...)`, not a bare call, so this moves no baseline figure. `AssertValidAddr`
(`:3315-3319`) is the mechanical case the entry called out by name: its return value IS the
caller's assertion (`ASSERT(m_oObject.AssertValidAddr(aDesc))`, `MsgDesc.cpp:258`), so
`return P2PmsgHeap_IsValidAlloc(...)` in place of the repairing form is the whole fix.

`P3PmsgObject::AssertValid` (`:3272-3295`) was the one member of the five that could not
take the mechanical swap and stop, because its call was bare and its return value was
already being thrown away -- the repair was the only reason the line existed. Swapping the
callee alone would have left a call to a pure predicate whose answer nobody reads, which is
not a fix, it is the same defect with a quieter symptom. `AssertValid` throws on every other
corruption it finds, so this one now does too: `if ( m_hVBList &&
!P2PmsgHeap_IsValidAlloc(m_hVBList,m_aVBLock) ) EVERR->...->Throw()` (`:3285-3288`),
consistent with the checks on either side of it rather than silent either way it used to be.

### `SharedMode` assigns now

`P2PmsgMgr::SharedMode` (`P2PmsgMgr.cpp:519-522`) is `m_dwSharedMode = dwSharedMode; return
TRUE;`. Nothing in the tree calls it for its return value -- grepped, not assumed -- so there
was no contract to preserve beyond the name doing what it says. It stays off the flat
surface: the active `Load` and `Save` paths do not consult `m_dwSharedMode` either
(`P2PmsgMgr.cpp:185-191` hardcodes `FILE_SHARE_READ|FILE_SHARE_WRITE`, `:465` hardcodes `0`
for the temp file), so a C caller would gain a working setter for a field two of the three
paths that could read it still ignore. `tools/ci/api-drift.allow` is reworded to say so
instead of the old "assigns nothing," which stopped being true the moment this landed.

### `P3PmsgBSTR` links

`IsFragmented` (`P2PmsgBSTR.h:208` before this) is deleted outright -- `P3PmsgVect::IsName`'s
precedent from §37, applied on the same ground: no `P2PmsgHeap` primitive answers
fragmentation, so a body would have been a policy invented for the occasion, not one wired
in. `SetDefaultSizeof` and `VBLockBSTR_vp` took the branch `10c7004` left open for one of the
three, because both have something to delegate to that `IsFragmented` did not. `SetDefaultSizeof`
(`P2PmsgBSTR.cpp:469-472`) returns `BSTR_INITIAL_SiZE`, the constant every constructor in
the file already uses as the size a `P3PmsgBSTR` gets when none is given.
`VBLockBSTR_vp` (`:450-453`) returns `P2PmsgHeap_pIOmage(m_hBSTR)`, the same heap primitive
`Sizeof` and `IsDirty` delegate to beside it. Both are one line, like their neighbours;
`tools/ci/api-drift.allow` keeps both off the flat surface, for the ordinary reasons already
argued next to `PrepareP2Piomage` and `SetDefaultP2Pmsgnn` rather than for failing to link.

### Measured, not assumed

Debug|x64 (`Msgcore(2022).vcxproj`, the toolset available to check this on) rebuilds clean:
0 errors, no warning at any of the changed lines. The unit suite passes unchanged in both
link modes -- 263 cases / 1423 checks static, 247 / 1330 dll, the same figures `e42cf19`
recorded before this round touched anything, which is what "no regression" means when four
of those cases (`Test_IsValidAllocIsPure`, `tests/MsgcoreSuite.cpp:5848-5914`) already probe
`IsValidAlloc` and `AssertValidAlloc` by name and by byte. The C4 Save/Load
test passes. `check_asserts.ps1` reports `callwrap 245`, `marker 209`, `predicate 183` --
the same three figures as before, confirming the five swaps above really do all classify as
`predicate`. `check_api_drift.ps1` reports zero UNTRIAGED and flags neither of the two
reworded entries as stale or redundant.

Nothing new was found reading the code beside these three. That is worth stating plainly,
because `10c7004` opened by observing that a ledger like this one "could not reach zero."
This time it does.


## 45. What is left

Nothing is. §44 closed the only three entries this document was carrying, and reading the
code around each of them turned up no fourth. That is not a claim that this repository is
defect-free -- only that every consequence-bearing defect forty-four sections of deliberate
reading found now has a fix, a citation, and a gate or a test that would catch its return.

The per-block reference count was never on this list and still is not: it is a standing
property of the storage layer, §26 states it, and restating it here would be the same mistake
`10c7004` named -- an observation dressed as an open item. The chain question is closed by
measurement, not by omission -- §32 asked all nineteen images and none carries a chain longer
than one link. Two flat-surface additions a caller might still want are argued in
`tools/ci/api-drift.allow`, where that decision belongs, not here.

The `^` grammar was settled at §28. §29 to §44 measured what the settling touched, and every
case they opened has an answer pinned by a test or by a gate that runs on every push. Five
gates pass together, and now so does a sixth reader's check: nothing left standing open.


## 46. Reproducing this document

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

### The image scan of §32

§32's answer is a measurement over files rather than over code, so it is reproduced the
same way — one probe, built against the static library, pointed at every arena on disk:

```
cl /nologo /EHsc /MDd /std:c++17 /Zc:wchar_t ^
   /D_DEBUG /D_CONSOLE /D_UNICODE /DUNICODE /D_AFXDLL /D_WIN32_WINNT=0x0603 ^
   /DMsgcore_STATIC /I. chain_probe.cpp ^
   /link /LIBPATH:out\x64\DebugLib Msgcore.lib MsWsock.lib ws2_32.lib ^
   comsuppwd.lib Propsys.lib /OUT:chain_probe.exe

chain_probe.exe <control-image-to-write> <image> [image ...]
```

It reuses `ChainLen29`, `ChainTail29` and `Lengthen29` from `Test_ChainLongerThanOne`
verbatim, writes its own three-link image as argument one — the positive control, without
which zero findings prove nothing — and then opens each remaining file with
`P2PmsgHeap_Create*` and walks the arena with the library's own `VBLock_Hdr_u_SizeNN`,
`VBLock_IsData`, `VBLockData_IsChained` and `VBLockData_GetChain2Next`. Install a
`_CrtSetReportHook` that counts and returns, as `TestFramework` does. Before §35 the four
assertions What-is-left named would otherwise stop the run in a dialog; they no longer
fire, but the probe's trusted-delegate phase still trips 115 of them by design, and
counting is what makes that phase a control rather than a crash.

Eleven of the nineteen are refused by the heap-open, which is those files working: most of
the corpus is malformed on purpose. Reading them needs a scanner that parses the arena
directly and bounds every declared size to the file length — a block header is
`uVBLockDefs` (address width in bits 0-1, type in bits 2-5, `VBLock_Data == 0x20`) followed
by the size field, and a `VBLockData` is a link exactly when its `uDataType` is `0xFF`,
with the onward offset in `u.aChain2Next{16,32,64}`. Two arena formats reach it: IOMAGE,
whose `oSync` is a complement pair and whose size is `uiSync1 & 0x00FFFFFF` with the first
block at offset 8; and BSTRio, whose `oDefs` is the complement pair, whose size is
`oSize.aSize1`, and whose first block is at `offsetof(cTag)`, 48. `P2PmsgMgr::Load` tests
BSTRio FIRST, and it has to: the IOMAGE complement test also accepts a BSTRio `oDefs` word,
so the other order misreads a BSTRio image as an IOMAGE one.

### The gate differential of §35

The four-image result is a differential, and a differential needs two trees. Add a pristine
worktree and one carrying only the change:

```
git worktree add <scratch>/wt-46a63f8 46a63f8
git worktree add <scratch>/wt-bstrio  46a63f8
```

Build `DebugLib` in each, and pass `/p:WDMSCS_LIB=<that worktree>\lib` when you do.
`WDMSCS_LIB` is an environment variable at User scope naming the shared
`MSCS\lib`, and `Directory.Build.props` lets it win over the standalone default, so without
the override a worktree stages its library over the main tree's and the two arms of the
differential quietly become one. Link each probe against its own tree's
`out\x64\DebugLib`, not against the staged copy. The cheap way to prove you did: the
`__FILE__` baked into each library shows its own worktree path in the trapped assertion
messages, so the before arm reports `wt-46a63f8\MsgVBHeap.cpp(3609)` and the after arm
does not report it at all.

The probe is §32's image walker with one phase added -- re-open each image through the
trusted single-argument delegate, outside any gate -- which is what turns "the guard is
present" into "the guard does something". Count assertions with `_CrtSetReportHook` rather
than letting them raise; the trusted phase trips 115 by design and the run has to survive
them to be a control.

The one-line mechanism is smaller than the corpus and shows the same thing. Save a store,
add one to `oKeys.nAllocEntries`, and open the image twice: through the untrusted overload
it is refused by name, with an assertion before the fix and none after; through the trusted
overload it is ACCEPTED in both builds, asserting at `MsgVBHeap.cpp:2087`, because outside
a gate the walk repairs the counter and then answers true.

### The matcher measurement of §37

`check_api_drift.ps1 -ShowBound` names every binding, every fallback match and, since §37,
every candidate the fallback refused and the test that refused it. The three tests are
worth re-running separately against the 43 the old matcher passed, because the interesting
property is not the new total but which members change side: 28 of the 43 are §34's 28,
member for member, and §34 arrived at that list by hand months of reading apart from the
rule that now reproduces it.

### The assert replay of §38

The category counts are source-text counts, so they can be replayed over history without
building anything. Walk the range with the script's own classifiers:

```
git rev-list --reverse 4d39d0d..HEAD
```

and at each commit run the three patterns over the tracked sources, excluding `Platform/`
and `tests/`. That is what establishes the two facts the bare numbers do not carry:
`callwrap` crossed its ceiling at `d2763ce` and the gate has been red on every commit
since, and `marker` never once rose above 216 across the whole range -- it fell to 207 and
came back to 208, and the rise was reported nowhere, because a ceiling only fails upward.

### The validator differential of §39

Same two-worktree shape as §35's, and the same `WDMSCS_LIB` trap applies -- pass
`/p:WDMSCS_LIB=<that worktree>\lib` or the two arms stage over each other and become one.
What is worth copying is how the diff was read rather than how it was produced. The raw
19-image comparison shows 18 differing lines, which looks like a result and is not: every
one of them is `MsgVBHeap.cpp(2049)` against `(2100)`, the same assertion at a line the
patch moved. **Normalise assertion line numbers before comparing, or a refactor reads as a
behaviour change.** With them normalised the diff is empty, which is the finding: 8
accepted, 11 refused, identical messages, identical counts either side.

Two measurements in that section need no corpus at all. For the type confusion, take
`offsetof` of `uVBListType` in a `VBListHANDLE` and read what sits at the same offset in a
`VBListBSTRio`: it is the low byte of `oDefs.uComp2`, which `P2PmsgHeap_IsBSTRio` has just
required to be the exact complement of the type byte, and no 8-bit value is its own
complement. For the repair, allocate one block and print `oHdr.uVBLockDefs` -- a fresh
block from `P2PmsgHeap_Alloc` is `0x47`, Alloc set and Linked clear, so the repair arm is
the ordinary path. Call the pure form and it stays `0x47`; call the repairing form and it
becomes `0xc7`.

### The C API suite of §41

The flat surface has its own suite, `tests/MsgcoreCApiSuite.cpp`, run by the same
`build_run_suite.bat`. A predicate whose two answers are not symmetric wants a test for
each answer separately, and the row that matters is neither of the obvious two: it is the
one pinning that FALSE is not relied upon, so that a later reader cannot "fix" the
predicate into promising both directions without a test going red. The teeth are cheap and
worth taking -- repoint the body at `r_Object().IsSole()` instead of
`P3PmsgField::IsSole` and exactly one check fails, the floating field with descendants,
which is the case the object's version cannot tell from a stranger.

### The matcher and the classifier of §40 and §42

Both corrections are one operator or one character, and both want the same discipline: run
the rule over the whole scanned set and count what moves in BOTH directions, not just the
direction you are hoping for. Dropping `0-9` from the snake-caser's lookbehind moves 44
spellings, gains 3 exact bindings and loses none. An `r_` to `get_` rule, tried the same
way, gains nine and **binds three wrongly**, which is how it was rejected rather than
shipped -- `msgcore_curs_get_name` is `c_wstr`'s counterpart, not `r_name`'s. A rule that
is only measured forwards is how the 28 of §37 got there in the first place.

For the classifier, the whole finding is visible in one line of output. Print the `$inner`
capture for `Msgexception.h:353` under both operators:

```
line     : Assert ( ) { ASSERT(0); return this; }
-match  inner: [ ) { ASSERT(0); return this; }]
-cmatch inner: [0); return this; }]
```

The case-insensitive pattern matches the method's own name before it reaches the `ASSERT`,
so the site that defines the `ASSERT(0)`-as-marker idiom was itself filed as a predicate.
Anything that reasons about a classifier without running it on its own source will
reproduce §38's error, which was to infer that this site was already counted correctly.

### The store-sole probe of §43

The subtracting version can be measured without modifying the library, and should be,
because the question is whether to change a predicate the rest of this document rests on.
`HeapHolders` is virtual as of §31 and `IsSole` dispatches through it, so a class derived
from `P2PmsgMgr` that adds one for `m_hMgr` **is** the four lines under discussion. Build
it in the scratchpad, run the same `refs=`/`mine=`/`sole=` rows, and the flip is visible
with nothing in the repository touched.

The constant is the part to check first and it needs no probe: ask a store for
`GetP2Pos()` and it answers 48, on every store, at every addressing width, with or without
a tree. Then confirm the control -- `P2PmsgHeap_Connect` and `P2PmsgHeap_IsRoot` on a SYS
handle fall through to `ASSERT(0)` and hand back `~0u` -- because the argument is not that
the store root is reachable but that it is the ONLY block that is.
