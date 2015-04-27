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
*along with this program.If not, see <http://www.gnu.org/licenses/>.
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
//#include <pulse_sync.h>

#define UART_BUF_SIZE	16
#define TDMA_SLOT_LEN 4000//us

nrk_task_type TEST_TASK;
NRK_STK test_task_stack[NRK_APP_STACKSIZE];
void test_task(void);

RF_RX_INFO rfRxInfo;

#define NUM_NODES 2

uint8_t nodeID = 1;

uint8_t time_slots[NUM_NODES];

void nrk_create_taskset();

void nrk_register_drivers();

int main() {
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);
	
	nrk_init();
	
	nrk_led_clr(0);
	nrk_led_clr(1);
	nrk_led_clr(2);
	nrk_led_clr(3);
	
	nrk_time_set(0, 0);
	flash_init(14);
	flash_timer_setup();

	nrk_create_taskset();
	nrk_start();
	
	
	return 0;
}

void node_data_callb ack(uint8_t *data, uint64_t time)
{
	uint32_t time_32 = (uint32_t)time;
	
	printf("received packet [%d] time %lu\r\n", data[0], time_32);
	// TODO: Parse to JSON
	/*
	printf("{\"mac\":%d;\"temp\":%d}\n",
		data[0], *(uint16_t*)(data+1));
	*/
	
	nrk_led_toggle(GREEN_LED);
}

uint8_t rx_buf[RF_MAX_PAYLOAD_SIZE];

void rx_task ()
{
	/*
	rfRxInfo.pPayload = rx_buf;
	rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
	rfRxInfo.ackRequest= 0;
	nrk_int_enable();
	rf_init (&rfRxInfo, 13, 0xffff, 0x0);
	printf( "Waiting for packet...\r\n" );

	rx_end_callback(callback2);
*/
	uint8_t sync_buf[5];
	sync_buf[0] = nodeID;
	printf("sending synchronization buffer\r\n");
	uint32_t timestamp = (uint32_t)flash_get_current_time();
	*(uint32_t *)(sync_buf + 1) = timestamp;
	
	printf("transmitting packet [%d,%lu]\r\n", sync_buf[0], *(uint32_t *)(sync_buf + 1));
	printf("waiting to propagate flood\r\n");
	
	flash_tx_pkt(sync_buf, 5);
	flash_set_retransmit(0);
	
	while(1){
		/*
		 * //uint32_t cycle_start_time = flash_get_current_time();
		for (i=0; i<100; i++)
			nrk_spin_wait_us(1000);
		printf("waiting for flash packet - retransmit off\r\n");
		*/
		flash_enable(10, NULL, node_data_callback);
	}
}

void nrk_create_taskset() {
	TEST_TASK.task = rx_task;
	nrk_task_set_stk(&TEST_TASK, test_task_stack, NRK_APP_STACKSIZE);
	TEST_TASK.prio = 2;
	TEST_TASK.FirstActivation = TRUE;
	TEST_TASK.Type = BASIC_TASK;
	TEST_TASK.SchType = PREEMPTIVE;
	TEST_TASK.period.secs = 10;
	TEST_TASK.period.nano_secs = 0;
	TEST_TASK.cpu_reserve.secs = 0;
	TEST_TASK.cpu_reserve.nano_secs = 0;
	TEST_TASK.offset.secs = 0;
	TEST_TASK.offset.nano_secs = 0;

	nrk_activate_task(&TEST_TASK);
}
