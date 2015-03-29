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
*  Madhav Iyengar
*******************************************************************************/

/*
 * PulseSync protocol implementation for Nano-RK
 * built on top of Flash network flooding protocol
 *
 * pulse_sync.h: function declarations for PulseSync API
 *
 * 18-748 S15: Group 13
 * Madhav Iyengar
 * Nat Jeffries
 * Miguel Sotolongo
 */

#ifndef _PULSE_SYNC_H
#define _PULSE_SYNC_H

#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <stdint.h>

// enable precision timing
#ifndef NRK_SUB_TICK_TIMING
#define NRK_SUB_TICK_TIMING
#endif

#include <nrk.h>
#include <nrk_error.h>
#include <nrk_timer.h>

#include <flash.h>

// number of samples for regression line calculation
#define MAX_SAMPLES 4

// define this if you desire compensated forwarding in addition to simple
#define COMPENSATED_FORWARDING

#ifdef COMPENSATED_FORWARDING
#define PKT_SIZE 16
#else
#define PKT_SIZE 8
#endif

// latency of physical transmission process
#define RE_TX_DELAY 1223334
#define TX_DELAY 

// initiate/reset pulsesync
void psync_init(uint8_t high_prio, uint8_t root, uint8_t chan);

// add a synchronization data point for regression line calculation
void psync_add_point(uint64_t loc_time, uint64_t glob_time);

// determine whether the node is fully synchronized
uint8_t psync_is_synced();

// get current global time
void psync_get_time(nrk_time_t* global_time);

// convert from local to global time
void psync_local_to_global(nrk_time_t* local_time, nrk_time_t* global_time);

// convert from global to local time
void psync_global_to_local(nrk_time_t* global_time, nrk_time_t* local_time);

// local time difference from global time difference
void psync_local_diff(nrk_time_t* glob_diff, nrk_time_t* loc_diff);

// initiate a pulsesynce flood cycle
void psync_flood_wait(nrk_time_t* time);

#endif
