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
*  Nat Jeffries
*******************************************************************************/

#include <include.h>
#include <ulib.h>
#include <stdlib.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <nrk.h>
#include <nrk_events.h>
#include <nrk_timer.h>
#include <nrk_error.h>
#include <nrk_reserve.h>
#include <nrk_cfg.h>
#include <flash.h>

/**
 * The Flash Flooding protocol exploits the capture effect observed in radio
 * reception, which allows some packets to be received correctly despite
 * concurrent transmissions on the same channel. This Flash Flood
 * implementation (Flash I) immediately re-transmits when a packet is received by the
 * CC2420 radio. 
 */
uint32_t flash_err_count;

int8_t flash_ReTx_ready, is_enabled, flash_chan;
RF_RX_INFO flash_rfRxInfo;
RF_TX_INFO flash_rfTxInfo;
nrk_signal_t flash_ReTx_signal;


/**
 *  This is a callback if you require immediate response to a packet
 */
RF_RX_INFO *rf_rx_callback (RF_RX_INFO * pRRI)
{
    flash_ReTx_ready = 1;
	nrk_event_signal(flash_ReTx_signal);
    return pRRI;
}

nrk_sig_t flash_ReTx_signal()
{
	int8_t n;
	nrk_signal_register(flashReTx_signal);
	// rf rx dest should already be flash_rfRxInfo
	// packet should already be ready
	while((n = rf_rx_check_fifop()) == 0);
	n = 0;
	while((n = rf_polling_rx_packet()) == 0);
	rf_rx_off();
	if (n == 1)
	{
		//CRC and checksum passed
		flashReTx_ready = 0;
	}
	else 
	{
		//CRC failed
		flash_err_count += 1;		
	}
	// start transmission process to re-transmit buffer
	printf("rx packet size: %d | tx packet size: %d\r\n", sizeof(RF_RX_INFO),
			sizeof(RF_TX_INFO));
	//copy rx buff into tx buf to allow for exact retransmission
	memcpy(&flash_rfTxInfo, &flash_rfRxInfo, sizeof(RF_RX_INFO));
	rf_test_mode();
	rf_data_mode();
	rf_rx_off();
	rf_tx_packet(&flash_rfTxInfo);
	rf_rx_off();

}

int8_t flash_set_rf_power(uint8_t power){
	if (power>31) return NRK_ERROR;
	rf_tx_power(power);
	return NRK_OK;
}

void flash_reset_err_count()
{
	flash_err_count = 0;
}

uint32_t flash_get_err_count()
{
	return flash_err_count;
}

int8_t flash_init (uint8_t chan)
{	
	// Setup flash signal handler
	flash_ReTx_signal = nrk_signal_create();
	if (flash_ReTx_signal == NRK_ERROR)
	{
		nrk_kprintf(PSTR("FLASH ERROR: creating re-transmit signal failed\r\n"));
		nrk_kernal_error_add(NRK_SIGNAL_CREATE_ERROR, nrk_cur_task_TCB->task_ID);
		return NRK_ERROR;
	}
	flash_err_count = 0;
    // Setup the cc2420 chip
    rf_init (&flash_rfRxInfo, chan, 0xffff, 0);

    FASTSPI_SETREG(CC2420_RSSI, 0xE580); // CCA THR=-25
    FASTSPI_SETREG(CC2420_TXCTRL, 0x80FF); // TX TURNAROUND = 128 us
    FASTSPI_SETREG(CC2420_RXCTRL1, 0x0A56);

	// Setup channel number
	flash_chan = chan;
    return NRK_OK;
}

void flash_disable()
{
    is_enabled=0;
    rf_power_down();
	rf_rx_off();
}

void flash_enable()
{
    is_enabled=1;
    rf_power_up();
	rf_rx_on();
}
