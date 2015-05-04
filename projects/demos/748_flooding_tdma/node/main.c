/*
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
*
* Flooding TDMA is a TDMA protocol developed for 18-748 Wireless Sensor Networks
* as a bonus project.  Flooding TDMA takes the concepts from a one hop star
* network and extends them to a multihop enviroment.  Optimal conditions
* for operation of this type of network is a sensor network where sensors
* periodically update the root node with their sensor data.
*
* Primary benefits of Flooding TDMA: 
* -Low latency send from leaf nodes to root node
* -Use of Flash Flooding and PulseSync time synchronization results in very
*  low latency flooding and high accuracy timestamping across a multihop 
*  network. This means that TDMA slots can be kept reliably short
* -Simplicity of design
* 
* Primary downsides of Flooding TDMA:
* -Concurrent propatagion of messgaes in independent parts of network not 
*  possibl
* -Current implementation requires nodes to be assigned a fixed ID at
*  programming time
*
*  Authors:
*  Nat Jeffries
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
//#include <pulse_sync.h>
#define UART_BUF_SIZE	16

nrk_task_type TEST_TASK;
NRK_STK test_task_stack[NRK_APP_STACKSIZE];
void test_task(void);

#define NUM_NODES 10

uint8_t nodeID = 1;

uint8_t time_slots[NUM_NODES];

void nrk_create_taskset();

void nrk_register_drivers();

uint8_t val;

void time_sync_callback(uint8_t *buf, uint64_t recv_time){
	uint32_t prev_local_time = (uint32_t)recv_time;
	uint32_t global_time = *(uint32_t *)(buf + 1);
	flash_set_time(global_time + 673); 
}	

uint8_t msg[PKT_LEN];

void main() {
	/* set up ports for LEDS etc. */
	nrk_setup_ports();
	/* set to 9K6 since higher baud rates crash my delicate Apple FTDI drivers */
	nrk_setup_uart(UART_BAUDRATE_9K6);
	
	nrk_led_clr(0);
	nrk_led_clr(1);
	nrk_led_clr(2);
	nrk_led_clr(3);
	
	//nrk_time_set(0, 0); unnecessary since we use flash_get_current_time

	flash_init(CHAN);
	/* initialized timer 3 interrupt and counters in flash */
	flash_timer_setup();

	/* register firefly basic sensors */
	val = nrk_register_driver(&dev_manager_ff3_sensors, FIREFLY_3_SENSOR_BASIC);
	if (val == NRK_ERROR) printf("failed to register drivers\r\n");
	
	int i,cycles_since_sync;
	int8_t fd, ret;
	uint32_t press;
	volatile bool already_tx = false;

	fd = nrk_open(FIREFLY_3_SENSOR_BASIC, READ);

	printf("waiting for sync message\r\n");

	/* wait for first synchronization message before initialization protocol */
	flash_enable(5, NULL, time_sync_callback);

	/* perform first TDMA cycle of initialization */
	uint8_t cycle = (flash_get_current_time() / TDMA_SLOT_LEN) % NUM_NODES;
	while (cycle != nodeID){
		cycle = (flash_get_current_time() / TDMA_SLOT_LEN) % NUM_NODES;
	}
	msg[0] =  
	flash_tx_pkt()
		
	/* perform second TDMA cycle of initialization */


	//flash_set_retransmit(0); //only enable this line for one-hop TDMA
	cycles_since_sync = 0;
	bool already_sync = false;
	int cycle_count;
	
	/* do sensing during a convenient slot such that if sensor reading takes
	 * longer than a TDMA slot we won't miss gateway's sync or our own slot */
	uint8_t sensing_slot = (nodeID == NUM_NODES) ? 1 : nodeID + 1;
	while(1){
		/* debugging */ 
		cycle_count ++;
		if (cycle_count > 1000){
			cycle_count = 0;
			printf("press:%lu\r\n", press);
		}
		uint32_t cycle_start_time = flash_get_current_time();
		int slot = ((cycle_start_time/TDMA_SLOT_LEN) % NUM_NODES);
		/* this slot can overflow into next one - hence selecting it carefully above */
		if (slot == sensing_slot){
		 	already_tx = false;
		 	ret = nrk_set_status(fd,SENSOR_SELECT,PRESS);
		 	ret = nrk_read(fd,(uint8_t *)&press,4);
			nrk_spin_wait_us(flash_get_current_time()%TDMA_SLOT_LEN);
		}
		/* root node's synchronization slot */
		if ((slot == 0) && (!already_sync)){ //root's slot
			cycles_since_sync ++;
			if (cycles_since_sync >= INTER_SYNC_CYCLES){
				flash_enable(5, NULL, time_sync_callback);
				already_sync = true;
			}
		}
		/* occurs only one time per full tdma cycle */
		else if ((slot == nodeID) && (!already_tx)){
			already_sync = false;
			
			nrk_led_toggle(RED_LED);
			/* fill buffer with node id and sensor data */
			msg[0] = nodeID;
			*(uint32_t *)(msg + 1) = press;
			//add some redundancy (tunable)
			for (i=0; i<1; i++){
				flash_tx_pkt(msg, 10);
			}
			already_tx = true;
		}
	}
}
