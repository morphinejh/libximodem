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

#include "FS.h"
#include "xi_platform.h"   // xi_mkdir(); pulls <sys/stat.h>, <direct.h> as needed
#include <sys/stat.h>
#include <cstring>

namespace fs {

class FileImpl {
  public:
    FILE *fp = nullptr;
    DIR *dir = nullptr;
    std::string vpath;   // virtual (firmware-visible) path, e.g. "/zconfig.txt"
    std::string hpath;   // host path
    bool isDir = false;
    ~FileImpl() {
      if (fp) fclose(fp);
      if (dir) closedir(dir);
    }
};

std::string FS::hostPath(const char *path) {
  std::string p = path ? path : "";
  if (!p.empty() && p[0] != '/') p = "/" + p;
  return _root + p;
}

bool FS::begin(bool formatOnFail) {
  (void)formatOnFail;
  if (_root.empty()) _root = "./ximodem-data";
  xi_mkdir(_root.c_str());
  struct stat st;
  return stat(_root.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool FS::format() {
  // Wipe regular files in the root (non-recursive is enough for SPIFFS use).
  DIR *d = opendir(_root.c_str());
  if (!d) return begin();
  struct dirent *e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    std::string f = _root + "/" + e->d_name;
    ::remove(f.c_str());
  }
  closedir(d);
  return true;
}

File FS::open(const char *path, const char *mode) {
  auto impl = std::make_shared<FileImpl>();
  impl->vpath = path ? path : "";
  impl->hpath = hostPath(path);

  struct stat st;
  if (stat(impl->hpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    impl->isDir = true;
    impl->dir = opendir(impl->hpath.c_str());
    if (!impl->dir) return File();
    return File(impl);
  }

  const char *m = mode ? mode : "r";
  // Arduino modes map close enough to stdio; ensure binary.
  char bm[4] = {0};
  bm[0] = m[0];
  bm[1] = 'b';
  if (m[0] == 'w' || m[0] == 'a') { bm[2] = '+'; }
  impl->fp = fopen(impl->hpath.c_str(), bm);
  if (!impl->fp) return File();
  return File(impl);
}

bool FS::exists(const char *path) {
  struct stat st;
  return stat(hostPath(path).c_str(), &st) == 0;
}
bool FS::remove(const char *path) { return ::remove(hostPath(path).c_str()) == 0; }
bool FS::rename(const char *from, const char *to) {
  return ::rename(hostPath(from).c_str(), hostPath(to).c_str()) == 0;
}
bool FS::mkdir(const char *path) { return xi_mkdir(hostPath(path).c_str()) == 0; }
bool FS::rmdir(const char *path) { return ::rmdir(hostPath(path).c_str()) == 0; }

// ---- File ------------------------------------------------------------------
size_t File::write(uint8_t c) { return _p && _p->fp ? fwrite(&c, 1, 1, _p->fp) : 0; }
size_t File::write(const uint8_t *buf, size_t size) {
  return _p && _p->fp ? fwrite(buf, 1, size, _p->fp) : 0;
}
int File::available() {
  if (!_p || !_p->fp) return 0;
  long cur = ftell(_p->fp);
  fseek(_p->fp, 0, SEEK_END);
  long end = ftell(_p->fp);
  fseek(_p->fp, cur, SEEK_SET);
  return (int)(end - cur);
}
int File::read() {
  if (!_p || !_p->fp) return -1;
  int c = fgetc(_p->fp);
  return c == EOF ? -1 : c;
}
int File::peek() {
  if (!_p || !_p->fp) return -1;
  int c = fgetc(_p->fp);
  if (c != EOF) ungetc(c, _p->fp);
  return c == EOF ? -1 : c;
}
void File::flush() { if (_p && _p->fp) fflush(_p->fp); }
size_t File::read(uint8_t *buf, size_t size) {
  return _p && _p->fp ? fread(buf, 1, size, _p->fp) : 0;
}
size_t File::readBytes(char *buffer, size_t length) {
  return _p && _p->fp ? fread(buffer, 1, length, _p->fp) : 0;
}
String File::readString() {
  String s;
  if (_p && _p->fp) {
    int c;
    while ((c = fgetc(_p->fp)) != EOF) s += (char)c;
  }
  return s;
}
String File::readStringUntil(char terminator) {
  String s;
  if (_p && _p->fp) {
    int c;
    while ((c = fgetc(_p->fp)) != EOF && (char)c != terminator) s += (char)c;
  }
  return s;
}
bool File::seek(uint32_t pos, int mode) {
  return _p && _p->fp ? fseek(_p->fp, pos, mode) == 0 : false;
}
size_t File::position() const { return _p && _p->fp ? ftell(_p->fp) : 0; }
size_t File::size() const {
  if (!_p || !_p->fp) return 0;
  long cur = ftell(_p->fp);
  fseek(_p->fp, 0, SEEK_END);
  long end = ftell(_p->fp);
  fseek(_p->fp, cur, SEEK_SET);
  return (size_t)end;
}
void File::close() {
  if (_p) {
    if (_p->fp) { fclose(_p->fp); _p->fp = nullptr; }
    if (_p->dir) { closedir(_p->dir); _p->dir = nullptr; }
  }
}
File::operator bool() const { return _p && (_p->fp || _p->dir); }
const char *File::name() const { return _p ? _p->vpath.c_str() : ""; }
const char *File::path() const { return _p ? _p->vpath.c_str() : ""; }
bool File::isDirectory() { return _p && _p->isDir; }
void File::rewindDirectory() { if (_p && _p->dir) rewinddir(_p->dir); }
File File::openNextFile(const char *mode) {
  (void)mode;
  if (!_p || !_p->dir) return File();
  struct dirent *e;
  while ((e = readdir(_p->dir))) {
    if (e->d_name[0] == '.') continue;
    auto impl = std::make_shared<FileImpl>();
    impl->vpath = _p->vpath + "/" + e->d_name;
    impl->hpath = _p->hpath + "/" + e->d_name;
    struct stat st;
    if (stat(impl->hpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
      impl->isDir = true;
      impl->dir = opendir(impl->hpath.c_str());
    } else {
      impl->fp = fopen(impl->hpath.c_str(), "rb");
    }
    return File(impl);
  }
  return File();
}

} // namespace fs
