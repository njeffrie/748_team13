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
 * pulse_sync.c: implementation of PulseSync API
 *
 * 18-748 S15: Group 13
 * Madhav Iyengar
 * Nat Jeffries
 * Miguel Sotolongo
 */

#include "pulse_sync.h"

// stores previous sync data in table for compensated forwarding
struct sync_point_info {
	uint64_t glob_time;
	uint64_t loc_time;
	int64_t skew_inv;
};

// values to be saved between synchronizations for faster regression line calculation
uint64_t loc_sum, loc_avg;
int64_t off_sum, off_avg;
uint64_t loc_sq_sum;
int64_t off_sq_sum, skew_inv;
uint8_t samples, curr_ind;

// new times to be added to regression line
uint64_t new_loc, new_glob;
uint8_t edit;

// semaphore (used as mutex) for using/changing current skew value
nrk_sem_t* skew_lock;

// circular buffer for recent regression line data
struct sync_point_info line_data[MAX_SAMPLES];

// whether or not this node is root, set on initialization
uint8_t is_root;

/*
 * function for initializing/reseting pulsesync
 */
void psync_init(uint8_t high_prio, uint8_t root, uint8_t chan) {
	if (skew_lock != NULL)
		nrk_sem_delete(skew_lock);
	skew_lock = nrk_sem_create(1, high_prio);
	flash_init(chan);
	is_root = root;
	loc_sum = 0;
	loc_avg = 0;
	off_sum = 0;
	off_avg = 0;
	loc_sq_sum = 1;
	off_sq_sum = 0;
	skew_inv = 0x7fffffffffffffffL;
	samples = 0;
	curr_ind = 0;
}

/*
 * helper function to get current time without using nrk_time_get 
 * and converting back to uint64_t
 */
inline uint64_t get_time_exp() {
    uint64_t time = 1000000000L * (uint64_t)nrk_system_time.secs + nrk_system_time.nano_secs;
    time += ((uint64_t)_nrk_precision_os_timer_get()) * (uint64_t)NANOS_PER_PRECISION_TICK;
    return time + ((uint64_t)_nrk_os_timer_get()) * (uint64_t)NANOS_PER_TICK; 
}

/*
 * helper function to change nrk_time_t to uint64_t
 */
inline uint64_t get_full_time(nrk_time_t time) {
	return (uint64_t)(time.secs) * 1000000000L + time.nano_secs;
}

/*
 * helper function to change uint64_t to nrk_time_t
 */
inline nrk_time_t get_pack_time(uint64_t time) {
	nrk_time_t nrk_time;
	nrk_time.secs = time / 1000000000;
	nrk_time.nano_secs = time - 1000000000L * (uint64_t)nrk_time.secs;
	return nrk_time;
}

/*
 * function for adding new synchronization point to regression line calculation
 */
void psync_add_point(uint64_t loc_time, uint64_t glob_time) {
	int64_t tmp_avg;
	int8_t tmp_rem;
	uint8_t next_ind;

	// determine new number of samples
	uint8_t new_samples = samples < MAX_SAMPLES ? samples + 1 : samples;

	next_ind = (curr_ind + 1) % MAX_SAMPLES;

	// assemble previous values
	uint64_t prev_loc = line_data[next_ind].loc_time;
	int64_t prev_off = line_data[next_ind].glob_time - prev_loc;
	int64_t off_time = glob_time - loc_time;

	//// remove old average values from square sums
	// lock skew values
	nrk_sem_pend(skew_lock);

	// remove terms in local time deviation squared sum containing old mean local time
	loc_sq_sum += (loc_avg >> 3) * ((int64_t)(2 * loc_sum - samples * loc_avg) >> 3);
	// overwrite old local time in square with new local time
	loc_sq_sum += ((loc_time - prev_loc) >> 3) * ((loc_time + prev_loc) >> 3);

	// remove terms in offset deviation squared sum containing old mean local time and offset
	off_sq_sum += (off_avg >> 0) * ((loc_sum - samples * loc_avg) >> 0) + (loc_avg >> 0) * (off_sum >> 0);
	// overwrite old local time and offset terms with new local time and offset terms
	off_sq_sum += (loc_time >> 0) * (off_time >> 0) - (prev_loc >> 0) * (prev_off >> 0);

	//// calculate new mean local time and local time deviation squared
	loc_sum += loc_time - prev_loc;
	tmp_avg = loc_sum / new_samples;
	tmp_rem = loc_sum - new_samples * tmp_avg; 
	loc_avg = tmp_avg + (tmp_rem + new_samples / 2) / new_samples;

	// add terms in square containing new mean local time
	loc_sq_sum += (loc_avg >> 3) * ((int64_t)(new_samples * loc_avg - 2 * loc_sum) >> 3);

	//// calculate new mean global-local time offset and offset deviation squared
	tmp_avg = line_data[curr_ind].glob_time - line_data[curr_ind].loc_time;
	off_sum += off_time - prev_off;
	tmp_avg = off_sum / new_samples;
	tmp_rem = off_sum - new_samples * tmp_avg;
	off_avg = tmp_avg + (tmp_rem + new_samples / 2) / new_samples;

	// add terms to square containing new mean local time and offset
	off_sq_sum += (off_avg >> 0) * ((new_samples * loc_avg - loc_sum) >> 0) - (loc_avg >> 0) * (off_sum >> 0);

	//// save new data into next entry in circular buffer
	curr_ind = next_ind;
	samples = new_samples;
	// check for divide-by-0 errors
	if (!off_sq_sum)
		skew_inv = 0x7fffffffffffffffL;
	else if (!loc_sq_sum) 
		skew_inv = off_sq_sum > 0 ? 1 : -1;
	else
		skew_inv = off_sq_sum > 0 ? ~(loc_sq_sum / off_sq_sum) + 1 : loc_sq_sum / (~off_sq_sum + 1);
	line_data[curr_ind] = (struct sync_point_info){glob_time, loc_time, skew_inv};

	// unlock skew values
	nrk_sem_post(skew_lock);
}

/*
 * function to determine if synchronization is complete
 */
uint8_t psync_is_synced() {
	return samples == MAX_SAMPLES;
}

/*
 * function to obtain current global time
 */
void psync_get_time(nrk_time_t* global_time) {
	//nrk_time_get(global_time);
	uint64_t full_time = get_time_exp();//get_full_time(*global_time);
	nrk_sem_pend(skew_lock);
	*global_time = get_pack_time(full_time + off_avg + ((off_sq_sum * (full_time - loc_avg) / loc_sq_sum) >> 6));
	nrk_sem_post(skew_lock);
}

/*
 * function to convert a local time to a global time
 */
void psync_local_to_global(nrk_time_t* local_time, nrk_time_t* global_time) {
	uint64_t loc_time = get_full_time(*local_time);
	nrk_sem_pend(skew_lock);
	*global_time = get_pack_time(loc_time + off_avg + ((off_sq_sum * (loc_time - loc_avg) / loc_sq_sum) >> 6));
	nrk_sem_post(skew_lock);
}

/*
 * function to convert a global time to a local time
 */
void psync_global_to_local(nrk_time_t* global_time, nrk_time_t* local_time) {
	uint64_t glob_time = get_full_time(*global_time);
	nrk_sem_pend(skew_lock);
	uint64_t approx_loc = glob_time - off_avg;
	*local_time = get_pack_time(approx_loc - ((off_sq_sum * (approx_loc - loc_avg) / loc_sq_sum) >> 6));
	nrk_sem_post(skew_lock);
}

/* 
 * function to obtain local time difference from global time difference
 */
void psync_local_diff(nrk_time_t* glob_diff, nrk_time_t* loc_diff) {
	uint64_t g_diff = get_full_time(*glob_diff);
	nrk_sem_pend(skew_lock);
	*loc_diff = get_pack_time(g_diff + (int64_t)(g_diff / (skew_inv << 6)));
	nrk_sem_post(skew_lock);
}

/*
 * function to handle received PulseSync floods
 * saves values to be added to regression table
 */
void psync_rx_callback(uint8_t* buf, nrk_time_t* rcv_time) {
	new_loc = get_full_time(*rcv_time);
	uint64_t* buf64 = (uint64_t*)buf;
	#ifdef COMPENSATED FORWARDING
	new_glob = buf64[1];
	#else
	new_glob = buf64[0];
	#endif
}

/*
 * function to handle (re)transmission of PulseSynce floods
 * sets/alters buffer for Rx-Tx time delta
 */
void psync_tx_callback(uint16_t len, uint8_t* buf) {
	uint64_t* buf64 = (uint64_t*)buf;
	if (is_root) { 
		buf64[0] = get_time_exp();
		#ifdef COMPENSATED_FORWARDING
		buf64[1] = buf64[0];
		#endif
	}
	else {
		uint64_t diff = get_time_exp() - new_loc;
	
		#ifdef COMPENSATED_FORWARDING
		if (samples == MAX_SAMPLES) {
			uint8_t ind = (curr_ind + 1) % MAX_SAMPLES;
			buf64[1] += diff - (int64_t)(diff / (line_data[ind].skew_num << 6));
		}
		else
			buf64[1] += diff;
		#endif
		buf64[0] += diff;
		edit = 1;
	}
}

/*
 * function to initiate a pulsesync flood cycle 
 */
void psync_flood_wait(nrk_time_t* time) {
	// buffer for holding data to send/receive with flash (in this case uint64_t with time data)
	#ifdef COMPENSATED_FORWARDING
	uint64_t buf[2];
	#else
	uint64_t buf[1];
	#endif
	
	// functionality to be executed if the node is set as the network global clock
	if (is_root) {
		buf[0] = 0;
		#ifdef COMPENSATED_FORWARDING
		buf[1] = 0;
		#endif
		flash_tx_callback_set(psync_tx_callback);
		flash_tx_pkt((uint8_t*)buf, PKT_SIZE);
	}
	// functionality to be executed if the node is synchronizing to an external global clock
	else {
		edit = 0;
		nrk_time_t cur_time, time_after;
		nrk_time_get(&cur_time);
		nrk_time_add(&time_after, cur_time, *time);
		flash_tx_callback_set(psync_tx_callback);
		flash_enable(16, time, psync_rx_callback);
		while (!edit && (nrk_time_compare(&cur_time, &time_after) < 0))
			nrk_time_get(&cur_time);
		if (edit) {
			psync_add_point(new_loc, new_glob);
			edit = 0;
		}
	}
}
