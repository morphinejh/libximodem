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

#include "Esp.h"
#include "SPI.h"
#include "Update.h"
#include "driver/uart.h"
#include "ximodem_internal.h"
#include <cstdlib>

EspClass ESP;
SPIClass SPI;
UpdateClass Update;

// ESP-IDF UART driver shims. Pin inversion is modelled at the TL16C2550 layer;
// the RTS/CTS handshake itself is modelled there too, but the DTE-facing byte
// path in libximodem needs to know whether the firmware wants CTS gating, so
// forward the mode.
extern "C" int uart_set_hw_flow_ctrl(uart_port_t, uart_hw_flowcontrol_t mode, uint8_t) {
  xi_dte_apply_flow(mode == UART_HW_FLOWCTRL_CTS_RTS || mode == UART_HW_FLOWCTRL_CTS);
  return 0;
}
extern "C" int uart_set_pin(uart_port_t, int, int, int, int) { return 0; }
extern "C" int uart_set_line_inverse(uart_port_t, uint32_t) { return 0; }
extern "C" int uart_set_baudrate(uart_port_t, uint32_t) { return 0; }

void EspClass::restart() {
  // A host library can't reboot; treat as a no-op (or exit under a debug flag).
}
