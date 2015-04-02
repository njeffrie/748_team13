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
*  Nat Jeffries
*******************************************************************************/

#include "flash.h"

#define DEBUG

#ifndef FLASH_STACKSIZE
#define FLASH_STACKSIZE 256
#endif

#ifndef FLASH_TASK_PRIORITY
#define FLASH_TASK_PRIORITY 20
#endif

#ifndef MAC_ADDR
#define MAC_ADDR 0
#endif

static nrk_task_type flash_task;
static NRK_STK flash_task_stack[FLASH_STACKSIZE];

static nrk_task_type timeout_task;
static NRK_STK timeout_task_stack[FLASH_STACKSIZE];

// flash related flags
static uint16_t flash_message_len;
static uint32_t flash_err_count;
static int8_t flash_chan;
static int8_t is_enabled;
static uint8_t flash_started;
//static uint8_t *flash_buf;
static uint8_t flash_buf[FLASH_MAX_PKT_LEN];

//user supplied when flash_enable is called
nrk_time_t *listen_timeout;

//first rx'd packet is automatically timestamped
nrk_time_t last_rx_time;

//user function to modify buffer upon receive 
void (*user_rx_callback)(uint8_t* buf, nrk_time_t* rcv_time);
void(*flash_tx_callback)(uint16_t len, uint8_t *buf);

RF_RX_INFO flash_rfRxInfo;
RF_TX_INFO flash_rfTxInfo;

//signals to allow predictable tx and rx turnarounds
nrk_sig_t packetRxSignal;
nrk_sig_t flash_tx_pkt_done_signal;
nrk_sig_t packetTxSignal;

/**
 *  This is a callback if you require immediate response to a packet
 */
RF_RX_INFO *rf_rx_callback (RF_RX_INFO * pRRI)
{
    //code here appears to never run under current setup
	return pRRI;
}

void rx_started_callback()
{
	nrk_time_get(&last_rx_time);
}

/* this will be called whenver flash listening is on */
void rx_finished_callback()
{
	//printf("flash received a packet\r\n");
	nrk_event_signal(packetRxSignal);
}
	

// return 1 if a > b
// return 0 if a == b
// return -1 if a < b
int8_t nrk_time_compare(nrk_time_t *a, nrk_time_t *b)
{
	nrk_time_compact_nanos(a);
	nrk_time_compact_nanos(b);
	if (a->secs > b->secs)
		return 1;
	if (a->secs < b->secs)
		return -1;
	if (a->secs == b->secs)
	{
		if (a->nano_secs > b->nano_secs)
			return 1;
		if (a->nano_secs < b->nano_secs)
			return -1;
	}
	return 0;
}

void flash_nw_task()
{
	uint32_t mask;
	// wait until flash has been initialized	
	//nrk_kprintf(PSTR("flash nw task waiting until flash starts\r\n"));
	while(!flash_started) 
		nrk_wait_until_next_period();
	//nrk_kprintf(PSTR("flash started\r\n"));
	
	if (nrk_signal_register(packetRxSignal) != NRK_OK)
		nrk_kprintf( PSTR("failed to register packet rx signal"));
	if (nrk_signal_register(packetTxSignal) != NRK_OK)
		nrk_kprintf( PSTR("failed to register packet tx signal"));
	
	while(1){
		//wait for either tx or rx to be ready for processing
		mask = nrk_event_wait(SIG(packetRxSignal) | SIG(packetTxSignal));
		
		if ((mask & SIG(packetRxSignal)) > 0){
			nrk_led_set(ORANGE_LED);

			//gets most recently received buffer and puts rx data into falsh_rfRxInfo
			if (rf_rx_packet_nonblock() != NRK_OK)
				printf("failed to correctly receive nonblocking packet\r\n");

			//ensure that rf rx if off after message has been received
			rf_rx_off();
			
			//get metadata about received packet
			flash_message_len = flash_rfRxInfo.length;
			memcpy(flash_buf, flash_rfRxInfo.pPayload, flash_rfRxInfo.length);
			//flash_buf = flash_rfRxInfo.pPayload;

			//call user callback function on buffer
			if (user_rx_callback != NULL)
				user_rx_callback(flash_buf, &last_rx_time);
			nrk_led_clr(ORANGE_LED);
		}
		if ((mask & (SIG(packetRxSignal) | SIG(packetTxSignal))) > 0){
			nrk_led_set(BLUE_LED);

			//set packet to be transmitted
			flash_rfTxInfo.pPayload = flash_buf;
			
			//get this right
			flash_rfTxInfo.length = flash_message_len;
			flash_rfTxInfo.ackRequest = 0;
			flash_rfTxInfo.cca = 0;
			//rf_rx_on();
			if (flash_tx_callback != NULL)
				flash_tx_callback(flash_message_len, flash_buf);
			rf_tx_packet(&flash_rfTxInfo);
			//rf_rx_off();
			nrk_event_signal(flash_tx_pkt_done_signal);
			nrk_led_clr(BLUE_LED);
		}
		else {
			nrk_kprintf(PSTR("woke up due to wrong signal\r\n"));
		}
		//nrk_kprintf(PSTR("finished cycle of flash nw task\r\n"));
	}
}

void flash_timeout_task()
{
	nrk_time_t cur_time;
	//printf("timeout task started\r\n");
	while (1){
		// wait until a timeout is set
		while(listen_timeout == NULL) nrk_wait_until_next_period();
		//printf("timeout has been set... checking\r\n");
		nrk_time_get(&cur_time);
		//check if timeout condition has occurred
		if (nrk_time_compare(&cur_time, listen_timeout) < 0){
			//printf("flash receive timed out!\r\n");
			is_enabled = 0;
			rf_rx_off();
			listen_timeout = NULL;
		}
		nrk_wait_until_next_period();
	}
}

void flash_msg_len_set(uint16_t msg_len)
{
	flash_message_len = msg_len;
}

uint16_t flash_msg_len_get()
{
	return flash_message_len;
}

int8_t flash_rf_power_set(uint8_t power)
{
	if (power>31) return NRK_ERROR;
	rf_tx_power(power);
	return NRK_OK;
}

void flash_err_count_reset()
{
	flash_err_count = 0;
}

uint32_t flash_err_count_get()
{
	return flash_err_count;
}

void flash_tx_pkt(uint8_t *buf, uint8_t len)
{
	memcpy(flash_buf, buf, len);
	flash_message_len = len;
	/*if(rf_tx_pkt_blocking(buf, len) == NRK_ERROR){
		nrk_printf(PSTR("ERROR: TX timed out\r\n"));
	}*/
	nrk_event_signal (packetTxSignal);
	nrk_signal_register (flash_tx_pkt_done_signal);
	nrk_event_wait (SIG(flash_tx_pkt_done_signal));
	
}

void flash_tx_callback_set(void(*callback)(uint16_t, uint8_t *))
{
	flash_tx_callback = callback;
}

void flash_run_tests()
{
	bool bad = false;
	nrk_time_t t1, t2;
	t1.secs = 1;
	t2.secs = 2;
	if (nrk_time_compare(&t1, &t2) != -1)
		bad = true;
	if (nrk_time_compare(&t2, &t1) != 1)
		bad = true;
	t2.secs = 1;
	t1.nano_secs = 30;
	t2.nano_secs = 31;
	if (nrk_time_compare(&t1, &t2) != -1)
		bad = true;
	if (nrk_time_compare(&t2, &t1) != 1)
		bad = true;
	t2.nano_secs = 30;
	if (nrk_time_compare(&t1, &t2) != 0)
		bad = true;
	if (nrk_time_compare(&t2, &t1) != 0)
		bad = true;
	if (bad){
		printf("time compare error\r\n");
		exit(1);
	}
}

int8_t flash_init (uint8_t chan)
{	
	// Setup flash signal handler
	packetRxSignal = nrk_signal_create();
	flash_tx_pkt_done_signal = nrk_signal_create();
	packetTxSignal = nrk_signal_create();

	// tests to ensure proper performance of sub-modules
#ifdef DEBUG 
	flash_run_tests(); 
#endif
	// 16 byte payload and 1 byte header for flash id and re tx number
	flash_message_len = RF_MAX_PAYLOAD_SIZE;
	if (packetRxSignal == NRK_ERROR)
	{
		nrk_kprintf(PSTR("FLASH ERROR: creating re-transmit signal failed\r\n"));
		nrk_kernel_error_add(NRK_SIGNAL_CREATE_ERROR, nrk_cur_task_TCB->task_ID);
		return NRK_ERROR;
	}
	flash_err_count = 0;
	flash_rfRxInfo.pPayload = flash_buf;
	flash_rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
	flash_rfRxInfo.ackRequest = 0;
	
	//set callback functions for start and end of packet reception
	rx_start_callback(rx_started_callback);
	rx_end_callback(rx_finished_callback);
    
	rf_power_up();
	rf_init (&flash_rfRxInfo, chan, 0xffff, 0);

	// Setup channel number
	flash_chan = chan;
	flash_started = 1;
    return NRK_OK;
}

void 
flash_enable(uint8_t msg_len, nrk_time_t* timeout, void (*edit_buf)(uint8_t *buf, nrk_time_t* rcv_time))
{
    is_enabled=1;
	flash_message_len = msg_len;
	if (timeout != NULL){
		nrk_time_t current_time;
		nrk_time_get(&current_time);
		//calculate time
		nrk_time_add(listen_timeout, *timeout, current_time);
	}
	user_rx_callback = edit_buf;
	rf_rx_on();
}

void flash_task_config()
{
	nrk_task_set_entry_function( &flash_task, flash_nw_task);
	nrk_task_set_stk( &flash_task, flash_task_stack, FLASH_STACKSIZE);
	flash_task.prio = FLASH_TASK_PRIORITY;
	flash_task.FirstActivation = TRUE;
	flash_task.Type = BASIC_TASK;
	flash_task.SchType = NONPREEMPTIVE;
	flash_task.period.secs = 0;
	flash_task.period.nano_secs = 10 * NANOS_PER_MS;
	flash_task.cpu_reserve.secs = 0;
	flash_task.cpu_reserve.nano_secs = 0;
	flash_task.offset.secs = 0;
	flash_task.offset.nano_secs = 0;
	nrk_activate_task (&flash_task);
	
	nrk_task_set_entry_function( &timeout_task, flash_timeout_task);
	nrk_task_set_stk( &timeout_task, timeout_task_stack, FLASH_STACKSIZE);
	timeout_task.prio = FLASH_TASK_PRIORITY - 1;
	timeout_task.FirstActivation = TRUE;
	timeout_task.Type = BASIC_TASK;
	timeout_task.SchType = NONPREEMPTIVE;
	timeout_task.period.secs = 0;
	timeout_task.period.nano_secs = 10 * NANOS_PER_MS;
	timeout_task.cpu_reserve.secs = 0;
	timeout_task.cpu_reserve.nano_secs = 0;
	timeout_task.offset.secs = 0;
	timeout_task.offset.nano_secs = 0;
	nrk_activate_task (&timeout_task);
}
