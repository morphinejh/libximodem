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

#ifndef XIMODEM_HARDWARESERIAL_H
#define XIMODEM_HARDWARESERIAL_H

#include "Stream.h"
#include <stdint.h>

// Arduino SERIAL_xxx line-format constants (ESP32 core values).
#define SERIAL_5N1 0x8000010
#define SERIAL_6N1 0x8000014
#define SERIAL_7N1 0x8000018
#define SERIAL_8N1 0x800001c
#define SERIAL_5N2 0x8000030
#define SERIAL_6N2 0x8000034
#define SERIAL_7N2 0x8000038
#define SERIAL_8N2 0x800003c
#define SERIAL_5E1 0x8000012
#define SERIAL_6E1 0x8000016
#define SERIAL_7E1 0x800001a
#define SERIAL_8E1 0x800001e
#define SERIAL_5O1 0x8000013
#define SERIAL_6O1 0x8000017
#define SERIAL_7O1 0x800001b
#define SERIAL_8O1 0x800001f

typedef uint32_t SerialConfig;

class HardwareSerial : public Stream {
  private:
    int _uart_num;
    bool _isDebug = false;
  public:
    HardwareSerial(int uart_nr = 0) : _uart_num(uart_nr) {}

    void begin(unsigned long baud, uint32_t config = SERIAL_8N1,
               int8_t rxPin = -1, int8_t txPin = -1, bool invert = false);
    void end() {}
    void updateBaudRate(unsigned long baud);
    void setRxBufferSize(size_t size) { (void)size; }
    void setTxBufferSize(size_t size) { (void)size; }
    void setDebugOutput(bool en) { _isDebug = en; }
    void setPins(int8_t rx, int8_t tx, int8_t cts = -1, int8_t rts = -1) {
      (void)rx; (void)tx; (void)cts; (void)rts;
    }

    int  available() override;
    int  availableForWrite() override;
    int  peek() override;
    int  read() override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;
    void flush() override;

    operator bool() const { return true; }
};

extern HardwareSerial Serial;

#endif
