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

#include "Stream.h"
#include "Arduino.h"

int Stream::timedRead() {
  _startMillis = millis();
  int c;
  do {
    c = read();
    if (c >= 0) return c;
    ximodem_yield();
  } while (millis() - _startMillis < _timeout);
  return -1;
}

int Stream::timedPeek() {
  _startMillis = millis();
  int c;
  do {
    c = peek();
    if (c >= 0) return c;
    ximodem_yield();
  } while (millis() - _startMillis < _timeout);
  return -1;
}

int Stream::peekNextDigit(bool detectDecimal) {
  int c;
  while (1) {
    c = timedPeek();
    if (c < 0) return c;
    if (c == '-') return c;
    if (c >= '0' && c <= '9') return c;
    if (detectDecimal && c == '.') return c;
    read();
  }
}

bool Stream::find(const char *target) { return findUntil(target, ""); }
bool Stream::find(const char *target, size_t length) {
  (void)length; return findUntil(target, "");
}
bool Stream::findUntil(const char *target, const char *terminator) {
  size_t tlen = strlen(target);
  size_t termLen = strlen(terminator);
  size_t index = 0, termIndex = 0;
  if (tlen == 0) return true;
  int c;
  while ((c = timedRead()) > 0) {
    if (c == target[index]) {
      if (++index >= tlen) return true;
    } else {
      index = (c == target[0]) ? 1 : 0;
    }
    if (termLen > 0) {
      if (c == terminator[termIndex]) {
        if (++termIndex >= termLen) return false;
      } else {
        termIndex = (c == terminator[0]) ? 1 : 0;
      }
    }
  }
  return false;
}

long Stream::parseInt() {
  bool isNegative = false;
  long value = 0;
  int c = peekNextDigit(false);
  if (c < 0) return 0;
  do {
    if (c == '-') isNegative = true;
    else if (c >= '0' && c <= '9') value = value * 10 + (c - '0');
    read();
    c = timedPeek();
  } while ((c >= '0' && c <= '9') || c == '-');
  return isNegative ? -value : value;
}

float Stream::parseFloat() {
  bool isNegative = false, isFraction = false;
  double value = 0.0;
  double frac = 1.0;
  int c = peekNextDigit(true);
  if (c < 0) return 0;
  do {
    if (c == '-') isNegative = true;
    else if (c == '.') isFraction = true;
    else if (c >= '0' && c <= '9') {
      if (isFraction) { frac *= 0.1; value += (c - '0') * frac; }
      else value = value * 10 + (c - '0');
    }
    read();
    c = timedPeek();
  } while ((c >= '0' && c <= '9') || (c == '.' && !isFraction));
  return isNegative ? -value : value;
}

size_t Stream::readBytes(char *buffer, size_t length) {
  size_t count = 0;
  while (count < length) {
    int c = timedRead();
    if (c < 0) break;
    *buffer++ = (char)c;
    count++;
  }
  return count;
}

size_t Stream::readBytesUntil(char terminator, char *buffer, size_t length) {
  size_t count = 0;
  while (count < length) {
    int c = timedRead();
    if (c < 0 || c == terminator) break;
    *buffer++ = (char)c;
    count++;
  }
  return count;
}

String Stream::readString() {
  String ret;
  int c = timedRead();
  while (c >= 0) { ret += (char)c; c = timedRead(); }
  return ret;
}

String Stream::readStringUntil(char terminator) {
  String ret;
  int c = timedRead();
  while (c >= 0 && c != terminator) { ret += (char)c; c = timedRead(); }
  return ret;
}
