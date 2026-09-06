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

// Host replacement for Zimodem's proto_ping.ino (which needs lwIP raw ICMP).
//
//  POSIX : unprivileged ICMP datagram socket (Linux SOCK_DGRAM/IPPROTO_ICMP,
//          gated by net.ipv4.ping_group_range; macOS allows it outright), with a
//          bounded non-blocking TCP-connect fallback when the kernel forbids it.
//  Win32 : IcmpSendEcho (Iphlpapi) -- no privileges needed.
//
// Returns the round-trip time in ms, or -1.
#include "Arduino.h"
#include "WiFi.h"          // pulls xi_platform.h
#include <cstring>

int ping(char *host) {
  if (!host || !*host) return -1;
  xi_net_startup();

  struct addrinfo hints, *res = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) return -1;
  struct sockaddr_in dst = *(struct sockaddr_in *)res->ai_addr;
  freeaddrinfo(res);

#ifdef _WIN32
  // ---- Windows: ICMP via Iphlpapi -----------------------------------------
  HANDLE h = IcmpCreateFile();
  if (h == INVALID_HANDLE_VALUE) return -1;
  char payload[32];
  memset(payload, 'a', sizeof(payload));
  unsigned char reply[sizeof(ICMP_ECHO_REPLY) + sizeof(payload) + 8];
  DWORD n = IcmpSendEcho(h, dst.sin_addr.s_addr, payload, sizeof(payload),
                         nullptr, reply, sizeof(reply), 3000);
  int rtt = -1;
  if (n > 0) {
    ICMP_ECHO_REPLY *er = (ICMP_ECHO_REPLY *)reply;
    if (er->Status == IP_SUCCESS)
      rtt = (int)er->RoundTripTime;
  }
  IcmpCloseHandle(h);
  return rtt;

#else
  // ---- POSIX: ICMP datagram socket, TCP-connect fallback ------------------
  auto icmp_cksum = [](const void *data, int len) -> uint16_t {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint16_t)~sum;
  };

  xi_sock_t s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
  if (xi_bad_sock(s)) {
    for (int port : {80, 443, 22}) {
      xi_sock_t t = ::socket(AF_INET, SOCK_STREAM, 0);
      if (xi_bad_sock(t)) continue;
      xi_set_nonblock(t, 1);
      struct sockaddr_in a = dst;
      a.sin_port = htons(port);
      unsigned long t0 = millis();
      int rc = ::connect(t, (struct sockaddr *)&a, sizeof(a));
      if (rc == 0) { xi_closesocket(t); return (int)(millis() - t0); }
      if (rc != 0 && xi_err_in_progress(xi_sock_errno())) {
        xi_pollfd pfd; memset(&pfd, 0, sizeof(pfd));
        pfd.fd = t; pfd.events = POLLOUT;
        if (xi_poll(&pfd, 1, 1000) > 0) {
          int err = 0; socklen_t el = sizeof(err);
          getsockopt(t, SOL_SOCKET, SO_ERROR, (char *)&err, &el);
          if (err == 0 || err == ECONNREFUSED) {  // RST still proves host alive
            xi_closesocket(t);
            return (int)(millis() - t0);
          }
        }
      }
      xi_closesocket(t);
    }
    return -1;
  }
  xi_set_nonblock(s, 1);

  uint8_t pkt[64];
  memset(pkt, 0, sizeof(pkt));
  struct icmphdr {
    uint8_t type, code; uint16_t checksum;
    uint16_t id, sequence;
  } *ic = (struct icmphdr *)pkt;
  ic->type = 8;   // ICMP_ECHO
  ic->code = 0;
  ic->id = htons(0x4242);
  ic->sequence = htons(1);
  for (int i = (int)sizeof(*ic); i < (int)sizeof(pkt); i++) pkt[i] = 'a' + (i % 23);
  ic->checksum = icmp_cksum(pkt, sizeof(pkt));

  unsigned long t0 = millis();
  if (::sendto(s, (const char *)pkt, sizeof(pkt), 0,
               (struct sockaddr *)&dst, sizeof(dst)) < 0) {
    xi_closesocket(s);
    return -1;
  }

  uint8_t rbuf[256];
  struct sockaddr_in from;
  socklen_t fl = sizeof(from);
  for (;;) {
    xi_pollfd pfd; memset(&pfd, 0, sizeof(pfd));
    pfd.fd = s; pfd.events = POLLIN;
    if (xi_poll(&pfd, 1, 3000) <= 0) { xi_closesocket(s); return -1; }
    int n = (int)::recvfrom(s, (char *)rbuf, sizeof(rbuf), 0,
                            (struct sockaddr *)&from, &fl);
    if (n <= 0) {
      if (xi_err_would_block(xi_sock_errno())) continue;
      xi_closesocket(s);
      return -1;
    }
    break;
  }
  xi_closesocket(s);
  unsigned long rtt = millis() - t0;
  return rtt > 65535 ? 65535 : (int)rtt;
#endif
}
