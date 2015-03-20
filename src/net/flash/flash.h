/******************************************************************************
*  Nano-RK, a real-time operating system for sensor networks.
*  Copyright (C) 2007, Real-Time and Multimedia Lab, Carnegie Mellon University
*  All rights reserved.
*
*  This is the Open Source Version of Nano-RK included as part of a Dual
*  Licensing Model. If you are unsure which license to use please refer to:
*  http://www.nanork.org/nano-RK/wiki/Licensing
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, version 2.0 of the License.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*  Contributing Authors (specific to this file):
*  Anthony Rowe
*******************************************************************************/


#ifndef _FLASH_H
#define _FLASH_H
#include <include.h>
#include <basic_rf.h>
#include <nrk.h>
#include <ulib.h>
#include <stdlib.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <nrk.h>
#include <nrk_events.h>
#include <nrk_timer.h>
#include <nrk_error.h>
#include <nrk_reserve.h>
#include <nrk_cfg.h>

#define FLASH_MAX_PKT_LEN 33

int8_t flash_init(uint8_t chan);
void flash_disable();
void flash_enable();
int8_t flash_rf_power_set(uint8_t power);
void flash_err_count_reset();
uint32_t flash_err_count_get();
void flash_msg_len_set(uint16_t msg_len);
uint16_t flash_msg_len_get();
void flash_tx_pkt(uint8_t *buffer, uint8_t len);

void flash_task_config();

#endif
