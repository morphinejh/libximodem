#!/usr/bin/env python3
#
# Part of libximodem's build system.
#
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jason Hill
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use
# this file except in compliance with the License. You may obtain a copy at
# http://www.apache.org/licenses/LICENSE-2.0 . Unless required by applicable law
# or agreed to in writing, software distributed under the License is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND.
#
# disclosure: ai-assisted (claude-sonnet-5), manual review -- see AI_DISCLOSURE.md
"""Reconstruct the buildable Zimodem sketch from the pinned upstream snapshot
plus the ximodem patch series.

    external/zimodem/       pristine Zimodem @ external/COMMIT  (never hand-edited)
    patches/SERIES          ordered list of patches to apply
      + patches/*.patch
        |
        v
    build/vendor/firmware/  generated -- what gen_unity.py and the compiler read

Run automatically by CMake (configure + every build); also runnable by hand:
    python3 scripts/patch_firmware.py [--verbose]

Line endings are normalised to LF on copy so the byte-exact patch context matches
regardless of how git checked the snapshot out (upstream ships CRLF).
"""
import os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC  = os.path.join(ROOT, "external", "zimodem")
DST  = os.path.join(ROOT, "build", "vendor", "firmware")
SERIES = os.path.join(ROOT, "patches", "SERIES")
COMMIT = os.path.join(ROOT, "external", "COMMIT")

TEXT_EXT = (".ino", ".h", ".c", ".cpp", ".hpp")
VERBOSE = "--verbose" in sys.argv or "-v" in sys.argv


def die(msg):
    sys.stderr.write("patch_firmware.py: %s\n" % msg)
    sys.exit(1)


def normalise_lf(root):
    for dirpath, _, files in os.walk(root):
        for name in files:
            if not name.endswith(TEXT_EXT):
                continue
            p = os.path.join(dirpath, name)
            with open(p, "rb") as fh:
                data = fh.read()
            if b"\r\n" in data:
                with open(p, "wb") as fh:
                    fh.write(data.replace(b"\r\n", b"\n"))


def main():
    if not os.path.isdir(SRC):
        die("missing %s -- is the upstream snapshot present?" % SRC)
    if not os.path.isfile(SERIES):
        die("missing %s" % SERIES)

    commit = "unknown"
    if os.path.isfile(COMMIT):
        commit = open(COMMIT).read().strip()

    if os.path.isdir(DST):
        shutil.rmtree(DST)
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    shutil.copytree(SRC, DST)
    normalise_lf(DST)

    with open(SERIES) as fh:
        patches = [ln.strip() for ln in fh
                   if ln.strip() and not ln.lstrip().startswith("#")]

    have_git = shutil.which("git") is not None
    if not have_git and shutil.which("patch") is None:
        die("need either 'git' or 'patch' on PATH to apply the patch series")

    for name in patches:
        patch_path = os.path.join(ROOT, "patches", name)
        if not os.path.isfile(patch_path):
            die("patch listed in SERIES not found: %s" % name)
        if have_git:
            # Exact context match, no fuzz; hunk line-offsets are still tolerated.
            cmd = ["git", "apply", "--directory",
                   os.path.relpath(DST, ROOT), "--unsafe-paths", patch_path]
            r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        else:
            # --fuzz=0: context must match exactly (GNU patch defaults to fuzz 2,
            # which silently absorbs nearby upstream drift -- exactly what we don't
            # want when deciding whether a patch still applies).
            cmd = ["patch", "-p1", "--fuzz=0", "--forward",
                   "--no-backup-if-mismatch", "-d", DST, "-i", patch_path]
            if not VERBOSE:
                cmd.insert(1, "--silent")
            r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write(r.stdout + r.stderr)
            die("patch failed to apply: %s\n"
                "  upstream (%s) has drifted -- rebase patches/%s\n"
                "  (hand-fix build/vendor/firmware/<f>, then regenerate:\n"
                "     diff -u external/zimodem/<f> build/vendor/firmware/<f>)"
                % (name, commit[:12], name))
        if VERBOSE:
            sys.stderr.write(r.stdout + "applied %s\n" % name)

    print("patch_firmware.py: Zimodem @ %s + %d patch(es) -> build/vendor/firmware"
          % (commit[:12], len(patches)))


if __name__ == "__main__":
    main()
