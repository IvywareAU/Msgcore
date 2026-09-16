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
//  Platform layer — strings: CString subset, TCHAR/_T, wide helpers.
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §4.2).
//
//  Decision (§4.2): the in-memory API surface keeps wchar_t. On _WIN32 this is the
//  real MFC/ATL CString and the real <tchar.h>. On Linux, p2pstr provides a CString
//  *subset* over std::wstring and a TCHAR/_T mapping to wchar_t. Serialized/wire data
//  is a SEPARATE domain pinned to 16-bit char16_t (see p2ptypes/P2PWCHAR below) and is
//  audited in Phase 1 — do NOT use CString/wchar_t for stored layouts.
//
//  Phase status: Windows side complete (pass-through). Linux CString subset is a
//  Phase-1 deliverable; this header currently declares the mapping and the intended
//  member surface as a scaffold.
//
#pragma once
#include "p2ptypes.h"

#if defined(_WIN32)
  #include <tchar.h>
  #include <atlstr.h>     // CString (shared ATL/MFC)
  #include <string>       // p2p_wkey fallback
  #include <cwchar>       // p2p_wkey: wcslen
  #include <cstring>      // p2p_wkey: memcpy
  // TCHAR, _T(), LPTSTR, LPCTSTR, CString all come from the real headers.

  //  Serialized-data element type. On Windows wchar_t is already 16-bit, so the
  //  "pin to 16-bit" contract is a no-op here and stored layouts are unchanged.
  using P2PWCHAR = wchar_t;

#else
  #include <string>
  #include <cwchar>
  #include <cwctype>
  #include <cstdarg>
  #include <cstdlib>
  #include <algorithm>

  //  In-memory wide char stays wchar_t (4 bytes on Linux) — API surface only.
  using TCHAR   = wchar_t;
  using WCHAR   = wchar_t;
  using LPTSTR  = wchar_t*;
  using LPCTSTR = const wchar_t*;
  using LPWSTR  = wchar_t*;
  using LPCWSTR = const wchar_t*;
  using LPSTR   = char*;
  using LPCSTR  = const char*;
  using PTSTR   = wchar_t*;
  using PCTSTR  = const wchar_t*;
  using PWSTR   = wchar_t*;
  using PCWSTR  = const wchar_t*;

  #define _T(x)  L##x
  #define _TEXT(x) L##x
  #define TEXT(x) L##x

  //  Serialized/wire element type is PINNED to 16-bit, independent of wchar_t width.
  //  Conversion wchar_t <-> char16_t happens at the BSTR-io boundary (Phase 1).
  using P2PWCHAR = char16_t;

  // -------------------------------------------------------------------------
  //  TCHAR string functions (Unicode build => _tcs* map to the wide CRT).
  //  Only the forms the compiled set names are mapped; extend as usage surfaces.
  //  _stprintf_s / swprintf_s take (buf, count, fmt, ...) — glibc swprintf matches.
  //  Case-insensitive compares use the glibc wide extensions (wcscasecmp family).
  // -------------------------------------------------------------------------
  #define _tcslen    wcslen
  #define _tcscpy    wcscpy
  #define _tcsncpy   wcsncpy
  #define _tcscat    wcscat
  #define _tcschr    wcschr
  #define _tcsrchr   wcsrchr
  #define _tcsstr    wcsstr
  #define _tcscmp    wcscmp
  #define _tcsncmp   wcsncmp
  #define _tcsicmp   wcscasecmp
  #define _tcsnicmp  wcsncasecmp
  #define _tcstoul   wcstoul
  #define _tcstol    wcstol
  #define _tprintf   wprintf
  #define _tcscpy_s  wcscpy_s
  //  MSVC vs glibc WIDE printf %s/%c semantics differ. In a wide format string MSVC
  //  treats %s/%c as WIDE (wchar_t*/wchar_t) and %hs/%hc as narrow; glibc treats %s/%c
  //  as NARROW (char*/int) and %ls/%lc as wide. The legacy code follows the MSVC rule,
  //  so on Linux a wide %s reads a wchar_t* as char* and prints only the first byte
  //  (this silently corrupted every Format()/Message() and, e.g., the atomic-Save temp
  //  path). Rewrite the format to glibc conventions before formatting: %s->%ls, %c->%lc,
  //  %ws/%wc->%ls/%lc, %hs/%hc->%s/%c; numeric/other specifiers pass through unchanged.
  inline std::wstring p2p_fix_wformat(const wchar_t* fmt) {
      std::wstring out;
      if (!fmt) return out;
      for (const wchar_t* p = fmt; *p; ++p) {
          if (*p != L'%') { out.push_back(*p); continue; }
          std::wstring spec(1, L'%'); ++p;
          if (*p == L'%') { out += L"%%"; continue; }
          while (*p && wcschr(L"-+ #0123456789.*", *p)) { spec.push_back(*p); ++p; }
          int wide = 0;   // 1 = force wide (l), -1 = force narrow, 0 = MSVC default (wide)
          while (*p && wcschr(L"hlLjztw", *p)) {
              if      (*p == L'w')                 wide = 1;              // drop 'w'
              else if (*p == L'l' || *p == L'L') { wide = 1; spec.push_back(*p); }
              else if (*p == L'h')               { wide = -1; spec.push_back(*p); }
              else                                 spec.push_back(*p);
              ++p;
          }
          if (!*p) { out += spec; break; }
          wchar_t conv = *p;
          if (conv == L's' || conv == L'c') {
              if (wide == -1) { auto h = spec.rfind(L'h'); if (h != std::wstring::npos) spec.erase(h, 1); }
              else if (spec.find(L'l') == std::wstring::npos) spec.push_back(L'l');
          }
          spec.push_back(conv);
          out += spec;
      }
      return out;
  }
  inline int p2p_vswprintf(wchar_t* buf, size_t n, const wchar_t* fmt, va_list ap) {
      std::wstring f = p2p_fix_wformat(fmt);
      return ::vswprintf(buf, n, f.c_str(), ap);
  }
  inline int p2p_swprintf(wchar_t* buf, size_t n, const wchar_t* fmt, ...) {
      std::wstring f = p2p_fix_wformat(fmt);
      va_list ap; va_start(ap, fmt);
      int r = ::vswprintf(buf, n, f.c_str(), ap);
      va_end(ap);
      return r;
  }
  #define _stprintf_s  p2p_swprintf
  #define _sntprintf_s p2p_swprintf
  #define swprintf_s   p2p_swprintf
  #define _vstprintf_s p2p_vswprintf
  #define vswprintf_s  p2p_vswprintf
  //  sprintf_s exists in TWO forms on MSVC and the compiled set uses both:
  //      sprintf_s(buf, size, fmt, ...)   explicit count
  //      sprintf_s(buf[N], fmt, ...)      template, count deduced from the array
  //  `#define sprintf_s snprintf` models only the first. Under the macro the
  //  ARRAY form binds the format string to snprintf's size_t parameter and
  //  promotes the first vararg to be the format - and because these TUs build
  //  -fpermissive, that is a warning rather than an error. It compiles, and
  //  then writes a completely different string.
  //
  //  This was not theoretical: TargetcoreSuite's IdTempPath produced "/tmp/"
  //  instead of "/tmp/p2pid_suite_id_<pid>.tmp", so every identity-store case
  //  was handed a DIRECTORY as its file path - 18 failed checks across 7 cases,
  //  all of which read as "the identity store is broken on Linux" and none of
  //  which were. Overloads, not macros, so the array form binds correctly and a
  //  genuinely wrong call fails to compile instead of misbehaving.
  inline int sprintf_s(char* d, size_t n, const char* fmt, ...) {
      va_list ap; va_start(ap, fmt);
      int r = ::vsnprintf(d, n, fmt, ap); va_end(ap); return r; }
  template <size_t N> inline int sprintf_s(char (&d)[N], const char* fmt, ...) {
      va_list ap; va_start(ap, fmt);
      int r = ::vsnprintf(d, N, fmt, ap); va_end(ap); return r; }

  inline int vsprintf_s(char* d, size_t n, const char* fmt, va_list ap)
  { return ::vsnprintf(d, n, fmt, ap); }
  template <size_t N> inline int vsprintf_s(char (&d)[N], const char* fmt, va_list ap)
  { return ::vsnprintf(d, N, fmt, ap); }

  //  MSVC _snprintf_s(buf, bufsize, count, fmt, ...) — the extra `count` arg (usually
  //  _TRUNCATE) bounds writes to count chars; the shim honours only the buffer size,
  //  which is the safe subset the logging TU relies on. _TRUNCATE => "as much as fits".
  #ifndef _TRUNCATE
    #define _TRUNCATE ((size_t)-1)
  #endif
  inline int p2p_snprintf_s(char* buf, size_t bufsize, size_t /*count*/, const char* fmt, ...) {
      va_list ap; va_start(ap, fmt);
      int r = ::vsnprintf(buf, bufsize, fmt, ap);
      va_end(ap);
      return r;
  }
  #define _snprintf_s p2p_snprintf_s

  //  _tsplitpath_s (CRT secure path split, wide in the Unicode build). No drive on Linux
  //  (drive=""); dir = up to & incl the last '/'; ext = last '.' in the filename onward.
  //  Any out buffer may be null. Returns errno_t 0.
  #ifndef _MAX_DRIVE
    #define _MAX_DRIVE 3
    #define _MAX_DIR   256
    #define _MAX_FNAME 256
    #define _MAX_EXT   256
  #endif
  inline int p2p_wsplitpath(const wchar_t* path,
                            wchar_t* drv, size_t drvsz, wchar_t* dir, size_t dirsz,
                            wchar_t* fn, size_t fnsz, wchar_t* ext, size_t extsz) {
      auto put = [](wchar_t* dst, size_t sz, const wchar_t* b, const wchar_t* e) {
          if (!dst || sz == 0) return;
          size_t n = 0;
          for (const wchar_t* p = b; p < e && n + 1 < sz; ++p) dst[n++] = *p;
          dst[n] = 0;
      };
      if (drv && drvsz) drv[0] = 0;
      const wchar_t* p = path ? path : L"";
      const wchar_t* end = p; while (*end) ++end;
      const wchar_t* slash = p;
      for (const wchar_t* q = p; q < end; ++q) if (*q == L'/' || *q == L'\\') slash = q + 1;
      put(dir, dirsz, p, slash);
      const wchar_t* dot = end;
      for (const wchar_t* q = slash; q < end; ++q) if (*q == L'.') dot = q;
      put(fn,  fnsz,  slash, dot);
      put(ext, extsz, dot,   end);
      return 0;
  }
  //  Array form (MSVC's size-deducing secure overload): _tsplitpath_s(path, drv, dir, fn, ext).
  template <size_t D, size_t DI, size_t F, size_t E>
  inline int p2p_wsplitpath(const wchar_t* path, wchar_t (&drv)[D], wchar_t (&dir)[DI],
                            wchar_t (&fn)[F], wchar_t (&ext)[E]) {
      return p2p_wsplitpath(path, drv, D, dir, DI, fn, F, ext, E);
  }
  #define _wsplitpath_s p2p_wsplitpath
  #define _tsplitpath_s p2p_wsplitpath

  //  MSVC "secure" string copies (count-bounded). Map to bounded copies + NUL.
  inline void wcscpy_s(wchar_t* d, size_t n, const wchar_t* s) {
      if (!d || !n) return; std::wcsncpy(d, s ? s : L"", n); d[n - 1] = 0; }
  inline void strcpy_s(char* d, size_t n, const char* s) {
      if (!d || !n) return; std::strncpy(d, s ? s : "", n); d[n - 1] = 0; }

  //  The two-argument array forms MSVC also provides. Callers use both spellings
  //  interchangeably, and an array's bound IS the count, so this is the same
  //  function with the size deduced rather than a weaker overload.
  template <size_t N> inline void strcpy_s(char (&d)[N], const char* s)
  { strcpy_s(d, N, s); }
  template <size_t N> inline void wcscpy_s(wchar_t (&d)[N], const wchar_t* s)
  { wcscpy_s(d, N, s); }

  //  MSVC CRT non-_t case-insensitive compares named directly by the compiled set.
  #define _wcsicmp   wcscasecmp
  #define _wcsnicmp  wcsncasecmp
  #define _stricmp   strcasecmp
  #define _strnicmp  strncasecmp
  #define _wcsdup    wcsdup

  // -------------------------------------------------------------------------
  //  ATL string-conversion macros (USES_CONVERSION / W2A / A2W / T2A / T2W).
  //  Live use is confined to a couple of diagnostic sites. The wide<->narrow
  //  helpers return a thread_local buffer (valid until the next call on the same
  //  thread) — adequate for the "format then log" pattern that names them.
  // -------------------------------------------------------------------------
  #define USES_CONVERSION ((void)0)
  inline const char* p2p_w2a(const wchar_t* w) {
      static thread_local std::string s;
      s.clear();
      if (w) for (; *w; ++w) s.push_back((char)(unsigned)(*w & 0xFF));
      return s.c_str();
  }
  inline const wchar_t* p2p_a2w(const char* a) {
      static thread_local std::wstring s;
      s.clear();
      if (a) for (; *a; ++a) s.push_back((wchar_t)(unsigned char)*a);
      return s.c_str();
  }
  #define W2A(x) p2p_w2a(x)
  #define A2W(x) p2p_a2w(x)
  #define T2A(x) p2p_w2a(x)
  #define T2W(x) (x)
  #define W2T(x) (x)
  #define A2T(x) p2p_a2w(x)

  //  Msgexception.h builds a wide function-name via `_T(__FUNCTION__)`, i.e. the
  //  token paste `L##__FUNCTION__` => `L__FUNCTION__`. MSVC recognises that token;
  //  GCC does not (its __FUNCTION__ is a runtime variable, not a literal). Because a
  //  ##-pasted token is rescanned for macros, defining L__FUNCTION__ here intercepts
  //  it and yields the wide function name at runtime — no edit to the legacy header.
  #define L__FUNCTION__ (::p2p_a2w(__func__))

  //  WideCharToMultiByte (the subset the code + tests name): CP_UTF8/CP_ACP wide->narrow.
  //  cchWideChar < 0 means the source is NUL-terminated and the terminator is encoded too
  //  (Win32 contract). cbMultiByte == 0 queries the required byte count. wchar_t is UTF-32
  //  on Linux, so each element encodes directly as 1-4 UTF-8 bytes. lpUsedDefaultChar/
  //  lpDefaultChar are accepted for signature parity; no lossy narrowing happens for UTF-8.
  #ifndef CP_ACP
    #define CP_ACP  0
  #endif
  #ifndef CP_UTF8
    #define CP_UTF8 65001u
  #endif
  #ifndef ERROR_INSUFFICIENT_BUFFER
    #define ERROR_INSUFFICIENT_BUFFER 122
  #endif
  inline int WideCharToMultiByte(unsigned /*CodePage*/, unsigned long /*flags*/,
                                 const wchar_t* lpWide, int cchWide,
                                 char* lpMB, int cbMB,
                                 const char* /*lpDefaultChar*/, int* lpUsedDefaultChar) {
      if (lpUsedDefaultChar) *lpUsedDefaultChar = 0;
      bool   nulTerm = (cchWide < 0);
      size_t n = nulTerm ? (lpWide ? std::wcslen(lpWide) : 0) : (size_t)cchWide;
      std::string s;
      for (size_t i = 0; i < n; ++i) {
          char32_t c = (char32_t)lpWide[i];
          if      (c < 0x80)     s.push_back((char)c);
          else if (c < 0x800)  { s.push_back((char)(0xC0 | (c >> 6)));
                                 s.push_back((char)(0x80 | (c & 0x3F))); }
          else if (c < 0x10000){ s.push_back((char)(0xE0 | (c >> 12)));
                                 s.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
                                 s.push_back((char)(0x80 | (c & 0x3F))); }
          else                 { s.push_back((char)(0xF0 | (c >> 18)));
                                 s.push_back((char)(0x80 | ((c >> 12) & 0x3F)));
                                 s.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
                                 s.push_back((char)(0x80 | (c & 0x3F))); }
      }
      if (nulTerm) s.push_back('\0');
      if (cbMB == 0) return (int)s.size();               // size query
      if ((int)s.size() > cbMB) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
      if (lpMB) s.copy(lpMB, s.size());   // std::string::copy — no <cstring> dependency here
      return (int)s.size();
  }

  //  MultiByteToWideChar (CP_UTF8 subset): narrow UTF-8 -> wide. wchar_t is UTF-32 on
  //  Linux, so each decoded code point is one element (no surrogate pairs). cbMB < 0 means
  //  the source is NUL-terminated and the terminator is decoded too (Win32 contract).
  //  cchWide == 0 queries the required element count. Malformed sequences -> U+FFFD.
  inline int MultiByteToWideChar(unsigned /*CodePage*/, unsigned long /*flags*/,
                                 const char* lpMB, int cbMB,
                                 wchar_t* lpWide, int cchWide) {
      bool   nulTerm = (cbMB < 0);
      size_t n = nulTerm ? (lpMB ? std::strlen(lpMB) : 0) : (size_t)cbMB;
      std::wstring w;
      const unsigned char* p = (const unsigned char*)lpMB;
      for (size_t i = 0; i < n; ) {
          unsigned char c = p[i];
          char32_t cp; size_t len;
          if      (c < 0x80)          { cp = c;          len = 1; }
          else if ((c & 0xE0) == 0xC0){ cp = c & 0x1F;   len = 2; }
          else if ((c & 0xF0) == 0xE0){ cp = c & 0x0F;   len = 3; }
          else if ((c & 0xF8) == 0xF0){ cp = c & 0x07;   len = 4; }
          else                        { cp = 0xFFFD;     len = 1; }
          if (i + len > n)            { cp = 0xFFFD;     len = n - i; }
          else for (size_t k = 1; k < len; ++k) {
              if ((p[i + k] & 0xC0) != 0x80) { cp = 0xFFFD; len = 1; break; }
              cp = (cp << 6) | (p[i + k] & 0x3F);
          }
          w.push_back((wchar_t)cp);
          i += len;
      }
      if (nulTerm) w.push_back(L'\0');
      if (cchWide == 0) return (int)w.size();             // size query
      if ((int)w.size() > cchWide) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
      if (lpWide) for (size_t i = 0; i < w.size(); ++i) lpWide[i] = w[i];
      return (int)w.size();
  }

  // -------------------------------------------------------------------------
  //  CString subset over std::wstring (the Linux port plan §4.2 — in-memory only).
  //  Member set inventoried from the compiled TUs; anything outside it is a
  //  deliberate compile error (§4 "subset only" rule). Serialized/wire layouts
  //  must NOT use this type — they are pinned to P2PWCHAR (char16_t).
  //
  //  Mixed narrow/wide literals appear at call sites (e.g. Format(_T("..")) vs
  //  Format("..")); narrow overloads widen char->wchar_t so both compile as they
  //  do against ATL's CStringW.
  // -------------------------------------------------------------------------
  class CString {
      std::wstring m_s;

      static std::wstring widen(const char* p) {
          std::wstring w;
          if (p) { while (*p) w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p++))); }
          return w;
      }

  public:
      CString() = default;
      CString(const CString&) = default;
      CString(CString&&) noexcept = default;
      CString& operator=(const CString&) = default;
      CString& operator=(CString&&) noexcept = default;

      CString(const wchar_t* p)          : m_s(p ? p : L"") {}
      CString(const wchar_t* p, int n)   : m_s(p ? p : L"", n < 0 ? 0 : (size_t)n) {}
      CString(wchar_t c, int n = 1)      : m_s(n < 0 ? 0 : (size_t)n, c) {}
      CString(const char* p)             : m_s(widen(p)) {}
      CString(const std::wstring& s)     : m_s(s) {}

      CString& operator=(const wchar_t* p) { m_s = p ? p : L""; return *this; }
      CString& operator=(const char* p)    { m_s = widen(p);    return *this; }
      CString& operator=(wchar_t c)        { m_s.assign(1, c);  return *this; }

      int  GetLength() const  { return static_cast<int>(m_s.size()); }
      bool IsEmpty()   const  { return m_s.empty(); }
      void Empty()            { m_s.clear(); }
      const wchar_t* GetString() const { return m_s.c_str(); }
      operator const wchar_t*() const  { return m_s.c_str(); }   // LPCTSTR cast

      wchar_t GetAt(int i) const   { return m_s[(size_t)i]; }
      wchar_t operator[](int i) const { return m_s[(size_t)i]; }

      CString& MakeLower() {
          std::transform(m_s.begin(), m_s.end(), m_s.begin(),
                         [](wchar_t c){ return (wchar_t)towlower((wint_t)c); });
          return *this;
      }
      CString& MakeReverse() { std::reverse(m_s.begin(), m_s.end()); return *this; }
      CString& MakeUpper() {
          std::transform(m_s.begin(), m_s.end(), m_s.begin(),
                         [](wchar_t c){ return (wchar_t)towupper((wint_t)c); });
          return *this;
      }

      CString Right(int n) const {
          if (n < 0) n = 0;
          if ((size_t)n >= m_s.size()) return *this;
          return CString(m_s.substr(m_s.size() - (size_t)n));
      }
      CString Left(int n) const {
          if (n < 0) n = 0;
          return CString(m_s.substr(0, (size_t)n));
      }
      CString Mid(int start, int n = -1) const {
          if (start < 0) start = 0;
          if ((size_t)start >= m_s.size()) return CString();
          return CString(m_s.substr((size_t)start, n < 0 ? std::wstring::npos : (size_t)n));
      }

      //  Returns the 0-based index of the first match, or -1 (ATL semantics).
      int Find(const wchar_t* sub) const {
          auto p = m_s.find(sub ? sub : L"");
          return p == std::wstring::npos ? -1 : (int)p;
      }
      int Find(wchar_t c) const {
          auto p = m_s.find(c);
          return p == std::wstring::npos ? -1 : (int)p;
      }
      int Find(const char* sub) const { return Find(widen(sub).c_str()); }

      //  MFC Tokenize(delims, iStart): return the next token (any char in `delims`
      //  separates), advancing iStart past the token+delimiter; skip leading delimiters;
      //  return empty + set iStart=-1 when exhausted. Callers loop until IsEmpty().
      CString Tokenize(const wchar_t* delims, int& iStart) const {
          if (iStart < 0) return CString();
          const std::wstring d = delims ? delims : L"";
          const std::size_t n = m_s.size();
          std::size_t i = (std::size_t)iStart;
          while (i < n && d.find(m_s[i]) != std::wstring::npos) ++i;   // skip leading delims
          if (i >= n) { iStart = -1; return CString(); }
          std::size_t start = i;
          while (i < n && d.find(m_s[i]) == std::wstring::npos) ++i;   // collect token
          CString tok(m_s.substr(start, i - start));
          iStart = (i < n) ? (int)(i + 1) : (int)n;                    // past the delimiter
          return tok;
      }

      int Compare(const wchar_t* s) const       { return m_s.compare(s ? s : L""); }
      int Compare(const char* s) const          { return Compare(widen(s).c_str()); }
      int CompareNoCase(const wchar_t* s) const  { return wcscasecmp(m_s.c_str(), s ? s : L""); }
      int CompareNoCase(const char* s) const     { return CompareNoCase(widen(s).c_str()); }

      BSTR AllocSysString() const {
          //  ATL AllocSysString returns an OLE-owned copy. The shim allocates a plain
          //  wchar_t buffer (OLECHAR == wchar_t here); the BSTR-io boundary (Phase 1)
          //  owns freeing. Length-prefix is not modelled — no live caller reads it.
          size_t n = m_s.size();
          auto* b = new wchar_t[n + 1];
          wmemcpy(b, m_s.c_str(), n + 1);
          return b;
      }

      CString& Format(const wchar_t* fmt, ...) {
          std::wstring wf = p2p_fix_wformat(fmt);   // MSVC %s/%c -> glibc %ls/%lc
          const wchar_t* f = wf.c_str();
          va_list ap; va_start(ap, fmt);
          va_list ap2; va_copy(ap2, ap);
          int need = ::vswprintf(nullptr, 0, f, ap);   // size probe
          if (need < 0) {                               // glibc: probe may fail -> grow
              size_t cap = 256;
              std::wstring buf;
              for (;;) {
                  buf.resize(cap);
                  va_list apn; va_copy(apn, ap2);
                  int w = ::vswprintf(&buf[0], cap, f, apn);
                  va_end(apn);
                  if (w >= 0) { buf.resize((size_t)w); m_s = buf; break; }
                  cap *= 2;
                  if (cap > (1u << 20)) { buf.clear(); m_s = buf; break; }
              }
          } else {
              std::wstring buf((size_t)need + 1, L'\0');
              ::vswprintf(&buf[0], buf.size(), f, ap2);
              buf.resize((size_t)need);
              m_s = buf;
          }
          va_end(ap2); va_end(ap);
          return *this;
      }

      CString& operator+=(const CString& o)  { m_s += o.m_s; return *this; }
      CString& operator+=(const wchar_t* p)  { if (p) m_s += p; return *this; }
      CString& operator+=(const char* p)     { m_s += widen(p); return *this; }
      CString& operator+=(wchar_t c)         { m_s += c; return *this; }

      friend CString operator+(const CString& a, const CString& b)  { CString r(a); r += b; return r; }
      friend CString operator+(const CString& a, const wchar_t* b)  { CString r(a); r += b; return r; }
      friend CString operator+(const wchar_t* a, const CString& b)  { CString r(a); r += b; return r; }

      bool operator==(const wchar_t* s) const { return m_s == (s ? s : L""); }
      bool operator==(const CString& o) const { return m_s == o.m_s; }
      bool operator!=(const wchar_t* s) const { return !(*this == s); }
      bool operator!=(const CString& o) const { return !(*this == o); }
  };

  //  Narrow CString (ATL CStringA). Live use is `(LPCSTR)CStringA(wideStr)` to hand a
  //  narrow buffer to fprintf. Wide->narrow truncates to the low byte (diagnostic path).
  class CStringA {
      std::string m_s;
  public:
      CStringA() = default;
      CStringA(const char* p) : m_s(p ? p : "") {}
      CStringA(const wchar_t* p) { if (p) for (; *p; ++p) m_s.push_back((char)(unsigned)(*p & 0xFF)); }
      CStringA(const CString& w) { const wchar_t* p = w.GetString(); if (p) for (; *p; ++p) m_s.push_back((char)(unsigned)(*p & 0xFF)); }
      operator const char*() const { return m_s.c_str(); }
      const char* GetString() const { return m_s.c_str(); }
      int  GetLength() const { return (int)m_s.size(); }
      bool IsEmpty()   const { return m_s.empty(); }
  };

  //  Some sites qualify the string classes as ATL::CString / ATL::CStringA, and use
  //  ATL::CA2WEX(narrow).m_szBuffer to widen a const char* to a wchar_t buffer.
  namespace ATL {
      using ::CString;
      using ::CStringA;
      class CA2WEX {
          std::wstring m_s;
      public:
          const wchar_t* m_szBuffer;
          CA2WEX(const char* psz) {
              if (psz) for (; *psz; ++psz) m_s.push_back((wchar_t)(unsigned char)*psz);
              m_szBuffer = m_s.c_str();
          }
          operator const wchar_t*() const { return m_s.c_str(); }
      };
  }

#endif // _WIN32

// ---------------------------------------------------------------------------
//  UTF-16 pinning boundary helpers (the Linux port plan §4.2 / the Linux UTF-16 audit).
//  Move wide string data between the in-memory wchar_t API and the 16-bit P2PWCHAR
//  *stored* representation. On Windows P2PWCHAR == wchar_t, so store/load are plain
//  element copies == the previous memcpy (byte-identical). On Linux they narrow
//  (wchar_t 4B -> char16_t 2B) on store and widen on load, per element.
//  These live outside the _WIN32 split; P2PWCHAR is defined in both branches above.
// ---------------------------------------------------------------------------
#include <string>
#include <cstring>
//  The stored 16-bit units live inside #pragma pack(1) heap images and can therefore
//  land at ODD byte offsets. A typed `char16_t` load/store assumes 2-byte alignment —
//  that is UB and it FAULTS on strict-alignment targets (aarch64 is a stated goal); on
//  x86 it is merely tolerated. So every unit is moved byte-wise via memcpy. On Windows
//  P2PWCHAR == wchar_t (2 bytes) and this is byte-identical to the previous element copy,
//  so the serialized image is unchanged there.
//  Astral (> U+FFFF) code points: on Linux wchar_t is 32-bit UTF-32, so a single
//  wchar_t may need a UTF-16 SURROGATE PAIR (two P2PWCHAR units) in the store. On
//  Windows wchar_t IS UTF-16 (astral chars already arrive as two wchar_t units, and
//  wcslen counts them as two), so the conversion is a 1:1 element copy == byte-identical
//  to the previous memcpy. Because one astral code point stores as two units, the STORED
//  length (in P2PWCHAR units) can exceed the wchar_t element count; callers that pre-size
//  a store from the source length must first ask p2p_wide_units() for the real unit count.
//
//  p2p_wide_units: number of P2PWCHAR units required to store the first n wchar_t elements
//  of src. On Windows / for pure-BMP input this is exactly n (byte-identical behaviour).
inline size_t p2p_wide_units(const wchar_t* src, size_t n) {
#if defined(_WIN32)
    (void)src; return n;
#else
    size_t m = 0;
    for (size_t i = 0; i < n; ++i)
        m += ((unsigned)src[i] > 0xFFFFu) ? 2 : 1;
    return m;
#endif
}
//  p2p_store_wide: encode n wchar_t elements of src into dst as 16-bit P2PWCHAR units,
//  emitting a surrogate pair for each astral code point (Linux). Returns the number of
//  units written (== n on Windows / BMP; up to 2n on Linux). Byte-wise via memcpy because
//  dst can land at an odd offset inside a #pragma pack(1) image.
inline size_t p2p_store_wide(P2PWCHAR* dst, const wchar_t* src, size_t n) {
    unsigned char* d = reinterpret_cast<unsigned char*>(dst);
    size_t m = 0;
#if defined(_WIN32)
    for (size_t i = 0; i < n; ++i) {
        P2PWCHAR u = (P2PWCHAR)src[i];
        std::memcpy(d + m * sizeof(P2PWCHAR), &u, sizeof u); ++m;
    }
#else
    for (size_t i = 0; i < n; ++i) {
        unsigned cp = (unsigned)src[i];
        if (cp > 0xFFFFu && cp <= 0x10FFFFu) {
            cp -= 0x10000u;
            P2PWCHAR hi = (P2PWCHAR)(0xD800u + (cp >> 10));
            P2PWCHAR lo = (P2PWCHAR)(0xDC00u + (cp & 0x3FFu));
            std::memcpy(d + m * sizeof(P2PWCHAR), &hi, sizeof hi); ++m;
            std::memcpy(d + m * sizeof(P2PWCHAR), &lo, sizeof lo); ++m;
        } else {
            P2PWCHAR u = (P2PWCHAR)(cp > 0x10FFFFu ? 0xFFFDu : cp);   // U+FFFD for out-of-range
            std::memcpy(d + m * sizeof(P2PWCHAR), &u, sizeof u); ++m;
        }
    }
#endif
    return m;
}
//  p2p_load_wide: decode m P2PWCHAR units of src into dst as wchar_t, combining surrogate
//  pairs into one code point (Linux). Returns the number of wchar_t elements written
//  (== m on Windows / BMP; fewer when pairs are combined). dst must hold >= m elements.
inline size_t p2p_load_wide(wchar_t* dst, const P2PWCHAR* src, size_t m) {
    const unsigned char* s = reinterpret_cast<const unsigned char*>(src);
    size_t o = 0;
#if defined(_WIN32)
    for (size_t i = 0; i < m; ++i) {
        P2PWCHAR u; std::memcpy(&u, s + i * sizeof(P2PWCHAR), sizeof u);
        dst[o++] = (wchar_t)u;
    }
#else
    for (size_t i = 0; i < m; ++i) {
        P2PWCHAR u; std::memcpy(&u, s + i * sizeof(P2PWCHAR), sizeof u);
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < m) {
            P2PWCHAR lo; std::memcpy(&lo, s + (i + 1) * sizeof(P2PWCHAR), sizeof lo);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                unsigned cp = 0x10000u
                            + (((unsigned)(u  - 0xD800) << 10)
                            |   (unsigned)(lo - 0xDC00));
                dst[o++] = (wchar_t)cp; ++i; continue;
            }
        }
        dst[o++] = (wchar_t)u;
    }
#endif
    return o;
}
//  EXIT helper for accessors that hand back a pointer into stored wide data
//  (c_wstr / c_name / wide variant). On Windows the store IS wchar_t, so return it
//  directly (unchanged semantics). On Linux, widen into a thread-local scratch
//  buffer valid until the next call on the same thread (matches the transient way
//  these accessors are consumed).
#if defined(_WIN32)
inline const wchar_t* p2p_wstr_from_store(const P2PWCHAR* src, size_t) {
    return reinterpret_cast<const wchar_t*>(src);
}
#else
inline const wchar_t* p2p_wstr_from_store(const P2PWCHAR* src, size_t n) {
    //  A RING of scratch buffers, not one: on Windows each c_wstr()/c_name() returns a
    //  distinct pointer into its own object's storage, so several results are commonly
    //  live at once — e.g. c_name() AND c_wstr() as args to a single printf, or nested
    //  P3Pmsg::Print recursion. A single thread_local buffer made every such pointer alias
    //  the last write (values printed as their field names). The ring gives each recent
    //  call its own storage, approximating the Windows lifetime for the transient way these
    //  accessors are consumed. (Not unbounded-safe, but the call sites hold only a handful
    //  simultaneously.)
    constexpr std::size_t RING = 16;
    static thread_local std::wstring bufs[RING];
    static thread_local std::size_t  next = 0;
    std::wstring& buf = bufs[next]; next = (next + 1) % RING;
    if (n == 0) { buf.clear(); return buf.c_str(); }
    buf.assign(n, L'\0');                              // upper bound: >= decoded length
    size_t o = p2p_load_wide(&buf[0], src, n);         // combines surrogate pairs; may shrink
    buf.resize(o);
    return buf.c_str();
}
#endif

//  p2p_wkey — a search key held still for the duration of a by-name scan.
//
//  A scan that compares a caller-supplied name against stored names must not keep
//  re-reading the key out of the caller's storage while it walks. Off Win32 a key
//  taken from an accessor — P3PmsgName::c_name(), P3PmsgData::c_wstr() — is a
//  pointer into the 16-slot thread-local ring above, and EVERY sibling the scan
//  compares spends one more slot, because looking at that sibling's stored name
//  means widening it through the same ring. So the budget is not 16 accessor calls
//  the caller can count; it is 16 SIBLINGS, and a container with as few as 8
//  members wraps the ring and rewrites the key underneath the comparison.
//
//  What that produces is not a crash but a wrong answer. The recycled slot now
//  holds the CURRENT sibling's name, and the key points at that same buffer, so
//  the comparison is buffer-against-itself: equal. The scan stops on the WRONG
//  item and the caller that asked for field X is handed field Y and writes into
//  it. (assign()/resize() in the ring may also reallocate, which dangles the key
//  outright — the same defect's louder half.)
//
//  Copying the key HERE, in the callee, is what makes such a scan safe however it
//  is called. The alternative — a rule that every caller copies before calling —
//  is invisible to any search, and has already been broken twice.
//
//  The copy stays off the heap for every name the store can hold: P3PmsgName
//  refuses a name over 63 UTF-16 units, which decodes to at most 63 wchar_t, so
//  the inline buffer covers all of them. Only a key too long to match anything
//  reaches the std::wstring fallback.
//
//  Applied on Windows too, where the ring does not exist: one behaviour to reason
//  about, and a key pointing into a store the scan may itself resize is a hazard
//  on that side as well.
class p2p_wkey
{
  public:
    explicit p2p_wkey(const wchar_t* key) : m_key(key) {
        if (key == nullptr) return;          // NULL is the caller's error, passed through
        const size_t n = std::wcslen(key);
        if (n < INLINE_MAX) {
            std::memcpy(m_inline, key, (n + 1) * sizeof(wchar_t));
            m_key = m_inline;
        } else {
            m_heap.assign(key, n);
            m_key = m_heap.c_str();
        }
    }
    operator const wchar_t*() const { return m_key; }
    const wchar_t* c_str()    const { return m_key; }

    //  Copying would leave m_key aimed at the source's inline buffer.
    p2p_wkey(const p2p_wkey&)            = delete;
    p2p_wkey& operator=(const p2p_wkey&) = delete;

  private:
    enum { INLINE_MAX = 64 };                // 63 stored units + NUL
    const wchar_t* m_key;
    wchar_t        m_inline[INLINE_MAX];
    std::wstring   m_heap;
};
