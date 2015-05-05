/******************************************************************************
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
* Test of naive flash based time sync to be used with time sync evaluation
* GUI
*
* Author: Nat Jeffries
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

#define UART_BUF_SIZE	16
#define PKT_SIZE 8
#define CYCLE_PERIOD 100000
#define CYCLES_PER_SYNC 100

uint8_t nodeID = 1;
uint8_t msg[PKT_SIZE];
uint64_t timeout;

uint32_t current_time;

int sync_cycle_count;

void time_report_callback(uint8_t *buf, uint64_t recv_time)
{
	printf("l %lu g %lu\r\n", (uint32_t)recv_time, *(uint32_t *)(buf));
}	

void main() 
{
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_9K6);
	
	/* setup flash to not retransmit */
	flash_init(14);
	flash_timer_setup();
	flash_set_retransmit(0);

	timeout = CYCLE_PERIOD;
	sync_cycle_count = 0;

	/* start by sending time sync */
	msg[0] = 1;
	*(uint32_t *)(msg + 1) = flash_get_current_time();
	flash_tx_pkt(msg, PKT_SIZE);
	
	while(1){
		sync_cycle_count ++;
		printf("cycle\r\n");
		if (sync_cycle_count > CYCLES_PER_SYNC){
			msg[0] = 1;
			*(uint32_t *)(msg + 1) = flash_get_current_time();
			flash_tx_pkt(msg, PKT_SIZE);
		}
		/* listen to node for time info */
		flash_enable(PKT_SIZE, &timeout, time_report_callback);
	}
}
