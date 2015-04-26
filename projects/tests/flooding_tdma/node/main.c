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
#define TDMA_SLOT_LEN 50000//us

nrk_task_type TEST_TASK;
NRK_STK test_task_stack[NRK_APP_STACKSIZE];
void test_task(void);

#define NUM_NODES 10

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

void time_sync_callback(uint8_t *buf, uint64_t recv_time){
	uint32_t global_time = *(uint32_t *)(buf + 1);
	//printf("got sync time from node %d\r\n", buf[0]);
	//printf("local time :%lu global time :%lu\r\n", (uint32_t)recv_time, global_time);
	flash_set_time(global_time); 
	printf("local time :%lu global time :%lu\r\n", (uint32_t)flash_get_current_time(), global_time);
}	

void test_task() {
	char msg[10];
	int i;
	volatile bool already_tx = false;
	for (i=0; i<10; i++)
		msg[i] = nodeID;

	printf("waiting for sync message\r\n");
	flash_enable(5, NULL, time_sync_callback);
	while(1){
		uint32_t cycle_start_time = flash_get_current_time();
		int slot = ((flash_get_current_time()/TDMA_SLOT_LEN) % NUM_NODES);
		if (slot != nodeID)
			already_tx = false;
		else if (!already_tx){
			//it's my turn!
			printf("transmitting slot=%d\r\n", slot);
			nrk_led_toggle(RED_LED);
			//add some redundancy
			for (i=0; i<3; i++)
				flash_tx_pkt(msg, 10);
			already_tx = true;
		}
	}
}

void nrk_create_taskset() {
	TEST_TASK.task = test_task;
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
