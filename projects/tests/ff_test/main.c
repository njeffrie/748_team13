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

NRK_STK flood_stack[NRK_APP_STACKSIZE];
nrk_task_type floodTask;
void flood_task (void);

void nrk_create_taskset();

int main()
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
	printf("configuring flash task\r\n");
	flash_task_config();
	nrk_create_taskset();
	nrk_start();
	return 1;
}

void flood_task()
{
	flash_enable();
	printf("flash error count after init = %d\r\n", (int)flash_err_count_get());
	printf("currnent packet size = %d\r\n", (int)flash_msg_len_get());
	flash_disable();
	printf("flash disabled\r\n");
	flash_enable();
	printf("flash enabled\r\n");
	flash_disable();
	printf("flash disabled\r\n");
	
	char *buf = "this is a buffer";
	while (1){
		printf("transmitting packet [%s]\r\n", buf);
		flash_tx_pkt((uint8_t *)buf, (uint8_t)strlen(buf));
		printf("success...maybe!\r\n");
		nrk_wait_until_next_period();
	}
}


void
nrk_create_taskset()
{
  nrk_task_set_entry_function( &floodTask, flood_task);
  nrk_task_set_stk( &floodTask, flood_stack, NRK_APP_STACKSIZE);
  floodTask.prio = 1;
  floodTask.FirstActivation = TRUE;
  floodTask.Type = BASIC_TASK;
  floodTask.SchType = PREEMPTIVE;
  floodTask.period.secs = 5;
  floodTask.period.nano_secs = 0;
  floodTask.cpu_reserve.secs = 0;
  floodTask.cpu_reserve.nano_secs = 0;
  floodTask.offset.secs = 0;
  floodTask.offset.nano_secs= 0;
  nrk_activate_task (&floodTask);
}
