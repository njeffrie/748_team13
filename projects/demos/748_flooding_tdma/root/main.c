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
#include "../tdma.h"

#define UART_BUF_SIZE	16

uint8_t nodeID = 0;

/* structure to hold shortests paths collected in phase 1 of startup */
uint8_t shortest_paths[NUM_NODES][PKT_LEN];
uint8_t tx_buf[PKT_LEN];

int32_t correct = 0;
int32_t failed = 0;

inline uint8_t get_curr_tdma_cycle();

void node_data_callback(uint8_t *data, uint64_t time)
{
	uint8_t sender_node = data[0];
	uint32_t press = *(uint32_t *)(data + 1);
	uint32_t sense_time = *(uint32_t *)(data + 5);
	uint8_t slot = get_curr_tdma_cycle();
	printf("got sender %d press %lu sense time %lu\r\n", sender_node, press, sense_time);
	if (sender_node != slot){
		failed ++;
		printf("slot error[slot:%d,sender:%d]\r\n", slot, sender_node);
	}
	else{
		correct ++;
		printf("slot correct\r\n");
	}
	//if (!((failed + correct)%10))
	//printf("percentage correct:%ld, [c:%ld, f:%ld] press:%lu\r\n", 
	//			(correct * 100)/(correct + failed), correct, failed, press);

	nrk_led_toggle(GREEN_LED);
}

void startup_phase1_callback(uint8_t *buf, uint64_t recv_time)
{
	uint8_t orig_node = buf[0];
	uint8_t slot = get_curr_tdma_cycle();
	if (orig_node != slot){
		// will cause current node to fail to receive timeslot
		// maybe we should signal error and restart?
		printf("error: flood message from %d overflowed to tdma slot %d\r\n", 
				orig_node, slot);
		return;
	}
	memcpy(shortest_paths[slot], buf, PKT_LEN);

#ifdef DEBUG
	uint8_t i;
	printf("got pkt [");
	for (i=0; i<PKT_LEN; i++){
		printf("%d ", buf[i]);
	}
	printf("]\r\n");
#endif
	memset(buf, 0, PKT_LEN);
}

/* get current tdma slot number, indication which node's turn it is */
inline uint8_t get_curr_tdma_cycle()
{
	return (flash_get_current_time()/TDMA_SLOT_LEN) % NUM_NODES;
}

/* waits for the end of TDMA slot slot_old */
void wait_remainder_of_tdma_period(uint8_t slot_old)
{
	//printf("cycle:%d, old slot:%d\r\n", get_curr_tdma_cycle(), slot_old);
	
	if (get_curr_tdma_cycle() != slot_old)
		return;
	uint32_t slot_len = TDMA_SLOT_LEN;
	uint32_t remainder = slot_len - ((uint32_t)flash_get_current_time() % slot_len);
	//printf("remainder: %lums\r\n", remainder/1000);
	int i;
	remainder -= 100;
	for (i=0; i<remainder/50000; i++)
		nrk_spin_wait_us(50000);
	nrk_spin_wait_us(remainder%50000);
	
	/*
	uint8_t slot_new = get_curr_tdma_cycle();
	while(slot_new == slot_old)
		slot_new = get_curr_tdma_cycle();
	*/
}

void main ()
{
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_9K6);
	
	flash_init(CHAN);
	flash_timer_setup();
	
	/* root node should act as a sink - should never propagate flood */
	flash_set_retransmit(0);
	flash_rf_power_set(RF_POWER);

	nrk_int_enable();

	/* send synchronization buffer to start initialization protocol */
	tx_buf[0] = 255; //indicate this is first time sync message
	uint32_t timestamp = (uint32_t)flash_get_current_time();
	*(uint32_t *)(tx_buf + 1) = timestamp;
	flash_tx_pkt(tx_buf, PKT_LEN);

	/* wait for slot 0 to be done */
	wait_remainder_of_tdma_period(0);
	
	/* listen for network topology messages flooding the network */
	uint64_t timeout = TDMA_SLOT_LEN - 1000;
	uint8_t cycle = get_curr_tdma_cycle();
	//printf("phase1 cycle 1\r\n");
	while (cycle > 0){
		flash_enable(PKT_LEN, &timeout, startup_phase1_callback);
		//if (cycle < 4) printf("phase1 cycle %d\r\n", cycle + 1);
		//wait_remainder_of_tdma_period(cycle);
		cycle = get_curr_tdma_cycle();
	}

	/* again wait for slot 0 to finish */
	wait_remainder_of_tdma_period(0);
	cycle = get_curr_tdma_cycle();
	while (cycle > 0){
		memcpy(tx_buf, shortest_paths[cycle], PKT_LEN);
#ifdef DEBUG
		printf("sending topology msg %d [%d, %d, %d, %d]\r\n", cycle, shortest_paths[cycle][0], 
				shortest_paths[cycle][1], shortest_paths[cycle][2], shortest_paths[cycle][3]);
		printf("phase2 cycle %d\r\n", cycle);
#endif
		nrk_spin_wait_us(IN_SLOT_TX_DELAY);
		flash_tx_pkt(tx_buf, PKT_LEN);
		wait_remainder_of_tdma_period(cycle);
		cycle = get_curr_tdma_cycle();
	}

#ifdef DEBUG
	int i, j;
	for (i=0; i<NUM_NODES; i++){
		printf("paths[%d] = [", i);
		for (j=0; j<PKT_LEN; j++)
			printf("%d, ", shortest_paths[i][j]);
		printf("]\r\n");
	}
#endif
		
	uint32_t cycles_since_sync = 0;
	while(1){
		cycle = get_curr_tdma_cycle();
		/* root node's timeslot - send sync message if INTER_SYNC_CYCLES
		 * have elapsed */
		//if (cycle == 1) printf("waiting for msg\r\n");
		if (cycle == 0){
			if (cycles_since_sync >= INTER_SYNC_CYCLES){
				timestamp = (uint32_t)flash_get_current_time();
				tx_buf[0] = 0;
				*(uint32_t *)(tx_buf + 1) = timestamp;
				flash_tx_pkt(tx_buf, PKT_LEN);
				cycles_since_sync = 0;
			}
			else {
				memset(tx_buf, 0, PKT_LEN);
				tx_buf[0] = 1; //indicate this is not timestamp message
				flash_tx_pkt(tx_buf, PKT_LEN);
				cycles_since_sync ++;
			}
		}
		else {
			//printf("waiting for data\r\n");
			nrk_int_enable();
			flash_enable(PKT_LEN, &timeout, node_data_callback);
		}
		//printf("time:%lu\r\n", (uint32_t)flash_get_current_time());

		wait_remainder_of_tdma_period(cycle);
		//printf("cycle old:%d, new:%d\r\n", cycle, get_curr_tdma_cycle());
		//printf("%lu\r\n", flash_get_current_time() % TDMA_SLOT_LEN);
	}
}
