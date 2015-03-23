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

RF_TX_INFO rfTxInfo;
RF_RX_INFO rfRxInfo;
uint8_t tx_buf[RF_MAX_PAYLOAD_SIZE];
uint8_t rx_buf[RF_MAX_PAYLOAD_SIZE];


int main (void)
{
    uint8_t i;

    nrk_setup_ports(); 
    nrk_setup_uart (UART_BAUDRATE_115K2);
 
    rfRxInfo.pPayload = rx_buf;
    rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
	nrk_int_enable();
	rf_init (&rfRxInfo, 13, 0xffff, 0);
	while(1){
		DPDS1 |= 0x3;
		rfTxInfo.pPayload=tx_buf;
		sprintf( tx_buf, "%s", "this is a test message"); 
		rfTxInfo.length= strlen(tx_buf) + 1;
		rfTxInfo.destAddr = 0x0;
		rfTxInfo.cca = 0;
		rfTxInfo.ackRequest = 0;

		printf( "Sending\r\n" );
		if(rf_tx_packet(&rfTxInfo) != 1)
			printf("--- RF_TX ERROR ---\r\n");

		for(i=0; i<10; i++ )
			halWait(10000);
		nrk_led_toggle(RED_LED);
	}

}

/**
 *  This is a callback if you require immediate response to a packet
 */
RF_RX_INFO *rf_rx_callback (RF_RX_INFO * pRRI)
{
	// Any code here gets called the instant a packet is received from the interrupt   
	return pRRI;
}
