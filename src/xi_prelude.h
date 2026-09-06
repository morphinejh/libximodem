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

// Included at the very top of the Zimodem unity translation unit.
// Establishes the host platform identity and pulls in the Arduino/ESP shim.
#ifndef XI_PRELUDE_H
#define XI_PRELUDE_H

// Force the ESP32 code paths in the firmware (SSH/SD/CBM support, X16 pin map).
// zimodem.ino's platform chain keys off ARDUINO_ARCH_ESP32; setting it here
// makes that chain pick ESP32 cleanly (otherwise it falls through to the
// ESP8266 branch, which also `#undef`s INCLUDE_SSH).
#ifndef ARDUINO_ARCH_ESP32
#define ARDUINO_ARCH_ESP32 1
#endif
#ifndef ZIMODEM_ESP32
#define ZIMODEM_ESP32 1
#endif
// The vendored libssh2 headers wrap their entire contents in `#if defined(ESP32)`.
#ifndef ESP32
#define ESP32 1
#endif
#define ZIMODEM_HOST 1
#include "ximodem.h"   /* XIMODEM_VERSION */

// Make the firmware select its Commander X16 I/O-card configuration.
#define INCLUDE_CMDRX16 true

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>

#include "../compat/Arduino.h"
#include "../compat/HardwareSerial.h"
#include "../compat/Esp.h"
#include "../compat/WiFi.h"
#include "../compat/FS.h"
#include "../compat/SPIFFS.h"
#include "../compat/SD.h"
#include "../compat/SPI.h"
#include "../compat/Update.h"
#include "../compat/ximodem_internal.h"

#endif
