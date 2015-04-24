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
	uint32_t temp1 = 0;
	uint32_t temp2 = 0;
	uint32_t temp3 = 0;
	while(1){
		
		temp1 = (uint32_t)pulse_sync_get_current_time();
		//for (i=0; i<1000; i++){
			nrk_spin_wait_us(10000);
		//}
		temp2 = (uint32_t)pulse_sync_get_current_time();
		temp3 = (uint32_t)pulse_sync_get_current_time();
		

		printf("1ms time diff: %lu\r\n", temp2-temp1);
		printf("back-to-back time diff: %lu\r\n", temp3-temp2);

		nrk_led_clr(RED_LED);
		nrk_led_clr(ORANGE_LED);
		nrk_led_clr(GREEN_LED);
		if (temp2 == 0) nrk_led_set(RED_LED);
		else if (temp2-temp1 < 10) nrk_led_set(GREEN_LED);
		else nrk_led_set(ORANGE_LED);
		nrk_led_toggle(BLUE_LED);
		for (i=0; i<1000; i++)
			nrk_spin_wait_us(1000);

	}
	return 1;
}
