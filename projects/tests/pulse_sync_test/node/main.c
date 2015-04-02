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
#include <pulse_sync.h>

#define UART_BUF_SIZE	16

nrk_task_type TEST_TASK;
NRK_STK test_task_stack[NRK_APP_STACKSIZE];
void test_task(void);

void nrk_create_taskset();

void nrk_register_drivers();

int main() {
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);
	
	nrk_init();
	
	nrk_led_clr(0);
	nrk_led_clr(1);
	nrk_led_clr(2);
	nrk_led_clr(3);
	
	nrk_time_set(0, 0);

	psync_init(2, 0, 13);

	nrk_create_taskset();
	nrk_start();
	
	return 0;
}

void test_task() {
	printf("waiting to receive PulseSync flood\r\n");

	// loop receiving to measure determinacy in sync floods
	while (1) {
		psync_flood_wait(NULL);
	}
}

void nrk_create_taskset() {
	TEST_TASK.task = test_task;
	nrk_task_set_stk(&TEST_TASK, test_task_stack, NRK_APP_STACKSIZE);
	TEST_TASK.prio = 2;
	TEST_TASK.FirstActivation = TRUE;
	TEST_TASK.Type = BASIC_TASK;
	TEST_TASK.SchType = PREEMPTIVE;
	TEST_TASK.period.secs = 10;
	TEST_TASK.period.nano_secs = 0;
	TEST_TASK.cpu_reserve.secs = 0;
	TEST_TASK.cpu_reserve.nano_secs = 0;
	TEST_TASK.offset.secs = 0;
	TEST_TASK.offset.nano_secs = 0;

	nrk_activate_task(&TEST_TASK);
}
