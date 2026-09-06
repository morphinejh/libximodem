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

#ifndef XIMODEM_FS_H
#define XIMODEM_FS_H

#include "Stream.h"
#include "WString.h"
#include <cstdio>
#include <memory>
#include <string>
#include <dirent.h>

#define FILE_READ   "r"
#define FILE_WRITE  "w"
#define FILE_APPEND "a"

namespace fs {

class FileImpl;

class File : public Stream {
  private:
    std::shared_ptr<FileImpl> _p;
  public:
    File() {}
    File(std::shared_ptr<FileImpl> p) : _p(p) {}

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buf, size_t size) override;
    using Print::write;
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    size_t read(uint8_t *buf, size_t size);
    size_t readBytes(char *buffer, size_t length) override;
    // A file has a real EOF; don't run Stream::timedRead's _timeout spin (1 s of
    // busy-wait at end-of-file on every config / phonebook / listener load).
    String readString() override;
    String readStringUntil(char terminator) override;

    bool seek(uint32_t pos, int mode);
    bool seek(uint32_t pos) { return seek(pos, SEEK_SET); }
    size_t position() const;
    size_t size() const;
    void close();
    operator bool() const;
    const char *name() const;
    const char *path() const;
    bool isDirectory();
    File openNextFile(const char *mode = FILE_READ);
    void rewindDirectory();
};

class FS {
  private:
    std::string _root;
  public:
    FS() {}
    void setRoot(const std::string &r) { _root = r; }
    const std::string &root() const { return _root; }

    bool begin(bool formatOnFail = false);
    void end() {}
    bool format();

    File open(const char *path, const char *mode = FILE_READ);
    File open(const String &path, const char *mode = FILE_READ) { return open(path.c_str(), mode); }
    bool exists(const char *path);
    bool exists(const String &path) { return exists(path.c_str()); }
    bool remove(const char *path);
    bool remove(const String &path) { return remove(path.c_str()); }
    bool rename(const char *from, const char *to);
    bool rename(const String &from, const String &to) { return rename(from.c_str(), to.c_str()); }
    bool mkdir(const char *path);
    bool rmdir(const char *path);

    size_t totalBytes() { return 0; }
    size_t usedBytes()  { return 0; }
    int    cardType()   { return 1; }

    std::string hostPath(const char *path);
};

} // namespace fs

using fs::File;
using fs::FS;

#endif
