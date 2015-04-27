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

//#include <pulse_sync_timer.h>
#include <flash.h>

// number of samples for regression line calculation
#define MAX_SAMPLES 5

/*// define this if you desire compensated forwarding in addition to simple
#define COMPENSATED_FORWARDING

#ifdef COMPENSATED_FORWARDING
//#define PKT_SIZE 35
#define PKT_SIZE 16
#else
#define PKT_SIZE 8
//#define PKT_SIZE 27
#endif*/
#define PKT_SIZE 8

// maximum skew values
#define MAX_SKEW ((float)(0.000002))

// latency of physical transmission process
/*#define TIME_STAMP_DELAY 585
#define TS_EARLY_DELAY 55
#define TS_END_DELAY 530 (in testing appears to be 652 cycles)
#define PLL_ON_TX_BUSY_DELAY 256
#define SHR_PHR_DELAY 3072
#define TX_TICK_DELAY (TS_END_DELAY + PLL_ON_TX_BUSY_DELAY + SHR_PHR_DELAY)
#define PSYNC_TX_DELAY ((uint32_t)TX_TICK_DELAY * 62L + (uint32_t)(TX_TICK_DELAY >> 1))
#define RX_TICK_DELAY (5 + TS_EARLY_DELAY)
#define PSYNC_RX_DELAY ((uint32_t)RX_TICK_DELAY * 62L + (uint32_t)(RX_TICK_DELAY >> 1))*/
#define PSYNC_TX_DELAY 244
#define PSYNC_COMP_FORW_DELAY 30
#define PSYNC_RX_DELAY 4

/*uint64_t _loc_sq_sum;
int64_t _off_sq_sum;
float _skew;*/

/*int64_t _off_time_avg;
uint64_t _loc_time_avg;
float _skew;*/

// initiate/reset pulsesync
void psync_init(uint8_t high_prio, uint8_t comp_forw, uint8_t is_root, uint8_t chan);

// set whether root or not
void psync_set_root(uint8_t is_root);

// add a synchronization data point for regression line calculation
void psync_add_point(uint64_t loc_time, uint64_t glob_time);

// determine whether the node is fully synchronized
uint8_t psync_is_synced();

// get current global time as nrk_time_t
void psync_get_nrk_time(nrk_time_t* global_time);

// get current global time as uint64_t in us
uint64_t psync_get_time();

// convert from local to global time
void psync_local_to_global(nrk_time_t* local_time, nrk_time_t* global_time);

// convert from global to local time
void psync_global_to_local(nrk_time_t* global_time, nrk_time_t* local_time);

// local time difference from global time difference
void psync_local_diff(nrk_time_t* glob_diff, nrk_time_t* loc_diff);

// initiate a pulsesynce flood cycle
void psync_flood_wait(nrk_time_t* time);

#endif
