/*
 * pulse_sync.h: PulseSynce protocol implementation for Nano-RK
 * built on top of Flash network flooding protocol
 *
 * 18-748 S15: Group 13
 * Madhav Iyengar
 * Nat Jeffries
 * Miguel Sotolongo
 */

#ifndef NRK_PULSE_SYNC
#define NRK_PULSE_SYNC

#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <stdint.h>

#include <nrk.h>
#include <nrk_error.h>

// number of samples for regression line calculation
#define MAX_SAMPLES 4

// define this if you desire compensated forwarding in addition to simple
#define COMPENSATED_FORWARDING

// delay in ns for the physical reception and transmission processes
#define RX_TX_DELAY 100

// data structure for storing previous sync data in table for compensated forwarding
struct sync_point_info {
	uint64_t glob_time;
	uint64_t loc_time;
	uint64_t skew_num;
	uint64_t skew_den;
};

// values to be saved between synchronizations for faster regression line calculation
uint64_t loc_sum, loc_avg;
int64_t off_sum, off_avg;
uint64_t loc_sq_sum;
int64_t off_sq_sum;
uint8_t samples, curr_ind;

// semaphore (used as mutex) for using/changing current skew value
nrk_sem_t* skew_lock;

// circular buffer for recent regression line data
struct sync_point_info line_data[MAX_SAMPLES];

// whether or not this node is root, set on initialization
uint8_t is_root;

// function to initiate/reset pulsesync
void psync_init(uint8_t high_prio, uint8_t root);

// function to add a synchronization data point for regression line calculation
void psync_add_point(uint64_t loc_time, uint64_t glob_time);

// function to determine whether the node is fully synchronized
uint8_t psync_is_synced();

// function to get current global time
void psync_get_time(nrk_time_t* global_time);

// function to convert from local to global time
void psync_convert_local_to_global(nrk_time_t* local_time, nrk_time_t* global_time);

// function to convert from global to local time
void psync_convert_global_to_local(nrk_time_t* global_time, nrk_time_t* local_time);

// function to obtain local time difference from global time difference
void psync_get_local_diff(nrk_time_t* glob_diff, nrk_time_t* loc_diff);

// function to initiate a pulsesynce flood cycle
void psync_flood_wait();

#endif
