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
*  possible
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

#include "../tdma.h"

#define UART_BUF_SIZE	16

/* slot types in time slots array */
#define OFF 	0
#define LISTEN 	1
#define FLOOD 	2 

uint8_t nodeID = 1;
uint8_t time_slots[NUM_NODES];
uint8_t msg[PKT_LEN];
uint8_t val;
bool time_synchronized;

/* general purpose callback for doing time synchronizations during root slot */
void time_sync_callback(uint8_t *buf, uint64_t recv_time)
{
	if (buf[0] == 0){
		//uint32_t prev_local_time = (uint32_t)recv_time;
		uint32_t global_time = *(uint32_t *)(buf + 1);
		flash_set_time(global_time + 673);
		printf("diff: %ld\r\n", (int32_t)(flash_get_current_time() - global_time));
		/* update timestamp to account for tx delays */
		uint32_t current_time = flash_get_current_time();
		*(uint32_t *)(buf + 1) = current_time;
		/* signal to mainloop that at least one time sync has occurred */
		time_synchronized = true;
	}
	else{
		//TODO: handle root node setup messages here
	}
}

/* flash callback to handle phase where leaf nodes flood the network during
 * their TDMA slots to allow the root node to gather topology data */
void startup_phase1_callback(uint8_t *buf, uint64_t recv_time)
{
	uint8_t num_retransmissions = buf[1];
	printf("rx pkt retx #=%d", num_retransmissions);
	if (num_retransmissions >= 6) 
		printf("too many retransmissions %d\r\n", num_retransmissions);
	else{
		buf[1] += 1;
		buf[num_retransmissions + 2] = nodeID;
	}
#ifdef DEBUG
	int8_t i;
	for (i=0; i<8; i++)
		printf("buf[%d] = %d\r\n", i, buf[i]);
#endif
}

/* flash callback to handle phase where root sends out data about when each 
 * node needs to be listening/off depending on observed topology */
void startup_phase2_callback(uint8_t *buf, uint64_t recv_time)
{
	/* buffer organized as {num retransmissions, orign node, re-tx nodes} */
	uint8_t num_retx = buf[1];
	uint8_t slot = buf[0];
	uint8_t i; 
	time_slots[slot] = OFF;
	/* check if this nodeID is in the list of retransmitters */
	for (i=2; i<PKT_LEN; i++){
		if (buf[i] == nodeID)
			time_slots[slot] = LISTEN;
	}
}

/* get current tdma slot number, indication which node's turn it is */
inline uint8_t get_curr_tdma_cycle()
{
	return (flash_get_current_time()/TDMA_SLOT_LEN) % NUM_NODES;
}

/* waits for the end of TDMA slot slot_old */
void wait_remainder_of_tdma_period(uint8_t slot_old)
{
	uint8_t slot_new = get_curr_tdma_cycle();
	while(slot_new == slot_old)
		slot_new = get_curr_tdma_cycle();
}

void main() 
{
	/* set up ports for LEDS etc. */
	nrk_setup_ports();
	/* set to 9K6 since higher baud rates crash my delicate Apple FTDI drivers */
	nrk_setup_uart(UART_BAUDRATE_9K6);
	
	flash_init(CHAN);
	/* initialized timer 3 interrupt and counters in flash */
	flash_timer_setup();
	nrk_int_enable();

	/* register firefly basic sensors */
	val = nrk_register_driver(&dev_manager_ff3_sensors, FIREFLY_3_SENSOR_BASIC);
	if (val == NRK_ERROR) printf("failed to register drivers\r\n");
	
	int8_t fd;
	fd = nrk_open(FIREFLY_3_SENSOR_BASIC, READ);

	printf("waiting for sync message\r\n");

	/* wait for first synchronization message before initialization protocol */
	time_synchronized = false;
	while(!time_synchronized){
		flash_enable(PKT_LEN, NULL, time_sync_callback);
	}

	/* wait for root node timeslot to finish */
	wait_remainder_of_tdma_period(0);
		
	flash_set_retransmit(1); //ensure we are propagating flood 
	
	uint64_t timeout = TDMA_SLOT_LEN - 1000;
	uint8_t cycle = get_curr_tdma_cycle();
	/* perform first TDMA cycle of initialization */
	while (cycle > 0){
		/* start network flood originating from this node during timeslot */
		if (cycle == nodeID){
			msg[0] = nodeID;
			msg[1] = 0;
			flash_tx_pkt(msg, PKT_LEN);
		}
		/* listen for other nodes' floods */
		else
			flash_enable(PKT_LEN, &timeout, startup_phase1_callback);
		printf("init1 cycle %d\r\n", cycle);
		wait_remainder_of_tdma_period(cycle);
		cycle = get_curr_tdma_cycle();
	}
	
	/* perform second TDMA cycle of initialization */
	/* wait for root node timeslot to finish */
	wait_remainder_of_tdma_period(0);

	/* listen during each timeslot.  flooded message during timeslot corresponds
	 * to metadata about which nodes must be on during that node's slot */
	cycle = get_curr_tdma_cycle();
	while (cycle > 0){
		printf("init2 cycle %d\r\n", cycle);
		flash_enable(PKT_LEN, &timeout, startup_phase2_callback);
		wait_remainder_of_tdma_period(cycle);
		cycle = get_curr_tdma_cycle();
	}

	/* set slot 0 as always listen */
	time_slots[0] = LISTEN;
	time_slots[nodeID] = FLOOD;

	int i, cycle_count;
	uint32_t press;
	uint32_t last_sense_time;
	
#ifdef DEBUG 
	for (i=0; i<NUM_NODES; i++)
		printf("time slots[%d] = %d\r\n", i, time_slots[i]);
#endif
	/* do sensing during a convenient slot such that if sensor reading takes
	 * longer than a TDMA slot we won't miss gateway's sync or our own slot */
	uint8_t sensing_slot = (nodeID + 2 >= NUM_NODES) ? 1 : nodeID + 1;
	uint8_t cycle_type;
	cycle = get_curr_tdma_cycle();
	while(1){
		cycle = get_curr_tdma_cycle();
		cycle_type = time_slots[cycle];

		switch(cycle_type){
			case OFF: //must be long enough tdma slot length for sensor read
				nrk_set_status(fd, SENSOR_SELECT, PRESS);
				nrk_read(fd, (uint8_t *)&press, 4);
				last_sense_time = (uint32_t)flash_get_current_time();
				break;
			case LISTEN:
				flash_enable(PKT_LEN, &timeout, time_sync_callback);
				break;
			case FLOOD:
				msg[0] = nodeID;
				*(uint32_t *)(msg + 1) = press;
				*(uint32_t *)(msg + 5) = last_sense_time;
				nrk_spin_wait_us(1000);
				//add some redundancy (tunable)
				for (i=0; i<FLOOD_PROP_REDUNDANCY; i++)
					flash_tx_pkt(msg, PKT_LEN);
				break;
			default:
				printf("incorrect time slot type\r\n");
				break;
		}
	
		/* debugging */ 
		printf("cycle %d, action:%d, press:%lu\r\n", cycle, cycle_type, press);
		/*
		cycle_count ++;
		if (cycle_count > 1000){
			cycle_count = 0;
			printf("press:%lu\r\n", press);
		}*/
		wait_remainder_of_tdma_period(cycle);
	}
}
