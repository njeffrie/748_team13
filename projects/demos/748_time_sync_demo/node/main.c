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
#include <nrk_timer.h>
#include <nrk_driver_list.h>
#include <nrk_driver.h>
#include <ff_basic_sensor.h>

//#include <pulse_sync.h>
#define UART_BUF_SIZE	16
#define PKT_SIZE 8
#define CYCLE_PERIOD 100000

uint8_t nodeID = 1;
uint8_t msg[PKT_SIZE];

uint64_t timeout;

uint32_t current_time;

void time_sync_callback(uint8_t *buf, uint64_t recv_time)
{
	/* only set time when root node indicates it is a time sync cycle */
	if (buf[0] == 1){
		uint32_t prev_local_time = (uint32_t)recv_time;//(uint32_t)flash_get_current_time();
		uint32_t global_time = *(uint32_t *)(buf + 1);
		flash_set_time(global_time + 673); 
	}
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

	while(1){
		printf("receiving data..\r\n");
		flash_enable(PKT_SIZE, &timeout, time_sync_callback);
		current_time = flash_get_current_time();
		*(uint32_t *)(msg) = current_time;
		flash_tx_pkt(msg, PKT_SIZE);
	}
}
