/*
 * Part of libximodem -- the Zimodem (Commander X16 config) firmware built as a
 * host shared library for the Commander X16 emulator's virtual modem.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Jason Hill
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * disclosure: ai-assisted (claude-sonnet-5), manual review -- see AI_DISCLOSURE.md
 */

// The one place in ximodem with #ifdef _WIN32. Everything else -- the Arduino/ESP
// shim, the firmware, the glue -- uses these names and stays platform-neutral.
//
// Windows target: MSYS2 UCRT64 (MinGW-w64 GCC, native Win32, Winsock).
#ifndef XIMODEM_XI_PLATFORM_H
#define XIMODEM_XI_PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// ===========================================================================
#ifdef _WIN32
// ---------------------------------------------------------------------------
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN            // trims ole/rpc/etc from <windows.h>
#endif
#ifndef NOMINMAX
#define NOMINMAX                       // keep windows' min()/max() macros out
#endif
#ifndef NOGDI
#define NOGDI                          // drop wingdi.h -> no bare ERROR macro
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601            // Windows 7+  (WSAPoll, GetAdaptersAddresses)
#endif

// Arduino.h (included earlier) defines INPUT/OUTPUT as small ints; winuser.h
// declares `typedef struct tagINPUT {...} INPUT;`. Park the Arduino macros
// across the Windows headers, then restore them for the firmware code that
// follows.
#pragma push_macro("INPUT")
#pragma push_macro("OUTPUT")
#undef INPUT
#undef OUTPUT
#include <winsock2.h>                  // must precede <windows.h>
#include <ws2tcpip.h>                  // getaddrinfo, inet_pton, socklen_t
#include <iphlpapi.h>                  // GetAdaptersAddresses
#include <icmpapi.h>                   // IcmpSendEcho
#include <direct.h>                    // _mkdir
#include <io.h>
#pragma pop_macro("OUTPUT")
#pragma pop_macro("INPUT")

typedef SOCKET xi_sock_t;
#define XI_BAD_SOCK   INVALID_SOCKET

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

typedef WSAPOLLFD xi_pollfd;
static inline int xi_poll(xi_pollfd *fds, unsigned n, int timeout_ms) {
  return WSAPoll(fds, (ULONG)n, timeout_ms);
}

static inline int  xi_sock_errno(void)      { return WSAGetLastError(); }
static inline int  xi_err_would_block(int e) { return e == WSAEWOULDBLOCK; }
// Windows non-blocking connect() reports WSAEWOULDBLOCK, not WSAEINPROGRESS.
static inline int  xi_err_in_progress(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
static inline void xi_closesocket(xi_sock_t s) { closesocket(s); }

static inline int xi_set_nonblock(xi_sock_t s, int nb) {
  u_long mode = nb ? 1u : 0u;
  return ioctlsocket(s, FIONBIO, &mode);
}
static inline int xi_sock_nread(xi_sock_t s, int *out) {
  u_long n = 0;
  if (ioctlsocket(s, FIONREAD, &n) != 0) return -1;
  *out = (int)n;
  return 0;
}

static inline int xi_mkdir(const char *path) { return _mkdir(path); }

#define xi_srandom(seed) srand((unsigned)(seed))
#define xi_random()      rand()

int xi_net_startup(void);   // WSAStartup once; impl in xi_platform.cpp

// ===========================================================================
#else  // POSIX
// ---------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

typedef int xi_sock_t;
#define XI_BAD_SOCK   (-1)

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

typedef struct pollfd xi_pollfd;
static inline int xi_poll(xi_pollfd *fds, unsigned n, int timeout_ms) {
  return poll(fds, n, timeout_ms);
}

static inline int  xi_sock_errno(void)      { return errno; }
static inline int  xi_err_would_block(int e) { return e == EAGAIN || e == EWOULDBLOCK; }
static inline int  xi_err_in_progress(int e) { return e == EINPROGRESS; }
static inline void xi_closesocket(xi_sock_t s) { close(s); }

static inline int xi_set_nonblock(xi_sock_t s, int nb) {
  int fl = fcntl(s, F_GETFL, 0);
  if (fl < 0) return -1;
  return fcntl(s, F_SETFL, nb ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
}
static inline int xi_sock_nread(xi_sock_t s, int *out) {
  return ioctl(s, FIONREAD, out);
}

static inline int xi_mkdir(const char *path) { return mkdir(path, 0755); }

#define xi_srandom(seed) srandom((unsigned long)(seed))
#define xi_random()      random()

static inline int xi_net_startup(void) { return 0; }

#endif  // _WIN32
// ===========================================================================

static inline int xi_bad_sock(xi_sock_t s) { return s == XI_BAD_SOCK; }

// send()/recv() take void* / ssize_t on POSIX and char* / int on Winsock;
// normalise to (void*, int) so callers don't care.
static inline int xi_send(xi_sock_t s, const void *buf, int len, int flags) {
  return (int)send(s, (const char *)buf, len, flags);
}
static inline int xi_recv(xi_sock_t s, void *buf, int len, int flags) {
  return (int)recv(s, (char *)buf, len, flags);
}

#endif  // XIMODEM_XI_PLATFORM_H
