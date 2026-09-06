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

#include "WString.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cctype>

static std::string numToStr(long v, int base) {
  if (base == 10) return std::to_string(v);
  char buf[34];
  const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
  bool neg = false;
  unsigned long u;
  if (v < 0 && base == 10) { neg = true; u = (unsigned long)(-v); }
  else u = (unsigned long)v;
  int i = sizeof(buf);
  buf[--i] = 0;
  do { buf[--i] = digits[u % base]; u /= base; } while (u && i > 0);
  std::string r(&buf[i]);
  if (neg) r.insert(r.begin(), '-');
  return r;
}
static std::string uNumToStr(unsigned long v, int base) {
  if (base == 10) return std::to_string(v);
  char buf[34];
  const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
  int i = sizeof(buf);
  buf[--i] = 0;
  do { buf[--i] = digits[v % base]; v /= base; } while (v && i > 0);
  return std::string(&buf[i]);
}

String::String(int v, int base)           : s(numToStr(v, base)) {}
String::String(unsigned int v, int base)  : s(uNumToStr(v, base)) {}
String::String(long v, int base)          : s(numToStr(v, base)) {}
String::String(unsigned long v, int base) : s(uNumToStr(v, base)) {}
String::String(float v, int dp) {
  char buf[40]; snprintf(buf, sizeof(buf), "%.*f", dp, (double)v); s = buf;
}
String::String(double v, int dp) {
  char buf[40]; snprintf(buf, sizeof(buf), "%.*f", dp, v); s = buf;
}

static char zeroCh = 0;
char &String::operator[](unsigned int i) {
  if (i >= s.size()) { zeroCh = 0; return zeroCh; }
  return s[i];
}

bool String::equalsIgnoreCase(const String &o) const {
  if (s.size() != o.s.size()) return false;
  for (size_t i = 0; i < s.size(); i++)
    if (tolower((unsigned char)s[i]) != tolower((unsigned char)o.s[i])) return false;
  return true;
}
bool String::startsWith(const String &p) const {
  return s.size() >= p.s.size() && s.compare(0, p.s.size(), p.s) == 0;
}
bool String::endsWith(const String &suf) const {
  return s.size() >= suf.s.size() &&
         s.compare(s.size() - suf.s.size(), suf.s.size(), suf.s) == 0;
}

int String::indexOf(char ch) const { auto p = s.find(ch); return p == std::string::npos ? -1 : (int)p; }
int String::indexOf(char ch, unsigned int from) const { auto p = s.find(ch, from); return p == std::string::npos ? -1 : (int)p; }
int String::indexOf(const String &str) const { auto p = s.find(str.s); return p == std::string::npos ? -1 : (int)p; }
int String::indexOf(const String &str, unsigned int from) const { auto p = s.find(str.s, from); return p == std::string::npos ? -1 : (int)p; }
int String::lastIndexOf(char ch) const { auto p = s.rfind(ch); return p == std::string::npos ? -1 : (int)p; }
int String::lastIndexOf(const String &str) const { auto p = s.rfind(str.s); return p == std::string::npos ? -1 : (int)p; }

String String::substring(unsigned int b) const {
  if (b >= s.size()) return String();
  return String(s.substr(b));
}
String String::substring(unsigned int b, unsigned int e) const {
  if (b > e) std::swap(b, e);
  if (b >= s.size()) return String();
  if (e > s.size()) e = s.size();
  return String(s.substr(b, e - b));
}

void String::replace(char f, char r) { std::replace(s.begin(), s.end(), f, r); }
void String::replace(const String &f, const String &r) {
  if (f.s.empty()) return;
  size_t pos = 0;
  while ((pos = s.find(f.s, pos)) != std::string::npos) {
    s.replace(pos, f.s.size(), r.s);
    pos += r.s.size();
  }
}
void String::remove(unsigned int index) {
  if (index < s.size()) s.erase(index);
}
void String::remove(unsigned int index, unsigned int count) {
  if (index < s.size()) s.erase(index, count);
}
void String::toLowerCase() { for (auto &c : s) c = tolower((unsigned char)c); }
void String::toUpperCase() { for (auto &c : s) c = toupper((unsigned char)c); }
void String::trim() {
  size_t b = s.find_first_not_of(" \t\r\n");
  size_t e = s.find_last_not_of(" \t\r\n");
  if (b == std::string::npos) s.clear();
  else s = s.substr(b, e - b + 1);
}
