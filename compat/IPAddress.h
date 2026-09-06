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

#ifndef XIMODEM_IPADDRESS_H
#define XIMODEM_IPADDRESS_H

#include <stdint.h>
#include "WString.h"

class IPAddress {
  private:
    union {
      uint8_t bytes[4];
      uint32_t dword;
    } _address;
  public:
    IPAddress() { _address.dword = 0; }
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
      _address.bytes[0] = a; _address.bytes[1] = b;
      _address.bytes[2] = c; _address.bytes[3] = d;
    }
    IPAddress(uint32_t addr) { _address.dword = addr; }
    IPAddress(unsigned long addr) { _address.dword = (uint32_t)addr; }
    IPAddress(const uint8_t *addr) {
      for (int i = 0; i < 4; i++) _address.bytes[i] = addr[i];
    }

    operator uint32_t() const { return _address.dword; }
    bool operator==(const IPAddress &o) const { return _address.dword == o._address.dword; }
    bool operator!=(const IPAddress &o) const { return _address.dword != o._address.dword; }
    bool operator==(uint32_t o) const { return _address.dword == o; }

    uint8_t operator[](int index) const { return _address.bytes[index]; }
    uint8_t &operator[](int index) { return _address.bytes[index]; }

    IPAddress &operator=(uint32_t addr) { _address.dword = addr; return *this; }
    IPAddress &operator=(const uint8_t *addr) {
      for (int i = 0; i < 4; i++) _address.bytes[i] = addr[i];
      return *this;
    }

    bool fromString(const char *address);
    bool fromString(const String &address) { return fromString(address.c_str()); }
    String toString() const;

    const uint8_t *raw_address() const { return _address.bytes; }
};

const IPAddress INADDR_NONE_ADDR(0, 0, 0, 0);

#endif
