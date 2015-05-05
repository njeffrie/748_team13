/******************************************************************************
*	Lab 3 - Build Your Own Sensor Network (Gateway)
*	Madhav Iyengar
*	Nathaniel Jeffries
*	Miguel Sotolongo
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
#include <nrk_timer.h>
#include <nrk_driver_list.h>
#include <nrk_driver.h>
#include <ff_basic_sensor.h>

#include "../tdma_constants.h"
#include <pulse_sync.h>
#define UART_BUF_SIZE	16

nrk_task_type TEST_TASK;
NRK_STK test_task_stack[NRK_APP_STACKSIZE];
void test_task(void);

//#define NUM_NODES 10

uint8_t nodeID = 4;

uint8_t time_slots[NUM_NODES];

void nrk_create_taskset();

void nrk_register_drivers();

uint8_t val;

int main_disabled() {
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

	val = nrk_register_driver(&dev_manager_ff3_sensors, FIREFLY_3_SENSOR_BASIC);
	if (val == NRK_ERROR) printf("failed to register drivers\r\n");

	//nrk_create_taskset();
	nrk_start();
	
	return 0;
}

/*void time_sync_callback(uint8_t *buf, uint64_t recv_time){
	uint32_t prev_local_time = (uint32_t)recv_time;//(uint32_t)flash_get_current_time();
	uint32_t global_time = *(uint32_t *)(buf + 1);
	flash_set_time(global_time + 673); 

	//printf("global time is %ld us ahead of local time\r\n", (int32_t)prev_local_time - (int32_t)global_time);
	//printf("prev time :%lu global time :%lu\r\n", prev_local_time, global_time);
}*/	

uint8_t msg[PKT_LEN];

void main() {
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);
	
	//nrk_init();
	
	nrk_led_clr(0);
	nrk_led_clr(1);
	nrk_led_clr(2);
	nrk_led_clr(3);
	
	nrk_time_set(0, 0);

	flash_init(13);
	flash_timer_setup();

	psync_init(1, 0, 13);

	val = nrk_register_driver(&dev_manager_ff3_sensors, FIREFLY_3_SENSOR_BASIC);
	if (val == NRK_ERROR) printf("failed to register drivers\r\n");

	//nrk_create_taskset();
	//nrk_start();
	int i;
	//uint32_t time_since_sync;
	int8_t fd, ret;
	uint32_t press;
	volatile bool already_tx = false;
	uint64_t sync_time = 0;

	//////
	//press = 0;

	uint32_t num_sync = 0;

	fd = nrk_open(FIREFLY_3_SENSOR_BASIC, READ);

	printf("waiting for sync message\r\n");

	psync_flood_wait(NULL, 1);
	num_sync++;
	//flash_enable(5, NULL, time_sync_callback);
	flash_set_retransmit(0);
	//time_sinc_sync = TRANS_SYNC_TIME;
	bool already_sync = false;
	uint8_t already_sense = 0;
	int cycle_count;
	uint8_t sensing_slot = (nodeID >= NUM_NODES - 2) ? 1 : nodeID + 1;
	while(1){
		cycle_count ++;
		if (cycle_count > 10000){
			cycle_count = 0;
			printf("press:%lu\r\n", press);
		}
		uint64_t cycle_start_time = psync_get_time();//flash_get_current_time();
		uint8_t slot = (((cycle_start_time + 55) / TDMA_SLOT_LEN) % NUM_NODES);
		if ((slot == sensing_slot) && !already_sense){
			//printf("s");
		 	already_tx = false;
		 	ret = nrk_set_status(fd,SENSOR_SELECT,PRESS);
		 	ret = nrk_read(fd,(uint8_t *)&press,4);
			nrk_spin_wait_us((psync_get_time() - 55) % TDMA_SLOT_LEN);
			already_sense = 1;
		}
		// root node's synchronization slot 
		else if ((slot == 0)/* && (!already_sync)*/){ //root's slot
			//time_sinc_sync ++;
			uint64_t comp = psync_is_synced() ? STEADY_SYNC_TIME + sync_time : TRANS_SYNC_TIME + sync_time;
			if (cycle_start_time > comp/*time_sinc_sync >= TRANS_SYNC_TIME*/){
				//time_sinc_sync = 0;
				//printf("\r\nwaiting for sync message\r\n");
				//flash_enable(5, NULL, time_sync_callback);
				//printf("slot_before: %lu\r\n", (psync_get_time()/TDMA_SLOT_LEN));
				psync_flood_wait(NULL, 0);
				//printf("slot_after: %lu\r\n", (psync_get_time()/TDMA_SLOT_LEN));
				sync_time = cycle_start_time;
				//already_sync = true;
				already_tx = true;
				num_sync++;
				printf("s: %lu\r\n", num_sync);
			}
			//else
				//already_tx = false;
		}
		//else if (slot == 0)
			//already_tx = false;

		////TODO: add additional message to pulse sync????

		/* occurs only one time per full tdma cycle */
		else if ((slot == nodeID) && (!already_tx)){
			//it's my turn!
			//printf("t");
			//already_sync = false;
			//TODO: Send timestamp to master
			//TODO: Send RSSI to master
			
			nrk_led_toggle(RED_LED);
			// fill buffer with node id and sensor data 
			msg[0] = nodeID;
			*(uint32_t *)(msg + 1) = press;
			//add some redundancy
			//flash_tx_callback_set(NULL);
			for (i=0; i<1; i++){
				flash_tx_pkt(msg, 5);
			}	
			already_tx = true;
			already_sense = 0;
		}
	}
}
/*
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
}*/
