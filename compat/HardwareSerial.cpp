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

#include "HardwareSerial.h"
#include "ximodem_internal.h"
#include <cstdio>

// UART 0 in the firmware's pin map is the debug console (DBSerial); everything
// else is the DTE link to the X16 (HWSerial).
HardwareSerial Serial(0);

static bool isDebugUart(int n) { return n == 0; }

void HardwareSerial::begin(unsigned long baud, uint32_t config,
                           int8_t rxPin, int8_t txPin, bool invert) {
  (void)rxPin; (void)txPin; (void)invert;
  if (isDebugUart(_uart_num)) { _isDebug = true; return; }
  xi_dte_set_baud((uint32_t)baud);
  xi_dte_set_format(config);
}

void HardwareSerial::updateBaudRate(unsigned long baud) {
  if (!isDebugUart(_uart_num)) xi_dte_set_baud((uint32_t)baud);
}

int HardwareSerial::available() {
  return isDebugUart(_uart_num) ? 0 : xi_dte_available();
}
int HardwareSerial::availableForWrite() {
  return isDebugUart(_uart_num) ? 4096 : xi_dte_write_space();
}
int HardwareSerial::peek() {
  return isDebugUart(_uart_num) ? -1 : xi_dte_peek();
}
int HardwareSerial::read() {
  return isDebugUart(_uart_num) ? -1 : xi_dte_read();
}

// The debug UART (DBSerial) carries the firmware's debugPrintf() chatter. It is
// swallowed unless verbose diagnostics are enabled; the DTE UART is untouched.
size_t HardwareSerial::write(uint8_t c) {
  if (isDebugUart(_uart_num) || _isDebug) {
    if (xi_verbose_enabled()) fputc(c, stderr);
    return 1;
  }
  return xi_dte_write(&c, 1);
}
size_t HardwareSerial::write(const uint8_t *buffer, size_t size) {
  if (isDebugUart(_uart_num) || _isDebug) {
    if (xi_verbose_enabled()) fwrite(buffer, 1, size, stderr);
    return size;
  }
  return xi_dte_write(buffer, (int)size);
}
void HardwareSerial::flush() {
  if (isDebugUart(_uart_num) || _isDebug) {
    if (xi_verbose_enabled()) fflush(stderr);
  }
}
