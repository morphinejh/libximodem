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

// WiFiClientSecure: real TLS over OpenSSL when XIMODEM_HAVE_OPENSSL is defined,
// otherwise a plaintext fallback (upstream Zimodem calls setInsecure() anyway).
#include "WiFi.h"
#include "ximodem_internal.h"

#ifndef XIMODEM_HAVE_OPENSSL
// ----------------------------- plaintext fallback ---------------------------
bool WiFiClientSecure::tlsHandshake(const char *) { return true; }
void WiFiClientSecure::tlsClose() {}
int WiFiClientSecure::connect(const char *h, uint16_t p, int32_t t) {
  xi_debug("WiFiClientSecure: no TLS in this build; connecting in the clear\n");
  return WiFiClient::connect(h, p, t);
}
int WiFiClientSecure::connect(const char *h, uint16_t p) { return connect(h, p, 10000); }
int WiFiClientSecure::connect(IPAddress ip, uint16_t p, int32_t t) {
  xi_debug("WiFiClientSecure: no TLS in this build; connecting in the clear\n");
  return WiFiClient::connect(ip, p, t);
}
int WiFiClientSecure::connect(IPAddress ip, uint16_t p) { return connect(ip, p, 10000); }
size_t WiFiClientSecure::write(uint8_t b) { return WiFiClient::write(b); }
size_t WiFiClientSecure::write(const uint8_t *b, size_t n) { return WiFiClient::write(b, n); }
int WiFiClientSecure::available() { return WiFiClient::available(); }
int WiFiClientSecure::read() { return WiFiClient::read(); }
int WiFiClientSecure::read(uint8_t *b, size_t n) { return WiFiClient::read(b, n); }
int WiFiClientSecure::peek() { return WiFiClient::peek(); }
void WiFiClientSecure::stop() { WiFiClient::stop(); }
uint8_t WiFiClientSecure::connected() { return WiFiClient::connected(); }
#else
// ------------------------------- OpenSSL path -------------------------------
#include "xi_platform.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <cstring>

static SSL_CTX *g_clientCtx = nullptr;
static void ensureCtx() {
  if (g_clientCtx) return;
  SSL_library_init();
  SSL_load_error_strings();
  g_clientCtx = SSL_CTX_new(TLS_client_method());
  SSL_CTX_set_min_proto_version(g_clientCtx, TLS1_2_VERSION);
  SSL_CTX_set_default_verify_paths(g_clientCtx);
}

void WiFiClientSecure::tlsClose() {
  if (_ssl) { SSL_shutdown((SSL *)_ssl); SSL_free((SSL *)_ssl); _ssl = nullptr; }
}

bool WiFiClientSecure::tlsHandshake(const char *host) {
  ensureCtx();
  int sock = fd();
  if (sock < 0) return false;

  SSL *ssl = SSL_new(g_clientCtx);
  if (!ssl) return false;
  SSL_set_fd(ssl, sock);
  if (host && *host) {
    SSL_set_tlsext_host_name(ssl, host);
    if (!_insecure) {
      SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
      SSL_set1_host(ssl, host);
      SSL_set_verify(ssl, SSL_VERIFY_PEER, nullptr);
    }
  }

  // socket is non-blocking (set by WiFiClient::connect); pump the handshake
  for (int i = 0; i < 2000; i++) {
    int r = SSL_connect(ssl);
    if (r == 1) { _ssl = ssl; return true; }
    int err = SSL_get_error(ssl, r);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      xi_pollfd pfd; memset(&pfd, 0, sizeof(pfd));
      pfd.fd = (xi_sock_t)sock;
      pfd.events = (short)(err == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT);
      xi_poll(&pfd, 1, 50);
      ximodem_yield();
      continue;
    }
    xi_debug("TLS handshake failed: %s\n", ERR_error_string(ERR_get_error(), nullptr));
    SSL_free(ssl);
    return false;
  }
  SSL_free(ssl);
  return false;
}

int WiFiClientSecure::connect(const char *host, uint16_t port, int32_t timeout_ms) {
  tlsClose();
  if (!WiFiClient::connect(host, port, timeout_ms)) return 0;
  if (!tlsHandshake(host)) { WiFiClient::stop(); return 0; }
  return 1;
}
int WiFiClientSecure::connect(const char *host, uint16_t port) {
  return connect(host, port, 10000);
}
int WiFiClientSecure::connect(IPAddress ip, uint16_t port, int32_t timeout_ms) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  tlsClose();
  if (!WiFiClient::connect(ip, port, timeout_ms)) return 0;
  // no SNI/host name to verify against a bare IP
  _insecure = true;
  if (!tlsHandshake(buf)) { WiFiClient::stop(); return 0; }
  return 1;
}
int WiFiClientSecure::connect(IPAddress ip, uint16_t port) {
  return connect(ip, port, 10000);
}

size_t WiFiClientSecure::write(uint8_t b) { return write(&b, 1); }
size_t WiFiClientSecure::write(const uint8_t *buf, size_t size) {
  if (!_ssl) return 0;
  size_t sent = 0;
  while (sent < size) {
    int n = SSL_write((SSL *)_ssl, buf + sent, (int)(size - sent));
    if (n > 0) { sent += n; continue; }
    int err = SSL_get_error((SSL *)_ssl, n);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) { ximodem_yield(); continue; }
    break;
  }
  return sent;
}
int WiFiClientSecure::available() {
  if (!_ssl) return 0;
  int p = SSL_pending((SSL *)_ssl);
  if (p > 0) return p;
  // force a read attempt to move data into SSL buffers
  unsigned char c;
  int n = SSL_peek((SSL *)_ssl, &c, 1);
  if (n == 1) return SSL_pending((SSL *)_ssl) + 1;
  return 0;
}
int WiFiClientSecure::read() {
  uint8_t c;
  return read(&c, 1) == 1 ? c : -1;
}
int WiFiClientSecure::read(uint8_t *buf, size_t size) {
  if (!_ssl) return -1;
  int n = SSL_read((SSL *)_ssl, buf, (int)size);
  if (n > 0) return n;
  int err = SSL_get_error((SSL *)_ssl, n);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
  return -1;
}
int WiFiClientSecure::peek() {
  if (!_ssl) return -1;
  unsigned char c;
  return SSL_peek((SSL *)_ssl, &c, 1) == 1 ? c : -1;
}
void WiFiClientSecure::stop() {
  tlsClose();
  WiFiClient::stop();
}
uint8_t WiFiClientSecure::connected() {
  return _ssl ? WiFiClient::connected() : 0;
}

#endif // XIMODEM_HAVE_OPENSSL
