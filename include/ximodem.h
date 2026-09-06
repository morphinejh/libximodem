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
 * libximodem — Zimodem (Commander X16 config) as a host shared library.
 *
 * The library runs the Zimodem firmware in-process. The embedder (e.g. the
 * x16-emulator UART model) exchanges a DTE-side byte stream with it and drives
 * the modem-control signal lines, exactly as if an ESP32 running Zimodem were
 * wired to the X16 serial card.
 *
 * Threading: create the instance, then call ximodem_poll() regularly from one
 * thread (either your emulator tick, or a dedicated thread). All other calls
 * must come from that same thread unless noted otherwise.
 */
#ifndef XIMODEM_H
#define XIMODEM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XIMODEM_VERSION "0.1"

/* Only the functions below are exported from the DLL/shared object. Define
 * XIMODEM_STATIC if you link libximodem statically. */
#if defined(_WIN32) && !defined(XIMODEM_STATIC)
#  ifdef XIMODEM_BUILD
#    define XIMODEM_API __declspec(dllexport)
#  else
#    define XIMODEM_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && !defined(XIMODEM_STATIC)
#  define XIMODEM_API __attribute__((visibility("default")))
#else
#  define XIMODEM_API
#endif

typedef struct ximodem_s ximodem_t;

/* Modem-control signal lines, named from the DTE (X16) point of view. */
typedef enum {
    XIMODEM_SIG_DCD = 0,  /* modem -> DTE : carrier detect            (out) */
    XIMODEM_SIG_RI  = 1,  /* modem -> DTE : ring indicator            (out) */
    XIMODEM_SIG_DSR = 2,  /* modem -> DTE : data set ready            (out) */
    XIMODEM_SIG_CTS = 3,  /* modem -> DTE : clear to send             (out) */
    XIMODEM_SIG_DTR = 4,  /* DTE -> modem : data terminal ready       (in)  */
    XIMODEM_SIG_RTS = 5,  /* DTE -> modem : request to send           (in)  */
    XIMODEM_SIG__COUNT
} ximodem_signal_t;

typedef struct {
    const char *data_dir;   /* where config/phonebook live; NULL => ./ximodem-data */
    int         verbose;    /* 1 => firmware debug console to stderr */
} ximodem_config_t;

/* Lifecycle -------------------------------------------------------------- */
XIMODEM_API ximodem_t *ximodem_create(const ximodem_config_t *cfg);
XIMODEM_API void       ximodem_destroy(ximodem_t *m);

/* Run one slice of the firmware loop. Non-blocking; call often (e.g. every
 * emulator frame or ~1 ms from a worker thread). Returns 0 on success. */
XIMODEM_API int        ximodem_poll(ximodem_t *m);

/* DTE byte stream ------------------------------------------------------ */
/* X16 -> modem. Returns bytes accepted (may be < len if the RX buffer fills). */
XIMODEM_API int        ximodem_write(ximodem_t *m, const uint8_t *buf, int len);
/* modem -> X16. Returns bytes copied (0 if none pending). */
XIMODEM_API int        ximodem_read(ximodem_t *m, uint8_t *buf, int len);
XIMODEM_API int        ximodem_read_available(ximodem_t *m);
XIMODEM_API int        ximodem_write_space(ximodem_t *m);

/* DTE line parameters the X16 UART programs ------------------------------ */
/* Tell the modem what bit rate the X16 serial card is programmed to. Pass 0
 * (or never call this) while the card is unconfigured. The card <-> modem link
 * is real async serial: NO bytes pass in either direction unless this rate and
 * the modem's own rate (config default 115200, or ATB / flash) agree to within
 * ~3%. Modem-control lines (DCD/DTR/...) are unaffected. */
XIMODEM_API void       ximodem_set_baud(ximodem_t *m, uint32_t baud);
/* The MODEM's serial rate -- the value ATI / CONNECT report -- not the DTE's. */
XIMODEM_API uint32_t   ximodem_get_baud(ximodem_t *m);

/* Byte throughput across the DTE boundary is metered to the effective line rate
 * (min of the two baud rates, adjusted for the frame's bits-per-byte) by
 * default, the way a real modem's UART paces the wire -- a fast TCP transfer is
 * still delivered to the X16 at, say, 115200 bps. Pass enabled=0 for a transport
 * with no line-rate meaning (a PTY, a raw test pipe). Default: enabled. */
XIMODEM_API void       ximodem_set_rate_limit(ximodem_t *m, int enabled);

/* Modem-control signals ---------------------------------------------- */
XIMODEM_API int        ximodem_get_signal(ximodem_t *m, ximodem_signal_t sig);      /* out lines */
XIMODEM_API void       ximodem_set_signal(ximodem_t *m, ximodem_signal_t sig, int level); /* in lines */

/* Optional: called from ximodem_poll's thread whenever an output signal
 * changes, so the emulator can update MSR / raise an IRQ promptly. */
typedef void (*ximodem_signal_cb)(void *user, ximodem_signal_t sig, int level);
XIMODEM_API void       ximodem_on_signal(ximodem_t *m, ximodem_signal_cb cb, void *user);

XIMODEM_API const char *ximodem_version(void);

#ifdef __cplusplus
}
#endif

#endif
