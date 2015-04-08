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
#include <pulse_sync_timer.h>
#define MAC_ADDR 0


int main()
{

	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);

	nrk_led_clr(ORANGE_LED);
	nrk_led_clr(BLUE_LED);
	nrk_led_clr(GREEN_LED);
	nrk_led_clr(RED_LED);
	
	if (pulse_sync_timer_setup() != NRK_OK){
		nrk_led_set(RED_LED);//printf("failed to properly setup timer3");
		while(1){
			nrk_led_toggle(RED_LED);
			int i;
			for (i=0; i<1000; i++)
				nrk_spin_wait_us(1000);
		}
	}

	nrk_int_enable();
	int i;
	uint64_t temp1 = 0;
	uint64_t temp2 = 0;
	while(1){
		//nrk_int_disable();
		
		/*asm volatile(
			"CLI \n\t" \
		);*/

		temp1 = pulse_sync_get_current_time();
		nrk_spin_wait_us(1000); //wait 1ms
		temp2 = pulse_sync_get_current_time();
		/*asm volatile(
			"SEI \n\t" \
		);*/
		uint32_t t1_low = (temp1 & 0xFFFF);
		uint32_t t1_high = (temp1 >> 32) & 0xFFFF;
		uint32_t t2_low = (temp2 & 0xFFFF);
		uint32_t t2_high = (temp2 >> 32) & 0xFFFF;

		uint64_t diff = temp2-temp1;
		uint32_t diff_low = (diff & 0xFFFF);
		uint32_t diff_high = (diff >> 32) & 0xFFFF;

		printf("time1 = %lu, %lu, time 2 = %lu, %lu, difference = %lu, %lu\r\n", t1_high, t1_low, t2_high, t2_low, diff_high, diff_low);
	
		//if ((diff_low < 40000) || (diff_low > 41000))
		//	printf("time diff unexpected: %lu\r\n", diff_low);
		//if (diff_high != 0)
		//	printf("terrible terrible terrible! %lu\r\n", diff_high);
		nrk_led_clr(RED_LED);
		nrk_led_clr(ORANGE_LED);
		nrk_led_clr(GREEN_LED);
		if (temp2 == 0) nrk_led_set(RED_LED);
		else if (temp2-temp1 < 10) nrk_led_set(GREEN_LED);
		else nrk_led_set(ORANGE_LED);
		nrk_led_toggle(BLUE_LED);
		for (i=0; i<100; i++)
			nrk_spin_wait_us(1000);

	}
	return 1;
}
