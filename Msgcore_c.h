// Copyright © 2026 Khrustal & Mann
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
// Msgcore_c.h
// Flat C wrapper over the Msgcore C++ API.
// This header is processed by jextract to produce Java Panama bindings.
// Build note: compile Msgcore_c.cpp into the Msgcore DLL (requires MFC Unicode build).
//
#pragma once

// Version identity. Macro-only and free of any Msgcore type, so it costs this
// header none of the self-containment the rest of it is careful about, and
// jextract sees nothing it has to translate.
#include "Msgcore_version.h"

// wchar_t. Seventy declarations below take or return one and until 2026-08-19
// this header declared none of them portably: in C++ wchar_t is a KEYWORD, so
// every in-tree consumer -- all of which are C++ -- compiled it without a
// murmur, while in C it is a typedef that arrives from <wchar.h> or <stddef.h>
// and nothing else. A C consumer on GCC got "unknown type name 'wchar_t'"
// seventy times over. MSVC hid it a second way: /Zc:wchar_t makes it a native
// type in C as well, so the one platform where the header was routinely used
// was also the one platform that could not report the defect.
//
// Found by the TargetCore install gate (its ctest test p2p_installtree), which
// is the first thing anywhere to compile a shipped header from C on Linux. That
// is what an install tree is for: the flat C API exists precisely so that
// somebody who is not us, on a toolchain that is not ours, can include it --
// and it had never once been asked to.
//
// TargetCore_c.h has included <wchar.h> all along. The two flat C surfaces must
// agree, or "the C API" means something different depending which half of it a
// consumer reaches for.
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

// Export/import decoration. Kept self-contained (no dependency on the Platform
// __declspec shim) so any consumer -- the TreeFS/FUSE frontend, Java Panama,
// etc. -- can include this header directly on either toolchain.
#if defined(_WIN32)
  #if defined(Msgcore_EXPORTS)
    #define MSGCORE_C_API __declspec(dllexport)
  #elif defined(Msgcore_STATIC)
    #define MSGCORE_C_API
  #else
    #define MSGCORE_C_API __declspec(dllimport)
  #endif
#else                                   // GCC/Clang (Linux port)
  #if defined(Msgcore_EXPORTS)
    #define MSGCORE_C_API __attribute__((visibility("default")))
  #else
    #define MSGCORE_C_API               // imports need no decoration on ELF
  #endif
#endif

// ---------------------------------------------------------------------------
// Opaque handle types (all are heap-allocated C++ objects cast to void*)
// ---------------------------------------------------------------------------
typedef void* MsgMgrHandle;     // P2PmsgMgr
typedef void* MsgFieldHandle;   // P3PmsgField / P3PmsgItem
typedef void* MsgListHandle;    // P3PmsgList
typedef void* MsgVectHandle;    // P3PmsgVect
typedef void* MsgAttrHandle;    // P3PmsgAttr
typedef void* MsgDescHandle;    // P3PmsgDesc
typedef void* MsgCursHandle;    // P3PmsgCurs

// ===========================================================================
// CONTRACTS — two rules this ABI cannot enforce and the caller must honour
// ===========================================================================
//
// Neither rule is checkable at a function signature, and breaking either one
// produces memory corruption rather than an error code. They are repeated at
// the declaration groups they govern; this is the statement in full.
//
// 1. NO INTERNAL SYNCHRONISATION.
//    The offset-addressed heap under every handle below contains no lock of
//    any kind. A single store (one MsgMgrHandle and every handle derived from
//    it) must be serialised by its caller: one writer, or external locking
//    around every call that can mutate. Concurrent readers are not safe
//    either, because a read can trigger the relocation described in rule 2.
//    Two independent managers in two threads are fine -- they share no state.
//    The COM server does exactly this, one critical section per store.
//
//    The one exception is the handle registry that validates the arguments to
//    these functions, which IS mutex-guarded. That protects the ABI against a
//    bad handle; it does not make the store behind a good one thread-safe.
//
// 2. NO POINTER OR HANDLE SURVIVES A MUTATION.
//    The heap addresses its blocks by offset and grows by reallocating its
//    base image. Any call that can allocate -- a declare, a rename, a retype,
//    a data write, a load -- may therefore move every block, invalidating
//    every raw pointer and every live-bound handle previously handed out.
//    Re-resolve from the manager, or from a stable positional handle
//    (msgcore_field_get_p2pos / msgcore_mgr_p2pos2field), after any such call.
//    Treat "obtain, use, discard" as the unit of work.
//
//    Handles from msgcore_mgr_as_field and the msgcore_*_select_item family
//    are detached deep copies and are exempt: they survive mutation because
//    they are not in the heap. They also do not see it -- writes through them
//    never reach the tree. The live-bound accessors are the ones rule 2 is
//    about; they are marked as such where they are declared.
//
// ===========================================================================

// ---------------------------------------------------------------------------
// P2PmsgMgr — top-level message store manager
//
// CONTRACT (rule 1 above): a manager and everything derived from it is
// unsynchronised. Serialise every call on one store in the caller.
// ---------------------------------------------------------------------------

// Create an empty manager (default addressing, initial heap ~2 KB)
MSGCORE_C_API MsgMgrHandle
msgcore_mgr_create(void);

// Create a manager with explicit addressing mode and heap sizes
// uAddrNN: VBLock_Addr16=1, VBLock_Addr32=2, VBLock_Addr64=3
MSGCORE_C_API MsgMgrHandle
msgcore_mgr_create_nn(unsigned char uAddrNN,
                      unsigned int  nSizeInitial,
                      unsigned int  nSizeMax);

// Load an existing .p2p file; returns NULL on failure
MSGCORE_C_API MsgMgrHandle
msgcore_mgr_open_file(const wchar_t* lpszFilename);

// Destroy (frees all heap memory)
MSGCORE_C_API void
msgcore_mgr_destroy(MsgMgrHandle hMgr);

// Serialisation
MSGCORE_C_API int
msgcore_mgr_load(MsgMgrHandle hMgr, const wchar_t* lpszFilename);

MSGCORE_C_API int
msgcore_mgr_save(MsgMgrHandle hMgr, const wchar_t* lpszFilename);

MSGCORE_C_API void
msgcore_mgr_nullify(MsgMgrHandle hMgr);

MSGCORE_C_API int
msgcore_mgr_rename(MsgMgrHandle hMgr, const wchar_t* lpszNewname);

// Properties
MSGCORE_C_API const wchar_t*
msgcore_mgr_get_filename(MsgMgrHandle hMgr);

MSGCORE_C_API const wchar_t*
msgcore_mgr_get_rootname(MsgMgrHandle hMgr);

MSGCORE_C_API int
msgcore_mgr_is_dirty(MsgMgrHandle hMgr);

MSGCORE_C_API int
msgcore_mgr_set_dirty(MsgMgrHandle hMgr, int bDirty);

MSGCORE_C_API unsigned int
msgcore_mgr_sizeof(MsgMgrHandle hMgr);

MSGCORE_C_API int
msgcore_mgr_is_valid(MsgMgrHandle hMgr);

// ---------------------------------------------------------------------------
// P3PmsgField / P3PmsgItem — name+data pair with list/vect/attr/desc support
// All returned handles are heap-allocated copies the caller must destroy.
//
// CONTRACT: because they are copies, they are detached -- exempt from rule 2
// (they survive a mutation) and equally unable to see one (writes through them
// never reach the tree). For handles that alias the live tree, and the
// invalidation rule that comes with them, see the FileSystem-layer group below.
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgFieldHandle
msgcore_mgr_as_field(MsgMgrHandle hMgr);

MSGCORE_C_API MsgFieldHandle
msgcore_field_create(void);

MSGCORE_C_API MsgFieldHandle
msgcore_field_clone(MsgFieldHandle hField);

MSGCORE_C_API void
msgcore_field_destroy(MsgFieldHandle hField);

// Navigation — caller owns the returned handle
MSGCORE_C_API MsgFieldHandle
msgcore_field_select_item(MsgFieldHandle hField, const wchar_t* lpszName);

MSGCORE_C_API int
msgcore_field_exists(MsgFieldHandle hField, const wchar_t* lpszName);

MSGCORE_C_API int
msgcore_field_delete_item(MsgFieldHandle hField, const wchar_t* lpszName);

MSGCORE_C_API void
msgcore_field_truncate(MsgFieldHandle hField);

// Declare (create or update) a child item with a typed value
MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_int(MsgFieldHandle hField, const wchar_t* lpszName, int value, int bUpdate);

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_int64(MsgFieldHandle hField, const wchar_t* lpszName, long long value, int bUpdate);

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_double(MsgFieldHandle hField, const wchar_t* lpszName, double value, int bUpdate);

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_bool(MsgFieldHandle hField, const wchar_t* lpszName, int bValue, int bUpdate);

MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_wstr(MsgFieldHandle hField, const wchar_t* lpszName, const wchar_t* lpszValue, int bUpdate);

// Name access
MSGCORE_C_API const wchar_t*
msgcore_field_get_name(MsgFieldHandle hField);

// Data getters / setters (operate on the field's own data slot)
MSGCORE_C_API unsigned char
msgcore_field_get_data_type(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_is_null(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_is_void(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_get_int(MsgFieldHandle hField);

MSGCORE_C_API void
msgcore_field_set_int(MsgFieldHandle hField, int value);

MSGCORE_C_API long long
msgcore_field_get_int64(MsgFieldHandle hField);

MSGCORE_C_API void
msgcore_field_set_int64(MsgFieldHandle hField, long long value);

MSGCORE_C_API double
msgcore_field_get_double(MsgFieldHandle hField);

MSGCORE_C_API void
msgcore_field_set_double(MsgFieldHandle hField, double value);

MSGCORE_C_API int
msgcore_field_get_bool(MsgFieldHandle hField);

MSGCORE_C_API void
msgcore_field_set_bool(MsgFieldHandle hField, int bValue);

// Returns pointer to the internal wide-string buffer (valid until next call or destroy)
MSGCORE_C_API const wchar_t*
msgcore_field_get_wstr(MsgFieldHandle hField);

MSGCORE_C_API void
msgcore_field_set_wstr(MsgFieldHandle hField, const wchar_t* lpszValue);

// Properties
MSGCORE_C_API int
msgcore_field_is_list(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_is_vect(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_is_attr(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_is_desc(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_is_stacked(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_is_attributed(MsgFieldHandle hField);

MSGCORE_C_API int
msgcore_field_is_descendant(MsgFieldHandle hField);

// ---------------------------------------------------------------------------
// P3PmsgList — doubly-linked list of P3PmsgData
// ---------------------------------------------------------------------------

// Wrap a field that contains a list (takes ownership via copy-construction)
MSGCORE_C_API MsgListHandle
msgcore_list_from_field(MsgFieldHandle hField);

MSGCORE_C_API void
msgcore_list_destroy(MsgListHandle hList);

MSGCORE_C_API int
msgcore_list_get_count(MsgListHandle hList);

MSGCORE_C_API void
msgcore_list_add_head_int(MsgListHandle hList, int value);

MSGCORE_C_API void
msgcore_list_add_tail_int(MsgListHandle hList, int value);

MSGCORE_C_API void
msgcore_list_add_head_double(MsgListHandle hList, double value);

MSGCORE_C_API void
msgcore_list_add_tail_double(MsgListHandle hList, double value);

MSGCORE_C_API void
msgcore_list_add_head_wstr(MsgListHandle hList, const wchar_t* lpszValue);

MSGCORE_C_API void
msgcore_list_add_tail_wstr(MsgListHandle hList, const wchar_t* lpszValue);

MSGCORE_C_API void
msgcore_list_drop_head(MsgListHandle hList);

MSGCORE_C_API void
msgcore_list_drop_tail(MsgListHandle hList);

MSGCORE_C_API void
msgcore_list_truncate(MsgListHandle hList);

// ---------------------------------------------------------------------------
// P3PmsgVect — indexed vector of P3PmsgField elements
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgVectHandle
msgcore_vect_from_field(MsgFieldHandle hField);

MSGCORE_C_API void
msgcore_vect_destroy(MsgVectHandle hVect);

MSGCORE_C_API int
msgcore_vect_is_data(MsgVectHandle hVect, int nElem);

MSGCORE_C_API int
msgcore_vect_is_field(MsgVectHandle hVect, int nElem);

MSGCORE_C_API int
msgcore_vect_is_list(MsgVectHandle hVect, int nElem);

MSGCORE_C_API int
msgcore_vect_is_vect(MsgVectHandle hVect, int nElem);

MSGCORE_C_API int
msgcore_vect_get_int(MsgVectHandle hVect, int nElem);

MSGCORE_C_API double
msgcore_vect_get_double(MsgVectHandle hVect, int nElem);

MSGCORE_C_API const wchar_t*
msgcore_vect_get_wstr(MsgVectHandle hVect, int nElem);

MSGCORE_C_API MsgFieldHandle
msgcore_vect_get_item(MsgVectHandle hVect, int nElem);

MSGCORE_C_API void
msgcore_vect_truncate(MsgVectHandle hVect);

// ---------------------------------------------------------------------------
// P3PmsgAttr — attribute collection attached to a field (keyed by '@')
// ---------------------------------------------------------------------------

// Connect to the attribute collection of hField (Create=true allocates if absent)
MSGCORE_C_API MsgAttrHandle
msgcore_attr_from_field(MsgFieldHandle hField, int bCreate);

MSGCORE_C_API void
msgcore_attr_destroy(MsgAttrHandle hAttr);

MSGCORE_C_API int
msgcore_attr_get_count(MsgAttrHandle hAttr);

MSGCORE_C_API int
msgcore_attr_is_empty(MsgAttrHandle hAttr);

MSGCORE_C_API int
msgcore_attr_exists(MsgAttrHandle hAttr, const wchar_t* lpszName);

MSGCORE_C_API MsgFieldHandle
msgcore_attr_select_item(MsgAttrHandle hAttr, const wchar_t* lpszName);

MSGCORE_C_API MsgFieldHandle
msgcore_attr_declare_int(MsgAttrHandle hAttr, const wchar_t* lpszName, int value, int bUpdate);

MSGCORE_C_API MsgFieldHandle
msgcore_attr_declare_double(MsgAttrHandle hAttr, const wchar_t* lpszName, double value, int bUpdate);

MSGCORE_C_API MsgFieldHandle
msgcore_attr_declare_wstr(MsgAttrHandle hAttr, const wchar_t* lpszName, const wchar_t* lpszValue, int bUpdate);

MSGCORE_C_API int
msgcore_attr_delete(MsgAttrHandle hAttr, const wchar_t* lpszName);

MSGCORE_C_API void
msgcore_attr_truncate(MsgAttrHandle hAttr);

// ---------------------------------------------------------------------------
// P3PmsgDesc — descendant (child) collection attached to a field
// ---------------------------------------------------------------------------

MSGCORE_C_API MsgDescHandle
msgcore_desc_from_field(MsgFieldHandle hField, int bCreate);

MSGCORE_C_API void
msgcore_desc_destroy(MsgDescHandle hDesc);

MSGCORE_C_API int
msgcore_desc_get_count(MsgDescHandle hDesc);

MSGCORE_C_API int
msgcore_desc_is_empty(MsgDescHandle hDesc);

MSGCORE_C_API int
msgcore_desc_exists(MsgDescHandle hDesc, const wchar_t* lpszName);

MSGCORE_C_API MsgFieldHandle
msgcore_desc_select_item(MsgDescHandle hDesc, const wchar_t* lpszName);

MSGCORE_C_API MsgFieldHandle
msgcore_desc_declare_int(MsgDescHandle hDesc, const wchar_t* lpszName, int value, int bUpdate);

MSGCORE_C_API MsgFieldHandle
msgcore_desc_declare_double(MsgDescHandle hDesc, const wchar_t* lpszName, double value, int bUpdate);

MSGCORE_C_API MsgFieldHandle
msgcore_desc_declare_wstr(MsgDescHandle hDesc, const wchar_t* lpszName, const wchar_t* lpszValue, int bUpdate);

MSGCORE_C_API int
msgcore_desc_delete(MsgDescHandle hDesc, const wchar_t* lpszName);

MSGCORE_C_API void
msgcore_desc_truncate(MsgDescHandle hDesc);

// ---------------------------------------------------------------------------
// P3PmsgCurs — iterator / cursor over a field's children
// ---------------------------------------------------------------------------

// Create a cursor over a P3PmsgItem (field/list/vect hierarchy)
MSGCORE_C_API MsgCursHandle
msgcore_curs_from_field(MsgFieldHandle hField);

MSGCORE_C_API MsgCursHandle
msgcore_curs_from_attr(MsgAttrHandle hAttr);

MSGCORE_C_API MsgCursHandle
msgcore_curs_from_desc(MsgDescHandle hDesc);

MSGCORE_C_API void
msgcore_curs_destroy(MsgCursHandle hCurs);

// Advance / rewind
MSGCORE_C_API void
msgcore_curs_next(MsgCursHandle hCurs);

MSGCORE_C_API void
msgcore_curs_seek(MsgCursHandle hCurs);

MSGCORE_C_API int
msgcore_curs_goto_name(MsgCursHandle hCurs, const wchar_t* lpszName);

MSGCORE_C_API int
msgcore_curs_goto_index(MsgCursHandle hCurs, int nElem);

// State queries
MSGCORE_C_API int
msgcore_curs_is_eo_cursor(MsgCursHandle hCurs);

MSGCORE_C_API int
msgcore_curs_is_so_cursor(MsgCursHandle hCurs);

MSGCORE_C_API int
msgcore_curs_get_count(MsgCursHandle hCurs);

MSGCORE_C_API int
msgcore_curs_item_index(MsgCursHandle hCurs);

MSGCORE_C_API int
msgcore_curs_is_item(MsgCursHandle hCurs);

MSGCORE_C_API int
msgcore_curs_is_list(MsgCursHandle hCurs);

MSGCORE_C_API int
msgcore_curs_is_vect(MsgCursHandle hCurs);

// Current element access (caller owns returned handle)
MSGCORE_C_API MsgFieldHandle
msgcore_curs_get_field(MsgCursHandle hCurs);

MSGCORE_C_API const wchar_t*
msgcore_curs_get_name(MsgCursHandle hCurs);

MSGCORE_C_API void
msgcore_curs_delete(MsgCursHandle hCurs);

// ---------------------------------------------------------------------------
// FileSystem-layer support. (The TreeFS design note that motivated this group is
// internal and not published with Msgcore; everything needed to CALL these is here.)
// Additive, ABI-stable wrappers over existing Msgcore addressing / refactor /
// type helpers. The TreeFS core (FUSE / WinFsp frontend) calls these; other
// consumers can ignore them. Nothing here changes existing behaviour.
// ---------------------------------------------------------------------------

// LIVE tree access. Unlike msgcore_mgr_as_field / msgcore_*_select_item -- which
// return a DETACHED deep copy (safe to read, but mutations never reach the tree)
// -- these return a handle that ALIASES the live node in the manager's heap, so
// declares / renames / retypes through it persist and are visible to Save. This
// is the accessor set the TreeFS write path must use. Returned handles are
// caller-owned (msgcore_field_destroy).
//
// CONTRACT (rule 2 at the top of this header): these are the handles the
// pointer-invalidation rule governs. A live handle is invalidated by a VBHeap
// relocation on a later mutation -- any call that can grow the heap may move
// the base image, so a handle obtained before such a call must not be used
// after it. Re-resolve from the manager per operation, or hold the positional
// handle (msgcore_field_get_p2pos) rather than the field handle: a p2pos is a
// heap offset, not an address, so it still denotes the same node after a
// relocation has moved it. That is what makes it the right thing to cache
// across a mutation -- but only while the node lives. It is not an identity:
// delete the node and the offset can be handed back out to a later allocation,
// so a stale p2pos resolves to whatever now occupies that block rather than
// failing. Treat "obtain, use, discard" as the unit of work.

// Live-bound handle to the manager's root field.
MSGCORE_C_API MsgFieldHandle
msgcore_mgr_root(MsgMgrHandle hMgr);

// Live-bound handle to the descendant named lpszName (NULL if it does not exist).
MSGCORE_C_API MsgFieldHandle
msgcore_field_child(MsgFieldHandle hField, const wchar_t* lpszName);

// Stable positional handle of a field -- the natural inode number (st_ino).
// Returns 0 for a null handle or an unaddressable (standalone) field.
MSGCORE_C_API unsigned long long
msgcore_field_get_p2pos(MsgFieldHandle hField);

// Resolve a positional handle back to a field within a manager's tree.
// Returned handle is caller-owned (destroy with msgcore_field_destroy);
// NULL if the position does not resolve.
MSGCORE_C_API MsgFieldHandle
msgcore_mgr_p2pos2field(MsgMgrHandle hMgr, unsigned long long pos);

// Resolve a positional handle to its full path. The returned pointer is a
// thread-local buffer valid until the next msgcore_mgr_p2pos2path call on the
// same thread; NULL on failure.
MSGCORE_C_API const wchar_t*
msgcore_mgr_p2pos2path(MsgMgrHandle hMgr, unsigned long long pos);

// Stable ASCII name for a MSGCORE_DATA_* type constant (for the ".type" file).
// Never NULL; returns "UNKNOWN" for an unrecognised code.
MSGCORE_C_API const char*
msgcore_type_name(unsigned char uDataType);

// Inverse of msgcore_type_name: map a type name (case-sensitive, as emitted by
// msgcore_type_name -- e.g. "INT08", "UINT64", "WSTR16", "GUID") to its
// MSGCORE_DATA_* code. Returns 0xFF for an unrecognised name, so a ".type"
// write can reject an invalid type with EINVAL. The two functions share one
// vocabulary so the FS never disagrees with itself about a node's type.
MSGCORE_C_API unsigned char
msgcore_type_from_name(const char* lpszTypeName);

// Node timestamp (st_mtime), stored as the "$TStamp$" attribute in seconds since
// the epoch. get returns 0 when unset. set records tsValue (pass -1 for "now")
// and returns the stored value. Mutation must be through a LIVE handle
// (msgcore_mgr_root / msgcore_field_child) to reach the tree; a detached handle
// stamps only its private copy.
MSGCORE_C_API long long
msgcore_field_get_tstamp(MsgFieldHandle hField);
MSGCORE_C_API long long
msgcore_field_set_tstamp(MsgFieldHandle hField, long long tsValue);

// Rename a child of hField in place (rename(2) within one directory).
// No-op success when lpszOldName == lpszNewName. Returns 1 on success (the
// child existed), 0 otherwise.
MSGCORE_C_API int
msgcore_field_rename_child(MsgFieldHandle hField, const wchar_t* lpszOldName, const wchar_t* lpszNewName);

// Move the child named lpszName from hSrcField to hDstField (rename(2) across
// directories). Returns 1 on success (the source child existed), 0 otherwise.
MSGCORE_C_API int
msgcore_field_move_child(MsgFieldHandle hSrcField, MsgFieldHandle hDstField, const wchar_t* lpszName);

// Retype an existing child (the ".type" write / setxattr path). Changes the
// child's data type to that of the supplied value only when it differs, seeding
// the given value. Returns 1 if the child exists, 0 otherwise.
MSGCORE_C_API int
msgcore_field_retype_child_int(MsgFieldHandle hField, const wchar_t* lpszName, int value);
MSGCORE_C_API int
msgcore_field_retype_child_int64(MsgFieldHandle hField, const wchar_t* lpszName, long long value);
MSGCORE_C_API int
msgcore_field_retype_child_double(MsgFieldHandle hField, const wchar_t* lpszName, double value);
MSGCORE_C_API int
msgcore_field_retype_child_bool(MsgFieldHandle hField, const wchar_t* lpszName, int bValue);
MSGCORE_C_API int
msgcore_field_retype_child_wstr(MsgFieldHandle hField, const wchar_t* lpszName, const wchar_t* lpszValue);

// --- Extended value types (Phase 1 completion) -----------------------------
// Declare/read the value types the scalar declares above cannot express, so a
// value-file write preserves a node's exact type instead of normalising it.

// Declare (create/update) an integer child at an EXPLICIT width. uDataType is one
// of MSGCORE_DATA_INT08..UINT64; the value is stored at that width (an
// unrecognised type falls back to INT32). Unlike msgcore_field_declare_int --
// which always yields INT32 -- this lets a "value" write keep a narrow/unsigned
// int node's declared subtype. Returns a caller-owned handle or NULL.
MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_int_typed(MsgFieldHandle hField, const wchar_t* lpszName,
                                long long value, unsigned char uDataType, int bUpdate);

// Width-agnostic integer read. Fills *pValue with the field's value (as raw bits
// in a long long) and *pUnsigned with 1 for the unsigned subtypes; returns 1 on
// success, 0 when the field is not an integer-family scalar (INT08..UINT64/BOOL).
// The strict msgcore_field_get_int / _int64 throw on any non-INT32 / non-INT64
// subtype, so this is the accessor the "value" render path must use.
MSGCORE_C_API int
msgcore_field_get_int_any(MsgFieldHandle hField, long long* pValue, int* pUnsigned);

// FLOAT (32-bit) read. get_double is strict to DOUBLE and throws on a FLOAT node.
MSGCORE_C_API double
msgcore_field_get_float(MsgFieldHandle hField);

// BLOB16 raw bytes. get returns a pointer to the child's bytes (valid until the
// next call or destroy) and writes the byte count through *pnSize; returns NULL
// (and *pnSize = 0) when the field is not a BLOB16. declare copies nSize bytes.
MSGCORE_C_API const void*
msgcore_field_get_blob(MsgFieldHandle hField, int* pnSize);
MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_blob(MsgFieldHandle hField, const wchar_t* lpszName,
                           const void* pvData, int nSize, int bUpdate);

// GUID as canonical text ("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX", upper hex, no
// braces). get returns a thread-local buffer valid until the next
// msgcore_field_get_guid_str call on the same thread, or NULL when the field is
// not a GUID. declare parses the text (optional surrounding braces tolerated)
// and returns NULL on a malformed string.
MSGCORE_C_API const wchar_t*
msgcore_field_get_guid_str(MsgFieldHandle hField);
MSGCORE_C_API MsgFieldHandle
msgcore_field_declare_guid_str(MsgFieldHandle hField, const wchar_t* lpszName,
                               const wchar_t* lpszGuid, int bUpdate);

// ---------------------------------------------------------------------------
// Change notification (headless triggers)
// ---------------------------------------------------------------------------
// The legacy trigger facility posts a Windows message to an HWND when an armed
// node changes. These wrappers add the windowless path a FUSE/daemon host needs:
// install one function-pointer sink on the manager, arm nodes by P2Pos, and the
// sink is invoked (in-thread, synchronously) whenever an armed node fires.

// Trigger-type bits (mirror the core TRIGGER_* masks). A sink invocation carries
// exactly one of these.
#define MSGCORE_TRIGGER_INSERT  1
#define MSGCORE_TRIGGER_UPDATE  2
#define MSGCORE_TRIGGER_DELETE  4
#define MSGCORE_TRIGGER_ACTIVE  8

// Sink signature: pUser is the pointer handed to msgcore_mgr_set_trigger_sink;
// nTriggerType is one MSGCORE_TRIGGER_* bit; p2pos is the changed node's inode.
// The callback runs on the thread performing the mutation -- keep it short and
// non-throwing (do real work by posting to your own queue).
typedef void (*msgcore_trigger_fn)(void* pUser, unsigned int nTriggerType,
                                   unsigned long long p2pos);

// Install (fn == NULL clears) the manager's headless trigger sink.
MSGCORE_C_API void
msgcore_mgr_set_trigger_sink(MsgMgrHandle hMgr, msgcore_trigger_fn fn, void* pUser);

// Arm / disarm a node (by P2Pos) for the given MSGCORE_TRIGGER_* mask. DELETE
// fires automatically when a node is freed; INSERT/UPDATE fire when the writer
// calls msgcore_mgr_fire_trigger (the FS does so after a successful mutation).
MSGCORE_C_API void
msgcore_mgr_create_trigger(MsgMgrHandle hMgr, unsigned int nMask, unsigned long long p2pos);
MSGCORE_C_API void
msgcore_mgr_drop_triggers(MsgMgrHandle hMgr, unsigned int nMask, unsigned long long p2pos);

// Fire the given trigger mask for a node by P2Pos (invokes the sink + any HWND
// registrations). Returns the number of registrations that fired.
MSGCORE_C_API int
msgcore_mgr_fire_trigger(MsgMgrHandle hMgr, unsigned int nMask, unsigned long long p2pos);

// ===========================================================================
// Containers II — creating a list or a vect, and completing both
// ===========================================================================
// The container blocks above can only WRAP a field that already holds a list or
// a vect, and msgcore_list_from_field / msgcore_vect_from_field wrap it by
// copy-construction, so a write through the result never reaches the store.
// There was no way to CREATE either container at all, which left every list and
// vect entry point reachable only over a file some other tool had written.
//
// LIVE vs DETACHED, stated once for this whole block:
//   msgcore_*_declare_list / _declare_vect   LIVE  (alias the node in the heap)
//   msgcore_*_select_list  / _select_vect    LIVE
//   msgcore_list_from_field / _vect_from_field   DETACHED (a private copy)
// A live handle is invalidated by a VBHeap relocation on a later mutation, as
// everywhere else in this header -- re-resolve per operation. Destroy every
// returned handle with msgcore_list_destroy / msgcore_vect_destroy; destroying
// a LIVE wrapper releases the wrapper only, never the node.
//
// A note on why there is no positional list walk. P3PmsgList is walked in C++
// by VBLaddr (GetHeadPos / GetNext / GetPrev), and VBLaddr is a raw heap
// address -- precisely the thing a relocation invalidates. Handing one across
// this ABI would export a dangling-pointer hazard to callers who have no way to
// detect it, so the flat surface is indexed instead: _at(nIndex) walks from the
// head each call. That is O(n) per access and O(n^2) for a full scan; for a
// large list use msgcore_curs_from_field, which holds its own position.

// --- creation --------------------------------------------------------------
// Build the container, attach it to hField's descendant collection (creating
// that collection if absent), and return a live handle. NULL on failure.
// nElems is the vect's initial element count (0 is legal -- a vect grows);
// uDataType is the element prototype, one of MSGCORE_DATA_* (an unrecognised
// value falls back to INT32).
MSGCORE_C_API MsgListHandle
msgcore_field_declare_list(MsgFieldHandle hField, const wchar_t* lpszName);
MSGCORE_C_API MsgVectHandle
msgcore_field_declare_vect(MsgFieldHandle hField, const wchar_t* lpszName,
                           int nElems, unsigned char uDataType);

// The same, into a field's ATTRIBUTE collection rather than its descendants.
MSGCORE_C_API MsgListHandle
msgcore_attr_declare_list(MsgAttrHandle hAttr, const wchar_t* lpszName);
MSGCORE_C_API MsgVectHandle
msgcore_attr_declare_vect(MsgAttrHandle hAttr, const wchar_t* lpszName,
                          int nElems, unsigned char uDataType);

// --- live typed read-back --------------------------------------------------
// NULL when the name does not exist OR exists but is not a container of that
// kind, so a caller never has to pre-check with _exists.
MSGCORE_C_API MsgListHandle
msgcore_field_select_list(MsgFieldHandle hField, const wchar_t* lpszName);
MSGCORE_C_API MsgVectHandle
msgcore_field_select_vect(MsgFieldHandle hField, const wchar_t* lpszName);
MSGCORE_C_API MsgListHandle
msgcore_desc_select_list(MsgDescHandle hDesc, const wchar_t* lpszName);
MSGCORE_C_API MsgVectHandle
msgcore_desc_select_vect(MsgDescHandle hDesc, const wchar_t* lpszName);
MSGCORE_C_API MsgListHandle
msgcore_attr_select_list(MsgAttrHandle hAttr, const wchar_t* lpszName);
MSGCORE_C_API MsgVectHandle
msgcore_attr_select_vect(MsgAttrHandle hAttr, const wchar_t* lpszName);

// --- list: the widths the original block omitted ---------------------------
MSGCORE_C_API void
msgcore_list_add_head_int64(MsgListHandle hList, long long value);
MSGCORE_C_API void
msgcore_list_add_tail_int64(MsgListHandle hList, long long value);
MSGCORE_C_API void
msgcore_list_add_head_bool(MsgListHandle hList, int bValue);
MSGCORE_C_API void
msgcore_list_add_tail_bool(MsgListHandle hList, int bValue);

// --- list: indexed element access ------------------------------------------
// nIndex is 0-based. Reads return 0 / NULL when the index is out of range or
// the cell is not of the requested type; msgcore_list_get_type_at answers
// MSGCORE_DATA_NULL for an out-of-range index, so it is the safe probe.
// Setters return 1 on success, 0 when the index is out of range.
MSGCORE_C_API unsigned char
msgcore_list_get_type_at(MsgListHandle hList, int nIndex);
MSGCORE_C_API int
msgcore_list_get_int_at(MsgListHandle hList, int nIndex);
MSGCORE_C_API long long
msgcore_list_get_int64_at(MsgListHandle hList, int nIndex);
MSGCORE_C_API double
msgcore_list_get_double_at(MsgListHandle hList, int nIndex);
MSGCORE_C_API int
msgcore_list_get_bool_at(MsgListHandle hList, int nIndex);
// Thread-local buffer, valid until the next msgcore_list_get_wstr_at on this
// thread (mirrors msgcore_field_get_wstr).
MSGCORE_C_API const wchar_t*
msgcore_list_get_wstr_at(MsgListHandle hList, int nIndex);

MSGCORE_C_API int
msgcore_list_set_int_at(MsgListHandle hList, int nIndex, int value);
MSGCORE_C_API int
msgcore_list_set_int64_at(MsgListHandle hList, int nIndex, long long value);
MSGCORE_C_API int
msgcore_list_set_double_at(MsgListHandle hList, int nIndex, double value);
MSGCORE_C_API int
msgcore_list_set_bool_at(MsgListHandle hList, int nIndex, int bValue);
MSGCORE_C_API int
msgcore_list_set_wstr_at(MsgListHandle hList, int nIndex, const wchar_t* lpszValue);

// Remove the element at nIndex. Returns 1 on success, 0 if out of range.
MSGCORE_C_API int
msgcore_list_delete_at(MsgListHandle hList, int nIndex);

// --- vect: the element count, and the accessors the original block omitted --
// msgcore_vect_get_count had no flat entry point at all, which made every
// indexed vect getter above unusable without guessing the bound.
MSGCORE_C_API int
msgcore_vect_get_count(MsgVectHandle hVect);

MSGCORE_C_API unsigned char
msgcore_vect_get_type(MsgVectHandle hVect, int nElem);
MSGCORE_C_API long long
msgcore_vect_get_int64(MsgVectHandle hVect, int nElem);
MSGCORE_C_API int
msgcore_vect_get_bool(MsgVectHandle hVect, int nElem);
// Element NAME (a vect element is a field, so it carries one).
MSGCORE_C_API const wchar_t*
msgcore_vect_get_name(MsgVectHandle hVect, int nElem);

MSGCORE_C_API int
msgcore_vect_set_int(MsgVectHandle hVect, int nElem, int value);
MSGCORE_C_API int
msgcore_vect_set_int64(MsgVectHandle hVect, int nElem, long long value);
MSGCORE_C_API int
msgcore_vect_set_double(MsgVectHandle hVect, int nElem, double value);
MSGCORE_C_API int
msgcore_vect_set_bool(MsgVectHandle hVect, int nElem, int bValue);
MSGCORE_C_API int
msgcore_vect_set_wstr(MsgVectHandle hVect, int nElem, const wchar_t* lpszValue);

// Nested containers: a vect element may itself be a list or a vect.
// NULL when the element is not of that kind. Live.
MSGCORE_C_API MsgListHandle
msgcore_vect_get_list(MsgVectHandle hVect, int nElem);
MSGCORE_C_API MsgVectHandle
msgcore_vect_get_vect(MsgVectHandle hVect, int nElem);

// Deep-copy hField into the vect at nElem, growing the vect if nElem is past
// the end. Returns 1 on success.
MSGCORE_C_API int
msgcore_vect_insert_at(MsgVectHandle hVect, int nElem, MsgFieldHandle hField);
// Remove the element at nElem. Returns 1 on success, 0 if out of range.
MSGCORE_C_API int
msgcore_vect_delete(MsgVectHandle hVect, int nElem);
// Move the vect's internal cursor. Returns the resulting index, -1 on failure.
MSGCORE_C_API int
msgcore_vect_goto(MsgVectHandle hVect, int nElem);

// ===========================================================================
// MsgStck — the field position stack
// ===========================================================================
// MsgStck remembers where you were. Connect it to a field, Push to save that
// position, navigate, then Pop to return -- the C++ walker idiom for "descend,
// do work, come back" without the caller keeping its own stack of names.
//
// The handle owns a C++ MsgStck; the FIELD it is connected to is not owned and
// must outlive it. msgcore_stck_get_* return LIVE handles onto the current
// position (destroy the wrapper, never the node) and NULL when the stack is
// empty or the current position is not of that kind.

typedef void* MsgStckHandle;    // MsgStck

MSGCORE_C_API MsgStckHandle
msgcore_stck_create(void);
// Create and connect in one call. hField must outlive the returned stack.
MSGCORE_C_API MsgStckHandle
msgcore_stck_from_field(MsgFieldHandle hField);
MSGCORE_C_API void
msgcore_stck_destroy(MsgStckHandle hStck);

MSGCORE_C_API void
msgcore_stck_connect(MsgStckHandle hStck, MsgFieldHandle hField);
MSGCORE_C_API void
msgcore_stck_nullify(MsgStckHandle hStck);

// Push saves the current position, Pop restores the last saved one. Both
// return 1 when the call was made against a connected stack and 0 when it was
// not -- MsgStck::Push and MsgStck::Pop dereference their field without a null
// check, so an unconnected stack is refused here rather than crashing there.
//
// Pop's return is NOT "something was restored". The core's Pop is a silent
// no-op when nothing is stacked, so `while (msgcore_stck_pop(h)) ;` never
// terminates. Drain with msgcore_stck_is_empty instead.
MSGCORE_C_API int
msgcore_stck_push(MsgStckHandle hStck);
MSGCORE_C_API int
msgcore_stck_pop(MsgStckHandle hStck);
// Release the stacked position without restoring it.
MSGCORE_C_API void
msgcore_stck_drop(MsgStckHandle hStck);

// Rename the field at the current position. bRecurse renames nested
// occurrences too (the C++ default is true). Returns 1 on success.
MSGCORE_C_API int
msgcore_stck_rename(MsgStckHandle hStck, const wchar_t* lpszName, int bRecurse);

// 1 when nothing is stacked. An unconnected stack reports 1 here, though
// MsgStck::IsEmpty itself reports the opposite for that case -- see the note
// at the implementation.
MSGCORE_C_API int
msgcore_stck_is_empty(MsgStckHandle hStck);
MSGCORE_C_API const wchar_t*
msgcore_stck_get_name(MsgStckHandle hStck);
MSGCORE_C_API MsgFieldHandle
msgcore_stck_get_field(MsgStckHandle hStck);
MSGCORE_C_API MsgFieldHandle
msgcore_stck_get_item(MsgStckHandle hStck);
MSGCORE_C_API MsgListHandle
msgcore_stck_get_list(MsgStckHandle hStck);
MSGCORE_C_API MsgVectHandle
msgcore_stck_get_vect(MsgStckHandle hStck);

// ===========================================================================
// P2PmsgRecurs — the recursive subtree walker
// ===========================================================================
// A whole subtree from one flat loop. P2PmsgRecurs owns a chain of cursors and
// splices descent into its ++ operator, but it descends only when the caller
// says push -- which is what makes it a WALKER rather than an iterator, and
// what lets a caller prune a branch by simply not descending into it.
//
// The canonical loop:
//
//     MsgRecursHandle r = msgcore_recurs_from_field(hRoot);
//     while (!msgcore_recurs_is_eo_recurs(r)) {
//         const wchar_t* name = msgcore_recurs_get_name(r);
//         if (want_to_descend(name)) msgcore_recurs_push(r);   // else prune
//         msgcore_recurs_next(r);
//     }
//     msgcore_recurs_destroy(r);
//
// The walker holds cursors into the tree, so it must not outlive a mutation of
// that tree; finish the walk, then mutate.

typedef void* MsgRecursHandle;  // P2PmsgRecurs

// NULL if hField/hAttr is null or does not admit a walk.
MSGCORE_C_API MsgRecursHandle
msgcore_recurs_from_field(MsgFieldHandle hField);
MSGCORE_C_API MsgRecursHandle
msgcore_recurs_from_attr(MsgAttrHandle hAttr);
MSGCORE_C_API void
msgcore_recurs_destroy(MsgRecursHandle hRecurs);

// Advance (operator++). At the end of a pushed level this pops automatically.
MSGCORE_C_API void
msgcore_recurs_next(MsgRecursHandle hRecurs);

// Descend into the current element. Returns the new depth (1-based), or -1 if
// the current element is not descendable -- the C++ Push throws in that case,
// so testing msgcore_recurs_is_field first is optional, not required.
MSGCORE_C_API int
msgcore_recurs_push(MsgRecursHandle hRecurs);
// Ascend one level. Returns the resulting depth, or -1 if nothing was pushed.
MSGCORE_C_API int
msgcore_recurs_pop(MsgRecursHandle hRecurs);
// Abandon every pushed level at once and resume at the outermost.
MSGCORE_C_API void
msgcore_recurs_break(MsgRecursHandle hRecurs);

MSGCORE_C_API int
msgcore_recurs_is_eo_recurs(MsgRecursHandle hRecurs);
MSGCORE_C_API int
msgcore_recurs_is_field(MsgRecursHandle hRecurs);
MSGCORE_C_API int
msgcore_recurs_is_list(MsgRecursHandle hRecurs);
MSGCORE_C_API int
msgcore_recurs_is_vect(MsgRecursHandle hRecurs);

// Current element. get_name returns the walker's own thread-local text
// (c_wstr); the handle getters return LIVE handles and NULL off-kind.
MSGCORE_C_API const wchar_t*
msgcore_recurs_get_name(MsgRecursHandle hRecurs);
MSGCORE_C_API MsgFieldHandle
msgcore_recurs_get_item(MsgRecursHandle hRecurs);
MSGCORE_C_API MsgListHandle
msgcore_recurs_get_list(MsgRecursHandle hRecurs);
MSGCORE_C_API MsgVectHandle
msgcore_recurs_get_vect(MsgRecursHandle hRecurs);

// ===========================================================================
// Paging — large datasets held outside the heap
// ===========================================================================
// A manager can delegate the residency of a subtree to its host: the host is
// asked to page a dataset IN before it is read and OUT when it is finished
// with, so a store may describe far more data than it holds. These wrap the
// P2PmsgMgr paging facility.

// Push/pop the current callback registration. PageRegistrationPush saves the
// installed sinks and clears them; Pop restores them. This is what the C++
// SafeRegistrationPush does with a constructor and a destructor, and it is the
// way to run a section of code with paging suppressed -- e.g. a Save, which
// must not trigger a page-in of everything it walks. Returns 1 on success.
// NOTE: push does not nest. Pushing twice without an intervening pop asserts
// in a debug core and loses the first saved set in a release one.
MSGCORE_C_API int
msgcore_mgr_page_registration_push(MsgMgrHandle hMgr);
MSGCORE_C_API int
msgcore_mgr_page_registration_pop(MsgMgrHandle hMgr);

// Drive paging explicitly for the dataset rooted at p2pos. With no sink
// installed both are successful no-ops. Return 1 on success.
MSGCORE_C_API int
msgcore_mgr_page_dataset_in(MsgMgrHandle hMgr, unsigned long long p2pos);
MSGCORE_C_API int
msgcore_mgr_page_dataset_out(MsgMgrHandle hMgr, unsigned long long p2pos, int bFlush);

// Recount a paged item after additions/removals; returns the new summary count.
MSGCORE_C_API unsigned int
msgcore_mgr_page_summ(MsgMgrHandle hMgr, MsgFieldHandle hItem,
                      unsigned int nAdditions, unsigned int nRemovals);

// --- the sinks -------------------------------------------------------------
// These are NOT the queued, deferred notification that the trigger sink is.
// The core calls them SYNCHRONOUSLY, on the thread performing the access, and
// WAITS FOR THE ANSWER -- a page-in that has not returned yet is data that is
// not there, so there is no version of this that defers. A sink therefore:
//   * must not block for long, and must not re-enter the manager that called
//     it beyond the subtree it was asked to populate;
//   * returns 1 for "done, the data is resident" and 0 for "could not";
//   * receives the pUser handed to the installer, unchanged.
// The trigger sink's contract is the opposite on every one of these points --
// see msgcore_mgr_set_trigger_sink. Do not model one on the other.
typedef int (*msgcore_pagein_fn)(void* pUser, unsigned long long p2pos);
typedef int (*msgcore_pageout_fn)(void* pUser, unsigned long long p2pos, int bFlush);
typedef int (*msgcore_populate_fn)(void* pUser, unsigned long long p2pos);

// Install (either fn may be NULL) the page-in/page-out pair. Passing NULL for
// both clears the registration and releases the record. Returns 1 on success.
MSGCORE_C_API int
msgcore_mgr_set_paging_sinks(MsgMgrHandle hMgr, msgcore_pagein_fn pfnIn,
                             msgcore_pageout_fn pfnOut, void* pUser);
// Install (NULL clears) the populate sink. Returns 1 on success.
MSGCORE_C_API int
msgcore_mgr_set_populate_sink(MsgMgrHandle hMgr, msgcore_populate_fn pfnPopulate,
                              void* pUser);

// ---------------------------------------------------------------------------
// Utility helpers
// ---------------------------------------------------------------------------

// Global helper: compare wide strings wildcard-style (Msgcore built-in)
MSGCORE_C_API int
msgcore_wildcard_match(const wchar_t* lpszWildcard, const wchar_t* lpszName);

// ===========================================================================
// UTF-8 (_u8) parallel entry points — the recommended cross-platform FFI
// surface. The wchar_t entry points above keep
// each platform's native wide layout (UTF-16 on Windows, UTF-32 on Linux), so
// a wchar_t* string cannot be passed portably through Panama/jextract. These
// _u8 variants take/return UTF-8 (const char*) instead, converting at the
// boundary, and are ABI-identical on both OSes. Only the string-bearing
// functions get a _u8 twin; scalar functions (get_int, is_list, ...) are
// already portable and are called directly.
//
// Returned const char* points at a THREAD-LOCAL buffer valid until the next
// _u8 call that returns a string on the same thread (mirrors the wchar_t
// getters' "valid until next call" contract). Copy it if you need to keep it.
// ===========================================================================

// P2PmsgMgr
MSGCORE_C_API MsgMgrHandle msgcore_mgr_open_file_u8(const char* lpszFilename);
MSGCORE_C_API int          msgcore_mgr_load_u8(MsgMgrHandle hMgr, const char* lpszFilename);
MSGCORE_C_API int          msgcore_mgr_save_u8(MsgMgrHandle hMgr, const char* lpszFilename);
MSGCORE_C_API int          msgcore_mgr_rename_u8(MsgMgrHandle hMgr, const char* lpszNewname);
MSGCORE_C_API const char*  msgcore_mgr_get_filename_u8(MsgMgrHandle hMgr);
MSGCORE_C_API const char*  msgcore_mgr_get_rootname_u8(MsgMgrHandle hMgr);

// P3PmsgField
MSGCORE_C_API MsgFieldHandle msgcore_field_select_item_u8(MsgFieldHandle hField, const char* lpszName);
MSGCORE_C_API int            msgcore_field_exists_u8(MsgFieldHandle hField, const char* lpszName);
MSGCORE_C_API int            msgcore_field_delete_item_u8(MsgFieldHandle hField, const char* lpszName);
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_int_u8(MsgFieldHandle hField, const char* lpszName, int value, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_int64_u8(MsgFieldHandle hField, const char* lpszName, long long value, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_double_u8(MsgFieldHandle hField, const char* lpszName, double value, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_bool_u8(MsgFieldHandle hField, const char* lpszName, int bValue, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_wstr_u8(MsgFieldHandle hField, const char* lpszName, const char* lpszValue, int bUpdate);
MSGCORE_C_API const char*    msgcore_field_get_name_u8(MsgFieldHandle hField);
MSGCORE_C_API const char*    msgcore_field_get_wstr_u8(MsgFieldHandle hField);
MSGCORE_C_API void           msgcore_field_set_wstr_u8(MsgFieldHandle hField, const char* lpszValue);

// P3PmsgList
MSGCORE_C_API void msgcore_list_add_head_wstr_u8(MsgListHandle hList, const char* lpszValue);
MSGCORE_C_API void msgcore_list_add_tail_wstr_u8(MsgListHandle hList, const char* lpszValue);

// P3PmsgVect
MSGCORE_C_API const char* msgcore_vect_get_wstr_u8(MsgVectHandle hVect, int nElem);

// P3PmsgAttr
MSGCORE_C_API int            msgcore_attr_exists_u8(MsgAttrHandle hAttr, const char* lpszName);
MSGCORE_C_API MsgFieldHandle msgcore_attr_select_item_u8(MsgAttrHandle hAttr, const char* lpszName);
MSGCORE_C_API MsgFieldHandle msgcore_attr_declare_int_u8(MsgAttrHandle hAttr, const char* lpszName, int value, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_attr_declare_double_u8(MsgAttrHandle hAttr, const char* lpszName, double value, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_attr_declare_wstr_u8(MsgAttrHandle hAttr, const char* lpszName, const char* lpszValue, int bUpdate);
MSGCORE_C_API int            msgcore_attr_delete_u8(MsgAttrHandle hAttr, const char* lpszName);

// P3PmsgDesc
MSGCORE_C_API int            msgcore_desc_exists_u8(MsgDescHandle hDesc, const char* lpszName);
MSGCORE_C_API MsgFieldHandle msgcore_desc_select_item_u8(MsgDescHandle hDesc, const char* lpszName);
MSGCORE_C_API MsgFieldHandle msgcore_desc_declare_int_u8(MsgDescHandle hDesc, const char* lpszName, int value, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_desc_declare_double_u8(MsgDescHandle hDesc, const char* lpszName, double value, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_desc_declare_wstr_u8(MsgDescHandle hDesc, const char* lpszName, const char* lpszValue, int bUpdate);
MSGCORE_C_API int            msgcore_desc_delete_u8(MsgDescHandle hDesc, const char* lpszName);

// P3PmsgCurs
MSGCORE_C_API int            msgcore_curs_goto_name_u8(MsgCursHandle hCurs, const char* lpszName);
MSGCORE_C_API const char*    msgcore_curs_get_name_u8(MsgCursHandle hCurs);

// FileSystem-layer support
MSGCORE_C_API MsgFieldHandle msgcore_field_child_u8(MsgFieldHandle hField, const char* lpszName);
MSGCORE_C_API const char*    msgcore_mgr_p2pos2path_u8(MsgMgrHandle hMgr, unsigned long long pos);
MSGCORE_C_API int            msgcore_field_rename_child_u8(MsgFieldHandle hField, const char* lpszOldName, const char* lpszNewName);
MSGCORE_C_API int            msgcore_field_move_child_u8(MsgFieldHandle hSrcField, MsgFieldHandle hDstField, const char* lpszName);
MSGCORE_C_API int            msgcore_field_retype_child_int_u8(MsgFieldHandle hField, const char* lpszName, int value);
MSGCORE_C_API int            msgcore_field_retype_child_int64_u8(MsgFieldHandle hField, const char* lpszName, long long value);
MSGCORE_C_API int            msgcore_field_retype_child_double_u8(MsgFieldHandle hField, const char* lpszName, double value);
MSGCORE_C_API int            msgcore_field_retype_child_bool_u8(MsgFieldHandle hField, const char* lpszName, int bValue);
MSGCORE_C_API int            msgcore_field_retype_child_wstr_u8(MsgFieldHandle hField, const char* lpszName, const char* lpszValue);
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_int_typed_u8(MsgFieldHandle hField, const char* lpszName, long long value, unsigned char uDataType, int bUpdate);
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_blob_u8(MsgFieldHandle hField, const char* lpszName, const void* pvData, int nSize, int bUpdate);
MSGCORE_C_API const char*    msgcore_field_get_guid_str_u8(MsgFieldHandle hField);
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_guid_str_u8(MsgFieldHandle hField, const char* lpszName, const char* lpszGuid, int bUpdate);

// Containers II
MSGCORE_C_API MsgListHandle msgcore_field_declare_list_u8(MsgFieldHandle hField, const char* lpszName);
MSGCORE_C_API MsgVectHandle msgcore_field_declare_vect_u8(MsgFieldHandle hField, const char* lpszName, int nElems, unsigned char uDataType);
MSGCORE_C_API MsgListHandle msgcore_attr_declare_list_u8(MsgAttrHandle hAttr, const char* lpszName);
MSGCORE_C_API MsgVectHandle msgcore_attr_declare_vect_u8(MsgAttrHandle hAttr, const char* lpszName, int nElems, unsigned char uDataType);
MSGCORE_C_API MsgListHandle msgcore_field_select_list_u8(MsgFieldHandle hField, const char* lpszName);
MSGCORE_C_API MsgVectHandle msgcore_field_select_vect_u8(MsgFieldHandle hField, const char* lpszName);
MSGCORE_C_API MsgListHandle msgcore_desc_select_list_u8(MsgDescHandle hDesc, const char* lpszName);
MSGCORE_C_API MsgVectHandle msgcore_desc_select_vect_u8(MsgDescHandle hDesc, const char* lpszName);
MSGCORE_C_API MsgListHandle msgcore_attr_select_list_u8(MsgAttrHandle hAttr, const char* lpszName);
MSGCORE_C_API MsgVectHandle msgcore_attr_select_vect_u8(MsgAttrHandle hAttr, const char* lpszName);
MSGCORE_C_API const char*   msgcore_list_get_wstr_at_u8(MsgListHandle hList, int nIndex);
MSGCORE_C_API int           msgcore_list_set_wstr_at_u8(MsgListHandle hList, int nIndex, const char* lpszValue);
MSGCORE_C_API const char*   msgcore_vect_get_name_u8(MsgVectHandle hVect, int nElem);
MSGCORE_C_API int           msgcore_vect_set_wstr_u8(MsgVectHandle hVect, int nElem, const char* lpszValue);

// MsgStck
MSGCORE_C_API int         msgcore_stck_rename_u8(MsgStckHandle hStck, const char* lpszName, int bRecurse);
MSGCORE_C_API const char* msgcore_stck_get_name_u8(MsgStckHandle hStck);

// P2PmsgRecurs
MSGCORE_C_API const char* msgcore_recurs_get_name_u8(MsgRecursHandle hRecurs);

// Utility
MSGCORE_C_API int msgcore_wildcard_match_u8(const char* lpszWildcard, const char* lpszName);

// VBLockData type constants (mirrors VBLockData_* defines)
#define MSGCORE_DATA_NULL     0
#define MSGCORE_DATA_INT08    1
#define MSGCORE_DATA_UINT08   2
#define MSGCORE_DATA_INT16    3
#define MSGCORE_DATA_UINT16   4
#define MSGCORE_DATA_INT32    5
#define MSGCORE_DATA_UINT32   6
#define MSGCORE_DATA_INT64    7
#define MSGCORE_DATA_UINT64   8
#define MSGCORE_DATA_FLOAT    9
#define MSGCORE_DATA_DOUBLE  10
#define MSGCORE_DATA_BOOL    13
#define MSGCORE_DATA_BSTR16  18
#define MSGCORE_DATA_WSTR16  26
#define MSGCORE_DATA_BLOB16  34
#define MSGCORE_DATA_GUID    47

// Addressing mode constants (mirrors VBLock_Addr* defines)
#define MSGCORE_ADDR_16  1
#define MSGCORE_ADDR_32  2
#define MSGCORE_ADDR_64  3

#ifdef __cplusplus
}
#endif
