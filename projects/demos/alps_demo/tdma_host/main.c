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

#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <hal.h>
#include <basic_rf.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>

#define NUM_SLOTS 3
#define SLOT_TIME 300 //ms
#define CHANNEL 26

void my_callback(uint16_t global_slot );

RF_TX_INFO rfTxInfo;
RF_RX_INFO rfRxInfo;

uint8_t tx_buf[RF_MAX_PAYLOAD_SIZE];
uint8_t rx_buf[RF_MAX_PAYLOAD_SIZE];
//------------------------------------------------------------------------------
//      void main (void)
//
//      DESCRIPTION:
//              Startup routine and main loop
//------------------------------------------------------------------------------
int main (void)
{
    uint8_t cnt,length;
uint16_t i;
    uint8_t cnt2;

    nrk_setup_ports(); 
    nrk_setup_uart (UART_BAUDRATE_115K2);
 
    printf( "Basic TX...\r\n" ); 
    nrk_led_set(0); 
    nrk_led_set(1); 
    nrk_led_clr(2); 
    nrk_led_clr(3); 
/*
	    while(1) {
		   
				for(i=0; i<40; i++ )
					halWait(10000);
		    nrk_led_toggle(1);

	    }

*/
  
    rfRxInfo.pPayload = rx_buf;
    rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
		nrk_int_enable();
    rf_init (&rfRxInfo, CHANNEL, 0x2420, 0x1214);
    cnt=0;

   DDRG=0xff;
   DDRF=0xff;
   PORTG=0x00; 
   PORTF=0xff;
   DDRD |= 0x01;
    //ANT_DIV |= (1<<ANT_DIV_EN) | (1<< ANT_EXT_SW_EN);; 
cnt2=0;
    while(1){


//				nrk_led_set(GREEN_LED);
    		rfTxInfo.pPayload=tx_buf;
    		sprintf( tx_buf, "a");
		tx_buf[0]=100;
		tx_buf[1]=100;
		tx_buf[2]=100;
		tx_buf[3]=100;
		tx_buf[4]=100;
		tx_buf[5]=100;
		tx_buf[6]=100;
		tx_buf[7]=100;
		tx_buf[8]=100;
		tx_buf[9]=100;
    		//rfTxInfo.length= 10;
		
    		rfTxInfo.length= strlen(tx_buf)+1;
		rfTxInfo.destAddr = 0x1215;
				rfTxInfo.cca = 0;
				rfTxInfo.ackRequest = 0;
				if(rf_tx_packet(&rfTxInfo) != 1)
					printf("--- RF_TX ERROR ---\r\n");
				nrk_gpio_toggle(NRK_LED_0);
//10000 -> 12.4ms
				// 10 -> 133.66ms
				// 20 -> 226.18ms
				// 30 -> 338.70ms

				// wait 850 ms
				//for(i=0; i<73; i++)
				//	nrk_spin_wait_us(10000);
				// Wait for 3s
				for(i=0; i<34; i++) //256 = 3s, 34 = 0.4s
					nrk_spin_wait_us(10000);
				nrk_spin_wait_us(14860); //15215 = 3s, 14860 = 0.4s
				PORTD ^= 0x01;
		}
}


void my_callback(uint16_t global_slot )
{
		static uint16_t cnt;

		printf( "callback %d %d\n",global_slot,cnt );
		cnt++;
}



/**
 *  This is a callback if you require immediate response to a packet
 */
RF_RX_INFO *rf_rx_callback (RF_RX_INFO * pRRI)
{
    // Any code here gets called the instant a packet is received from the interrupt   
    return pRRI;
}
