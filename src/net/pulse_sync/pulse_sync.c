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
 * pulse_sync.c: PulseSync protocol implementation for Nano-RK
 * built on top of Flash network flooding protocol
 *
 * 18-748 S15: Group 13
 * Madhav Iyengar
 * Nat Jeffries
 * Miguel Sotolongo
 */

#include "pulse_sync.h"

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
	samples = 0;
	curr_ind = 0;
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
	return (nrk_time_t){time / 1000000000, time % 1000000000};
}

/*
 * function for adding new synchronization point to regression line calculation
 */
void psync_add_point(uint64_t loc_time, uint64_t glob_time) {
	int64_t tmp_avg;
	int8_t tmp_rem;

	// determine new number of samples
	uint8_t new_samples = samples < MAX_SAMPLES ? samples + 1 : samples;

	// assemble previous values
	uint64_t prev_loc = line_data[(curr_ind + 1) % MAX_SAMPLES].loc_time;
	int64_t prev_off = line_data[(curr_ind + 1) % MAX_SAMPLES].glob_time - prev_loc;
	int64_t off_time = glob_time - loc_time;

	//// remove old average values from square sums
	// lock skew values
	nrk_sem_pend(skew_lock);

	// remove terms in local time deviation squared sum containing old mean local time
	loc_sq_sum += (loc_avg >> 5) * ((int64_t)(2 * loc_sum - samples * loc_avg) >> 5);
	// overwrite old local time in square with new local time
	loc_sq_sum += ((loc_time - prev_loc) >> 5) * ((loc_time + prev_loc) >> 5);

	// remove terms in offset deviation squared sum containing old mean local time and offset
	off_sq_sum += off_avg * (loc_sum - samples * loc_avg) + loc_avg * off_sum;
	// overwrite old local time and offset terms with new local time and offset terms
	off_sq_sum += loc_time * off_time - prev_loc * prev_off;

	//// calculate new mean local time and local time deviation squared
	loc_sum += loc_time - prev_loc;
	tmp_avg = loc_sum / new_samples;
	tmp_rem = loc_sum % new_samples;
	loc_avg = tmp_avg + (tmp_rem + new_samples / 2) / new_samples;

	// add terms in square containing new mean local time
	loc_sq_sum += (loc_avg >> 5) * ((int64_t)(new_samples * loc_avg - 2 * loc_sum) >> 5);

	//// calculate new mean global-local time offset and offset deviation squared
	tmp_avg = line_data[curr_ind].glob_time - line_data[curr_ind].loc_time;
	off_sum += off_time - prev_off;
	tmp_avg = off_sum / new_samples;
	tmp_rem = off_sum % new_samples;
	off_avg = tmp_avg + (tmp_rem + new_samples / 2) / new_samples;

	// add terms to square containing new mean local time and offset
	off_sq_sum += off_avg * (new_samples * loc_avg - loc_sum) - loc_avg * off_sum;

	//// save new data into next entry in circular buffer
	curr_ind = (curr_ind + 1) % MAX_SAMPLES;
	samples = new_samples;
	line_data[curr_ind] = (struct sync_point_info){glob_time, loc_time, off_sq_sum, loc_sq_sum};
	// set skew to zero if skew fraction undefined
	if (loc_sq_sum == 0) {
		line_data[curr_ind].skew_num = 0;
		line_data[curr_ind].skew_den = 1;
	}

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
	nrk_time_get(global_time);
	uint64_t full_time = get_full_time(*global_time);
	nrk_sem_pend(skew_lock);
	*global_time = get_pack_time(full_time + off_avg + (off_sq_sum * (int64_t)(full_time - loc_avg) / loc_sq_sum));
	nrk_sem_post(skew_lock);
}

/*
 * function to convert a local time to a global time
 */
void psync_convert_local_to_global(nrk_time_t* local_time, nrk_time_t* global_time) {
	uint64_t loc_time = get_full_time(*local_time);
	nrk_sem_pend(skew_lock);
	*global_time = get_pack_time(loc_time + off_avg + (off_sq_sum * (int64_t)(loc_time - loc_avg) / loc_sq_sum));
	nrk_sem_post(skew_lock);
}

/*
 * function to convert a global time to a local time
 */
void psync_convert_global_to_local(nrk_time_t* global_time, nrk_time_t* local_time) {
	uint64_t glob_time = get_full_time(*global_time);
	nrk_sem_pend(skew_lock);
	uint64_t approx_loc = glob_time - off_avg;
	*local_time = get_pack_time(approx_loc - (off_sq_sum * (int64_t)(approx_loc - loc_avg) / loc_sq_sum));
	nrk_sem_post(skew_lock);
}

/* 
 * function to obtain local time difference from global time difference
 */
void psync_get_local_diff(nrk_time_t* glob_diff, nrk_time_t* loc_diff) {
	uint64_t g_diff = get_full_time(*glob_diff);
	nrk_sem_pend(skew_lock);
	*loc_diff = get_pack_time(g_diff - ((int64_t)g_diff * off_sq_sum) / loc_sq_sum);
	nrk_sem_post(skew_lock);
}

/*
 * function applied to received timestamps before forwarding
 */
void psync_edit_buf(uint8_t* buf, nrk_time_t* rcv_time) {
	nrk_time_t time;
	nrk_time_get(&time);
	new_loc = get_full_time(rcv_time);
	uint64_t diff = get_full_time(time) - new_loc + TX_DELAY;
	uint64_t* buf64 = (uint64_t*)buf;
	#ifdef COMPENSATED_FORWARDING
	new_glob = buf64[1];
	if (samples == MAX_SAMPLES) {
		uint8_t ind = (curr_ind + 1) % MAX_SAMPLES;
		buf64[1] += diff + lien_data[ind].skew_num * diff / line_data[ind].skew_den;
	}
	else
		buf64[1] += diff;
	#else
	new_glob = buf64[0];
	#endif
	buf64[0] += diff;
	edit = 1;
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
		nrk_time_get(&time);
		buf[0] = get_full_time(time) + TX_DELAY;
		#ifdef COMPENSATED_FORWARDING
		buf[1] = buf[0];
		#endif
		flash_tx_pkt((uint8_t*)buf, PKT_SIZE);
	}
	// functionality to be executed if the node is synchronizing to an external global clock
	else {
		edit = 0;
		nrk_time_t cur_time, time_after;
		nrk_time_get(&cur_time);
		nrk_time_add(&time_after, cur_time, *time);
		nrk_sem_pend(skew_lock);
		flash_enable(time, psync_edit_buf);
		while (!edit && (get_full_time(cur_time) < get_full_time(time_after)))
			nrk_time_get(&cur_time);
		nrk_sem_post(skew_lock);
		if (edit) {
			psync_add_point(new_loc, new_glob);
			edit = 0;
		}
	}
}
