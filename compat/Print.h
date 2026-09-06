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

#ifndef XIMODEM_PRINT_H
#define XIMODEM_PRINT_H

#include <stdint.h>
#include <stddef.h>
#include "WString.h"

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class Print {
  private:
    int write_error = 0;
    size_t printNumber(unsigned long n, uint8_t base);
    size_t printFloat(double number, uint8_t digits);
  protected:
    void setWriteError(int err = 1) { write_error = err; }
  public:
    Print() {}
    virtual ~Print() {}

    int getWriteError() { return write_error; }
    void clearWriteError() { setWriteError(0); }

    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size);
    size_t write(const char *str) {
      if (!str) return 0;
      return write((const uint8_t *)str, strlen(str));
    }
    size_t write(const char *buffer, size_t size) {
      return write((const uint8_t *)buffer, size);
    }
    virtual int availableForWrite() { return 0; }

    size_t print(const String &s);
    size_t print(const char *s);
    size_t print(char c);
    size_t print(unsigned char n, int base = DEC);
    size_t print(int n, int base = DEC);
    size_t print(unsigned int n, int base = DEC);
    size_t print(long n, int base = DEC);
    size_t print(unsigned long n, int base = DEC);
    size_t print(double n, int digits = 2);

    size_t println(const String &s);
    size_t println(const char *s);
    size_t println(char c);
    size_t println(unsigned char n, int base = DEC);
    size_t println(int n, int base = DEC);
    size_t println(unsigned int n, int base = DEC);
    size_t println(long n, int base = DEC);
    size_t println(unsigned long n, int base = DEC);
    size_t println(double n, int digits = 2);
    size_t println(void);

    size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

    virtual void flush() {}
};

#endif
