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
//  Platform layer — serial ports: DCB/SetCommState/WaitCommEvent -> termios.
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §5.1, §6.2).
//
//  _WIN32 : pass-through. Linux: DCB/SetCommState -> termios (cfsetspeed + cs5..8 /
//  parity / stop-bit mapping); SetCommMask(EV_RXCHAR)+WaitCommEvent(overlapped) ->
//  io_uring_prep_poll_add(POLLIN) via the ring, then the normal (overlapped) read path.
//  \\.\COMx path translation lives in p2pfile.h's CreateFileW.
//
#pragma once
#include "p2ptypes.h"

#if defined(_WIN32)
  #include <windows.h>   // DCB, COMMTIMEOUTS, SetCommState, WaitCommEvent, EV_RXCHAR
#else
  #include "p2piocp.h"   // LPOVERLAPPED (async WaitCommEvent)
  #include <termios.h>
  #include <unistd.h>
  #include <poll.h>
  #include <fcntl.h>
  #include <cstring>
  #include <cerrno>

  //  Async POLLIN wait — provided weakly by p2piocp.cpp (linked into Targetcore, not
  //  Msgcore); the overlapped WaitCommEvent routes here (guarded by an address test).
  extern "C" BOOL p2p_iocp_poll(int fd, void* ov) __attribute__((weak));

  // -------------------------------------------------------------------------
  //  Comm constants (the subset the 232 transport names).
  // -------------------------------------------------------------------------
  #ifndef MAXDWORD
    #define MAXDWORD 0xFFFFFFFFu
  #endif
  #ifndef EV_RXCHAR
    #define EV_RXCHAR   0x0001
    #define EV_RXFLAG   0x0002
    #define EV_TXEMPTY  0x0004
    #define EV_CTS      0x0008
    #define EV_DSR      0x0010
    #define EV_RLSD     0x0020
    #define EV_BREAK    0x0040
    #define EV_ERR      0x0080
    #define EV_RING     0x0100
  #endif
  #ifndef NOPARITY
    #define NOPARITY    0
    #define ODDPARITY   1
    #define EVENPARITY  2
    #define MARKPARITY  3
    #define SPACEPARITY 4
  #endif
  #ifndef ONESTOPBIT
    #define ONESTOPBIT   0
    #define ONE5STOPBITS 1
    #define TWOSTOPBITS  2
  #endif
  #ifndef DTR_CONTROL_DISABLE
    #define DTR_CONTROL_DISABLE   0x00
    #define DTR_CONTROL_ENABLE    0x01
    #define DTR_CONTROL_HANDSHAKE 0x02
    #define RTS_CONTROL_DISABLE   0x00
    #define RTS_CONTROL_ENABLE    0x01
    #define RTS_CONTROL_HANDSHAKE 0x02
    #define RTS_CONTROL_TOGGLE    0x03
  #endif
  #ifndef CBR_9600
    #define CBR_110    110
    #define CBR_300    300
    #define CBR_600    600
    #define CBR_1200   1200
    #define CBR_2400   2400
    #define CBR_4800   4800
    #define CBR_9600   9600
    #define CBR_14400  14400
    #define CBR_19200  19200
    #define CBR_38400  38400
    #define CBR_57600  57600
    #define CBR_115200 115200
    #define CBR_128000 128000
    #define CBR_256000 256000
  #endif
  #ifndef PURGE_RXCLEAR
    #define PURGE_TXABORT 0x0001
    #define PURGE_RXABORT 0x0002
    #define PURGE_TXCLEAR 0x0004
    #define PURGE_RXCLEAR 0x0008
  #endif

  // -------------------------------------------------------------------------
  //  DCB — the Win32 layout (bitfield widths match), so `dcb.fBinary = TRUE` etc.
  //  compile; only the fields the transport reads are mapped to termios.
  // -------------------------------------------------------------------------
  struct DCB {
      DWORD DCBlength;
      DWORD BaudRate;
      DWORD fBinary            : 1;
      DWORD fParity            : 1;
      DWORD fOutxCtsFlow       : 1;
      DWORD fOutxDsrFlow       : 1;
      DWORD fDtrControl        : 2;
      DWORD fDsrSensitivity    : 1;
      DWORD fTXContinueOnXoff  : 1;
      DWORD fOutX              : 1;
      DWORD fInX               : 1;
      DWORD fErrorChar         : 1;
      DWORD fNull              : 1;
      DWORD fRtsControl        : 2;
      DWORD fAbortOnError      : 1;
      DWORD fDummy2            : 17;
      WORD  wReserved;
      WORD  XonLim;
      WORD  XoffLim;
      BYTE  ByteSize;
      BYTE  Parity;
      BYTE  StopBits;
      char  XonChar;
      char  XoffChar;
      char  ErrorChar;
      char  EofChar;
      char  EvtChar;
      WORD  wReserved1;
  };
  using LPDCB = DCB*;

  struct COMMTIMEOUTS {
      DWORD ReadIntervalTimeout;
      DWORD ReadTotalTimeoutMultiplier;
      DWORD ReadTotalTimeoutConstant;
      DWORD WriteTotalTimeoutMultiplier;
      DWORD WriteTotalTimeoutConstant;
  };
  using LPCOMMTIMEOUTS = COMMTIMEOUTS*;

  //  Win baud value (CBR_* == the literal rate) -> termios speed_t.
  inline speed_t p2p_baud_to_termios(DWORD b) {
      switch (b) {
          case 110:    return B110;    case 300:   return B300;    case 600:   return B600;
          case 1200:   return B1200;   case 2400:  return B2400;   case 4800:  return B4800;
          case 9600:   return B9600;   case 19200: return B19200;  case 38400: return B38400;
          case 57600:  return B57600;  case 115200:return B115200;
          case 230400: return B230400;
          default:     return B115200;
      }
  }

  // -------------------------------------------------------------------------
  //  Comm API over termios. HANDLE is the P2PHandle{Fd} from CreateFileW("\\.\COMx").
  // -------------------------------------------------------------------------
  inline BOOL GetCommState(HANDLE h, LPDCB dcb) {
      if (!h || h->kind != HKind::Fd || !dcb) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      struct termios tio{};
      if (::tcgetattr(h->fd, &tio) != 0) { SetLastError(win32_from_errno(errno)); return FALSE; }
      dcb->DCBlength = (DWORD)sizeof(DCB);
      dcb->BaudRate  = 115200;                                   // reported nominal
      dcb->ByteSize  = (tio.c_cflag & CSIZE) == CS7 ? 7 : 8;
      dcb->Parity    = (tio.c_cflag & PARENB) ? ((tio.c_cflag & PARODD) ? ODDPARITY : EVENPARITY) : NOPARITY;
      dcb->StopBits  = (tio.c_cflag & CSTOPB) ? TWOSTOPBITS : ONESTOPBIT;
      return TRUE;
  }

  inline BOOL SetCommState(HANDLE h, LPDCB dcb) {
      if (!h || h->kind != HKind::Fd || !dcb) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      struct termios tio{};
      if (::tcgetattr(h->fd, &tio) != 0) { SetLastError(win32_from_errno(errno)); return FALSE; }
      ::cfmakeraw(&tio);                                         // 8N1 raw baseline
      speed_t sp = p2p_baud_to_termios(dcb->BaudRate);
      ::cfsetispeed(&tio, sp); ::cfsetospeed(&tio, sp);
      tio.c_cflag &= ~CSIZE;
      switch (dcb->ByteSize) {
          case 5: tio.c_cflag |= CS5; break;  case 6: tio.c_cflag |= CS6; break;
          case 7: tio.c_cflag |= CS7; break;  default: tio.c_cflag |= CS8; break;
      }
      if (dcb->Parity == NOPARITY) tio.c_cflag &= ~PARENB;
      else { tio.c_cflag |= PARENB;
             if (dcb->Parity == ODDPARITY) tio.c_cflag |= PARODD; else tio.c_cflag &= ~PARODD; }
      if (dcb->StopBits == TWOSTOPBITS) tio.c_cflag |= CSTOPB; else tio.c_cflag &= ~CSTOPB;
      if (dcb->fOutxCtsFlow || dcb->fRtsControl == RTS_CONTROL_HANDSHAKE) tio.c_cflag |= CRTSCTS;
      else                                                                tio.c_cflag &= ~CRTSCTS;
      tio.c_cflag |= (CLOCAL | CREAD);
      tio.c_cc[VMIN] = 0; tio.c_cc[VTIME] = 0;                   // non-blocking; the ring drives I/O
      if (::tcsetattr(h->fd, TCSANOW, &tio) != 0) { SetLastError(win32_from_errno(errno)); return FALSE; }
      return TRUE;
  }

  //  Driver buffer sizing / mask are advisory under the io_uring model.
  inline BOOL SetupComm(HANDLE h, DWORD, DWORD) {
      return (h && h->kind == HKind::Fd) ? TRUE : (SetLastError(ERROR_INVALID_HANDLE), FALSE); }
  inline BOOL GetCommTimeouts(HANDLE h, LPCOMMTIMEOUTS t) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      if (t) std::memset(t, 0, sizeof *t); return TRUE; }
  inline BOOL SetCommTimeouts(HANDLE h, LPCOMMTIMEOUTS) {
      return (h && h->kind == HKind::Fd) ? TRUE : (SetLastError(ERROR_INVALID_HANDLE), FALSE); }
  inline BOOL SetCommMask(HANDLE h, DWORD) {   // only EV_RXCHAR is armed (via WaitCommEvent)
      return (h && h->kind == HKind::Fd) ? TRUE : (SetLastError(ERROR_INVALID_HANDLE), FALSE); }
  inline BOOL GetCommMask(HANDLE h, LPDWORD m) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      if (m) *m = EV_RXCHAR; return TRUE; }
  inline BOOL PurgeComm(HANDLE h, DWORD flags) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      int q = ((flags & (PURGE_RXCLEAR|PURGE_RXABORT)) && (flags & (PURGE_TXCLEAR|PURGE_TXABORT))) ? TCIOFLUSH
            : (flags & (PURGE_RXCLEAR|PURGE_RXABORT)) ? TCIFLUSH
            : (flags & (PURGE_TXCLEAR|PURGE_TXABORT)) ? TCOFLUSH : TCIOFLUSH;
      return ::tcflush(h->fd, q) == 0 ? TRUE : (SetLastError(win32_from_errno(errno)), FALSE); }

  //  WaitCommEvent(EV_RXCHAR): overlapped -> ring poll (FALSE + ERROR_IO_PENDING, Fix-4);
  //  synchronous -> block in poll(POLLIN). *lpEvtMask is set to the (only) armed event.
  inline BOOL WaitCommEvent(HANDLE h, LPDWORD lpEvtMask, LPOVERLAPPED ov) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      if (lpEvtMask) *lpEvtMask = EV_RXCHAR;
      if (ov) {
          if (&p2p_iocp_poll) return p2p_iocp_poll(h->fd, ov);
          SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
      }
      struct pollfd pfd{ h->fd, POLLIN, 0 };
      int rc = ::poll(&pfd, 1, -1);
      if (rc < 0) { SetLastError(win32_from_errno(errno)); return FALSE; }
      return TRUE;
  }
#endif
