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

#include "Print.h"
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <cstring>

size_t Print::write(const uint8_t *buffer, size_t size) {
  size_t n = 0;
  while (size--) {
    if (write(*buffer++)) n++;
    else break;
  }
  return n;
}

size_t Print::print(const String &s) { return write(s.c_str(), s.length()); }
size_t Print::print(const char *s) { return s ? write(s) : 0; }
size_t Print::print(char c) { return write((uint8_t)c); }
size_t Print::print(unsigned char n, int base) { return print((unsigned long)n, base); }
size_t Print::print(int n, int base) { return print((long)n, base); }
size_t Print::print(unsigned int n, int base) { return print((unsigned long)n, base); }
size_t Print::print(long n, int base) {
  if (base == 10 && n < 0) { size_t t = write('-'); return t + printNumber(-n, 10); }
  return printNumber(n, base);
}
size_t Print::print(unsigned long n, int base) { return printNumber(n, base); }
size_t Print::print(double n, int digits) { return printFloat(n, digits); }

size_t Print::println(void) { return write("\r\n"); }
size_t Print::println(const String &s) { return print(s) + println(); }
size_t Print::println(const char *s) { return print(s) + println(); }
size_t Print::println(char c) { return print(c) + println(); }
size_t Print::println(unsigned char n, int base) { return print(n, base) + println(); }
size_t Print::println(int n, int base) { return print(n, base) + println(); }
size_t Print::println(unsigned int n, int base) { return print(n, base) + println(); }
size_t Print::println(long n, int base) { return print(n, base) + println(); }
size_t Print::println(unsigned long n, int base) { return print(n, base) + println(); }
size_t Print::println(double n, int digits) { return print(n, digits) + println(); }

size_t Print::printNumber(unsigned long n, uint8_t base) {
  char buf[8 * sizeof(long) + 1];
  char *str = &buf[sizeof(buf) - 1];
  *str = '\0';
  if (base < 2) base = 10;
  do {
    char c = n % base;
    n /= base;
    *--str = c < 10 ? c + '0' : c + 'A' - 10;
  } while (n);
  return write(str);
}

size_t Print::printFloat(double number, uint8_t digits) {
  char fmt[16];
  snprintf(fmt, sizeof(fmt), "%%.%uf", (unsigned)digits);
  char buf[64];
  snprintf(buf, sizeof(buf), fmt, number);
  return write(buf);
}

size_t Print::printf(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char stackbuf[256];
  va_list ap2;
  va_copy(ap2, ap);
  int need = vsnprintf(stackbuf, sizeof(stackbuf), format, ap2);
  va_end(ap2);
  size_t written = 0;
  if (need < 0) { va_end(ap); return 0; }
  if ((size_t)need < sizeof(stackbuf)) {
    written = write((const uint8_t *)stackbuf, need);
  } else {
    char *heap = (char *)malloc(need + 1);
    if (heap) {
      vsnprintf(heap, need + 1, format, ap);
      written = write((const uint8_t *)heap, need);
      free(heap);
    }
  }
  va_end(ap);
  return written;
}
