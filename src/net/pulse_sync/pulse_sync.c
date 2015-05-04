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
 * 16-748 S15: Group 14
 * Madhav Iyengar
 * Nat Jeffries
 * Miguel Sotolongo
 */

#include "pulse_sync.h"

// stores previous sync data in table for compensated forwarding
struct _sync_point_info {
	uint64_t glob_time;
	uint64_t loc_time;
	//int64_t skew_inv;
	//uint8_t skew_sign; 
	float skew;
};

// testing variables
//uint64_t next_glob;

// values to be saved between synchronizations for faster regression line calculation
uint64_t _loc_time_sum, _loc_time_avg;
int64_t _off_time_sum, _off_time_avg;
uint64_t _loc_sq_sum;
int64_t _off_sq_sum;
uint8_t _samples, _curr_ind;
uint8_t _shift_val;
float _div_factor;

// skew value
float _skew;

// new times to be added to regression line
uint64_t _new_loc, _new_glob;
volatile uint8_t _edit;

// compensated forwarding flag
uint8_t _comp_forw;

// semaphore (used as mutex) for using/changing current skew value
//nrk_sem_t* _skew_lock;

// circular buffer for recent regression line data
struct _sync_point_info line_data[MAX_SAMPLES];

// whether or not this node is root, set on initialization
uint8_t _is_root;

/*
 * function for initializing/reseting pulsesync
 */
void psync_init(/*uint8_t high_prio, */uint8_t comp_forw, uint8_t is_root, uint8_t chan) {
	//if (_skew_lock != NULL)
		//nrk_sem_delete(_skew_lock);
	//_skew_lock = nrk_sem_create(1, high_prio);
	flash_init(chan);
	_comp_forw = comp_forw;
	_is_root = is_root;
	_loc_time_sum = 0;
	_loc_time_avg = 0;
	_off_time_sum = 0;
	_off_time_avg = 0;
	_shift_val = 0;
	_div_factor = 1.0;
	_loc_sq_sum = 1;
	_off_sq_sum = 0;
	//skew_inv = 0x7fffffffffffffffLL;
	_skew = 0.0;
	_samples = 0;
	_curr_ind = 0;
	flash_timer_setup();
}

/*
 * set whether or not root node
 */
void psync_set_root(uint8_t is_root) {
	_is_root = is_root;
}

/*
 * set whether compensated forwarding on long delays
 */
void psync_set_comp_forw(uint8_t comp_forw) {
	_comp_forw = comp_forw;
}

/*
 * helper function to change nrk_time_t to uint64_t
 */
inline uint64_t _get_full_time(nrk_time_t time) {
	return (uint64_t)(time.secs) * 1000000L + time.nano_secs / 1000;
}

/*
 * helper function to change uint64_t to nrk_time_t
 */
inline nrk_time_t _get_pack_time(uint64_t time) {
	nrk_time_t nrk_time;
	nrk_time.secs = time / 1000000L;
	nrk_time.nano_secs = 1000 * (uint32_t)(time - 1000000L * (uint64_t)nrk_time.secs);
	return nrk_time;
}

/*
 * function for adding new synchronization point to regression line calculation
 */
void psync_add_point(uint64_t loc_time, uint64_t glob_time) {
	int64_t tmp_avg;
	int8_t tmp_rem;
	uint8_t next_ind;

	// determine new number of _samples
	uint8_t new_samples = _samples < MAX_SAMPLES ? _samples + 1 : MAX_SAMPLES;//_samples;

	next_ind = (_curr_ind + 1) % MAX_SAMPLES;

	// assemble previous values
	uint64_t prev_loc = line_data[next_ind].loc_time;
	int64_t prev_off = line_data[next_ind].glob_time - prev_loc;
	int64_t off_time = glob_time - loc_time;

	//// remove old average values from square sums
	// lock skew values
	//nrk_sem_pend(_skew_lock);

	if (_loc_sq_sum > 0x8000000000000000L) {
		_shift_val++;
		_div_factor *= 2;
		_loc_sq_sum = _loc_sq_sum >> 2;
		_off_sq_sum = _off_sq_sum >> 1;
	}

	// remove terms in local time deviation squared sum containing old mean local time
	_loc_sq_sum += (_loc_time_avg >> _shift_val) * ((int64_t)(2 * _loc_time_sum - _samples * _loc_time_avg) >> _shift_val);
	// overwrite old local time in square with new local time
	_loc_sq_sum += ((loc_time - prev_loc) >> _shift_val) * ((loc_time + prev_loc) >> _shift_val);

	// remove terms in offset deviation squared sum containing old mean local time and offset
	_off_sq_sum += _off_time_avg * ((int64_t)(_loc_time_sum - _samples * _loc_time_avg) >> _shift_val) + (_loc_time_avg >> _shift_val) * _off_time_sum;
	// overwrite old local time and offset terms with new local time and offset terms
	_off_sq_sum += (loc_time >> _shift_val) * off_time - (prev_loc >> _shift_val) * prev_off;

	//// calculate new mean local time and local time deviation squared
	_loc_time_sum += loc_time - prev_loc;
	tmp_avg = _loc_time_sum / new_samples;
	tmp_rem = _loc_time_sum - new_samples * tmp_avg; 
	_loc_time_avg = tmp_avg + (tmp_rem + new_samples / 2) / new_samples;

	// add terms in square containing new mean local time
	_loc_sq_sum += (_loc_time_avg >> _shift_val) * ((int64_t)(new_samples * _loc_time_avg - 2 * _loc_time_sum) >> _shift_val);

	//// calculate new mean global-local time offset and offset deviation squared
	tmp_avg = line_data[_curr_ind].glob_time - line_data[_curr_ind].loc_time;
	_off_time_sum += off_time - prev_off;
	tmp_avg = _off_time_sum / new_samples;
	tmp_rem = _off_time_sum - new_samples * tmp_avg;
	/*if ((tmp_rem >= -1) && (tmp_rem <= 1)) {
		tmp_rem = 0;
		_off_time_sum = new_samples * tmp_avg;
	}*/
	_off_time_avg = tmp_avg + (tmp_rem + new_samples / 2) / new_samples;

	// add terms to square containing new mean local time and offset
	_off_sq_sum += _off_time_avg * ((int64_t)(new_samples * _loc_time_avg - _loc_time_sum) >> _shift_val) - (_loc_time_avg >> _shift_val) * _off_time_sum;

	//// save new data into next entry in circular buffer
	_curr_ind = next_ind;
	_samples++;// = new_samples;
	// check for divide-by-0 errors
	if (_off_sq_sum && _loc_sq_sum)
		//skew_inv = _off_sq_sum > 0 ? _loc_sq_sum / _off_sq_sum : _loc_sq_sum / (~_off_sq_sum + 1);
		_skew = (float)_off_sq_sum / ((float)_loc_sq_sum * _div_factor);
	if (_skew > MAX_SKEW)
		_skew = MAX_SKEW;
	else if (_skew < -1.0 * MAX_SKEW)
		_skew = -1.0 * MAX_SKEW;
	//line_data[_curr_ind] = (struct _sync_point_info){glob_time, loc_time, skew_inv, (uint8_t)(_off_sq_sum > 0)};
	line_data[_curr_ind] = (struct _sync_point_info){glob_time, loc_time, _skew};

	// unlock skew values
	//nrk_sem_post(_skew_lock);
}

/*
 * function to determine if synchronization is complete
 */
uint8_t psync_is_synced() {
	return _samples >= MAX_SAMPLES;
}

/*
 * function to obtain current global time
 */
void psync_get_nrk_time(nrk_time_t* global_time) {
	uint64_t full_time = flash_get_current_time();
	//nrk_sem_pend(_skew_lock);
	//*global_time = _get_pack_time(full_time + (_off_time_avg << 0) + ((int64_t)(_off_sq_sum * (full_time - (_loc_time_avg << 0)) / _loc_sq_sum) >> 0/*>> _shift_val*/));
	*global_time = _get_pack_time(full_time + _off_time_avg + (int64_t)(_skew * (int64_t)(full_time - _loc_time_avg)));
	//nrk_sem_post(_skew_lock);
}

/*
 * function to obtain current global time as uint64_t
 */
uint64_t psync_get_time() {
	uint64_t full_time = flash_get_current_time();
	//nrk_sem_pend(_skew_lock);
	full_time += _off_time_avg + (int64_t)(_skew * (int64_t)(full_time - _loc_time_avg));	
	//nrk_sem_post(_skew_lock);
	return full_time;
}

/*
 * function to convert a local time to a global time
 */
void psync_local_to_global(nrk_time_t* local_time, nrk_time_t* global_time) {
	uint64_t loc_time = _get_full_time(*local_time);
	//nrk_sem_pend(_skew_lock);
	//*global_time = _get_pack_time(loc_time + (_off_time_avg << 0) + ((int64_t)(_off_sq_sum * (loc_time - (_loc_time_avg << 0)) / _loc_sq_sum) >> 0/*>> _shift_val*/));
	*global_time = _get_pack_time(loc_time + _off_time_avg + (int64_t)(_skew * (int64_t)(loc_time - _loc_time_avg)));
	//nrk_sem_post(_skew_lock);
}

/*
 * function to convert a global time to a local time
 */
void psync_global_to_local(nrk_time_t* global_time, nrk_time_t* local_time) {
	uint64_t glob_time = _get_full_time(*global_time);
	//nrk_sem_pend(_skew_lock);
	uint64_t approx_loc = glob_time - _off_time_avg;
	//*local_time = _get_pack_time(approx_loc - ((int64_t)(_off_sq_sum * (approx_loc - (_loc_time_avg << 0)) / _loc_sq_sum) >> 0/*>> _shift_val*/));
	//*local_time = _get_pack_time(approx_loc - (int64_t)(skew * (int64_t)(approx_loc - _loc_time_avg)));
	*local_time = _get_pack_time((approx_loc + (int64_t)(_skew * _loc_time_avg)) / (1.0 + _skew));
	//nrk_sem_post(_skew_lock);
}

/* 
 * function to obtain local time difference from global time difference
 */
void psync_local_diff(nrk_time_t* glob_diff, nrk_time_t* loc_diff) {
	uint64_t g_diff = _get_full_time(*glob_diff);
	//nrk_sem_pend(_skew_lock);
	//int64_t skew_comp = _off_sq_sum < 0 ? g_diff / (skew_inv << 0/*<< _shift_val*/) : ~(g_diff / (skew_inv << 0/*<< _shift_val*/)) + 1;
	//*loc_diff = _get_pack_time(g_diff + skew_comp);
	//*loc_diff = _get_pack_time(g_diff - (int64_t)(skew * g_diff));
	*loc_diff = _get_pack_time(g_diff / (1.0 + _skew));
	//nrk_sem_post(_skew_lock);
}

/*
 * function to obtain global time difference from local time difference
 */
void psync_global_diff(nrk_time_t* loc_diff, nrk_time_t* glob_diff) {
	uint64_t l_diff = _get_full_time(*loc_diff);
	//nrk_sem_pend(_skew_lock);
	//int64_t skew_comp = _off_sq_sum < 0 ? l_diff / (skew_inv << 0/*<< _shift_val*/) : ~(l_diff / (skew_inv << 0/*<< _shift_val*/)) + 1;
	//*glob_diff = _get_pack_time(l_diff - skew_comp);
	*glob_diff = _get_pack_time(l_diff + (int64_t)(_skew * l_diff));
	//nrk_sem_post(_skew_lock);
}

/*
 * function to handle received PulseSync floods
 * saves values to be added to regression table
 */
void psync_rx_callback(uint8_t* buf, uint64_t rcv_time) {
	_new_loc = rcv_time - PSYNC_RX_DELAY;
	_new_glob = *(uint64_t*)buf;
	/*uint64_t* buf64 = (uint64_t*)buf;
	#ifdef COMPENSATED_FORWARDING
	_new_glob = buf64[1];
	#else
	_new_glob = buf64[0];
	#endif*/
	_edit = 1;
}

/*
 * function to handle (re)transmission of PulseSynce floods
 * sets/alters buffer for Rx-Tx time delta
 */
void psync_tx_callback(uint16_t len, uint8_t* buf) {
	uint64_t* buf64 = (uint64_t*)buf;
	if (_is_root) { 
		buf64[0] = PSYNC_TX_DELAY;
		buf64[0] += flash_get_current_time();
		/*#ifdef COMPENSATED_FORWARDING
		buf64[1] = buf64[0];
		#endif*/
	}
	else {
		uint64_t diff = /*flash_get_current_time()*/ ~_new_loc + 1 + PSYNC_TX_DELAY;
		diff += flash_get_current_time();
		if ((diff > 500000L) && (_samples >= 2 * MAX_SAMPLES - 1)) {
			diff += PSYNC_COMP_FORW_DELAY;
			uint8_t ind = (_curr_ind + 1) % MAX_SAMPLES;
			diff += (int64_t)(line_data[ind].skew * diff);
		}
	
		/*#ifdef COMPENSATED_FORWARDING
		if (_samples >= 2 * MAX_SAMPLES - 1) {
			uint8_t ind = (_curr_ind + 1) % MAX_SAMPLES;
			buf64[1] += diff + (int64_t)(line_data[ind].skew * diff);
		}
		else
			buf64[1] += diff;
		#endif*/

		buf64[0] += diff;
		//next_glob = buf64[0];
		//_edit = 1;
	}
}

/*
 * function to initiate a pulsesync flood cycle 
 */
void psync_flood_wait(nrk_time_t* time, uint8_t retransmit) {
	// buffer for holding data to send/receive with flash (in this case uint64_t with time data)
	/*#ifdef COMPENSATED_FORWARDING
	uint64_t buf[2];
	#else
	uint64_t buf[1];
	#endif*/

	// set forwarding adjustment callback
	void* prev_tx_callback = flash_tx_callback_get();
	flash_tx_callback_set(psync_tx_callback);
	flash_set_retransmit(retransmit);

	// functionality to be executed if the node is set as the network global clock
	if (_is_root) {
		/*buf[0] = 0;
		#ifdef COMPENSATED_FORWARDING
		buf[1] = 0;
		#endif*/
		uint64_t buf[1] = {0};
		flash_tx_pkt((uint8_t*)buf, PKT_SIZE);
	}
	// functionality to be executed if the node is synchronizing to an external global clock
	else {
		_edit = 0;
		flash_enable(PKT_SIZE, time, psync_rx_callback);
		if (_edit) {
			//printf("loc: %lu << 32 + %lu, glob: %lu << 32 + %lu\r\n", ((uint32_t*)&_new_loc)[1], ((uint32_t*)&_new_loc)[0], ((uint32_t*)&_new_glob)[1], ((uint32_t*)&_new_glob)[0]);
			printf("loc: %luus, glob: %luus\r\n", (uint32_t)(_new_loc), (uint32_t)(_new_glob));
			psync_add_point(_new_loc, _new_glob);
			//printf("local: %luus, offsum: %ldus, offset: %ldus, skew * 10000000: %ld\r\n", (uint32_t)_loc_time_avg, (int32_t)_off_time_sum, (int32_t)_off_time_avg, (int32_t)(_skew * 10000000.0));
			//uint32_t shit_time = flash_get_current_time();
			//printf("loc_after: %lu\r\n", shit_time);
			_edit = 0;
		}
	}

	flash_tx_callback_set(prev_tx_callback);
}
