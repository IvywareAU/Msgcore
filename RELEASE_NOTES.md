# Msgcore release notes

User-facing changes, newest first. The full rationale for every version, including the ones
before this file existed, is kept in `Msgcore_version.h`.

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
