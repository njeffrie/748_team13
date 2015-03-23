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
*  Contributing Authors (specific to this file):
*  Zane Starr
*******************************************************************************/


#include <nrk.h>
#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <avr/sleep.h>
#include <hal.h>
#include <nrk_error.h>
#include <nrk_timer.h>
#include <nrk_driver_list.h>
#include <nrk_driver.h>
#include <hal_firefly3.h>
#include <avr/interrupt.h>
#include <nrk_pin_define.h>
#include <nrk_events.h>
#include <basic_rf.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>



#define BUFFER_SIZE 80
#define BASE_MS 200
#define MAC_ADDR        0x0003

RF_RX_INFO rfRxInfo;

nrk_task_type RX_TASK;
NRK_STK rx_task_stack[NRK_APP_STACKSIZE];
void rx_task (void);

void nrk_create_taskset ();

uint8_t rx_buf[RF_MAX_PAYLOAD_SIZE];

uint8_t packet_len=0;
uint8_t packet_ready=0;

char buf[BUFFER_SIZE];
//char receive_buf[BUFFER_SIZE];

void nrk_create_taskset();

int
main ()
{
  nrk_setup_ports();
  nrk_setup_uart(UART_BAUDRATE_115K2);

  printf( PSTR("starting...\r\n") );
  
  nrk_init();
  nrk_time_set(0,0);

  nrk_create_taskset ();
  nrk_start();
  
  return 0;
}

RF_RX_INFO *rf_rx_callback(RF_RX_INFO *pRRI)
{
	//printf("received packet!\r\n");
	return pRRI;
}

void callback1()
{
	printf("started packet rx\r\n");
}

void callback2()
{
	printf("finished packet rx\r\n");
	rf_rx_off();
	rf_rx_on();
}

void rx_task ()
{
  uint8_t cnt,i,length,n;
 
  rfRxInfo.pPayload = rx_buf;
  rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
  rfRxInfo.ackRequest= 0;
  nrk_int_enable();
  rf_init (&rfRxInfo, 13, 0xffff, 0x0);
  printf( "Waiting for packet...\r\n" );

  rx_start_callback(callback1);
  rx_end_callback(callback2);
  while(1){
	  rf_rx_on();
	  nrk_wait_until_next_period();
	
	/*
	rf_polling_rx_on();
	while ((n = rf_rx_check_sfd()) == 0)
	  continue; 
	if (n != 0) {
	  n = 0;
	  // Packet on its way
	  cnt=0;
	  while ((n = rf_polling_rx_packet ()) == 0) {
		if (cnt > 50) {
		  //printf( "PKT Timeout\r\n" );
		  break;		// huge timeout as failsafe
		}
		halWait(4000);
		cnt++;
	  }
	}

	//rf_rx_off();
	if (n == 1) {
	  nrk_led_toggle(RED_LED);
	  nrk_led_toggle(GREEN_LED);
	  // CRC and checksum passed
	  //printf("packet received\r\n");
	  for(i=0; i<rfRxInfo.length; i++ )
		printf( "%c", rfRxInfo.pPayload[i]);
	  printf(",%d\r\n",rfRxInfo.rssi);
	} 
	else if(n != 0){ 
	  //printf( "CRC failed!\r\n" ); nrk_led_set(RED_LED); 
	}*/
  }
}

void
nrk_create_taskset()
{
  RX_TASK.task = rx_task;
  nrk_task_set_stk( &RX_TASK, rx_task_stack, NRK_APP_STACKSIZE);
  RX_TASK.prio = 2;
  RX_TASK.FirstActivation = TRUE;
  RX_TASK.Type = BASIC_TASK;
  RX_TASK.SchType = PREEMPTIVE;
  RX_TASK.period.secs = 1;
  RX_TASK.period.nano_secs = 0;
  RX_TASK.cpu_reserve.secs = 0;
  RX_TASK.cpu_reserve.nano_secs = 0 * NANOS_PER_MS;
  RX_TASK.offset.secs = 0;
  RX_TASK.offset.nano_secs = 0;
  nrk_activate_task (&RX_TASK);
}
