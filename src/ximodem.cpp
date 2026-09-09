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

/*
 * libximodem glue: bridges the Zimodem firmware unity build to the public
 * C API in include/ximodem.h.
 *
 * Defines the xi_* internal hooks that compat/ calls, owns the DTE byte
 * ring buffers and the GPIO<->signal translation, and pumps setup()/loop().
 */
#include "ximodem.h"
#include "../compat/ximodem_internal.h"
#include "../compat/xi_platform.h"   // xi_net_startup()

#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

// Firmware entry points (from the unity TU).
extern void setup();
extern void loop();

// SPIFFS/SD roots live in compat; let the glue point them at the data dir.
#include "../compat/FS.h"
#include "../compat/SPIFFS.h"
#include "../compat/SD.h"
fs::FS   SPIFFS;
fs::SDFS SD;

// ---------------------------------------------------------------------------
// X16 pin map (mirror of firmware/zimodem.ino INCLUDE_CMDRX16 block).
// Modem-control lines on the X16 card are active-LOW.
// ---------------------------------------------------------------------------
// Firmware pin names are from the MODEM's point of view: pinRTS (21) is an
// output the modem drives, pinCTS (19) is an input the modem reads. From the
// DTE (X16) point of view those are CTS and RTS respectively -- hence the swap.
enum {
  PIN_DCD = 22, PIN_MODEM_CTS_IN = 19, PIN_MODEM_RTS_OUT = 21, PIN_RI = 18,
  PIN_DSR = 23, PIN_DTR = 25, PIN_OTH = 26,
};
static const int SIG_PIN[XIMODEM_SIG__COUNT] = {
  /*DCD*/ PIN_DCD,
  /*RI */ PIN_RI,
  /*DSR*/ PIN_DSR,
  /*CTS  (modem -> DTE)*/ PIN_MODEM_RTS_OUT,
  /*DTR  (DTE -> modem)*/ PIN_DTR,
  /*RTS  (DTE -> modem)*/ PIN_MODEM_CTS_IN,
};
// pin level (LOW=0) == asserted; expose 1 == asserted.
static inline int pin_to_sig(int pinLevel) { return pinLevel ? 0 : 1; }
static inline int sig_to_pin(int asserted)  { return asserted ? 0 : 1; }

// ---------------------------------------------------------------------------
struct Ring {
  static const int N = 1 << 16;
  uint8_t buf[N];
  std::atomic<int> head{0}, tail{0};
  int size() const {
    int h = head.load(), t = tail.load();
    return (h - t) & (N - 1);
  }
  int space() const { return N - 1 - size(); }
  int push(const uint8_t *p, int n) {
    int w = 0;
    while (w < n && space() > 0) {
      int h = head.load();
      buf[h] = p[w++];
      head.store((h + 1) & (N - 1));
    }
    return w;
  }
  int pop(uint8_t *p, int n) {
    int r = 0;
    while (r < n && size() > 0) {
      int t = tail.load();
      p[r++] = buf[t];
      tail.store((t + 1) & (N - 1));
    }
    return r;
  }
  int peek() const {
    if (size() == 0) return -1;
    return buf[tail.load()];
  }
  void reset() { tail.store(head.load()); }
};

struct ximodem_s {
  Ring toModem;    // X16 -> firmware HWSerial.read()
  Ring toDTE;      // firmware HWSerial.write() -> X16
  // The X16 serial card <-> modem UART link is real async serial: bytes only
  // pass when both ends run the same bit rate. dteBaud is what the emulator's
  // TL16C2550 is programmed to (0 = the X16 program has not set the card's
  // divisor yet -- indeterminate on real hardware, so: no traffic); fwBaud is
  // the firmware's own HWSerial rate (config default 115200, or ATB / flash).
  std::atomic<uint32_t> dteBaud{0};
  std::atomic<uint32_t> fwBaud{115200};
  std::atomic<int>      lastBaudMatch{-1};   // -1 unknown, 0 mismatch, 1 match
  std::atomic<uint32_t> format{0x800001c /*8N1*/};

  // Byte-rate pacing. A physical modem's UART delivers bytes to/from the DTE at
  // the line rate; nothing else in this stack does that (the emulator's
  // TL16C2550 model relies on real serial hardware to pace, which isn't here).
  // A time-budget accumulator meters the DTE-facing byte flow to the effective
  // line rate. Robust to coarse timers: a late wake just delivers more of the
  // bytes it is owed. CAP keeps it a rate, not a post-idle burst allowance.
  std::atomic<bool>     rateLimited{true};
  // Set when the firmware turns on hardware (RTS/CTS) flow control. While set,
  // the modem->DTE byte path honours the DTE's RTS line byte-for-byte, the way
  // the ESP32 UART peripheral gates TX on its CTS input -- a deasserted line
  // holds bytes (never drops them) until the DTE is ready again.
  std::atomic<bool>     hwFlowRtsCts{false};
  double                txCredit = 0.0;   // X16 -> modem, bytes
  double                rxCredit = 0.0;   // modem -> X16, bytes
  std::chrono::steady_clock::time_point lastCredit{};
  ximodem_signal_cb sig_cb = nullptr;
  void *sig_user = nullptr;
  int lastOut[XIMODEM_SIG__COUNT] = {0, 0, 0, 0, 0, 0};
  bool verbose = false;
  std::string dataDir;
  std::atomic<bool> inited{false};
  // Set if the firmware loop threw. A C++ exception escaping into the embedder's
  // pump thread is std::terminate -- i.e. it would take the whole emulator down.
  // Once faulted the instance is inert: every ximodem_poll() returns -1 without
  // re-entering the (probably wedged) firmware.
  std::atomic<bool> faulted{false};
};

// Report an unexpected C++ exception from vendored/compat code. Always goes to
// stderr (this is a "should never happen" condition), not gated on verbose.
static void xi_report_fault(const char *where, const char *what) {
  std::fprintf(stderr, "ximodem: %s threw: %s\n", where, what ? what : "(unknown)");
  std::fflush(stderr);
}

// Single active instance (the firmware has one set of globals).
static ximodem_s *g_inst = nullptr;
static std::mutex g_bigLock;

// True when the X16 card and the modem UART agree on the bit rate closely enough
// for bytes to survive the link. Real UARTs resample the start bit and tolerate a
// few percent of error; require the same here rather than an exact match (the X16
// divisor math rounds). dteBaud == 0 => the card has not been configured yet.
static bool xi_baud_match(ximodem_s *m) {
  uint32_t d = m->dteBaud.load(), f = m->fwBaud.load();
  if (d == 0 || f == 0) return false;
  uint32_t hi = d > f ? d : f;
  uint32_t diff = d > f ? d - f : f - d;
  return diff * 100u <= hi * 3u;
}

// Effective bits-per-byte for the programmed frame (start + data + parity + stop).
static uint32_t xi_frame_bits(uint32_t fmt) {
  // format word mirrors the ESP UART_NB_* layout the compat layer sets; default
  // 8N1. Keep it simple: data 5-8, +1 start, +1..2 stop, +0..1 parity.
  uint32_t data = 5 + ((fmt >> 2) & 0x3);
  uint32_t stop = ((fmt >> 4) & 0x3) >= 2 ? 2 : 1;
  uint32_t par  = ((fmt & 0x3) == 0) ? 0 : 1;
  uint32_t bits = 1 + data + par + stop;
  return bits < 7 ? 10 : bits;   // sane floor
}

// Slack the accumulator may bank during a stall, so a coarse-timer consumer
// still averages the target rate. ~the firmware's own 4 KB serial buffer; big
// enough for a scheduling hiccup even at high baud, small enough to stay a rate.
static const double XI_CREDIT_CAP = 4096.0;

// Add elapsed-time worth of byte credit to both directions, clamped.
static void xi_pace_refill(ximodem_s *m) {
  auto now = std::chrono::steady_clock::now();
  double dt = std::chrono::duration<double>(now - m->lastCredit).count();
  m->lastCredit = now;
  if (dt <= 0.0) return;
  uint32_t d = m->dteBaud.load(), f = m->fwBaud.load();
  uint32_t line = (d && f) ? (d < f ? d : f) : (d ? d : f);
  if (line == 0) return;
  double bps = (double)line / (double)xi_frame_bits(m->format.load());
  double add = dt * bps;
  m->txCredit = (m->txCredit + add < XI_CREDIT_CAP) ? m->txCredit + add : XI_CREDIT_CAP;
  m->rxCredit = (m->rxCredit + add < XI_CREDIT_CAP) ? m->rxCredit + add : XI_CREDIT_CAP;
}

// Log link up/down transitions once, so a silent modem has an obvious cause.
static void xi_baud_note(ximodem_s *m) {
  int now = xi_baud_match(m) ? 1 : 0;
  int was = m->lastBaudMatch.exchange(now);
  if (now == was) return;
  uint32_t d = m->dteBaud.load(), f = m->fwBaud.load();
  if (now)
    std::fprintf(stderr, "ximodem: serial link up (DTE %u == modem %u baud)\n", d, f);
  else if (d == 0)
    std::fprintf(stderr, "ximodem: modem is mute -- the X16 serial card is not configured yet\n");
  else
    std::fprintf(stderr, "ximodem: serial link down -- DTE %u baud vs modem %u baud (no data will pass)\n", d, f);
  std::fflush(stderr);
}

// ---------------------------------------------------------------------------
// xi_* internal hooks called by compat/
// ---------------------------------------------------------------------------
extern "C" void xi_host_yield(void) { /* cooperative: nothing to do yet */ }

// Whether ximodem's chatty diagnostics (firmware debug console + xi_debug) go to
// stderr. Off by default -- only the "serial link up/down" transitions in
// xi_baud_note() are unconditional. Opt in per-instance via ximodem_config.verbose
// or, before an instance exists, via the XIMODEM_VERBOSE environment variable.
extern "C" int xi_verbose_enabled(void) {
  if (g_inst) return g_inst->verbose ? 1 : 0;
  static int env = -1;
  if (env < 0) env = getenv("XIMODEM_VERBOSE") ? 1 : 0;
  return env;
}

extern "C" void xi_debug(const char *fmt, ...) {
  if (!xi_verbose_enabled()) return;
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

// Modem <-> DTE byte traffic only flows when the two UARTs' bit rates match.
// A mismatch on real hardware yields framing errors / garbage; we model that as
// silence (a cleaner diagnostic). Modem-control lines are DC levels and stay
// live regardless -- they are not gated here.
extern "C" int xi_dte_read(void) {
  if (!g_inst || !xi_baud_match(g_inst)) return -1;
  uint8_t c;
  int got = g_inst->toModem.pop(&c, 1) == 1 ? c : -1;
  return got;
}
extern "C" int xi_dte_peek(void) {
  return (g_inst && xi_baud_match(g_inst)) ? g_inst->toModem.peek() : -1;
}
extern "C" int xi_dte_available(void) {
  return (g_inst && xi_baud_match(g_inst)) ? g_inst->toModem.size() : 0;
}
// The firmware throttles its output against HWSerial.availableForWrite() as if
// it were a ~128-byte hardware TX FIFO (SER_BUFSIZE == 0x7F). Model that: report
// the space in a notional 128-byte FIFO whose "contents" are whatever the X16
// side has not yet drained from the toDTE ring. When the X16 stops reading the
// ring backs up, reported space hits 0, and the firmware stops draining its own
// 4 KB buffer -- which in turn makes ZStream apply flow control to the TCP peer.
static const int XI_DTE_TX_FIFO = 128;

// The ESP32 UART, with hw flow control on, will not clock a byte out while its
// CTS input (== the DTE's RTS line) is deasserted. Mirror that: when the
// firmware has RTS/CTS flow control enabled and the DTE has dropped RTS, the
// modem->DTE path is closed until RTS returns. When flow control is off (or the
// DTE keeps RTS asserted, as every single-threaded tool does) this is always
// open and xi_dte_write behaves exactly as before.
static bool xi_dte_flow_open(ximodem_s *m) {
  if (!m->hwFlowRtsCts.load()) return true;
  return ximodem_get_signal(m, XIMODEM_SIG_RTS) != 0;   // asserted?
}

// How much modem->DTE output to hold while the DTE cannot yet frame it (the X16
// card divisor is not programmed).  A real modem's UART, with hw flow control
// on, keeps unsent bytes in its ~256 B TX ring until the DTE asserts CTS and
// starts clocking -- which is why the boot banner survives to the first terminal
// open.  Mirror that with a bounded hold; ximodem_read()/_available() gate on
// the same baud match so the embedder doesn't drain it early.
static const int XI_PRECONFIG_HOLD = 1024;

extern "C" int xi_dte_write(const uint8_t *buf, int len) {
  if (!g_inst || len <= 0) return 0;
  ximodem_s *m = g_inst;
  // Baud mismatch: the modem still clocks bits out, the DTE just can't decode
  // them. Hold a bounded amount (see above) so the boot banner is delivered
  // once the card is configured; drop the overflow, as a real TX ring would.
  if (!xi_baud_match(m)) {
    int room = XI_PRECONFIG_HOLD - m->toDTE.size();
    if (room > 0) m->toDTE.push(buf, len < room ? len : room);
    return len;
  }
  // While the DTE has RTS low under hw flow control, hold here -- the ESP32 UART
  // would not clock the byte out either. A multi-threaded embedder (the
  // emulator) keeps draining toDTE on its other thread and re-raises RTS; the
  // ~5 s ceiling only guards against a wedged/steady-low line.
  if (!xi_dte_flow_open(m)) {
    for (int i = 0; i < 50000 && !xi_dte_flow_open(m); i++) {
      xi_host_yield();
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
  // On real hardware HWSerial.write() blocks until the UART has room; the ESP32
  // serialOutDeque() path relies on that for flow control. Mirror it with a
  // bounded cooperative block: wait for ring space, yielding so the embedder's
  // consumer thread can drain. Only drop as an absolute last resort.
  int written = 0;
  int spins = 0;
  const int MAX_SPINS = 6000;   // * 50us => ~300ms worst-case block
  while (written < len) {
    int n = m->toDTE.push(buf + written, len - written);
    written += n;
    if (written >= len) break;
    if (++spins > MAX_SPINS) break;   // consumer wedged: give up, report short
    xi_host_yield();
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  return written;
}
extern "C" int xi_dte_write_space(void) {
  if (!g_inst) return 0;
  ximodem_s *m = g_inst;
  if (!xi_dte_flow_open(m)) return 0;         // DTE has halted us
  int inflight = m->toDTE.size();
  int space = XI_DTE_TX_FIFO - inflight;
  if (space < 0) space = 0;
  if (space > m->toDTE.space()) space = m->toDTE.space();
  return space;
}
// Firmware toggled hardware (RTS/CTS) flow control -- see hwFlowRtsCts.
extern "C" void xi_dte_apply_flow(int rtscts) {
  if (g_inst) g_inst->hwFlowRtsCts.store(rtscts != 0);
}
// The firmware's own HWSerial rate (boot default, ATB, or flash config).
extern "C" void xi_dte_set_baud(uint32_t b) {
  if (!g_inst) return;
  g_inst->fwBaud.store(b);
  xi_baud_note(g_inst);
}
extern "C" void xi_dte_set_format(uint32_t f) { if (g_inst) g_inst->format.store(f); }

extern "C" void xi_pin_changed(int pin, int level) {
  if (!g_inst) return;
  for (int s = 0; s < XIMODEM_SIG__COUNT; s++) {
    if (SIG_PIN[s] != pin) continue;
    if (s == XIMODEM_SIG_DTR || s == XIMODEM_SIG_RTS) return; // inputs; ignore writes
    int asserted = pin_to_sig(level);
    if (asserted != g_inst->lastOut[s]) {
      g_inst->lastOut[s] = asserted;
      if (g_inst->sig_cb) g_inst->sig_cb(g_inst->sig_user, (ximodem_signal_t)s, asserted);
    }
    return;
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
ximodem_t *ximodem_create(const ximodem_config_t *cfg) {
  std::lock_guard<std::mutex> lk(g_bigLock);
  if (g_inst) return nullptr; // only one instance supported (firmware globals)
  xi_net_startup();            // WSAStartup on Windows; no-op elsewhere
  auto *m = new ximodem_s();
  m->verbose = cfg && cfg->verbose;
  m->dataDir = (cfg && cfg->data_dir) ? cfg->data_dir : "./ximodem-data";
  m->lastCredit = std::chrono::steady_clock::now();
  g_inst = m;

  SPIFFS.setRoot(m->dataDir);
  SD.setRoot(m->dataDir + "/sd");
  SPIFFS.begin(true);

  // A DTE that is present asserts DTR and RTS; default to that so a bare modem
  // works before the embedder drives the lines. The emulator's TL16C2550 will
  // override both from MCR immediately.
  xi_pin_force(SIG_PIN[XIMODEM_SIG_DTR], sig_to_pin(1));
  xi_pin_force(SIG_PIN[XIMODEM_SIG_RTS], sig_to_pin(1));

  try {
    setup();
  } catch (const std::exception &e) {
    xi_report_fault("setup()", e.what());
    g_inst = nullptr;
    delete m;
    return nullptr;
  } catch (...) {
    xi_report_fault("setup()", nullptr);
    g_inst = nullptr;
    delete m;
    return nullptr;
  }
  m->inited.store(true);
  return m;
}

void ximodem_destroy(ximodem_t *m) {
  std::lock_guard<std::mutex> lk(g_bigLock);
  if (!m || m != g_inst) return;
  g_inst = nullptr;
  delete m;
}

int ximodem_poll(ximodem_t *m) {
  if (!m || m != g_inst || !m->inited.load() || m->faulted.load()) return -1;
  try {
    loop();
  } catch (const std::exception &e) {
    m->faulted.store(true);
    xi_report_fault("loop()", e.what());
    return -1;
  } catch (...) {
    m->faulted.store(true);
    xi_report_fault("loop()", nullptr);
    return -1;
  }
  return 0;
}

static int xi_tx_budget(ximodem_s *m, int want) {
  if (!m->rateLimited.load()) return want;
  xi_pace_refill(m);
  int b = (int)m->txCredit;
  return want < b ? want : b;
}
static int xi_rx_budget(ximodem_s *m, int want) {
  if (!m->rateLimited.load()) return want;
  xi_pace_refill(m);
  int b = (int)m->rxCredit;
  return want < b ? want : b;
}

int ximodem_write(ximodem_t *m, const uint8_t *buf, int len) {
  if (!m) return 0;
  // Baud mismatch: the X16 clocks bytes out but the modem UART can't frame them.
  // Accept-and-drop rather than back the emulator up on a link that is dark.
  if (!xi_baud_match(m)) return len;
  int take = xi_tx_budget(m, len);
  if (take <= 0) return 0;                 // no budget -> caller keeps the bytes
  int n = m->toModem.push(buf, take);
  if (m->rateLimited.load()) m->txCredit -= n;
  return n;
}
int ximodem_read(ximodem_t *m, uint8_t *buf, int len) {
  if (!m) return 0;
  if (!xi_baud_match(m)) return 0;         // DTE can't frame bytes at a rate it isn't set to
  int want = xi_rx_budget(m, len);
  if (want <= 0) return 0;
  int n = m->toDTE.pop(buf, want);
  if (m->rateLimited.load()) m->rxCredit -= n;
  return n;
}
int ximodem_read_available(ximodem_t *m) {
  if (!m) return 0;
  if (!xi_baud_match(m)) return 0;
  int have = m->toDTE.size();
  return xi_rx_budget(m, have);            // advertise only what the line rate allows
}
int ximodem_write_space(ximodem_t *m) {
  if (!m) return 0;
  return xi_tx_budget(m, m->toModem.space());
}

// The rate the emulator's TL16C2550 is programmed to. 0 => card not configured.
void ximodem_set_baud(ximodem_t *m, uint32_t baud) {
  if (!m) return;
  uint32_t prev = m->dteBaud.exchange(baud);
  // Card divisor cleared (a machine reset -- Ctrl-R -- deconfigures the TL16C2550,
  // which also resets its RX FIFO). Drop anything still queued for the DTE so the
  // reset genuinely flushes the receive path: the firmware itself is not
  // restarted, so its boot banner must not reappear, and mid-stream bytes from a
  // torn-down session must not resurface. Safe here: openDevice() (the only
  // in-emulator caller) runs with both worker threads parked.
  if (baud == 0 && prev != 0)
    m->toDTE.reset();
  xi_baud_note(m);
}
// Reports the *modem's* serial rate (what ATI / CONNECT show), not the DTE's.
uint32_t ximodem_get_baud(ximodem_t *m) { return m ? m->fwBaud.load() : 0; }

// Byte-rate pacing to the effective line rate is on by default (mimics a real
// UART). Turn it off for a raw pipe with no line-rate meaning, e.g. tools/pty.
void ximodem_set_rate_limit(ximodem_t *m, int enabled) {
  if (m) m->rateLimited.store(enabled != 0);
}

int ximodem_get_signal(ximodem_t *m, ximodem_signal_t sig) {
  if (!m || sig < 0 || sig >= XIMODEM_SIG__COUNT) return 0;
  return pin_to_sig(xi_pin_peek(SIG_PIN[sig]));
}
void ximodem_set_signal(ximodem_t *m, ximodem_signal_t sig, int level) {
  if (!m || sig < 0 || sig >= XIMODEM_SIG__COUNT) return;
  xi_pin_force(SIG_PIN[sig], sig_to_pin(level));
}
void ximodem_on_signal(ximodem_t *m, ximodem_signal_cb cb, void *user) {
  if (!m) return;
  m->sig_cb = cb;
  m->sig_user = user;
}

const char *ximodem_version(void) {
  return "ximodem " XIMODEM_VERSION " (Zimodem host build / X16)";
}

// xi_pin_force / xi_pin_peek live in compat/core.cpp
extern "C" void xi_pin_force(int pin, int level);
extern "C" int  xi_pin_peek(int pin);
