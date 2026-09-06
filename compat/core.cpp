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

// Core Arduino runtime: timing, yield, GPIO -> modem-signal bridge.
#include "Arduino.h"
#include "xi_platform.h"   // xi_random() / xi_srandom() (random/srandom on POSIX, rand/srand on Win)
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdlib>
#include "ximodem_internal.h"
#undef random  // use xi_random() below, not the Arduino macro

static std::chrono::steady_clock::time_point g_epoch = std::chrono::steady_clock::now();

extern "C" unsigned long millis(void) {
  auto d = std::chrono::steady_clock::now() - g_epoch;
  return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}
extern "C" unsigned long micros(void) {
  auto d = std::chrono::steady_clock::now() - g_epoch;
  return (unsigned long)std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}
extern "C" void delay(unsigned long ms) {
  // Cooperative: let the host pump run while we wait.
  auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < until) {
    ximodem_yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
extern "C" void delayMicroseconds(unsigned int us) {
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}
extern "C" void ximodem_yield(void) {
  xi_host_yield();
}

extern "C" long xi_random1(long howbig) {
  return howbig <= 0 ? 0 : (long)((long)xi_random() % howbig);
}
extern "C" long xi_random2(long howsmall, long howbig) {
  if (howsmall >= howbig) return howsmall;
  return howsmall + xi_random1(howbig - howsmall);
}
extern "C" void randomSeed(unsigned long seed) { xi_srandom(seed); }

extern "C" char *dtostrf(double val, signed char width, unsigned char prec, char *s) {
  char fmt[16];
  snprintf(fmt, sizeof(fmt), "%%%d.%uf", (int)width, (unsigned)prec);
  sprintf(s, fmt, val);
  return s;
}
extern "C" char *itoa(int value, char *str, int base) {
  if (base == 10) { sprintf(str, "%d", value); return str; }
  char tmp[34]; const char *d = "0123456789abcdefghijklmnopqrstuvwxyz";
  unsigned v = (base == 10 && value < 0) ? -value : (unsigned)value;
  int i = 0; do { tmp[i++] = d[v % base]; v /= base; } while (v);
  int j = 0; if (base == 10 && value < 0) str[j++] = '-';
  while (i) str[j++] = tmp[--i];
  str[j] = 0; return str;
}
extern "C" char *ltoa(long value, char *str, int base) {
  if (base == 10) { sprintf(str, "%ld", value); return str; }
  return itoa((int)value, str, base);
}
extern "C" char *utoa(unsigned value, char *str, int base) {
  if (base == 10) { sprintf(str, "%u", value); return str; }
  return itoa((int)value, str, base);
}
extern "C" char *ultoa(unsigned long value, char *str, int base) {
  if (base == 10) { sprintf(str, "%lu", value); return str; }
  return itoa((int)value, str, base);
}

// --- GPIO --------------------------------------------------------------------
// The firmware drives/reads modem-control lines through digitalWrite/digitalRead
// on the X16 pin map (see firmware/zimodem.ino INCLUDE_CMDRX16 block). We store a
// flat pin array; xi_signal_* in the C-API layer translates specific pin numbers
// to/from the DTE-visible modem signal enum.
static std::atomic<int> g_pin[XI_MAX_PINS];

// Unconnected GPIOs read HIGH (external pull-ups / active-low convention). This
// matters for the factory-reset pin (GPIO0) and the DTR/OTH inputs, which the
// firmware treats as "inactive" when high.
struct PinInit {
  PinInit() { for (int i = 0; i < XI_MAX_PINS; i++) g_pin[i].store(1); }
} g_pinInit;

extern "C" void pinMode(uint8_t pin, uint8_t mode) { (void)pin; (void)mode; }
extern "C" void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin < XI_MAX_PINS) {
    g_pin[pin].store(val ? 1 : 0);
    xi_pin_changed(pin, val ? 1 : 0);
  }
}
extern "C" int digitalRead(uint8_t pin) {
  return (pin < XI_MAX_PINS) ? g_pin[pin].load() : 0;
}
extern "C" int analogRead(uint8_t pin) { (void)pin; return 0; }
extern "C" void analogWrite(uint8_t pin, int val) { (void)pin; (void)val; }

// Called by the C-API layer to inject a host-driven input line level.
extern "C" void xi_pin_force(int pin, int level) {
  if (pin >= 0 && pin < XI_MAX_PINS) g_pin[pin].store(level ? 1 : 0);
}
extern "C" int xi_pin_peek(int pin) {
  return (pin >= 0 && pin < XI_MAX_PINS) ? g_pin[pin].load() : 0;
}
