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

#ifndef XIMODEM_STREAM_H
#define XIMODEM_STREAM_H

#include "Print.h"
#include "WString.h"

class Stream : public Print {
  protected:
    unsigned long _timeout = 1000;
    unsigned long _startMillis = 0;
    int timedRead();
    int timedPeek();
    int peekNextDigit(bool detectDecimal = false);

  public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;

    Stream() {}
    virtual ~Stream() {}

    void setTimeout(unsigned long timeout) { _timeout = timeout; }
    unsigned long getTimeout(void) { return _timeout; }

    bool find(const char *target);
    bool find(const char *target, size_t length);
    bool findUntil(const char *target, const char *terminator);

    long parseInt();
    float parseFloat();

    virtual size_t readBytes(char *buffer, size_t length);
    virtual size_t readBytes(uint8_t *buffer, size_t length) {
      return readBytes((char *)buffer, length);
    }
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    size_t readBytesUntil(char terminator, uint8_t *buffer, size_t length) {
      return readBytesUntil(terminator, (char *)buffer, length);
    }

    virtual String readString();
    virtual String readStringUntil(char terminator);
};

#endif
