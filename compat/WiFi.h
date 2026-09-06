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

#ifndef XIMODEM_WIFI_H
#define XIMODEM_WIFI_H

#include "Arduino.h"
#include "IPAddress.h"
#include "Stream.h"
#include <memory>

// The ESP32 core's <WiFi.h> transitively exposes the BSD socket API; firmware
// (e.g. the SSH client) calls socket()/connect()/htons() directly. xi_platform.h
// pulls the right set for POSIX or Winsock.
#include "xi_platform.h"

// wl_status_t
#define WL_NO_SHIELD       255
#define WL_IDLE_STATUS     0
#define WL_NO_SSID_AVAIL   1
#define WL_SCAN_COMPLETED  2
#define WL_CONNECTED       3
#define WL_CONNECT_FAILED  4
#define WL_CONNECTION_LOST 5
#define WL_DISCONNECTED    6

// encryption types
#define WIFI_AUTH_OPEN 0
#define ENC_TYPE_NONE  WIFI_AUTH_OPEN

// wifi_mode_t
#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP  2
#define WIFI_AP_STA 3
typedef int wifi_mode_t;

class WiFiClient : public Stream {
  protected:
    struct Sock;
    std::shared_ptr<Sock> _s;
  public:
    WiFiClient();
    explicit WiFiClient(xi_sock_t sock);
    virtual ~WiFiClient() {}

    virtual int connect(IPAddress ip, uint16_t port);
    virtual int connect(const char *host, uint16_t port);
    virtual int connect(IPAddress ip, uint16_t port, int32_t timeout_ms);
    virtual int connect(const char *host, uint16_t port, int32_t timeout_ms);

    size_t write(uint8_t b) override;
    size_t write(const uint8_t *buf, size_t size) override;
    using Print::write;
    int available() override;
    int read() override;
    virtual int read(uint8_t *buf, size_t size);
    int peek() override;
    void flush() override;
    virtual void stop();
    virtual uint8_t connected();
    operator bool() { return connected(); }

    void setNoDelay(bool nodelay);
    bool getNoDelay();
    void setTimeout(uint32_t seconds);

    IPAddress remoteIP();
    uint16_t remotePort();
    IPAddress localIP();
    uint16_t localPort();
    virtual int fd() const;
};

// ESP32 Arduino core exposes static DNS via WiFiGenericClass; the SSH client
// uses it directly.
class WiFiGenericClass {
  public:
    static int hostByName(const char *aHostname, IPAddress &aResult);
};

class WiFiClientSecure : public WiFiClient {
    void *_ssl = nullptr;      // SSL*  (void to keep OpenSSL out of this header)
    void *_ctx = nullptr;      // SSL_CTX*
    bool  _insecure = false;
    const char *_caCert = nullptr;
    bool tlsHandshake(const char *host);
    void tlsClose();
  public:
    WiFiClientSecure() {}
    ~WiFiClientSecure() override { tlsClose(); }

    void setInsecure() { _insecure = true; }
    void setCACert(const char *ca) { _caCert = ca; }
    void setCertificate(const char *) {}
    void setPrivateKey(const char *) {}

    int connect(IPAddress ip, uint16_t port) override;
    int connect(const char *host, uint16_t port) override;
    int connect(IPAddress ip, uint16_t port, int32_t timeout_ms) override;
    int connect(const char *host, uint16_t port, int32_t timeout_ms) override;

    size_t write(uint8_t b) override;
    size_t write(const uint8_t *buf, size_t size) override;
    int available() override;
    int read() override;
    int read(uint8_t *buf, size_t size) override;
    int peek() override;
    void stop() override;
    uint8_t connected() override;
};

class WiFiServer {
  private:
    uint16_t _port;
    xi_sock_t _listenfd = XI_BAD_SOCK;
    bool _noDelay = false;
  public:
    WiFiServer(uint16_t port = 80) : _port(port) {}
    ~WiFiServer() { stop(); }
    void begin(uint16_t port = 0);
    void end() { stop(); }
    void stop();
    void close() { stop(); }
    bool hasClient();
    WiFiClient available();
    WiFiClient accept() { return available(); }
    void setNoDelay(bool n) { _noDelay = n; }
    operator bool() { return _listenfd != XI_BAD_SOCK; }
};

class WiFiUDP : public Stream {
  private:
    xi_sock_t _fd = XI_BAD_SOCK;
    uint16_t _localPort = 0;
    IPAddress _remoteIP;
    uint16_t _remotePort = 0;
    uint8_t _rxbuf[1600];
    int _rxlen = 0, _rxpos = 0;
    uint8_t _txbuf[1600];
    int _txlen = 0;
    IPAddress _txIP;
    uint16_t _txPort = 0;
  public:
    WiFiUDP() {}
    ~WiFiUDP() { stop(); }
    uint8_t begin(uint16_t port);
    void stop();
    int beginPacket(IPAddress ip, uint16_t port);
    int beginPacket(const char *host, uint16_t port);
    int endPacket();
    size_t write(uint8_t b) override;
    size_t write(const uint8_t *buf, size_t size) override;
    using Print::write;
    int parsePacket();
    int available() override;
    int read() override;
    int read(uint8_t *buf, size_t len);
    int read(char *buf, size_t len) { return read((uint8_t *)buf, len); }
    int peek() override;
    void flush() override {}
    IPAddress remoteIP() { return _remoteIP; }
    uint16_t remotePort() { return _remotePort; }
};

class WiFiClass {
  public:
    int begin(const char *ssid = nullptr, const char *pass = nullptr);
    // disconnect() makes the *next* status() report WL_DISCONNECTED once, then it
    // reverts. That's enough to end connectWifi()'s `while(status()==WL_CONNECTED)
    // disconnect();` spin (otherwise infinite on the host, where status() is
    // pinned CONNECTED) without leaving the host stuck down when no begin()
    // follows -- the normal case here, since there is no SSID to join.
    void disconnect(bool wifioff = false);
    uint8_t status();
    bool mode(wifi_mode_t) { return true; }
    bool setTxPower(int) { return true; }
    bool setSleep(bool) { return true; }

    IPAddress localIP();
    IPAddress gatewayIP();
    IPAddress subnetMask();
    IPAddress dnsIP(int n = 0);
    bool config(IPAddress local, IPAddress gw, IPAddress subnet,
                IPAddress dns1 = IPAddress(), IPAddress dns2 = IPAddress());

    String SSID();
    int32_t RSSI();
    uint8_t encryptionType(uint8_t i) { (void)i; return ENC_TYPE_NONE; }
    String macAddress();
    uint8_t *macAddress(uint8_t *mac);
    String hostname();
    bool setHostname(const char *name) { (void)name; return true; }
    bool hostByName(const char *aHostname, IPAddress &aResult);

    int16_t scanNetworks(bool async = false, bool hidden = false);
    String SSID(uint8_t i) { (void)i; return SSID(); }
    int32_t RSSI(uint8_t i) { (void)i; return RSSI(); }
    int8_t scanComplete() { return 1; }
    void scanDelete() {}
};

extern WiFiClass WiFi;

#endif
