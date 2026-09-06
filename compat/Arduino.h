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

// ximodem host Arduino compatibility shim.
// Provides just enough of the Arduino / ESP32 Arduino-core surface for the
// Zimodem X16 firmware to build and run as a host shared library.
#ifndef XIMODEM_ARDUINO_H
#define XIMODEM_ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

#ifdef __cplusplus
#include <algorithm>
using std::min;
using std::max;
#endif

// --- pin / digital constants -------------------------------------------------
#define HIGH 0x1
#define LOW  0x0
#define INPUT        0x01
#define OUTPUT       0x03
#define INPUT_PULLUP 0x05
#define INPUT_PULLDOWN 0x09
#define LSBFIRST 0
#define MSBFIRST 1
#define CHANGE 3
#define FALLING 2
#define RISING 1
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

// GPIO_NUM_* used by the X16 pin map in zimodem.ino
#ifndef GPIO_NUM_0
enum {
  GPIO_NUM_0=0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5,
  GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11,
  GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15, GPIO_NUM_16, GPIO_NUM_17,
  GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_20, GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23,
  GPIO_NUM_24, GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_28, GPIO_NUM_29,
  GPIO_NUM_30, GPIO_NUM_31, GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_34, GPIO_NUM_35,
  GPIO_NUM_36, GPIO_NUM_37, GPIO_NUM_38, GPIO_NUM_39
};
#endif

// --- misc macros -----------------------------------------------------------
#define PROGMEM
#define PGM_P const char *
#define PSTR(s) (s)
#define F(s) (s)
#define FPSTR(s) (reinterpret_cast<const char *>(s))
#ifndef _BV
#define _BV(b) (1UL << (b))
#endif
#define bitRead(v,b)   (((v) >> (b)) & 0x1)
#define bitSet(v,b)    ((v) |= _BV(b))
#define bitClear(v,b)  ((v) &= ~_BV(b))
#define constrain(a,l,h) ((a)<(l)?(l):((a)>(h)?(h):(a)))
#ifndef sq
#define sq(x) ((x)*(x))
#endif
#define interrupts()
#define noInterrupts()
#define yield() ximodem_yield()

// Arduino random() collides with glibc random(void); route through xi_random*.
#define XI_RAND_GET(_1, _2, NAME, ...) NAME
#define random(...) XI_RAND_GET(__VA_ARGS__, xi_random2, xi_random1)(__VA_ARGS__)

typedef uint8_t byte;
typedef uint8_t boolean;
typedef unsigned int word;

// pgm_read helpers (host has a flat address space)
#define pgm_read_byte(addr)   (*(const uint8_t *)(addr))
#define pgm_read_word(addr)   (*(const uint16_t *)(addr))
#define pgm_read_dword(addr)  (*(const uint32_t *)(addr))
#define pgm_read_ptr(addr)    (*(void * const *)(addr))
#define pgm_read_byte_near(addr)  pgm_read_byte(addr)
#define pgm_read_word_near(addr)  pgm_read_word(addr)
#define pgm_read_byte_far(addr)   pgm_read_byte(addr)
#define strncpy_P  strncpy
#define strcpy_P   strcpy
#define strcmp_P   strcmp
#define strlen_P   strlen
#define snprintf_P snprintf
#define sprintf_P  sprintf
#define memcpy_P   memcpy

#ifdef __cplusplus
extern "C" {
#endif

// --- timing / core ---------------------------------------------------------
unsigned long millis(void);
unsigned long micros(void);
void          delay(unsigned long ms);
void          delayMicroseconds(unsigned int us);
void          ximodem_yield(void);

// --- gpio (mapped onto the modem-control signal bridge, see compat/gpio.cpp)
void          pinMode(uint8_t pin, uint8_t mode);
void          digitalWrite(uint8_t pin, uint8_t val);
int           digitalRead(uint8_t pin);
int           analogRead(uint8_t pin);
void          analogWrite(uint8_t pin, int val);

long          xi_random1(long howbig);
long          xi_random2(long howsmall, long howbig);
void          randomSeed(unsigned long seed);
char *        itoa(int value, char *str, int base);
char *        ltoa(long value, char *str, int base);
char *        utoa(unsigned value, char *str, int base);
char *        ultoa(unsigned long value, char *str, int base);
char *        dtostrf(double val, signed char width, unsigned char prec, char *s);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include "WString.h"
#include "Print.h"
#include "Stream.h"
#include "IPAddress.h"
#endif

#endif // XIMODEM_ARDUINO_H
