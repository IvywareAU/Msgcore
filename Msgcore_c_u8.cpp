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
// Msgcore_c_u8.cpp
// UTF-8 (_u8) parallel entry points for the flat C API — the recommended
// cross-platform FFI surface. Each _u8 function
// converts its UTF-8 (const char*) strings to the platform-native wchar_t at
// the boundary and delegates to the matching wchar_t msgcore_* entry point, so
// this file adds zero new object-model logic. Conversion uses
// MultiByteToWideChar / WideCharToMultiByte with CP_UTF8 — the genuine Win32
// API on Windows (wchar_t = UTF-16) and the Platform shim on Linux
// (wchar_t = UTF-32); one code path, correct on both widths incl. astral
// (>U+FFFF) code points.
//
// Add to the Msgcore VS project alongside Msgcore_c.cpp (Msgcore_EXPORTS).
//
#include "stdafx.h"
#include "Msgcore_c.h"

#include <string>
#include <cstring>

// --- boundary conversion ---------------------------------------------------
// U8toW: UTF-8 -> wide, returned by value (lifetime = the call site's temporary).
static std::wstring U8toW(const char* s)
{
    if (!s || !*s) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);   // n includes the NUL
    if (n <= 1) return std::wstring();
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    w.resize((size_t)n - 1);                                       // drop the NUL from length
    return w;
}

// WtoU8: wide -> UTF-8 in a thread-local buffer (valid until the next _u8
// string-returning call on this thread — mirrors the wchar_t getters' contract).
// Returns NULL only when the wide source is NULL.
static const char* WtoU8(const wchar_t* w)
{
    thread_local std::string tls;
    if (!w) return nullptr;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr); // incl NUL
    if (n <= 0) { tls.clear(); return tls.c_str(); }
    tls.resize((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &tls[0], n, nullptr, nullptr);
    tls.resize((size_t)n - 1);                                     // drop NUL from length
    return tls.c_str();
}

extern "C" {

// --- P2PmsgMgr -------------------------------------------------------------
MSGCORE_C_API MsgMgrHandle msgcore_mgr_open_file_u8(const char* f)
{ return msgcore_mgr_open_file(U8toW(f).c_str()); }
MSGCORE_C_API int msgcore_mgr_load_u8(MsgMgrHandle h, const char* f)
{ return msgcore_mgr_load(h, U8toW(f).c_str()); }
MSGCORE_C_API int msgcore_mgr_save_u8(MsgMgrHandle h, const char* f)
{ return msgcore_mgr_save(h, f ? U8toW(f).c_str() : nullptr); }
MSGCORE_C_API int msgcore_mgr_rename_u8(MsgMgrHandle h, const char* n)
{ return msgcore_mgr_rename(h, U8toW(n).c_str()); }
MSGCORE_C_API const char* msgcore_mgr_get_filename_u8(MsgMgrHandle h)
{ return WtoU8(msgcore_mgr_get_filename(h)); }
MSGCORE_C_API const char* msgcore_mgr_get_rootname_u8(MsgMgrHandle h)
{ return WtoU8(msgcore_mgr_get_rootname(h)); }

// --- P3PmsgField -----------------------------------------------------------
MSGCORE_C_API MsgFieldHandle msgcore_field_select_item_u8(MsgFieldHandle h, const char* n)
{ return msgcore_field_select_item(h, U8toW(n).c_str()); }
MSGCORE_C_API int msgcore_field_exists_u8(MsgFieldHandle h, const char* n)
{ return msgcore_field_exists(h, U8toW(n).c_str()); }
MSGCORE_C_API int msgcore_field_delete_item_u8(MsgFieldHandle h, const char* n)
{ return msgcore_field_delete_item(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_int_u8(MsgFieldHandle h, const char* n, int v, int u)
{ return msgcore_field_declare_int(h, U8toW(n).c_str(), v, u); }
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_int64_u8(MsgFieldHandle h, const char* n, long long v, int u)
{ return msgcore_field_declare_int64(h, U8toW(n).c_str(), v, u); }
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_double_u8(MsgFieldHandle h, const char* n, double v, int u)
{ return msgcore_field_declare_double(h, U8toW(n).c_str(), v, u); }
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_bool_u8(MsgFieldHandle h, const char* n, int v, int u)
{ return msgcore_field_declare_bool(h, U8toW(n).c_str(), v, u); }
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_wstr_u8(MsgFieldHandle h, const char* n, const char* val, int u)
{ std::wstring wn = U8toW(n), wv = U8toW(val); return msgcore_field_declare_wstr(h, wn.c_str(), wv.c_str(), u); }
MSGCORE_C_API const char* msgcore_field_get_name_u8(MsgFieldHandle h)
{ return WtoU8(msgcore_field_get_name(h)); }
MSGCORE_C_API const char* msgcore_field_get_wstr_u8(MsgFieldHandle h)
{ return WtoU8(msgcore_field_get_wstr(h)); }
MSGCORE_C_API void msgcore_field_set_wstr_u8(MsgFieldHandle h, const char* val)
{ msgcore_field_set_wstr(h, U8toW(val).c_str()); }

// --- P3PmsgList ------------------------------------------------------------
MSGCORE_C_API void msgcore_list_add_head_wstr_u8(MsgListHandle h, const char* v)
{ msgcore_list_add_head_wstr(h, U8toW(v).c_str()); }
MSGCORE_C_API void msgcore_list_add_tail_wstr_u8(MsgListHandle h, const char* v)
{ msgcore_list_add_tail_wstr(h, U8toW(v).c_str()); }

// --- P3PmsgVect ------------------------------------------------------------
MSGCORE_C_API const char* msgcore_vect_get_wstr_u8(MsgVectHandle h, int e)
{ return WtoU8(msgcore_vect_get_wstr(h, e)); }

// --- P3PmsgAttr ------------------------------------------------------------
MSGCORE_C_API int msgcore_attr_exists_u8(MsgAttrHandle h, const char* n)
{ return msgcore_attr_exists(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgFieldHandle msgcore_attr_select_item_u8(MsgAttrHandle h, const char* n)
{ return msgcore_attr_select_item(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgFieldHandle msgcore_attr_declare_int_u8(MsgAttrHandle h, const char* n, int v, int u)
{ return msgcore_attr_declare_int(h, U8toW(n).c_str(), v, u); }
MSGCORE_C_API MsgFieldHandle msgcore_attr_declare_double_u8(MsgAttrHandle h, const char* n, double v, int u)
{ return msgcore_attr_declare_double(h, U8toW(n).c_str(), v, u); }
MSGCORE_C_API MsgFieldHandle msgcore_attr_declare_wstr_u8(MsgAttrHandle h, const char* n, const char* val, int u)
{ std::wstring wn = U8toW(n), wv = U8toW(val); return msgcore_attr_declare_wstr(h, wn.c_str(), wv.c_str(), u); }
MSGCORE_C_API int msgcore_attr_delete_u8(MsgAttrHandle h, const char* n)
{ return msgcore_attr_delete(h, U8toW(n).c_str()); }

// --- P3PmsgDesc ------------------------------------------------------------
MSGCORE_C_API int msgcore_desc_exists_u8(MsgDescHandle h, const char* n)
{ return msgcore_desc_exists(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgFieldHandle msgcore_desc_select_item_u8(MsgDescHandle h, const char* n)
{ return msgcore_desc_select_item(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgFieldHandle msgcore_desc_declare_int_u8(MsgDescHandle h, const char* n, int v, int u)
{ return msgcore_desc_declare_int(h, U8toW(n).c_str(), v, u); }
MSGCORE_C_API MsgFieldHandle msgcore_desc_declare_double_u8(MsgDescHandle h, const char* n, double v, int u)
{ return msgcore_desc_declare_double(h, U8toW(n).c_str(), v, u); }
MSGCORE_C_API MsgFieldHandle msgcore_desc_declare_wstr_u8(MsgDescHandle h, const char* n, const char* val, int u)
{ std::wstring wn = U8toW(n), wv = U8toW(val); return msgcore_desc_declare_wstr(h, wn.c_str(), wv.c_str(), u); }
MSGCORE_C_API int msgcore_desc_delete_u8(MsgDescHandle h, const char* n)
{ return msgcore_desc_delete(h, U8toW(n).c_str()); }

// --- P3PmsgCurs ------------------------------------------------------------
MSGCORE_C_API int msgcore_curs_goto_name_u8(MsgCursHandle h, const char* n)
{ return msgcore_curs_goto_name(h, U8toW(n).c_str()); }
MSGCORE_C_API const char* msgcore_curs_get_name_u8(MsgCursHandle h)
{ return WtoU8(msgcore_curs_get_name(h)); }

// --- FileSystem-layer support ----------------------------------------------
MSGCORE_C_API MsgFieldHandle msgcore_field_child_u8(MsgFieldHandle h, const char* n)
{ return msgcore_field_child(h, U8toW(n).c_str()); }
MSGCORE_C_API const char* msgcore_mgr_p2pos2path_u8(MsgMgrHandle h, unsigned long long pos)
{ return WtoU8(msgcore_mgr_p2pos2path(h, pos)); }
MSGCORE_C_API int msgcore_field_rename_child_u8(MsgFieldHandle h, const char* oldn, const char* newn)
{ std::wstring wo = U8toW(oldn), wn = U8toW(newn); return msgcore_field_rename_child(h, wo.c_str(), wn.c_str()); }
MSGCORE_C_API int msgcore_field_move_child_u8(MsgFieldHandle s, MsgFieldHandle d, const char* n)
{ return msgcore_field_move_child(s, d, U8toW(n).c_str()); }
MSGCORE_C_API int msgcore_field_retype_child_int_u8(MsgFieldHandle h, const char* n, int v)
{ return msgcore_field_retype_child_int(h, U8toW(n).c_str(), v); }
MSGCORE_C_API int msgcore_field_retype_child_int64_u8(MsgFieldHandle h, const char* n, long long v)
{ return msgcore_field_retype_child_int64(h, U8toW(n).c_str(), v); }
MSGCORE_C_API int msgcore_field_retype_child_double_u8(MsgFieldHandle h, const char* n, double v)
{ return msgcore_field_retype_child_double(h, U8toW(n).c_str(), v); }
MSGCORE_C_API int msgcore_field_retype_child_bool_u8(MsgFieldHandle h, const char* n, int v)
{ return msgcore_field_retype_child_bool(h, U8toW(n).c_str(), v); }
MSGCORE_C_API int msgcore_field_retype_child_wstr_u8(MsgFieldHandle h, const char* n, const char* val)
{ std::wstring wn = U8toW(n), wv = U8toW(val); return msgcore_field_retype_child_wstr(h, wn.c_str(), wv.c_str()); }
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_int_typed_u8(MsgFieldHandle h, const char* n, long long v, unsigned char t, int u)
{ return msgcore_field_declare_int_typed(h, U8toW(n).c_str(), v, t, u); }
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_blob_u8(MsgFieldHandle h, const char* n, const void* p, int sz, int u)
{ return msgcore_field_declare_blob(h, U8toW(n).c_str(), p, sz, u); }
MSGCORE_C_API const char* msgcore_field_get_guid_str_u8(MsgFieldHandle h)
{ return WtoU8(msgcore_field_get_guid_str(h)); }
MSGCORE_C_API MsgFieldHandle msgcore_field_declare_guid_str_u8(MsgFieldHandle h, const char* n, const char* g, int u)
{ std::wstring wn = U8toW(n), wg = U8toW(g); return msgcore_field_declare_guid_str(h, wn.c_str(), wg.c_str(), u); }

// --- Containers II ---------------------------------------------------------
MSGCORE_C_API MsgListHandle msgcore_field_declare_list_u8(MsgFieldHandle h, const char* n)
{ return msgcore_field_declare_list(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgVectHandle msgcore_field_declare_vect_u8(MsgFieldHandle h, const char* n, int e, unsigned char t)
{ return msgcore_field_declare_vect(h, U8toW(n).c_str(), e, t); }
MSGCORE_C_API MsgListHandle msgcore_attr_declare_list_u8(MsgAttrHandle h, const char* n)
{ return msgcore_attr_declare_list(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgVectHandle msgcore_attr_declare_vect_u8(MsgAttrHandle h, const char* n, int e, unsigned char t)
{ return msgcore_attr_declare_vect(h, U8toW(n).c_str(), e, t); }
MSGCORE_C_API MsgListHandle msgcore_field_select_list_u8(MsgFieldHandle h, const char* n)
{ return msgcore_field_select_list(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgVectHandle msgcore_field_select_vect_u8(MsgFieldHandle h, const char* n)
{ return msgcore_field_select_vect(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgListHandle msgcore_desc_select_list_u8(MsgDescHandle h, const char* n)
{ return msgcore_desc_select_list(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgVectHandle msgcore_desc_select_vect_u8(MsgDescHandle h, const char* n)
{ return msgcore_desc_select_vect(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgListHandle msgcore_attr_select_list_u8(MsgAttrHandle h, const char* n)
{ return msgcore_attr_select_list(h, U8toW(n).c_str()); }
MSGCORE_C_API MsgVectHandle msgcore_attr_select_vect_u8(MsgAttrHandle h, const char* n)
{ return msgcore_attr_select_vect(h, U8toW(n).c_str()); }
MSGCORE_C_API const char* msgcore_list_get_wstr_at_u8(MsgListHandle h, int i)
{ return WtoU8(msgcore_list_get_wstr_at(h, i)); }
MSGCORE_C_API int msgcore_list_set_wstr_at_u8(MsgListHandle h, int i, const char* v)
{ return msgcore_list_set_wstr_at(h, i, U8toW(v).c_str()); }
MSGCORE_C_API const char* msgcore_vect_get_name_u8(MsgVectHandle h, int e)
{ return WtoU8(msgcore_vect_get_name(h, e)); }
MSGCORE_C_API int msgcore_vect_set_wstr_u8(MsgVectHandle h, int e, const char* v)
{ return msgcore_vect_set_wstr(h, e, U8toW(v).c_str()); }

// --- MsgStck ---------------------------------------------------------------
MSGCORE_C_API int msgcore_stck_rename_u8(MsgStckHandle h, const char* n, int r)
{ return msgcore_stck_rename(h, U8toW(n).c_str(), r); }
MSGCORE_C_API const char* msgcore_stck_get_name_u8(MsgStckHandle h)
{ return WtoU8(msgcore_stck_get_name(h)); }

// --- P2PmsgRecurs ----------------------------------------------------------
MSGCORE_C_API const char* msgcore_recurs_get_name_u8(MsgRecursHandle h)
{ return WtoU8(msgcore_recurs_get_name(h)); }

// --- Utility ---------------------------------------------------------------
MSGCORE_C_API int msgcore_wildcard_match_u8(const char* wild, const char* name)
{ std::wstring ww = U8toW(wild), wn = U8toW(name); return msgcore_wildcard_match(ww.c_str(), wn.c_str()); }

} // extern "C"
