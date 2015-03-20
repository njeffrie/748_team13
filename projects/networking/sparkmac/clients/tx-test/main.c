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
#include <sparkmac.h>
#include <nrk_error.h>
#include <nrk_timer.h>
#include <nrk_eeprom.h>
#include <sparkmac_pkt.h>
#include <app_pkt.h>


// if SET_MAC is 0, then read MAC from EEPROM
// otherwise use the coded value
#define SET_MAC   0x0
#define CHAN    26



uint8_t sbuf[4];

uint8_t rx_buf[SM_MAX_PKT_SIZE];
uint8_t tx_buf[SM_MAX_PKT_SIZE];

uint32_t mac_address;


nrk_task_type MAIN_TASK;
NRK_STK main_task_stack[NRK_APP_STACKSIZE];
void main_task (void);


nrk_task_type SENSOR_TASK;
NRK_STK sensor_task_stack[NRK_APP_STACKSIZE];
void sensor_task (void);

void nrk_create_taskset ();
void nrk_register_drivers();


static nrk_time_t tmp_time;

uint8_t aes_key[] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee, 0xff};


sm_pkt_t tst_pkt;


int main ()
{
  uint8_t ds;
  nrk_setup_ports ();
  nrk_setup_uart (UART_BAUDRATE_115K2);

  nrk_init ();

  nrk_led_clr (0);
  nrk_led_clr (1);
  nrk_led_clr (2);
  nrk_led_clr (3);

  nrk_time_set (0, 0);

  sm_task_config();

  //nrk_register_drivers();
  nrk_create_taskset ();
  nrk_start ();

  return 0;
}


void main_task ()
{
  nrk_time_t t;
  int8_t v;
  uint8_t len, i;
  uint8_t chan;


  nrk_kprintf (PSTR ("Nano-RK Version "));
  printf ("%d\r\n", NRK_VERSION);


  printf ("RX Task PID=%u\r\n", nrk_get_pid ());
  t.secs = 5;
  t.nano_secs = 0;

  chan = CHAN;
  if (SET_MAC == 0x00) {

    v = read_eeprom_mac_address (&mac_address);
    if (v == NRK_OK) {
      v = read_eeprom_channel (&chan);
      v=read_eeprom_aes_key(aes_key);
    }
    else {
      while (1) {
        nrk_kprintf (PSTR
                     ("* ERROR reading MAC address, run eeprom-set utility\r\n"));
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



  sm_init (SM_EDGE_DEVICE, chan, mac_address);

  sm_aes_setkey(aes_key);
  sm_aes_enable();

  // For all tasks using sm_xxx commands this is a good idea...
  while (!sm_started ())
    nrk_wait_until_next_period ();

  while (1) {

      // Send a raw packet, typically not used by
      // application...
      tst_pkt.src_mac=mac_address;
      tst_pkt.dst_mac=0xffff;
      tst_pkt.mac_contention_control=1;
      tst_pkt.seq_num++;
      tst_pkt.ttl=3;
      tst_pkt.payload_len=3;
      tst_pkt.payload[0]=1;
      tst_pkt.payload[1]=2;
      tst_pkt.payload[2]=3;
      v=sm_tx_raw_pkt(&tst_pkt); 
      nrk_kprintf(PSTR("Sent packet\r\n"));
      nrk_led_toggle(0);


/*
      tx_buf[0]=0;
      tx_buf[1]=1;
      tx_buf[2]=2;
      sm_send(tx_buf,3);

*/

      nrk_wait_until_next_period();
  }

}




void nrk_create_taskset ()
{


  MAIN_TASK.task = main_task;
  nrk_task_set_stk (&MAIN_TASK, main_task, NRK_APP_STACKSIZE);
  MAIN_TASK.prio = 2;
  MAIN_TASK.FirstActivation = TRUE;
  MAIN_TASK.Type = BASIC_TASK;
  MAIN_TASK.SchType = PREEMPTIVE;
  MAIN_TASK.period.secs = 1;
  MAIN_TASK.period.nano_secs = 0;
  MAIN_TASK.cpu_reserve.secs = 2;
  MAIN_TASK.cpu_reserve.nano_secs = 0;
  MAIN_TASK.offset.secs = 0;
  MAIN_TASK.offset.nano_secs = 0;
  nrk_activate_task (&MAIN_TASK);


}

