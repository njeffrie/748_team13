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

/**
 * The Flash Flooding protocol exploits the capture effect observed in radio
 * reception, which allows some packets to be received correctly despite
 * concurrent transmissions on the same channel. This Flash Flood
 * implementation (Flash I) immediately re-transmits when a packet is received by the
 * CC2420 radio. 
*/

#ifndef FLASH_STACKSIZE
#define FLASH_STACKSIZE 128
#endif

#ifndef FLASH_TASK_PRIORITY
#define FLASH_TASK_PRIORITY 20
#endif

static nrk_task_type flash_task;
static NRK_STK flash_task_stack[FLASH_STACKSIZE];

// flash related flags
static uint16_t flash_message_len;
static uint32_t flash_err_count;
static int8_t flash_chan;
static int8_t auto_ReTx;
static int8_t flash_ReTx_ready;
static int8_t is_enabled;
static uint8_t flash_started;
static uint8_t current_ReTx_num;
static uint8_t flash_buf[FLASH_MAX_PKT_LEN];

//may not need these
RF_RX_INFO flash_rfRxInfo;
RF_TX_INFO flash_rfTxInfo;

nrk_sig_t packetRxSignal;
nrk_sig_t flash_tx_pkt_done_signal;
nrk_sig_t packetTxSignal;

/**
 *  This is a callback if you require immediate response to a packet
 */
RF_RX_INFO *rf_rx_callback (RF_RX_INFO * pRRI)
{
	if (is_enabled){
		flash_ReTx_ready = 1;
		printf("flash received a packet");
		nrk_event_signal(packetRxSignal);
	}
    return pRRI;
}

void flash_nw_task()
{
	int8_t n;
	uint32_t mask;
	// wait until flash has been initialized	
	while(!flash_started) nrk_wait_until_next_period();
	while(1){
		if (nrk_signal_register(packetRxSignal) != NRK_OK)
			nrk_kprintf( PSTR("failed to register packet rx signal"));
		if (nrk_signal_register(packetTxSignal) != NRK_OK)
			nrk_kprintf( PSTR("failed to register packet tx signal"));
		mask = nrk_event_wait(SIG(packetRxSignal) | SIG(packetTxSignal));
		if ((mask & SIG(packetRxSignal)) > 0){
			//receive packet
			n = rf_rx_packet_nonblock();
			if (n != 0)
			{
				n = 0;
				// wait for entire message to be received
				while((n = rf_polling_rx_packet()) == 0)
				{
					nrk_spin_wait_us(100);
				}
			}
			rf_rx_off();
			if (n != 1)
			{
				nrk_kprintf( PSTR("received message with incorrect checksum or CRC"));
			}
		}
		else if ((mask & SIG(packetTxSignal)) > 0){
			//transmit packet
			nrk_signal_register(flash_tx_pkt_done_signal);
			flash_rfTxInfo.pPayload = flash_buf;
			//get this right
			flash_rfTxInfo.length = flash_message_len;
			rf_data_mode();
			rf_rx_off();
			rf_tx_packet(&flash_rfTxInfo);
			rf_rx_off();
			nrk_event_signal(flash_tx_pkt_done_signal);
		}
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
	uint32_t mask;
	memcpy(flash_buf, buf, len);
	flash_message_len = len;
	nrk_signal_register (flash_tx_pkt_done_signal);
	nrk_event_signal (packetTxSignal);
	nrk_event_wait (SIG(flash_tx_pkt_done_signal));
}

int8_t flash_init (uint8_t chan)
{	
	// Setup flash signal handler
	packetRxSignal = nrk_signal_create();
	flash_tx_pkt_done_signal = nrk_signal_create();
	packetTxSignal = nrk_signal_create();
	auto_ReTx = 1;
	// make it so that next re transmission number is guaranteed to be less
	current_ReTx_num = 15;
	// 16 byte payload and 1 byte header for flash id and re tx number
	flash_message_len = FLASH_MAX_PKT_LEN;
	if (packetRxSignal == NRK_ERROR)
	{
		nrk_kprintf(PSTR("FLASH ERROR: creating re-transmit signal failed\r\n"));
		nrk_kernel_error_add(NRK_SIGNAL_CREATE_ERROR, nrk_cur_task_TCB->task_ID);
		return NRK_ERROR;
	}
	flash_err_count = 0;
	flash_rfRxInfo.pPayload = NULL;
	flash_rfRxInfo.max_length = 0;
    // Setup the cc2420 chip
    rf_power_up();
	rf_init (&flash_rfRxInfo, chan, 0xffff, 0);

	// Setup channel number
	flash_chan = chan;
	flash_started = 1;
    return NRK_OK;
}

void flash_disable()
{
    is_enabled=0;
    rf_power_down();
	rf_rx_off();
}

void flash_enable()
{
    is_enabled=1;
    rf_power_up();
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
}
