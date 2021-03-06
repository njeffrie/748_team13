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
#include <hal.h>
#include <nrk_error.h>
#include <nrk_timer.h>
#include <nrk_stack_check.h>
#include <nrk_stats.h>
#include <sparkmac.h>
#include <nrk_eeprom.h>
#include <slip.h>
#include <sparkmac_pkt.h>
#include <nrk_eeprom.h>

// if SET_MAC is 0xffff, then read MAC from EEPROM
// otherwise use the coded value
#define SET_MAC  		0xffff
//#define SET_MAC  		0x0000
#define DEFAULT_CHANNEL	 	26	

#define SLIPSTREAM_ACK

uint32_t mac_address;

NRK_STK rx_task_stack[NRK_APP_STACKSIZE];
nrk_task_type rx_task_info;
void rx_task(void);

NRK_STK tx_task_stack[NRK_APP_STACKSIZE];
nrk_task_type tx_task_info;
void tx_task(void);

void nrk_create_taskset();

uint8_t aes_key[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee, 0xff}; 


sm_pkt_t rx_pkt;
sm_pkt_t tx_sm_fd;
sm_pkt_t rx_sm_fd;

uint8_t slip_rx_buf[SM_MAX_PKT_SIZE];
uint8_t slip_tx_buf[SM_MAX_PKT_SIZE];

uint16_t last_seq_num,last_mac;

int
main ()
{
  nrk_setup_ports();
  nrk_setup_uart(UART_BAUDRATE_115K2);

  nrk_init();

  nrk_led_clr(ORANGE_LED);
  nrk_led_clr(BLUE_LED);
  nrk_led_clr(GREEN_LED);
  nrk_led_clr(RED_LED);
 
  nrk_time_set(0,0);
  nrk_create_taskset ();
  nrk_start();
  
  return 0;
}

void tx_task()
{
int8_t v,i;
uint8_t len,cnt;
uint8_t ack_buf[2];
  
printf( "Gateway Tx Task PID=%u\r\n",nrk_get_pid());


#ifdef SLIPSTREAM_ACK
	// send at startup to cope with empty network and rebooting
	ack_buf[0]='N';
	slip_tx(ack_buf, 1);
#endif
cnt=0;

  while(1) {
	// This is simply a place holder in case you want to add Host -> Client Communication
	v = slip_rx ( slip_rx_buf, SM_MAX_PKT_SIZE);
	if (v > 0) {
		   
		   ack_buf[0]='A';
		  // nrk_kprintf (PSTR ("Sending data: "));
		  // for (i = 0; i < v; i++)
		  // 	printf ("%d ", slip_rx_buf[i]);
		  //  printf ("\r\n");

		  sm_buf_to_pkt(slip_rx_buf,&tx_sm_fd); 
	 	   	//v=sm_send(&tx_sm_fd, &slip_rx_buf, v, SM_BLOCKING );	
	 	   	v=sm_tx_raw_pkt(&tx_sm_fd);	
	} else ack_buf[0]='N';
	
#ifdef SLIPSTREAM_ACK
	slip_tx(ack_buf, 1);
#endif
nrk_led_toggle(BLUE_LED);

	}
}

void rx_task()
{
nrk_time_t t;
uint16_t cnt;
int8_t v;
uint8_t len,i,chan;


cnt=0;
nrk_kprintf( PSTR("Nano-RK Version ") );
printf( "%d\r\n",NRK_VERSION );

  
printf( "Gateway Task PID=%u\r\n",nrk_get_pid());
t.secs=30;
t.nano_secs=0;

// setup a software watch dog timer
nrk_sw_wdt_init(0, &t, NULL);
nrk_sw_wdt_start(0);

/*
rx_sm_fd.payload_len=0;
rx_sm_fd.src_mac=0x0000;
rx_sm_fd.dst_mac=0xffff;
rx_sm_fd.ttl=3;

len=sm_pkt_to_buf(&rx_sm_fd, &slip_tx_buf );

slip_init (stdin, stdout, 0, 0);
  while(1) {
	nrk_kprintf( PSTR("Sending fake pkt...\r\n" ) );
	slip_tx ( slip_tx_buf, len );
	nrk_sw_wdt_update(0); 
	nrk_wait_until_next_period();
  }
*/



//for(i=0; i<32; i++ )
//nrk_eeprom_write_byte(i,0xff);


  chan = DEFAULT_CHANNEL;
  if (SET_MAC == 0xffff) {

    v = read_eeprom_mac_address (&mac_address);
    if (v == NRK_OK) {
      v = read_eeprom_channel (&chan);
      v=read_eeprom_aes_key(aes_key);
    }
    else {
      while (1) {
        nrk_kprintf (PSTR
                     ("* ERROR reading MAC address, run eeprom-set utility\r\n"));
  	nrk_led_toggle(RED_LED);
        nrk_wait_until_next_period ();
      }
    }
  }
  else
    mac_address = SET_MAC;

  printf ("MAC ADDR: %x\r\n", mac_address & 0xffff);
  printf ("chan = %d\r\n", chan);
  len=0;
  for(i=0; i<16; i++ ) { len+=aes_key[i]; }
  printf ("AES checksum = %d\r\n", len);


sm_init(SM_GATEWAY, chan, mac_address);


sm_aes_setkey(aes_key);
sm_aes_enable();

slip_init (stdin, stdout, 0, 0);
last_seq_num=0;
last_mac=0;

while(!sm_started()) nrk_wait_until_next_period();
nrk_led_set(GREEN_LED);
  while(1) {
	v=sm_rx_raw_pkt(&rx_sm_fd);	
	nrk_led_set(ORANGE_LED);
	if(v!=NRK_ERROR)
	{
	printf( "got pkt!\r\n" );
		len=sm_pkt_to_buf(&rx_sm_fd,slip_tx_buf);
		if((rx_sm_fd.seq_num!=last_seq_num) || (rx_sm_fd.src_mac!=last_mac) ) slip_tx ( slip_tx_buf, len );
		last_seq_num=rx_sm_fd.seq_num;
		last_mac=rx_sm_fd.src_mac;
	}
	nrk_led_clr(ORANGE_LED);
	nrk_sw_wdt_update(0); 
  	nrk_wait_until_next_period();
	}
}

void
nrk_create_taskset()
{
  nrk_task_set_entry_function( &rx_task_info, rx_task);
  nrk_task_set_stk( &rx_task_info, rx_task_stack, NRK_APP_STACKSIZE);
  rx_task_info.prio = 50;
  rx_task_info.FirstActivation = TRUE;
  rx_task_info.Type = BASIC_TASK;
  rx_task_info.SchType = PREEMPTIVE;
  rx_task_info.period.secs = 0;
  rx_task_info.period.nano_secs = 4*NANOS_PER_MS;
  rx_task_info.cpu_reserve.secs = 0;
  rx_task_info.cpu_reserve.nano_secs = 500*NANOS_PER_MS;
  rx_task_info.offset.secs = 0;
  rx_task_info.offset.nano_secs= 0;
  nrk_activate_task (&rx_task_info);

  nrk_task_set_entry_function( &tx_task_info, tx_task);
  nrk_task_set_stk( &tx_task_info, tx_task_stack, NRK_APP_STACKSIZE);
  tx_task_info.prio = 1;
  tx_task_info.FirstActivation = TRUE;
  tx_task_info.Type = BASIC_TASK;
  tx_task_info.SchType = PREEMPTIVE;
  tx_task_info.period.secs = 1;
  tx_task_info.period.nano_secs = 10*NANOS_PER_MS;
  tx_task_info.cpu_reserve.secs = 1;
  tx_task_info.cpu_reserve.nano_secs = 500*NANOS_PER_MS;
  tx_task_info.offset.secs = 0;
  tx_task_info.offset.nano_secs= 0;
  nrk_activate_task (&tx_task_info);

  sm_task_config();

}


