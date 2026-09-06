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

#include "WiFi.h"
#include "ximodem_internal.h"
#include <cstring>
#include <cstdio>

#ifndef _WIN32
#include <ifaddrs.h>
#include <net/if.h>
#endif

WiFiClass WiFi;

static inline bool bad(xi_sock_t s) { return s == XI_BAD_SOCK; }
static inline int  sockerr()        { return xi_sock_errno(); }
static inline bool wouldblock()     { return xi_err_would_block(sockerr()); }

// ---------------------------------------------------------------------------
// WiFiClient
// ---------------------------------------------------------------------------
struct WiFiClient::Sock {
  xi_sock_t fd = XI_BAD_SOCK;
  bool connected = false;
  ~Sock() { if (!bad(fd)) xi_closesocket(fd); }
};

WiFiClient::WiFiClient() : _s(std::make_shared<Sock>()) {}
WiFiClient::WiFiClient(xi_sock_t sock) : _s(std::make_shared<Sock>()) {
  _s->fd = sock;
  _s->connected = !bad(sock);
}

int WiFiClient::connect(IPAddress ip, uint16_t port) { return connect(ip, port, 10000); }
int WiFiClient::connect(const char *host, uint16_t port) { return connect(host, port, 10000); }
int WiFiClient::connect(IPAddress ip, uint16_t port, int32_t timeout_ms) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return connect(buf, port, timeout_ms);
}
int WiFiClient::connect(const char *host, uint16_t port, int32_t timeout_ms) {
  xi_net_startup();
  if (!_s) _s = std::make_shared<Sock>();
  struct addrinfo hints, *res = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  char portstr[8];
  snprintf(portstr, sizeof(portstr), "%u", port);
  if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
    xi_debug("WiFiClient: DNS failed for %s\n", host);
    return 0;
  }
  xi_sock_t s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (bad(s)) { freeaddrinfo(res); return 0; }
  xi_set_nonblock(s, 1);
  int rc = ::connect(s, res->ai_addr, (int)res->ai_addrlen);
  if (rc != 0 && xi_err_in_progress(sockerr())) {
    xi_pollfd pfd; memset(&pfd, 0, sizeof(pfd));
    pfd.fd = s; pfd.events = POLLOUT;
    if (xi_poll(&pfd, 1, timeout_ms > 0 ? timeout_ms : 10000) > 0) {
      int err = 0; socklen_t l = sizeof(err);
      getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &l);
      rc = err ? -1 : 0;
    } else rc = -1;
  }
  freeaddrinfo(res);
  if (rc != 0) { xi_closesocket(s); return 0; }
  int one = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
  _s->fd = s;
  _s->connected = true;
  return 1;
}

size_t WiFiClient::write(uint8_t b) { return write(&b, 1); }
size_t WiFiClient::write(const uint8_t *buf, size_t size) {
  if (!_s || bad(_s->fd)) return 0;
  size_t sent = 0;
  while (sent < size) {
    int n = xi_send(_s->fd, buf + sent, (int)(size - sent), MSG_NOSIGNAL);
    if (n > 0) { sent += n; continue; }
    if (n < 0 && wouldblock()) { ximodem_yield(); continue; }
    _s->connected = false;
    break;
  }
  return sent;
}
int WiFiClient::available() {
  if (!_s || bad(_s->fd)) return 0;
  int n = 0;
  return xi_sock_nread(_s->fd, &n) == 0 ? n : 0;
}
int WiFiClient::read() {
  uint8_t c;
  return read(&c, 1) == 1 ? c : -1;
}
int WiFiClient::read(uint8_t *buf, size_t size) {
  if (!_s || bad(_s->fd)) return -1;
  int n = xi_recv(_s->fd, buf, (int)size, 0);
  if (n == 0) { _s->connected = false; return -1; }
  if (n < 0) {
    if (wouldblock()) return 0;
    _s->connected = false;
    return -1;
  }
  return n;
}
int WiFiClient::peek() {
  if (!_s || bad(_s->fd)) return -1;
  uint8_t c;
  int n = xi_recv(_s->fd, &c, 1, MSG_PEEK);
  return n == 1 ? c : -1;
}
void WiFiClient::flush() {}
void WiFiClient::stop() {
  if (_s && !bad(_s->fd)) { xi_closesocket(_s->fd); _s->fd = XI_BAD_SOCK; }
  if (_s) _s->connected = false;
}
uint8_t WiFiClient::connected() {
  if (!_s || bad(_s->fd)) return 0;
  char c;
  int n = xi_recv(_s->fd, &c, 1, MSG_PEEK);   // socket is non-blocking
  if (n == 0) { _s->connected = false; return 0; }
  if (n < 0 && !wouldblock()) { _s->connected = false; return 0; }
  return _s->connected ? 1 : 0;
}
void WiFiClient::setNoDelay(bool nodelay) {
  if (_s && !bad(_s->fd)) {
    int v = nodelay ? 1 : 0;
    setsockopt(_s->fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&v, sizeof(v));
  }
}
bool WiFiClient::getNoDelay() { return true; }
void WiFiClient::setTimeout(uint32_t seconds) { Stream::setTimeout(seconds * 1000UL); }

IPAddress WiFiClient::remoteIP() {
  if (!_s || bad(_s->fd)) return IPAddress();
  struct sockaddr_in sa; socklen_t l = sizeof(sa);
  if (getpeername(_s->fd, (struct sockaddr *)&sa, &l) == 0)
    return IPAddress((uint32_t)sa.sin_addr.s_addr);
  return IPAddress();
}
uint16_t WiFiClient::remotePort() {
  if (!_s || bad(_s->fd)) return 0;
  struct sockaddr_in sa; socklen_t l = sizeof(sa);
  if (getpeername(_s->fd, (struct sockaddr *)&sa, &l) == 0)
    return ntohs(sa.sin_port);
  return 0;
}
IPAddress WiFiClient::localIP() {
  if (!_s || bad(_s->fd)) return IPAddress();
  struct sockaddr_in sa; socklen_t l = sizeof(sa);
  if (getsockname(_s->fd, (struct sockaddr *)&sa, &l) == 0)
    return IPAddress((uint32_t)sa.sin_addr.s_addr);
  return IPAddress();
}
uint16_t WiFiClient::localPort() {
  if (!_s || bad(_s->fd)) return 0;
  struct sockaddr_in sa; socklen_t l = sizeof(sa);
  if (getsockname(_s->fd, (struct sockaddr *)&sa, &l) == 0)
    return ntohs(sa.sin_port);
  return 0;
}
// Reported as int for the OpenSSL / libssh2-via-int-header boundary; real socket
// handles fit in an int on every platform OpenSSL itself supports.
int WiFiClient::fd() const { return _s ? (int)_s->fd : -1; }

// ---------------------------------------------------------------------------
// WiFiServer
// ---------------------------------------------------------------------------
void WiFiServer::begin(uint16_t port) {
  xi_net_startup();
  if (port) _port = port;
  stop();
  xi_sock_t s = ::socket(AF_INET, SOCK_STREAM, 0);
  if (bad(s)) return;
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port = htons(_port);
  if (::bind(s, (struct sockaddr *)&sa, sizeof(sa)) != 0 || ::listen(s, 4) != 0) {
    xi_closesocket(s);
    return;
  }
  xi_set_nonblock(s, 1);
  _listenfd = s;
}
void WiFiServer::stop() {
  if (!bad(_listenfd)) { xi_closesocket(_listenfd); _listenfd = XI_BAD_SOCK; }
}
bool WiFiServer::hasClient() {
  if (bad(_listenfd)) return false;
  xi_pollfd pfd; memset(&pfd, 0, sizeof(pfd));
  pfd.fd = _listenfd; pfd.events = POLLIN;
  return xi_poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN);
}
WiFiClient WiFiServer::available() {
  if (bad(_listenfd)) return WiFiClient(XI_BAD_SOCK);
  xi_sock_t s = ::accept(_listenfd, nullptr, nullptr);
  if (bad(s)) return WiFiClient(XI_BAD_SOCK);
  int one = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
  xi_set_nonblock(s, 1);
  return WiFiClient(s);
}

// ---------------------------------------------------------------------------
// WiFiUDP
// ---------------------------------------------------------------------------
uint8_t WiFiUDP::begin(uint16_t port) {
  xi_net_startup();
  stop();
  xi_sock_t s = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (bad(s)) return 0;
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port = htons(port);
  if (::bind(s, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
    xi_closesocket(s); return 0;
  }
  xi_set_nonblock(s, 1);
  _fd = s;
  _localPort = port;
  return 1;
}
void WiFiUDP::stop() { if (!bad(_fd)) { xi_closesocket(_fd); _fd = XI_BAD_SOCK; } }
int WiFiUDP::beginPacket(IPAddress ip, uint16_t port) {
  _txIP = ip; _txPort = port; _txlen = 0;
  return 1;
}
int WiFiUDP::beginPacket(const char *host, uint16_t port) {
  IPAddress ip;
  if (!WiFi.hostByName(host, ip)) return 0;
  return beginPacket(ip, port);
}
int WiFiUDP::endPacket() {
  if (bad(_fd)) return 0;
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = (uint32_t)_txIP;
  sa.sin_port = htons(_txPort);
  int n = (int)::sendto(_fd, (const char *)_txbuf, _txlen, 0,
                        (struct sockaddr *)&sa, sizeof(sa));
  return n == _txlen ? 1 : 0;
}
size_t WiFiUDP::write(uint8_t b) { return write(&b, 1); }
size_t WiFiUDP::write(const uint8_t *buf, size_t size) {
  size_t room = sizeof(_txbuf) - _txlen;
  if (size > room) size = room;
  memcpy(_txbuf + _txlen, buf, size);
  _txlen += size;
  return size;
}
int WiFiUDP::parsePacket() {
  if (bad(_fd)) return 0;
  struct sockaddr_in sa; socklen_t sl = sizeof(sa);
  int n = (int)::recvfrom(_fd, (char *)_rxbuf, sizeof(_rxbuf), 0,
                          (struct sockaddr *)&sa, &sl);
  if (n <= 0) return 0;
  _rxlen = n; _rxpos = 0;
  _remoteIP = IPAddress((uint32_t)sa.sin_addr.s_addr);
  _remotePort = ntohs(sa.sin_port);
  return _rxlen;
}
int WiFiUDP::available() { return _rxlen - _rxpos; }
int WiFiUDP::read() {
  if (_rxpos >= _rxlen) return -1;
  return _rxbuf[_rxpos++];
}
int WiFiUDP::read(uint8_t *buf, size_t len) {
  int avail = _rxlen - _rxpos;
  if (avail <= 0) return -1;
  if ((int)len > avail) len = avail;
  memcpy(buf, _rxbuf + _rxpos, len);
  _rxpos += len;
  return (int)len;
}
int WiFiUDP::peek() { return _rxpos < _rxlen ? _rxbuf[_rxpos] : -1; }

// ---------------------------------------------------------------------------
// WiFiClass  (host adapter = "connected")
// ---------------------------------------------------------------------------
#ifdef _WIN32
static bool hostHasNetwork(IPAddress *outIP, IPAddress *outMask) {
  ULONG sz = 16 * 1024;
  IP_ADAPTER_ADDRESSES *aa = (IP_ADAPTER_ADDRESSES *)malloc(sz);
  if (!aa) return false;
  ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG r = GetAdaptersAddresses(AF_INET, flags, nullptr, aa, &sz);
  if (r == ERROR_BUFFER_OVERFLOW) {
    free(aa);
    aa = (IP_ADAPTER_ADDRESSES *)malloc(sz);
    if (!aa) return false;
    r = GetAdaptersAddresses(AF_INET, flags, nullptr, aa, &sz);
  }
  bool found = false;
  if (r == NO_ERROR) {
    for (IP_ADAPTER_ADDRESSES *a = aa; a && !found; a = a->Next) {
      if (a->OperStatus != IfOperStatusUp) continue;
      if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
      for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u; u = u->Next) {
        if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
        struct sockaddr_in *sa = (struct sockaddr_in *)u->Address.lpSockaddr;
        if (outIP) *outIP = IPAddress((uint32_t)sa->sin_addr.s_addr);
        if (outMask) {
          ULONG bits = u->OnLinkPrefixLength;
          uint32_t m = (bits && bits <= 32)
                       ? htonl(bits == 32 ? 0xffffffffu : ~((1u << (32 - bits)) - 1))
                       : 0;
          *outMask = IPAddress(m);
        }
        found = true;
        break;
      }
    }
  }
  free(aa);
  return found;
}
#else
static bool hostHasNetwork(IPAddress *outIP, IPAddress *outMask) {
  struct ifaddrs *ifap = nullptr;
  if (getifaddrs(&ifap) != 0) return false;
  bool found = false;
  for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
    if (!(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK)) continue;
    struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
    if (outIP) *outIP = IPAddress((uint32_t)sa->sin_addr.s_addr);
    if (outMask && ifa->ifa_netmask) {
      struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
      *outMask = IPAddress((uint32_t)nm->sin_addr.s_addr);
    }
    found = true;
    break;
  }
  freeifaddrs(ifap);
  return found;
}
#endif

// See WiFi.h. disconnect() latches a single transient "down" that the very next
// status() reports and then clears -- enough to break connectWifi()'s
// `while(status()==WL_CONNECTED) disconnect();` spin without leaving the host
// stuck disconnected when nothing calls begin() afterwards (no SSID on the host).
static bool g_wifiTransientDown = false;

void WiFiClass::disconnect(bool wifioff) {
  (void)wifioff;
  g_wifiTransientDown = true;
}
int WiFiClass::begin(const char *ssid, const char *pass) {
  (void)ssid; (void)pass;
  xi_net_startup();
  g_wifiTransientDown = false;
  return status();
}
uint8_t WiFiClass::status() {
  xi_net_startup();
  if (g_wifiTransientDown) { g_wifiTransientDown = false; return WL_DISCONNECTED; }
  return hostHasNetwork(nullptr, nullptr) ? WL_CONNECTED : WL_DISCONNECTED;
}
IPAddress WiFiClass::localIP() {
  IPAddress ip;
  hostHasNetwork(&ip, nullptr);
  return ip;
}
IPAddress WiFiClass::subnetMask() {
  IPAddress ip, mask;
  hostHasNetwork(&ip, &mask);
  return mask;
}
IPAddress WiFiClass::gatewayIP() {
  IPAddress ip = localIP();
  return IPAddress(ip[0], ip[1], ip[2], 1);
}
IPAddress WiFiClass::dnsIP(int) { return gatewayIP(); }
bool WiFiClass::config(IPAddress, IPAddress, IPAddress, IPAddress, IPAddress) { return true; }

String WiFiClass::SSID() { return String("ximodem-host"); }
int32_t WiFiClass::RSSI() { return -40; }
String WiFiClass::macAddress() { return String("02:00:00:00:00:01"); }
uint8_t *WiFiClass::macAddress(uint8_t *mac) {
  static const uint8_t m[6] = {0x02, 0, 0, 0, 0, 0x01};
  memcpy(mac, m, 6);
  return mac;
}
String WiFiClass::hostname() { return String("ximodem"); }
bool WiFiClass::hostByName(const char *aHostname, IPAddress &aResult) {
  xi_net_startup();
  struct addrinfo hints, *res = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(aHostname, nullptr, &hints, &res) != 0 || !res) return false;
  struct sockaddr_in *sa = (struct sockaddr_in *)res->ai_addr;
  aResult = IPAddress((uint32_t)sa->sin_addr.s_addr);
  freeaddrinfo(res);
  return true;
}
int16_t WiFiClass::scanNetworks(bool, bool) { return 1; }

int WiFiGenericClass::hostByName(const char *aHostname, IPAddress &aResult) {
  return WiFi.hostByName(aHostname, aResult) ? 1 : 0;
}
