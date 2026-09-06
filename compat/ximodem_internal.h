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

// Shared internal glue between the compat shim and the libximodem C-API layer.
#ifndef XIMODEM_INTERNAL_H
#define XIMODEM_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

#define XI_MAX_PINS 50

#ifdef __cplusplus
extern "C" {
#endif

// ---- host cooperative pump ------------------------------------------------
// Called from inside delay()/yield() in the firmware so the host embedder can
// service I/O without a separate thread if it wants. Default: no-op.
void xi_host_yield(void);

// ---- DTE serial link (firmware HWSerial <-> host) ------------------------
// The firmware's HardwareSerial "HWSerial" is bound to these. Bytes the
// firmware writes to the DTE go OUT of the modem (to the X16); bytes the X16
// sends come IN.
int  xi_dte_read(void);                       // -1 if empty  (X16 -> modem)
int  xi_dte_peek(void);
int  xi_dte_available(void);
int  xi_dte_write(const uint8_t *buf, int len);// modem -> X16
int  xi_dte_write_space(void);
void xi_dte_set_baud(uint32_t baud);
void xi_dte_set_format(uint32_t cfg);          // Arduino SERIAL_xxx bitmap

// ---- GPIO / modem-signal bridge ----------------------------------------
void xi_pin_changed(int pin, int level);       // firmware drove an output pin
void xi_pin_force(int pin, int level);         // host sets an input pin level
int  xi_pin_peek(int pin);
void xi_dte_apply_flow(int rtscts);            // firmware toggled RTS/CTS flow control

// ---- debug ------------------------------------------------------------
void xi_debug(const char *fmt, ...);
int  xi_verbose_enabled(void);   // 1 => emit chatty diagnostics to stderr

#ifdef __cplusplus
}
#endif

#endif
