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

#include "IPAddress.h"
#include <cstdio>
#include <cstdlib>

bool IPAddress::fromString(const char *address) {
  if (!address) return false;
  uint16_t acc = 0;
  uint8_t dots = 0;
  uint8_t parts[4] = {0, 0, 0, 0};
  bool sawDigit = false;
  while (*address) {
    char c = *address++;
    if (c >= '0' && c <= '9') {
      acc = acc * 10 + (c - '0');
      if (acc > 255) return false;
      sawDigit = true;
    } else if (c == '.') {
      if (dots == 3 || !sawDigit) return false;
      parts[dots++] = acc;
      acc = 0;
      sawDigit = false;
    } else {
      return false;
    }
  }
  if (dots != 3 || !sawDigit) return false;
  parts[3] = acc;
  for (int i = 0; i < 4; i++) _address.bytes[i] = parts[i];
  return true;
}

String IPAddress::toString() const {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
           _address.bytes[0], _address.bytes[1],
           _address.bytes[2], _address.bytes[3]);
  return String(buf);
}
