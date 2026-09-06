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

#ifndef XIMODEM_SD_H
#define XIMODEM_SD_H
#include "FS.h"

// Card-type constants (ESP32 core)
#define CARD_NONE   0
#define CARD_MMC    1
#define CARD_SD     2
#define CARD_SDHC   3
#define CARD_UNKNOWN 4

namespace fs {
class SDFS : public FS {
  public:
    bool begin(uint8_t ssPin = 0) { (void)ssPin; return FS::begin(); }
    bool begin() { return FS::begin(); }
};
}
extern fs::SDFS SD;
#endif
