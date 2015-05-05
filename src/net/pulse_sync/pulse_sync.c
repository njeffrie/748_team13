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

#include "pulse_sync.h"

// stores previous sync data in table for compensated forwarding
struct _sync_point_info {
	uint64_t glob_time;
	uint64_t loc_time;
	float skew;
};

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

// circular buffer for recent regression line data
struct _sync_point_info line_data[MAX_SAMPLES];

// whether or not this node is root, set on initialization
uint8_t _is_root;

// additional header message size
uint8_t _msg_size;

// rx check callback function
uint8_t (*_rx_check_callback)(uint8_t* buf);

/*
 * function for initializing/reseting pulsesync
 */
void psync_init(uint8_t comp_forw, uint8_t is_root, uint8_t chan) {
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
	_skew = 0.0;
	_samples = 0;
	_curr_ind = 0;
	_msg_size = 0;
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

	// accomodate for overflow of the denominator in skew fraction
	if (_loc_sq_sum > 0x8000000000000000L) {
		_shift_val++;
		_div_factor *= 2;
		_loc_sq_sum = _loc_sq_sum >> 2;
		_off_sq_sum = _off_sq_sum >> 1;
	}

	//// remove old average values from square sums
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
	_off_time_avg = tmp_avg + (tmp_rem + new_samples / 2) / new_samples;

	// add terms to square containing new mean local time and offset
	_off_sq_sum += _off_time_avg * ((int64_t)(new_samples * _loc_time_avg - _loc_time_sum) >> _shift_val) - (_loc_time_avg >> _shift_val) * _off_time_sum;

	//// save new data into next entry in circular buffer
	_curr_ind = next_ind;
	_samples++;
	// check for divide-by-0 errors
	if (_off_sq_sum && _loc_sq_sum)
		_skew = (float)_off_sq_sum / ((float)_loc_sq_sum * _div_factor);
	if (_skew > MAX_SKEW)
		_skew = MAX_SKEW;
	else if (_skew < -1.0 * MAX_SKEW)
		_skew = -1.0 * MAX_SKEW;
	line_data[_curr_ind] = (struct _sync_point_info){glob_time, loc_time, _skew};
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
	*global_time = _get_pack_time(full_time + _off_time_avg + (int64_t)(_skew * (int64_t)(full_time - _loc_time_avg)));
}

/*
 * function to obtain current global time as uint64_t
 */
uint64_t psync_get_time() {
	uint64_t full_time = flash_get_current_time();
	return full_time + _off_time_avg + (int64_t)(_skew * (int64_t)(full_time - _loc_time_avg));	
}

/*
 * function to convert a local time to a global time
 */
void psync_local_to_global(nrk_time_t* local_time, nrk_time_t* global_time) {
	uint64_t loc_time = _get_full_time(*local_time);
	*global_time = _get_pack_time(loc_time + _off_time_avg + (int64_t)(_skew * (int64_t)(loc_time - _loc_time_avg)));
}

/*
 * function to convert a global time to a local time
 */
void psync_global_to_local(nrk_time_t* global_time, nrk_time_t* local_time) {
	uint64_t glob_time = _get_full_time(*global_time);
	uint64_t approx_loc = glob_time - _off_time_avg;
	*local_time = _get_pack_time((approx_loc + (int64_t)(_skew * _loc_time_avg)) / (1.0 + _skew));
}

/* 
 * function to obtain local time difference from global time difference
 */
void psync_local_diff(nrk_time_t* glob_diff, nrk_time_t* loc_diff) {
	uint64_t g_diff = _get_full_time(*glob_diff);
	*loc_diff = _get_pack_time(g_diff / (1.0 + _skew));
}

/*
 * function to obtain global time difference from local time difference
 */
void psync_global_diff(nrk_time_t* loc_diff, nrk_time_t* glob_diff) {
	uint64_t l_diff = _get_full_time(*loc_diff);
	*glob_diff = _get_pack_time(l_diff + (int64_t)(_skew * l_diff));
}

/*
 * function to handle received PulseSync floods
 * saves values to be added to regression table
 */
void _psync_rx_callback(uint8_t* buf, uint64_t rcv_time) {
	if (_rx_check_callback && !_rx_check_callback(buf))
		return;
	_new_loc = rcv_time - PSYNC_RX_DELAY;
	_new_glob = *(uint64_t*)(buf + _msg_size);
	_edit = 1;
}

/*
 * function to handle (re)transmission of PulseSynce floods
 * sets/alters buffer for Rx-Tx time delta
 */
void _psync_tx_callback(uint16_t len, uint8_t* buf) {
	uint64_t* buf64 = (uint64_t*)(buf + _msg_size);
	if (_is_root) { 
		buf64[0] = PSYNC_TX_DELAY;
		buf64[0] += flash_get_current_time();
	}
	else {
		uint64_t diff = ~_new_loc + 1 + PSYNC_TX_DELAY;
		diff += flash_get_current_time();
		if ((diff > 500000L) && (_samples >= 2 * MAX_SAMPLES - 1)) {
			diff += PSYNC_COMP_FORW_DELAY;
			uint8_t ind = (_curr_ind + 1) % MAX_SAMPLES;
			diff += (int64_t)(line_data[ind].skew * diff);
		}

		buf64[0] += diff;
	}
}

/*
 * function to block while listening for pulsesync flood
 */
int8_t psync_flood_rx(uint64_t* time, uint8_t retransmit, uint8_t msg_size, uint8_t (*check_func)(uint8_t* buf)) {
	// check if root
	if (_is_root)
		return NRK_ERROR;

	_rx_check_callback = check_func;
	_msg_size = msg_size;

	// set forwarding adjustment callback and retransmission, saving previous values
	void* prev_tx_callback = flash_tx_callback_get();
	uint8_t prev_retransmit = flash_get_retransmit();
	flash_tx_callback_set(_psync_tx_callback);
	flash_set_retransmit(retransmit);

	int8_t ret_val = 0;
	_edit = 0;
	flash_enable(PKT_SIZE + _msg_size, time, _psync_rx_callback);
	if (_edit) {
		ret_val = 1;
		printf("loc: %luus, glob: %luus\r\n", (uint32_t)(_new_loc), (uint32_t)(_new_glob));
		psync_add_point(_new_loc, _new_glob);
		printf("skew: %ld\r\n", (int32_t)(_skew * 1000000));
		_edit = 0;
		ret_val = NRK_OK;
	}

	// revert tx callback and retransmission
	flash_tx_callback_set(prev_tx_callback);
	flash_set_retransmit(prev_retransmit);
	_msg_size = 0;
	_rx_check_callback = NULL;

	return ret_val;
}

/*
 * function to initiate a pulsesync flood
 */
int8_t psync_flood_tx(uint8_t msg_size, uint8_t* msg) {
	// check if root
	if (!_is_root)
		return NRK_ERROR;

	_msg_size = msg_size;

	// set forwarding adjustment callback, saving previous function
	void* prev_tx_callback = flash_tx_callback_get();
	flash_tx_callback_set(_psync_tx_callback);

	uint8_t buf[PKT_SIZE + _msg_size];
	memcpy(buf, msg, _msg_size);

	*(uint64_t*)(buf + _msg_size) = 0;
	flash_tx_pkt(buf, PKT_SIZE + _msg_size);

	// revert tx callback
	flash_tx_callback_set(prev_tx_callback);
	_msg_size = 0;

	return NRK_OK;
}
