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

uint16_t tdma_slot_len = TDMA_SLOT_LEN, next_slot_len = TDMA_SLOT_LEN;

uint32_t steady_sync_time = STEADY_SYNC_TIME;

uint8_t nodeID = 2;

uint8_t initiate = 0;

//uint8_t rx_called = 0;

uint8_t time_slots[NUM_NODES];

void nrk_create_taskset();

void nrk_register_drivers();

uint8_t val;

uint8_t msg[16];

void rx_callback(uint8_t* buf, uint64_t rcv_time) {
	if (buf[0])
		return 0;

	// set next tdma slot length
	int temp = next_slot_len;
	next_slot_len = TDMA_SLOT_LEN + 64 * (uint16_t)buf[2];

	//rx_called = 1;

	if ((initiate == 1) && (temp == next_slot_len))
		initiate = 2;
	else if (!initiate)
		initiate = 1;

	switch (buf[1]) {
		case 1:
			steady_sync_time = *(uint32_t*)(buf + 3); 
			break;
		default: 
			break;
	}
}

uint8_t psync_rx_check_callback(uint8_t* buf, uint64_t rcv_time) {
	if (buf[0])
		return 0;

	rx_callback(buf, rcv_time);

	return *(uint16_t*)buf == 0;
}

void main() {
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);
	
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

	int i;
	int8_t fd, ret;
	uint32_t press;
	volatile bool already_tx = false;
	uint64_t sync_time = 0;

	fd = nrk_open(FIREFLY_3_SENSOR_BASIC, READ);

	printf("waiting for sync message\r\n");

	// initiate synchronization waiting followed by tdma slot detection waiting
	while (!psync_flood_rx(NULL, 0, 3, psync_rx_check_callback));
	while (initiate != 2)
		flash_enable(7, NULL, rx_callback);//(NULL, 0, 3, psync_rx_check_callback);
	tdma_slot_len = next_slot_len;

	sync_time = psync_get_time();
	flash_set_retransmit(0);
	uint8_t already_sense = 0;
	uint8_t already_slot0 = 0;
	int cycle_count;
	uint64_t timeout = tdma_slot_len;
	uint8_t sensing_slot = (nodeID >= NUM_NODES - 2) ? 1 : nodeID + 1;
	while(1) {
		cycle_count ++;
		if (cycle_count > 10000){
			cycle_count = 0;
			//printf("press:%lu\r\n", press);
		}
		uint64_t cycle_start_time = psync_get_time();
		uint8_t slot = (((cycle_start_time + 55) / tdma_slot_len) % NUM_NODES);
		if ((slot == sensing_slot) && !already_sense){
		 	//already_tx = false;
		 	ret = nrk_set_status(fd,SENSOR_SELECT,PRESS);
		 	ret = nrk_read(fd,(uint8_t *)&press,4);
			nrk_spin_wait_us((psync_get_time() - 55) % tdma_slot_len);
			already_sense = 1;
			already_slot0 = 0;
		}
		// root node's synchronization slot 
		else if (!slot && !already_slot0) { //root's slot
			uint64_t comp = psync_is_synced() ? steady_sync_time + sync_time : TRANS_SYNC_TIME + sync_time;
			already_slot0 = 1;
			//rx_called = 0;
			already_tx = 0;
			if ((cycle_start_time > comp) && psync_flood_rx(&timeout, 0, 3, psync_rx_check_callback)) {
				sync_time = cycle_start_time;
				already_tx = true;
				already_sense = 0;
			}
			else
				flash_enable(7, NULL, rx_callback);

			if (next_slot_len != tdma_slot_len) {
				printf("waiting to change slot len\r\n");
				initiate = 1;
				while (initiate != 2)
					flash_enable(7, NULL, rx_callback);
					//psync_flood_rx(NULL, 0, 3, psync_rx_check_callback);
				tdma_slot_len = next_slot_len;
				timeout = tdma_slot_len;
				printf("new slot_len: %hu\r\n", tdma_slot_len);
				//already_tx = true;
				//already_sense = 0;
			}
		}
		/* occurs only one time per full tdma cycle */
		else if ((slot == nodeID) && (!already_tx)){
			nrk_led_toggle(RED_LED);
			// fill buffer with node id and sensor data 
			msg[0] = nodeID;
			*(uint32_t *)(msg + 1) = press;
			*(uint32_t *)(msg + 5) = (uint32_t) psync_get_time();
			flash_tx_pkt(msg, 9);
			already_tx = true;
			already_sense = 0;
		}

		/*if ((slot == NUM_NODES - 1) && (next_slot_len != tdma_slot_len)) {
			tdma_slot_len = next_slot_len;
			while (((psync_get_time() + 55) / tdma_slot_len) % NUM_NODES);
		}*/
	}
}
