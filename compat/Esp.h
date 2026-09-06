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

#ifndef XIMODEM_ESP_H
#define XIMODEM_ESP_H
#include <stdint.h>
#include "WString.h"
class EspClass {
public:
  uint32_t getFreeHeap() { return 200000; }
  uint32_t getMinFreeHeap() { return 100000; }
  uint32_t getHeapSize() { return 300000; }
  uint32_t getMaxAllocHeap() { return 100000; }
  uint8_t  getChipRevision() { return 3; }
  uint32_t getCpuFreqMHz() { return 240; }
  uint32_t getFlashChipId() { return 0; }
  uint32_t getFlashChipSize() { return 4*1024*1024; }
  uint32_t getFlashChipRealSize() { return 4*1024*1024; }
  uint32_t getSketchSize() { return 0; }
  uint32_t getFreeSketchSpace() { return 1024*1024; }
  const char* getSdkVersion() { return "ximodem-host"; }
  void restart();
  void deepSleep(uint64_t) {}
  uint64_t getEfuseMac() { return 0x020000000001ULL; }
};
extern EspClass ESP;
#endif
