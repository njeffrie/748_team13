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

#ifndef MAC_ADDR
#define MAC_ADDR 0
#endif

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
uint64_t last_rx_time;

volatile bool flash_pkt_received;

// flash timer functions and variables
uint64_t current_time_ms;
void timer_3_callback();

//user function to modify buffer upon receive 
void (*user_rx_callback)(uint8_t* buf, uint64_t rcv_time);
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
	//printf("received packet\r\n");
	last_rx_time = nrk_full_time_get();
}

/* this will be called whenver flash listening is on */
void rx_finished_callback()
{
	//receive the packet
	nrk_led_set(ORANGE_LED);
	flash_pkt_received = true;
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
	nrk_led_set(BLUE_LED);
	flash_rfTxInfo.pPayload = buf;
	flash_rfTxInfo.length = len;
	if(rf_tx_packet_blocking(&flash_rfTxInfo) == NRK_ERROR){
		printf("ERROR: TX timed out\r\n");
	}

#ifdef FLASH2_ENABLED
	flash_rfTxInfo.cca = true;
	if (rf_tx_packet_blocking(&flash_rfTxInfo) == NRK_ERROR){
		printf("ERROR: TX timed out\r\n");
	}
#endif
	
	nrk_led_clr(BLUE_LED);
}

void flash_tx_callback_set(void(*callback)(uint16_t, uint8_t *))
{
	//flash_tx_callback = callback;
	tx_start_callback(callback);
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
	// tests to ensure proper performance of sub-modules
#ifdef DEBUG 
	flash_run_tests(); 
#endif
	// 16 byte payload and 1 byte header for flash id and re tx number
	flash_message_len = RF_MAX_PAYLOAD_SIZE;
	
	flash_err_count = 0;
	flash_rfRxInfo.pPayload = flash_buf;
	flash_rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
	flash_rfRxInfo.ackRequest = 0;
	
	flash_rfTxInfo.ackRequest = 0;
	flash_rfTxInfo.destAddr = 0;
	flash_rfTxInfo.cca = 0;

	//set callback functions for start and end of packet reception
	rx_start_callback(rx_started_callback);
	rx_end_callback(rx_finished_callback);
    
	rf_power_up();
	rf_init (&flash_rfRxInfo, chan, 0xffff, 0);
	nrk_int_enable();

	// Setup channel number
	flash_chan = chan;
	flash_started = 1;
    return NRK_OK;
}

void 
flash_enable(uint8_t msg_len, nrk_time_t* timeout, void (*edit_buf)(uint8_t *buf, uint64_t rcv_time))
{
    nrk_led_set(ORANGE_LED);
	is_enabled=1;
	flash_pkt_received = 0;
	flash_message_len = msg_len;
	/*
	if (timeout != NULL){
		nrk_time_t current_time;
		nrk_time_get(&current_time);
		//calculate time
		nrk_time_add(listen_timeout, *timeout, current_time);
	}
	*/
	user_rx_callback = edit_buf;
	nrk_int_enable();
	rf_rx_on();
	
	//gets most recently received buffer and puts rx data into falsh_rfRxInfo
	uint8_t resp;
	//int count = 0;
	while ((resp = (rf_rx_packet_nonblock())) == 0){
		//count ++;
		/*if (count > 10000){
			count = 0;
			printf("failed to receive packet... giving up!\r\n");
			break;
		}*/
	}

	if (!flash_pkt_received)
		printf("failed to correctly receive nonblocking packet\r\n");

	//ensure that rf rx if off after message has been received
	rf_rx_off();

	//get metadata about received packet
	flash_message_len = flash_rfRxInfo.length;
	memcpy(flash_buf, flash_rfRxInfo.pPayload, flash_rfRxInfo.length);
	//flash_buf = flash_rfRxInfo.pPayload;

	//call user callback function on buffer
	if (user_rx_callback != NULL)
		user_rx_callback(flash_buf, last_rx_time);
	nrk_led_clr(ORANGE_LED);

	//re transmission of packet
	nrk_led_set(BLUE_LED);

	//set packet to be transmitted
	flash_rfTxInfo.pPayload = flash_buf;

	//set tx structure
	flash_rfTxInfo.length = flash_message_len;
	flash_rfTxInfo.ackRequest = 0;
	flash_rfTxInfo.cca = 0;
	//if (flash_tx_callback != NULL)
		//flash_tx_callback(flash_message_len, flash_buf);
	rf_tx_packet(&flash_rfTxInfo);
	nrk_led_clr(BLUE_LED);
}

/* reset current time in microseconds and initialize the timer interrupt */
uint8_t flash_timer_setup(){
    uint8_t timer = NRK_APP_TIMER_0;

    current_time_ms = 0;
    /* inc counter every ms */
    if (nrk_timer_int_configure(timer, 1, 0x3E80, timer_3_callback) != NRK_OK)
        return NRK_ERROR;
    return nrk_timer_int_start(timer);
}

/* this will overflow after 2^32uS (4000 seconds... 1h 6m 40s) */
void timer_3_callback(){
    current_time_ms += 1;
}

uint64_t flash_get_current_time(){
	DISABLE_GLOBAL_INT();
    uint32_t offset_ticks = TCNT3;
	uint64_t ticks = current_time_ms;
	ENABLE_GLOBAL_INT();
    return (ticks * 1000000L) + offset_ticks * 62 + (offset_ticks >> 1);
}

void flash_reset_timer(){
    current_time_ms = 0;
}
