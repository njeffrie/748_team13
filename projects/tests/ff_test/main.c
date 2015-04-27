/******************************************************************************
*  Nano-RK, a real-time operating system for sensor networks.
*  Copyright (C) 2007, Real-Time and Multimedia Lab, Carnegie Mellon University
*  All rights reserved.
*
*  This is the Open Source Version of Nano-RK included as part of a Dual
*  Licensing Model. If you are unsure which license to use please refer to:
*  http://www.nanork.org/nano-RK/wiki/Licensing
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, version 2.0 of the License.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*******************************************************************************/

#include <nrk.h>
#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <avr/sleep.h>
#include <hal.h>
#include <nrk_error.h>
#include <nrk_time.h>
#include <avr/eeprom.h>
#include <nrk_eeprom.h>
#include <string.h>
#include <flash.h>

#define MAC_ADDR 2
void flash_test_callback();

volatile uint32_t rt_count;

#define PACKET_LEN 48

uint8_t tx_buf[PACKET_LEN];

void main()
{
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);

	nrk_init();
	nrk_led_clr(ORANGE_LED);
	nrk_led_clr(BLUE_LED);
	nrk_led_clr(GREEN_LED);
	nrk_led_clr(RED_LED);

	nrk_time_set(0,0);

	printf("initializing flash\r\n");
	flash_init(13);
	flash_timer_setup();
	printf("configuring flash task\r\n");
	
	tx_buf[0] = MAC_ADDR;
	nrk_spin_wait_us(10000);
	printf("global time = %lu\r\n", (uint32_t)flash_get_current_time());
	*(uint32_t *)(tx_buf + 1) = flash_get_current_time();
	int max_msg_len = PACKET_LEN;//strlen(buf) + 1;
	printf("transmitting packet [%d,%lu]\r\n", tx_buf[0], *(uint32_t *)(tx_buf + 1));
	
	printf("waiting to propagate flood\r\n");
	flash_tx_pkt(tx_buf, PACKET_LEN);//(uint8_t)strlen(buf));
	
	rt_count = 0;

	flash_enable((uint8_t)max_msg_len, NULL, flash_test_callback);
	
	while (1){
		flash_enable((uint8_t)max_msg_len, NULL, flash_test_callback);
	}
}

uint32_t old_time = 0;
uint32_t new_time = 0;

void flash_test_callback(uint8_t *buf, uint64_t rx_time)
{
	//printf("calback received buffer %s with rx time %d seconds\r\n", (char *)buf, (int)rx_time);
	rt_count ++;
	if (!(rt_count % 100)){
		new_time = (uint32_t)flash_get_current_time();
		printf("cycle %lu, diff %lu \r\n", rt_count, new_time - old_time);
		old_time = new_time;
	}
}
