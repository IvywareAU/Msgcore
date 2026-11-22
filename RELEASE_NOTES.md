# Msgcore release notes

User-facing changes, newest first. The full rationale for every version, including the ones
before this file existed, is kept in `Msgcore_version.h`.

## 3.5.2

A patch release: creating a message is faster. The flat C API, the C++ export surface, the
image format, saved files and every exported class layout are unchanged, and so are the bytes
of every message built.

### Changed: the message skeleton is built in place

Creating an empty message and adding its `Msg` item went through the general object model: a
name lookup on a root with no children, then a temporary field that was copied into the heap.
`InitItem` now writes the item straight into the heap, and `Exists` on the skeleton items asks
the root's own cursor.

| create + destroy (3.5.1 and 3.5.2 interleaved, 7 rounds, p50) | 3.5.1 | 3.5.2 |
|---|---|---|
| empty message, Windows Release | 5,471 ns | 3,283 ns (−40%) |
| empty message, Linux Release | 10.4 µs, 9 allocations | 6.3 µs (−40%), 7 allocations |

- **Same bytes.** A test builds 36 messages both ways — every addressing width; empty, scalar,
  string and blob values up to 3 KB — and compares the images byte for byte.
- Sorted collections and names over 63 characters take the old route, unchanged.
- `DeclareItem` (adding ordinary fields) is unchanged in this release.

## 3.5.1

A patch release: building a large message copies far less. The flat C API, the C++ export
surface, the image format, saved files and every exported class layout are unchanged.

### Changed: a message's heap doubles when it grows

When an allocation did not fit, a message's heap grew by a fifth of its size. So a message
built up to 1 MB grew 23 times and copied 5.3 times its own size doing it. It now doubles:

| built to (8 KB fields) | grows | bytes copied | build time (p50, Windows Release) |
|---|---|---|---|
| 64 KB | 8 → 4 | 3.8× → 1.1× its size | 106 → 95 µs |
| 1 MB | 23 → 8 | 5.3× → 1.2× its size | 1,978 → 1,312 µs |

- A message being built holds up to twice its live bytes in memory. It sends none of that
  slack: Targetcore 3.7.0 and later trim a message before sending it (`TrimP2Piomage`, 3.5.0).
- If you send a message's image yourself, call `TrimP2Piomage()` first, as you would on 3.5.0.
- **Stores are unchanged.** A `P2PmsgMgr` still grows by a fifth, because `Save` writes its whole
  heap and saved files stay byte-identical.
- Messages with many small fields build no faster. Their cost is the sibling insert, not the
  growth.

## 3.5.0

A minor release: a message can now be sent without its unused space. The flat C API, the image
format, saved files and every exported class layout are unchanged. The C++ export surface
gains one member, so **Targetcore 3.7.0 needs this release**.

### New: `P3PmsgBSTR::TrimP2Piomage()`

A message's wire image used to be its whole heap, including the free space at the end. A message
with one small field went out as about 2 KB. `TrimP2Piomage()` removes that free space, so the
next image ends at the message's last block.

- **One `int` field: 2,056 → 307 bytes.**
- Any receiver accepts a trimmed image, including older releases. An image says only how long it
  is, not how big its heap was, and a trimmed image passes every check a receiver makes.
- The message stays usable. If you add to it after trimming, its heap grows as it would from any
  full heap.
- It answers the bytes removed, and 0 while an image is prepared (the length is already stamped).
- Saving is unchanged: saved images are byte-identical to 3.4.7.

### Fixed: a heap created at a size that is not a multiple of 8

Such a heap rounds its storage up to a multiple of 8 but recorded the size it was asked for, so
its last free block ran past its own recorded end. Nothing read it wrongly, but the trim above
could not see that block, and sealed messages (whose copies are created at such sizes) did not
shrink. The heap now records the rounded size. Its image bytes are unchanged, and receivers
already used the rounded size.

## 3.4.7

A patch release with a **security fix** on the receive path, and faster name lookups in
messages built in-process. The flat C API, the C++ export surface, the image format and every
exported class layout are unchanged. **Upgrade if you relay or copy received messages.**

### Security: copying a received value could write past its destination

Copying a value trusted the lengths in the value's own header. A received binary value could
claim to use more bytes than its capacity. Copying it then wrote past the end of the
destination, and the bytes written were ones the sender chose. Copying could also read past the
end of the received value. A relay that copies a received message (for example, Targetcore
redirecting one) took this path.

- The copy now refuses a value whose used length is larger than its capacity.
- It refuses to read past the source value's block, or to write past the destination.
- A declared capacity larger than the destination now becomes the destination's real size,
  instead of being zero-filled past its end.
- **Setting a received value had the same flaw.** `c_vBlob`, `c_memcpy` and `c_wcscpy` trusted
  the capacity a received value declared, so writing a longer value into it could run past its
  block. They now check the block's real size and grow the value when it doesn't fit, as they
  always did for a value that was too small. `c_memset` refuses such a value.
- Values this library writes copy and set exactly as before.

### Faster: name lookups in messages built in-process

Every name is checked against its message's bounds before it is read, because a received
message's bytes come from someone else. A message this process built from empty holds no such
bytes, so a Release build now skips those checks on it.

- **Received messages are checked exactly as before.** Only a message that this process
  created empty skips the checks. One built from received or loaded bytes never does.
- **Lookups at 512 siblings, message built in-process:** −25…−29% on Windows, −15…−16% on
  Linux.
- **Building a 512-field message:** −22% on Windows and −13% on Linux.
- Debug builds still run every check.
- Only the DLL / shared library needs replacing; nothing rebuilds against new headers.

## 3.4.6

A patch release that makes name lookups a little faster again. The flat C API, the C++ export
surface, the image format and every class layout are unchanged.

### Faster: the safety bounds are read once per lookup

Each sibling a lookup steps over has its name checked against the received message's image.
The image's bounds are the same for the whole lookup, but they were read again for every
sibling. They are now read once per lookup. Every check still runs, in the same order.

- **Lookups:** −3% on Windows and −7% on Linux at 512 siblings. Misses are −9% on both.
- **Building a 512-field message:** −2% on Windows and −7% on Linux.
- Only the DLL / shared library needs replacing; nothing rebuilds against new headers.

## 3.4.5

A patch release that makes name lookups about twice as fast on Windows and three to four
times as fast on Linux, and building a large message three to six times as fast. The flat C
API, the C++ export surface, the image format and every class layout are unchanged.

### Faster: finding a name among many siblings

A lookup used to set up a cursor on every sibling it stepped over, convert the sibling's name
to a wide string (on Linux, a copy) and compare it with a locale-aware compare. Now it first
reads the stored name straight from the image, after the same safety checks as before, and
compares it as ASCII ignoring case. Most siblings are ruled out there.

- **Lookups are faster:** `Exists()`, `SelectObject()` and `FieldRef` reads at 512 siblings
  take 12.1 → 6.4 µs on Windows (−47%) and 70.6 → 19.5 µs on Linux (−72%). Misses gain the
  same share.
- **The answers are the same.** The fast check can only say "this one is different". Any
  sibling that might match goes through the original compare, and so does every name with a
  non-ASCII character, so the first match still wins and case rules are unchanged, including
  for characters such as the Kelvin sign and `ß`.

### Faster: adding a new field

Adding a field by name searched the siblings to prove the name was new, and then searched
them again to find the field it had just added. It now goes straight to the new field, which
is always the last one.

- **Building a 512-field message:** 13.25 → 4.38 ms on Windows (−67%) and 74.3 → 12.9 ms on
  Linux (−83%). Allocations are unchanged.
- Building still takes longer than linear time, because each new name must be checked against
  every sibling already there.
- Only the DLL / shared library needs replacing; nothing rebuilds against new headers.

## 3.4.4

A patch release that makes every name lookup on a received message faster. The flat C API,
the C++ export surface, the image format and every class layout are unchanged.

### Faster: name checks on a received message

When a lookup steps over the siblings of a received message, it checks each sibling's name
against the image. Each check is a few compares, but they were made through about 20 function
calls per sibling, and the same image bounds were read four times. Now the bounds are read
once and the checks run inline.

- **Lookups are faster:** `Exists()`, `SelectObject()` and `FieldRef` reads at 512 siblings
  take 15.4 → 12.1 µs on Windows (−21%) and 80.1 → 70.7 µs on Linux (−12%). Misses gain the
  same share.
- **The same checks still run.** None was dropped or loosened, and a malformed name is
  refused exactly as before, with the same message. The suite's refusal cases pass
  unchanged: a name outside its block, no terminator, a capacity larger than the block, and
  a name chained to address 0.
- Only the DLL / shared library needs replacing; nothing rebuilds against new headers.

## 3.4.3

A patch release that makes a name lookup that misses twice as fast. The flat C API, the C++
export surface, the image format and every class layout are unchanged.

### Fixed: a lookup that missed walked the siblings twice

`Exists()`, `SelectObject()` and every path lookup that did not find a plain name searched
the whole sibling list, then searched it again with the same name. (The second search exists
for stored names that contain a delimiter, such as `Log.pys`; for a plain name it repeated
the first one.) It now runs only when the path has a delimiter left.

- **Every miss is about twice as fast:** 62.5 → 30.2 µs on Windows and 304 → 155 µs on
  Linux, for a 512-sibling lookup on a received message.
- **Answers are unchanged.** A stored dotted name is still found; a new suite case pins
  that, and a plain miss still misses.
- Targetcore's message path looks names up with `Exists()`, so it benefits without a
  rebuild against new headers. Only the DLL / shared library needs replacing.

## 3.4.2

A patch release that makes images smaller. The flat C API, the C++ export surface and every
class layout are unchanged.

### Fixed: every name took a 127-byte slot

Every field, list and vect in a message reserved the full 127-byte name slot, whatever its
name: a field called `a` cost as much as one with a 63-character name. A name now takes the
room it needs: a short header plus its UTF-16 units, with a small minimum per addressing
mode.

- **Images are smaller**, by roughly the unused part of 127 bytes for every named item.
- **Old images still work.** Each name records its own size, so images written by 3.4.1 and
  earlier load, read and accept new items under 3.4.2, and 3.4.2 images are the same format.
- **Bytes on disk change.** The same tree saved by 3.4.2 is not byte-identical to 3.4.1's
  output. Anything that compares saved images byte-for-byte needs a new reference;
  MscsUnitTests' `golden_ref.p2p` has been regenerated.
- **Renaming in place:** an item's name block is now only as big as its name, so renaming it
  to a longer name moves the name to its own block (the existing chained-name path) instead
  of reusing spare room in the slot.

## 3.4.1

A patch release that brings **Android**. The flat C API, the C++ export surface, the image
format and every class layout are unchanged, and a Windows build compiles to the same code.

### New: Android (NDK), over an epoll backend

The cores now build with the Android NDK (clang, libc++, bionic) for `arm64-v8a` and `x86_64`,
API 29 and later. P2PeerLan's Android app is the first consumer: it runs them natively.

- **`Platform/p2piocp_epoll.cpp`** implements the `p2piocp` hooks over **epoll**. Android
  blocks io_uring for apps. The completion model is the same as on Windows and io_uring:
  - reads and writes on a socket complete in order;
  - every operation reports its completion;
  - a write completes in full or fails;
  - closing a socket cancels its pending work first.
- **Choosing it:** Targetcore's `P2P_IO_BACKEND=epoll` selects it, and it is the default when
  building for Android. Linux keeps io_uring by default. epoll also serves a Linux kernel too
  old for io_uring.
- **Tested:** the new `platform_iocp_epoll_test` passes 125/125 on Linux, clean under ASan and
  TSan. The whole MSCS suite built on epoll passes on Linux. P2PeerLan's native tests pass on an
  Android 14 emulator.

### Fixed (Linux and Android; nothing changes on Windows)

- **MSVC format sizes crashed bionic's printf.** `%I64`, `%I32` and `%I` are now rewritten to
  `ll`, nothing and `z` before formatting, for narrow and wide formats alike. glibc tolerated
  them by accident; on Android, `P3PmsgField::Print` crashed.
- **Pipe paths:** `p2p_pipe_path` falls back from `$XDG_RUNTIME_DIR` to `$TMPDIR`, then
  `/tmp`. Android has no `/tmp`.
- **Smaller fixes:** `CList` gains const `GetNext`/`GetPrev`, and `sys/timeb.h` is included
  only where it exists.

## 3.4.0

A minor release. The flat C API gains two calls, and nothing in it was removed or changed
signature. The C++ export surface, the image format and every class layout a consumer sees are
unchanged.

### Fixed: three ways to lose data

- **Moving a node onto a name the destination already holds destroyed the source.**
  `msgcore_field_move_child` (and the C++ `P3PmsgRefactor_Move`, all four overloads) kept the
  destination and deleted the source anyway. If the source held several items of that name, it
  moved one and deleted the rest. The move is now **refused**, and nothing changes.
- **Moving a node into its own subtree crashed the process** with a stack overflow
  (0xC00000FD). It is now **refused**, and nothing changes.
- **A p2pos held across a delete resolved to the wrong node.** The heap reuses freed blocks,
  so a cached p2pos silently named whatever node took the block. The new calls below give a
  number that is refused instead.

TreeFs already guarded both moves itself; MsgFacade's `Move` did not, and now reports
`MSGF_E_CORE` for them.

### Changed behaviour

- A refused move throws in C++ and makes `msgcore_field_move_child` return **0**.
  Previously a move onto a taken name returned 1.
- A move now moves exactly one item: the first of that name.

### New: checked inodes

```c
unsigned long long inode = msgcore_field_get_inode(hField);   /* 0 if the field has none */
MsgFieldHandle     hNode = msgcore_mgr_inode2field(hMgr, inode); /* NULL once stale */
```

An inode is the p2pos with two tags beside it: how many times its block has been freed, and a
stamp for the open store that minted it. `msgcore_mgr_inode2field` returns a live handle, or
NULL when the node was deleted, the store was reloaded or defragmented, or the inode came from
another store. It never resolves to a different node.

- **Kept in memory, never saved:** an inode is valid for one open of a store, as a FUSE inode
  is for one mount. Tracking starts on the first `msgcore_field_get_inode` call, so a store that
  never asks costs nothing.
- **A moved node gets a new inode.** `msgcore_field_move_child` copies the node to a new block,
  so fetch the inode again after a move.
- **Limits:** a p2pos above 32 bits gets no inode. A block's free count wraps after 65,536
  frees and the stamp after 65,535 stores; only an inode carrying exactly the wrapped-onto
  value could then resolve wrongly.
- **Existing p2pos calls are unchanged** and still recycle.

Guard a portable call with `MSGCORE_VERSION_AT_LEAST(3,4,0)`.
