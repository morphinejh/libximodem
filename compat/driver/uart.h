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

#ifndef XIMODEM_DRIVER_UART_H
#define XIMODEM_DRIVER_UART_H
#include <stdint.h>

typedef int uart_port_t;
#define UART_NUM_0 0
#define UART_NUM_1 1
#define UART_NUM_2 2
#define UART_PIN_NO_CHANGE (-1)

typedef enum {
  UART_HW_FLOWCTRL_DISABLE = 0,
  UART_HW_FLOWCTRL_RTS = 1,
  UART_HW_FLOWCTRL_CTS = 2,
  UART_HW_FLOWCTRL_CTS_RTS = 3
} uart_hw_flowcontrol_t;

typedef enum {
  UART_PARITY_DISABLE = 0x0,
  UART_PARITY_EVEN = 0x2,
  UART_PARITY_ODD = 0x3
} uart_parity_t;

// uart_signal_inv_t bits
#define UART_SIGNAL_INV_DISABLE 0
#define UART_SIGNAL_IRDA_TX_INV (1 << 0)
#define UART_SIGNAL_IRDA_RX_INV (1 << 1)
#define UART_SIGNAL_RXD_INV     (1 << 2)
#define UART_SIGNAL_CTS_INV     (1 << 3)
#define UART_SIGNAL_DSR_INV     (1 << 4)
#define UART_SIGNAL_TXD_INV     (1 << 5)
#define UART_SIGNAL_RTS_INV     (1 << 6)
#define UART_SIGNAL_DTR_INV     (1 << 7)

#ifdef __cplusplus
extern "C" {
#endif
int uart_set_hw_flow_ctrl(uart_port_t, uart_hw_flowcontrol_t, uint8_t);
int uart_set_pin(uart_port_t, int tx, int rx, int rts, int cts);
int uart_set_line_inverse(uart_port_t, uint32_t mask);
int uart_set_baudrate(uart_port_t, uint32_t);
#ifdef __cplusplus
}
#endif
#endif
