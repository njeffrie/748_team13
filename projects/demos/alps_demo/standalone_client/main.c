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
#include "i2cmaster.h"
#include "tlv320.h"
//#include "tdma_chirp_2.h"
#include "6k.h"

#define VOLUME 30 //0-100%
uint8_t vol;

int main() {
	uint8_t n;
	uint32_t i = 0, j = 0, k;
	uint8_t cnt = 0;
	int16_t curr_sample;
	uint8_t data_ptr = 0;
	uint8_t zero_cnt;

	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);

	vol = VOLUME;
	// Start up TLV230
	DDRE |= (1 << PE4);
	PORTE |= (1 << PE4);
	nrk_spin_wait_us(100);
	PORTE &= (0 << PE4);
	nrk_spin_wait_us(100);
	PORTE |= (1 << PE4);
	nrk_spin_wait_us(1200);

	tlv320_init();
	tlv320_set_volume(vol);

	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x00);
	tlv320_mute();

// Setup timer 0
// Set PD7 (T0) as input for external clock
	DDRD |= (0 << PD7);
	// Set PB7 as output for outputting LRCLK
	DDRB |= (1 << PB7);

	// Capture compare set to 16 cycles
	OCR0A = 15;
	// Toggle OC0A on output match, CTC mode
	TCCR0A |= (0 << COM0A1) | (1 << COM0A0) | (1 << WGM01) | (0 << WGM00);
	// Select external clock T0 with no prescaling
	TCCR0B |= (1 << CS02) | (1 << CS01) | (1 << CS00) | (0 << WGM02);
	// Set UART0 master SPI mode 0
	UCSR0C = (1 << UMSEL01) | (1 << UMSEL00) | (0 << UCPHA0) | (0 << UCPOL0);

	//Setting the XCK0 port pin as output, enables master mode.
	DDRE |= (1 << PE2);

	// Fast mode
	UCSR0A = (1 << U2X0);

	// Enable receiver and transmitter.
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	//Set baud rate. IMPORTANT: The Baud Rate must be set after the transmitter is enabled
	UBRR0 = 1;

	int16_t counter = 0;
	//TCNT0 = 0;
	tlv320_unmute();

	while (1) {

		// First word
		curr_sample = data_pointers[data_ptr][i];

		//TCNT0 = 0; // Reset timer counter before sending

		while (!(UCSR0A & (1 << UDRE0)))
			;
		// L
		UDR0 = (curr_sample >> 8);

		while (!(UCSR0A & (1 << UDRE0)))
			;
		UDR0 = (curr_sample & 0xff);
		i++;

		while (!(UCSR0A & (1 << UDRE0)))
			;
		// R
		UDR0 = (curr_sample >> 8);

		if (i == DATA_LENGTH) {
			if (data_ptr == 0) {
				while (!(UCSR0A & (1 << UDRE0)))
					;
				UDR0 = (curr_sample & 0xff);
				data_ptr = 0;
				i = 0;
				cnt++;
				continue;
			} else {
				data_ptr++;
				i = 0;
			}
		}
		while (!(UCSR0A & (1 << UDRE0)))
			;
		UDR0 = (curr_sample & 0xff);
	}

	return 0;
}
