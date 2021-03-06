/******************************************************************************
*	Lab 3 - Build Your Own Sensor Network (Gateway)
*	Madhav Iyengar
*	Miguel Sotolongo
*	Nathaniel Jeffries
-------------------------------------------------------------------------------
*
*Nano-RK, a real-time operating system for sensor networks.
*Copyright (C) 2007, Real-Time and Multimedia Lab, Carnegie Mellon University
*All rights reserved.
*
*This is the Open Source Version of Nano-RK included as part of a Dual
*Licensing Model. If you are unsure which license to use please refer to:
*http://www.nanork.org/nano-RK/wiki/Licensing
*
*This program is free software: you can redistribute it and/or modify
*it under the terms of the GNU General Public License as published by
*the Free Software Foundation, version 2.0 of the License.
*
*This program is distributed in the hope that it will be useful,
*but WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*GNU General Public License for more details.
*
*You should have received a copy of the GNU General Public License
*along with this program. If not, see <http://www.gnu.org/licenses/>.
*
*******************************************************************************/

#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <avr/sleep.h>

#include <nrk.h>
#include <nrk_error.h>

#include <hal.h>
#include <string.h>
#include <flash.h>
#include <pulse_sync.h>
#include "../tdma_constants.h"

#define UART_BUF_SIZE	16

nrk_task_type TEST_TASK;
NRK_STK test_task_stack[NRK_APP_STACKSIZE];
void test_task(void);

RF_RX_INFO rfRxInfo;

uint8_t nodeID = 0;

uint8_t time_slots[NUM_NODES];

void nrk_create_taskset();

void nrk_register_drivers();

int32_t correct = 0;
int32_t failed = 0;
uint8_t  off_sign = 0; // 1 Positive, 0 Negative
uint32_t offset = 0; // Initial overhead offset
void node_data_callback(uint8_t *data, uint64_t time)
{
	uint32_t time_32 = (uint32_t)time;
	uint32_t lt32 = *(uint32_t*) (data+1);
	lt32 = lt32 + PSYNC_TX_DELAY + PSYNC_RX_DELAY;

	int slot = (flash_get_current_time()/TDMA_SLOT_LEN)%NUM_NODES;
	int sender_node = data[0];
	if (sender_node != slot){
		failed ++;
		//printf("slot error[slot:%d,sender:%d]\r\n", slot, sender_node);
	}
	else{
		correct ++;
		//printf("slot correct\r\n");
	}
	//if (!((failed + correct)%10))
	//	printf("percentage correct:%ld, [c:%ld, f:%ld] press:%lu\r\n", 
	//			(correct * 100)/(correct + failed), correct, failed, press);
	if(!offset)
	{
		offset = (lt32 > time_32) ? lt32 - time_32 : time_32 - lt32;
		off_sign = (lt32 > time_32);
	} else lt32 = (off_sign) ? lt32 - offset : lt32 + offset;

	printf("{\"mac\":%d;\"slot\":%d;\"lt\":%lu;\"gt\":%lu;}\r\n", sender_node, slot, lt32, time_32);

	//printf("{\"mac\":%d;\"temp\":%d}\r\n",
	//	data[0], *(uint16_t*)(data+1));
	
	nrk_led_toggle(GREEN_LED);
}

uint8_t rx_buf[RF_MAX_PAYLOAD_SIZE];


void main ()
{
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);
	
	nrk_led_clr(0);
	nrk_led_clr(1);
	nrk_led_clr(2);
	nrk_led_clr(3);
	
	nrk_time_set(0, 0);
	flash_init(17);
	flash_timer_setup();

	psync_init(1, 1, 17);

	uint8_t sync_buf[16];
	sync_buf[0] = nodeID;
	printf("sending synchronization buffer\r\n");
		
	printf("waiting to propagate flood\r\n");

	uint8_t sync_msg[2] = {0, 0};
	
	psync_flood_tx(2, sync_msg);

	flash_set_retransmit(0);

	uint64_t timeout = TDMA_SLOT_LEN;

	uint8_t already_sync = 1;
	while(1){
		uint8_t slot = ((flash_get_current_time() + 30) / TDMA_SLOT_LEN) % NUM_NODES;
		if (!slot && !already_sync) { //perform time sync
			psync_flood_tx(2, sync_msg);
			already_sync = 1;
			nrk_spin_wait_us((flash_get_current_time() - 30) % TDMA_SLOT_LEN);
		}
		else if (slot) {
			already_sync = 0;
			flash_enable(5, &timeout, node_data_callback);
		}
	}
}
