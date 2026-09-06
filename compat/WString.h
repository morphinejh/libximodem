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

#ifndef XIMODEM_WSTRING_H
#define XIMODEM_WSTRING_H

#include <string>
#include <cstring>
#include <cstdint>

// Arduino String, reimplemented over std::string for the host build.
class String {
  private:
    std::string s;
  public:
    String() {}
    String(const char *cstr) : s(cstr ? cstr : "") {}
    String(const String &o) : s(o.s) {}
    String(const std::string &o) : s(o) {}
    explicit String(char c) : s(1, c) {}
    explicit String(unsigned char c) : s(1, (char)c) {}
    explicit String(int v, int base = 10);
    explicit String(unsigned int v, int base = 10);
    explicit String(long v, int base = 10);
    explicit String(unsigned long v, int base = 10);
    explicit String(float v, int decimalPlaces = 2);
    explicit String(double v, int decimalPlaces = 2);
    ~String() {}

    // capacity
    unsigned int length() const { return (unsigned int)s.length(); }
    bool reserve(unsigned int n) { s.reserve(n); return true; }
    void clear() { s.clear(); }
    bool isEmpty() const { return s.empty(); }

    const char *c_str() const { return s.c_str(); }
    char charAt(unsigned int i) const { return i < s.size() ? s[i] : 0; }
    void setCharAt(unsigned int i, char c) { if (i < s.size()) s[i] = c; }
    char operator[](unsigned int i) const { return i < s.size() ? s[i] : 0; }
    char &operator[](unsigned int i);

    // assignment
    String &operator=(const String &o) { s = o.s; return *this; }
    String &operator=(const char *cstr) { s = cstr ? cstr : ""; return *this; }
    String &operator=(char c) { s.assign(1, c); return *this; }

    // concatenation
    bool concat(const String &o) { s += o.s; return true; }
    bool concat(const char *cstr) { if (cstr) s += cstr; return true; }
    bool concat(char c) { s += c; return true; }
    bool concat(int v)          { s += String(v).s; return true; }
    bool concat(unsigned int v) { s += String(v).s; return true; }
    bool concat(long v)         { s += String(v).s; return true; }
    bool concat(unsigned long v){ s += String(v).s; return true; }
    bool concat(double v)       { s += String(v).s; return true; }

    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *cstr) { if (cstr) s += cstr; return *this; }
    String &operator+=(char c) { s += c; return *this; }
    String &operator+=(int v)          { concat(v); return *this; }
    String &operator+=(unsigned int v) { concat(v); return *this; }
    String &operator+=(long v)         { concat(v); return *this; }
    String &operator+=(unsigned long v){ concat(v); return *this; }
    String &operator+=(double v)       { concat(v); return *this; }

    // comparison
    int compareTo(const String &o) const { return s.compare(o.s); }
    bool equals(const String &o) const { return s == o.s; }
    bool equals(const char *cstr) const { return s == (cstr ? cstr : ""); }
    bool operator==(const String &o) const { return s == o.s; }
    bool operator==(const char *cstr) const { return s == (cstr ? cstr : ""); }
    bool operator!=(const String &o) const { return s != o.s; }
    bool operator!=(const char *cstr) const { return !(*this == cstr); }
    bool operator<(const String &o) const { return s < o.s; }
    bool operator>(const String &o) const { return s > o.s; }
    bool operator<=(const String &o) const { return s <= o.s; }
    bool operator>=(const String &o) const { return s >= o.s; }
    bool equalsIgnoreCase(const String &o) const;
    bool startsWith(const String &prefix) const;
    bool endsWith(const String &suffix) const;

    // search
    int indexOf(char ch) const;
    int indexOf(char ch, unsigned int fromIndex) const;
    int indexOf(const String &str) const;
    int indexOf(const String &str, unsigned int fromIndex) const;
    int lastIndexOf(char ch) const;
    int lastIndexOf(const String &str) const;

    // modification
    String substring(unsigned int beginIndex) const;
    String substring(unsigned int beginIndex, unsigned int endIndex) const;
    void replace(char find, char replace);
    void replace(const String &find, const String &replace);
    void remove(unsigned int index);
    void remove(unsigned int index, unsigned int count);
    void toLowerCase();
    void toUpperCase();
    void trim();

    // conversion
    long toInt() const { return strtol(s.c_str(), nullptr, 10); }
    float toFloat() const { return strtof(s.c_str(), nullptr); }
    double toDouble() const { return strtod(s.c_str(), nullptr); }

    const std::string &str() const { return s; }
};

inline String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, const char *b)   { String r(a); r += b; return r; }
inline String operator+(const char *a, const String &b)   { String r(a); r += b; return r; }
inline String operator+(const String &a, char b)          { String r(a); r += b; return r; }
inline String operator+(char a, const String &b)          { String r; r += a; r += b; return r; }
inline String operator+(const String &a, int b)           { String r(a); r += b; return r; }
inline String operator+(const String &a, unsigned int b)  { String r(a); r += b; return r; }
inline String operator+(const String &a, long b)          { String r(a); r += b; return r; }
inline String operator+(const String &a, unsigned long b) { String r(a); r += b; return r; }
inline String operator+(const String &a, double b)        { String r(a); r += b; return r; }

typedef String StringSumHelper;

#endif
