/**********************************************************/
/*                                                        */
/* Wireless Bootloader for Atmel ATmega micros            */
/*                                                        */
/* 20140625: Originally implemented for and tested on     */
/*           the ATmega128RFA1                            */
/*                                                        */
/* author: Max Buevich, WiSE Lab, CMU                     */
/* http://nano-rk.org                                     */
/*                                                        */
/* ------------------------------------------------------ */
/*                                                        */
/* Serial Bootloader for Atmel megaAVR Controllers        */
/*                                                        */
/* tested with ATmega8, ATmega128 and ATmega168           */
/* should work with other mega's, see code for details    */
/*                                                        */
/* ATmegaBOOT.c                                           */
/*                                                        */
/*                                                        */
/* 20090308: integrated Mega changes into main bootloader */
/*           source by D. Mellis                          */
/* 20080930: hacked for Arduino Mega (with the 1280       */
/*           processor, backwards compatible)             */
/*           by D. Cuartielles                            */
/* 20070626: hacked for Arduino Diecimila (which auto-    */
/*           resets when a USB connection is made to it)  */
/*           by D. Mellis                                 */
/* 20060802: hacked for Arduino by D. Cuartielles         */
/*           based on a previous hack by D. Mellis        */
/*           and D. Cuartielles                           */
/*                                                        */
/* Monitor and debug functions were added to the original */
/* code by Dr. Erik Lins, chip45.com. (See below)         */
/*                                                        */
/* Thanks to Karl Pitrich for fixing a bootloader pin     */
/* problem and more informative LED blinking!             */
/*                                                        */
/* For the latest version see:                            */
/* http://www.chip45.com/                                 */
/*                                                        */
/* ------------------------------------------------------ */
/*                                                        */
/* based on stk500boot.c                                  */
/* Copyright (c) 2003, Jason P. Kyle                      */
/* All rights reserved.                                   */
/* see avr1.org for original file and information         */
/*                                                        */
/* This program is free software; you can redistribute it */
/* and/or modify it under the terms of the GNU General    */
/* Public License as published by the Free Software       */
/* Foundation; either version 2 of the License, or        */
/* (at your option) any later version.                    */
/*                                                        */
/* This program is distributed in the hope that it will   */
/* be useful, but WITHOUT ANY WARRANTY; without even the  */
/* implied warranty of MERCHANTABILITY or FITNESS FOR A   */
/* PARTICULAR PURPOSE.  See the GNU General Public        */
/* License for more details.                              */
/*                                                        */
/* You should have received a copy of the GNU General     */
/* Public License along with this program; if not, write  */
/* to the Free Software Foundation, Inc.,                 */
/* 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */
/*                                                        */
/* Licence can be viewed at                               */
/* http://www.fsf.org/licenses/gpl.txt                    */
/*                                                        */
/* Target = Atmel AVR m128,m64,m32,m16,m8,m162,m163,m169, */
/* m8515,m8535. ATmega161 has a very small boot block so  */
/* isn't supported.                                       */
/*                                                        */
/* Tested with m168                                       */
/**********************************************************/


/* $Id$ */


/* some includes */
#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <util/delay.h>
#include <../basic_rf.h>

/* the current avr-libc eeprom functions do not support the ATmega168 */
/* own eeprom write/read functions are used instead */
//#if !defined(__AVR_ATmega168__) || !defined(__AVR_ATmega328P__)
#include <avr/eeprom.h>
//#endif

#define NUM_LED_FLASHES 1
#define MAX_TIME_COUNT 500000
//#define MAX_TIME_COUNT 1500000

//#define eeprom_write_byte(...)
//#define eeprom_read_byte(...)

/* Use the F_CPU defined in Makefile */

/* 20060803: hacked by DojoCorp */
/* 20070626: hacked by David A. Mellis to decrease waiting time for auto-reset */
/* set the waiting time for the bootloader */
/* get this from the Makefile instead */
/* #define MAX_TIME_COUNT (F_CPU>>4) */

/* 20070707: hacked by David A. Mellis - after this many errors give up and launch application */
#define MAX_ERROR_COUNT 5

/* set the UART baud rate */
/* 20060803: hacked by DojoCorp */
#define BAUD_RATE   57600

#define BOOTSIZE 4096
#define APP_END  (FLASHEND -(2*BOOTSIZE) + 1)


/* SW_MAJOR and MINOR needs to be updated from time to time to avoid warning message from AVR Studio */
/* never allow AVR Studio to do an update !!!! */
#define HW_VER	 0x02
#define SW_MAJOR 0x01
#define SW_MINOR 0x10


/* Adjust to suit whatever pin your hardware uses to enter the bootloader */
/* ATmega128 has two UARTS so two pins are used to enter bootloader and select UART */
/* ATmega1280 has four UARTS, but for Arduino Mega, we will only use RXD0 to get code */
/* BL0... means UART0, BL1... means UART1 */

#define BL_DDR  DDRF
#define BL_PORT PORTF
#define BL_PIN  PINF
#define BL0     PINF7
#define BL1     PINF6


/* onboard LED is used to indicate, that the bootloader was entered (3x flashing) */
/* if monitor functions are included, LED goes on after monitor was entered */
/* Onboard LED is connected to pin PB7 (e.g. Crumb128, PROBOmega128, Savvy128, Arduino Mega) */
#define LED_DDR  DDRD
#define LED_PORT PORTD
#define LED_PIN  PIND
#define LED      PIND4


/* monitor functions will only be compiled when using ATmega128, due to bootblock size constraints */
#if defined(__AVR_ATmega1281__) || defined(__AVR_ATmega1280__)
#define MONITOR 1
#endif


/* define various device id's */
/* manufacturer byte is always the same */
#define SIG1	0x1E	// Yep, Atmel is the only manufacturer of AVR micros.  Single source :(


#define SIG2	0x97
#define SIG3	0x04
#define PAGE_SIZE	0x80U	//128 words


/* function prototypes */
void putch(char);
char getch(void);
void getNch(uint8_t);
void byte_response(uint8_t);
void nothing_response(void);
char gethex(void);
void puthex(char);
void flash_led(uint8_t);

/* some variables */
union address_union {
	uint16_t word;
	uint8_t  byte[2];
} address;

union length_union {
	uint16_t word;
	uint8_t  byte[2];
} length;

struct flags_struct {
	unsigned eeprom : 1;
	unsigned rampz  : 1;
} flags;

uint8_t buff[256];
uint8_t address_high;

//uint8_t pagesz=0x80;

uint8_t i;
uint8_t bootuart;// = 0;
//uint8_t error_count; = 0;
uint32_t time_count;// = 0;
uint8_t flash_consistent;

/* RF variables */
RF_TX_INFO rfTxInfo;
RF_RX_INFO rfRxInfo;
uint8_t tx_buf[RF_MAX_PAYLOAD_SIZE];
uint8_t rx_buf[RF_MAX_PAYLOAD_SIZE];
uint8_t chari;


// only jump to flash if safe
void safe_start(void){
   if(flash_consistent)
      app_start();
   return;
}

//void (*app_start)(void) = 0x0000;
//void appstart(void);
void app_start(void){
	LED_PORT &= ~(1<<LED);    // set to default
	boot_rww_enable();              // enable application section
	
    asm volatile ( 
		"clr r1" "\n\t"
		"push r1" "\n\t"
    "push r1" "\n\t" 
    "ret"     "\n\t" 
  ::); 
    for(;;);
}

void __jumpMain     (void) __attribute__ ((naked)) __attribute__ ((section (".init9")));

void __jumpMain(void)
{    
	asm volatile ( ".set __stack, %0" :: "i" (RAMEND) );

	asm volatile ("ldi r24,%0":: "M" (RAMEND & 0xFF));          
	asm volatile ("ldi r25,%0":: "M" (RAMEND >> 8));
	asm volatile ("out __SP_H__,r25" ::);
	asm volatile ("out __SP_L__,r24" ::);

	asm volatile ( "clr __zero_reg__" );
	asm volatile ( "out %0, __zero_reg__" :: "I" (_SFR_IO_ADDR(SREG)) );
#ifndef REMOVE_PROG_PIN_PULLUP   
		LED_PORT |= (1<<LED);
#endif    
		asm volatile ( "rjmp main");                               // jump to main()
}


int main(void) __attribute__ ((OS_main));

/* main program starts here */
int main(void)
{
	uint8_t ch,ch2;
	uint16_t w;

   /* disable watchdog */
   MCUSR &= ~(1 << WDRF);
   WDTCSR |= (1<<WDCE) | (1<<WDE);
   WDTCSR = 0x00;

	asm volatile ("wdr\n\t");
#ifdef WATCHDOG_MODS
	ch = MCUSR;
	MCUSR = 0;

	WDTCSR |= _BV(WDCE) | _BV(WDE);
	WDTCSR = 0;

	// Check if the WDT was used to reset, in which case we dont bootload and skip straight to the code. woot.
	if (! (ch &  _BV(EXTRF))) // if its a not an external reset...
		app_start();  // skip bootloader
#else
	asm volatile("nop\n\t");
#endif

	/* set pin direction for bootloader pin and enable pullup */
	/* for ATmega128, two pins need to be initialized */
	BL_DDR &= ~_BV(BL0);
	BL_DDR &= ~_BV(BL1);
	BL_PORT |= _BV(BL0);
	BL_PORT |= _BV(BL1);


   flash_consistent = 1;
   time_count = 0;
	bootuart = 1;

	if(bootuart == 1) {
		UBRR0L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
		UBRR0H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
		UCSR0A = 0x00;
		UCSR0C = 0x06;
		UCSR0B = _BV(TXEN0)|_BV(RXEN0);
	}
	if(bootuart == 2) {
		UBRR1L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
		UBRR1H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
		UCSR1A = 0x00;
		UCSR1C = 0x06;
		UCSR1B = _BV(TXEN1)|_BV(RXEN1);
	}


	/* set LED pin as output */
	LED_DDR |= _BV(LED);


	/* 20050803: by DojoCorp, this is one of the parts provoking the
		 system to stop listening, cancelled from the original */
	//putch('\0');
  
   flash_consistent = 1;

   rfRxInfo.pPayload = rx_buf;
   rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
   rfRxInfo.length = 0;
   chari = 0;

   rfTxInfo.pPayload = tx_buf;
   rfTxInfo.destAddr = 0x4315;
   rfTxInfo.length = 0;
   rfTxInfo.cca = 0;

   rfRxInfo.ackRequest = 0;

   rf_init(&rfRxInfo, 11, 0x0000, 0xA6D9);

   rf_power_up();
   rf_rx_on();

   set_tx_num(3);

	/* forever loop */
	for (;;) {

	/* get character from UART */
	ch = getch();

   if((time_count > MAX_TIME_COUNT) && flash_consistent){
      app_start();
   }
   time_count = 0;
	
	/* A bunch of if...else if... gives smaller code than switch...case ! */

	/* Hello is anyone home ? */ 
	if(ch=='0') {
		nothing_response();
	}


	/* Request programmer ID */
	/* Not using PROGMEM string due to boot block in m128 being beyond 64kB boundry  */
	/* Would need to selectively manipulate RAMPZ, and it's only 9 characters anyway so who cares.  */
	else if(ch=='1') {
		if (getch() == ' ') {
			putch(0x14);
			putch('A');
			putch('V');
			putch('R');
			putch(' ');
			putch('I');
			putch('S');
			putch('P');
			putch(0x10);
         rf_tx();
		} /*else {
			if (++error_count == MAX_ERROR_COUNT)
				safe_start();
		}*/
	}


	/* AVR ISP/STK500 board commands  DON'T CARE so default nothing_response */
	else if(ch=='@') {
		ch2 = getch();
		if (ch2>0x85) getch();
		nothing_response();
	}


	/* AVR ISP/STK500 board requests */
	else if(ch=='A') {
		ch2 = getch();
		if(ch2==0x80) byte_response(HW_VER);		// Hardware version
		else if(ch2==0x81) byte_response(SW_MAJOR);	// Software major version
		else if(ch2==0x82) byte_response(SW_MINOR);	// Software minor version
		else if(ch2==0x98) byte_response(0x03);		// Unknown but seems to be required by avr studio 3.56
		else byte_response(0x00);				// Covers various unnecessary responses we don't care about
	}


	/* Device Parameters  DON'T CARE, DEVICE IS FIXED  */
	else if(ch=='B') {
		getNch(20);
		nothing_response();
	}


	/* Parallel programming stuff  DON'T CARE  */
	else if(ch=='E') {
		getNch(5);
		nothing_response();
	}


	/* P: Enter programming mode  */
	/* R: Erase device, don't care as we will erase one page at a time anyway.  */
	else if(ch=='P' || ch=='R') {
		nothing_response();
	}


	/* Leave programming mode  */
	else if(ch=='Q') {
		nothing_response();
      app_start();

#ifdef WATCHDOG_MODS
		// autoreset via watchdog (sneaky!)
		WDTCSR = _BV(WDE);
		while (1); // 16 ms
#endif
	}


	/* Set address, little endian. EEPROM in bytes, FLASH in words  */
	/* Perhaps extra address bytes may be added in future to support > 128kB FLASH.  */
	/* This might explain why little endian was used here, big endian used everywhere else.  */
	else if(ch=='U') {
		address.byte[0] = getch();
		address.byte[1] = getch();
		nothing_response();
	}


	/* Universal SPI programming command, disabled.  Would be used for fuses and lock bits.  */
	else if(ch=='V') {
		if (getch() == 0x30) {
			getch();
			ch = getch();
			getch();
			if (ch == 0) {
				byte_response(SIG1);
			} else if (ch == 1) {
				byte_response(SIG2); 
			} else {
				byte_response(SIG3);
			} 
		} else {
			getNch(3);
			byte_response(0x00);
		}
	}


	/* Write memory, length is big endian and is in bytes  */
	else if(ch=='d') {
		length.byte[1] = getch();
		length.byte[0] = getch();
      
      flags.eeprom = 0;
		if (getch() == 'E') flags.eeprom = 1;
		for (w=0;w<length.word;w++) {
			buff[w] = getch();	                        // Store data in buffer, can't keep up with serial data stream whilst programming pages
		}
		if (getch() == ' ') {
			if (flags.eeprom) {		                //Write to EEPROM one byte at a time
				// FIXME: Why is this multiplying by 2?
				// agr removed this line to stop the striding by 8 that avrdude was doing
				//address.word <<= 1;
				for(w=0;w<length.word;w++) {
					eeprom_write_byte((void *)address.word,buff[w]);
					address.word++;
				}			
			}
			else {
            flash_consistent = 0;
            //Write to FLASH one page at a time
				if (address.byte[1]>127) address_high = 0x01;	//Only possible with m128, m256 will need 3rd address byte. FIXME
				else address_high = 0x00;
				RAMPZ = address_high;
				address.word = address.word << 1;	        //address * 2 -> byte location
				/* if ((length.byte[0] & 0x01) == 0x01) length.word++;	//Even up an odd number of bytes */
				if ((length.byte[0] & 0x01)) length.word++;	//Even up an odd number of bytes
				cli();					//Disable interrupts, just to be sure
				while(bit_is_set(EECR,EEPE));			//Wait for previous EEPROM writes to complete
				asm volatile(
					 "clr	r17		\n\t"	//page_word_count
					 "lds	r30,address	\n\t"	//Address of FLASH location (in bytes)
					 "lds	r31,address+1	\n\t"
					 "ldi	r28,lo8(buff)	\n\t"	//Start of buffer array in RAM
					 "ldi	r29,hi8(buff)	\n\t"
					 "lds	r24,length	\n\t"	//Length of data to be written (in bytes)
					 "lds	r25,length+1	\n\t"
					 "length_loop:		\n\t"	//Main loop, repeat for number of words in block							 							 
					 "cpi	r17,0x00	\n\t"	//If page_word_count=0 then erase page
					 "brne	no_page_erase	\n\t"						 
					 "wait_spm1:		\n\t"
					 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
					 "andi	r16,1           \n\t"
					 "cpi	r16,1           \n\t"
					 "breq	wait_spm1       \n\t"
					 "ldi	r16,0x03	\n\t"	//Erase page pointed to by Z
					 "sts	%0,r16		\n\t"
					 "spm			\n\t"							 
#ifdef __AVR_ATmega163__
					 ".word 0xFFFF		\n\t"
					 "nop			\n\t"
#endif
					 "wait_spm2:		\n\t"
					 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
					 "andi	r16,1           \n\t"
					 "cpi	r16,1           \n\t"
					 "breq	wait_spm2       \n\t"									 

					 "ldi	r16,0x11	\n\t"	//Re-enable RWW section
					 "sts	%0,r16		\n\t"						 			 
					 "spm			\n\t"
#ifdef __AVR_ATmega163__
					 ".word 0xFFFF		\n\t"
					 "nop			\n\t"
#endif
					 "no_page_erase:		\n\t"							 
					 "ld	r0,Y+		\n\t"	//Write 2 bytes into page buffer
					 "ld	r1,Y+		\n\t"							 
								 
					 "wait_spm3:		\n\t"
					 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
					 "andi	r16,1           \n\t"
					 "cpi	r16,1           \n\t"
					 "breq	wait_spm3       \n\t"
					 "ldi	r16,0x01	\n\t"	//Load r0,r1 into FLASH page buffer
					 "sts	%0,r16		\n\t"
					 "spm			\n\t"
								 
					 "inc	r17		\n\t"	//page_word_count++
					 "cpi r17,%1	        \n\t"
					 "brlo	same_page	\n\t"	//Still same page in FLASH
					 "write_page:		\n\t"
					 "clr	r17		\n\t"	//New page, write current one first
					 "wait_spm4:		\n\t"
					 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
					 "andi	r16,1           \n\t"
					 "cpi	r16,1           \n\t"
					 "breq	wait_spm4       \n\t"
#ifdef __AVR_ATmega163__
					 "andi	r30,0x80	\n\t"	// m163 requires Z6:Z1 to be zero during page write
#endif							 							 
					 "ldi	r16,0x05	\n\t"	//Write page pointed to by Z
					 "sts	%0,r16		\n\t"
					 "spm			\n\t"
#ifdef __AVR_ATmega163__
					 ".word 0xFFFF		\n\t"
					 "nop			\n\t"
					 "ori	r30,0x7E	\n\t"	// recover Z6:Z1 state after page write (had to be zero during write)
#endif
					 "wait_spm5:		\n\t"
					 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
					 "andi	r16,1           \n\t"
					 "cpi	r16,1           \n\t"
					 "breq	wait_spm5       \n\t"									 
					 "ldi	r16,0x11	\n\t"	//Re-enable RWW section
					 "sts	%0,r16		\n\t"						 			 
					 "spm			\n\t"					 		 
#ifdef __AVR_ATmega163__
					 ".word 0xFFFF		\n\t"
					 "nop			\n\t"
#endif
					 "same_page:		\n\t"							 
					 "adiw	r30,2		\n\t"	//Next word in FLASH
					 "sbiw	r24,2		\n\t"	//length-2
					 "breq	final_write	\n\t"	//Finished
					 "rjmp	length_loop	\n\t"
					 "final_write:		\n\t"
					 "cpi	r17,0		\n\t"
					 "breq	block_done	\n\t"
					 "adiw	r24,2		\n\t"	//length+2, fool above check on length after short page write
					 "rjmp	write_page	\n\t"
					 "block_done:		\n\t"
					 "clr	__zero_reg__	\n\t"	//restore zero register
					 : "=m" (SPMCSR) : "M" (PAGE_SIZE) : "r0","r16","r17","r24","r25","r28","r29","r30","r31"
					 );
				/* Should really add a wait for RWW section to be enabled, don't actually need it since we never */
				/* exit the bootloader without a power cycle anyhow */
			}
         putch(0x14);
			putch(0x10);
         rf_tx();
		} /*else {
			if (++error_count == MAX_ERROR_COUNT)
				safe_start();
		}*/		
	}


	/* Read memory block mode, length is big endian.  */
	else if(ch=='t') {
		length.byte[1] = getch();
		length.byte[0] = getch();
		if (address.word>0x7FFF) flags.rampz = 1;		// No go with m256, FIXME
		else flags.rampz = 0;
		address.word = address.word << 1;	        // address * 2 -> byte location
		if (getch() == 'E') flags.eeprom = 1;
		else flags.eeprom = 0;
		if (getch() == ' ') {		                // Command terminator
			putch(0x14);
         rf_tx();
			for (w=0;w < length.word;w++) {		        // Can handle odd and even lengths okay
				if (flags.eeprom) {	                        // Byte access EEPROM read
#if defined(__AVR_ATmega168__)  || defined(__AVR_ATmega328P__)
					while(EECR & (1<<EEPE));
					EEAR = (uint16_t)(void *)address.word;
					EECR |= (1<<EERE);
					putch(EEDR);
#else
					putch(eeprom_read_byte((void *)address.word));
#endif
					address.word++;
				}
				else {

					if (!flags.rampz) putch(pgm_read_byte_near(address.word));
					else putch(pgm_read_byte_far(address.word + 0x10000));
					// Hmmmm, yuck  FIXME when m256 arrvies
					address.word++;
				}
            if((w+1)%65 == 0)
               rf_tx();
			}
			putch(0x10);
         rf_tx();
		}
	}


	/* Get device signature bytes  */
	else if(ch=='u') {
		if (getch() == ' ') {
			putch(0x14);
			putch(SIG1);
			putch(SIG2);
			putch(SIG3);
			putch(0x10);
         rf_tx();
		} /*else {
			if (++error_count == MAX_ERROR_COUNT)
				safe_start();
		}*/
	}


	/* Read oscillator calibration byte */
	else if(ch=='v') {
		byte_response(0x00);
	}


#if defined MONITOR 

	/* here come the extended monitor commands by Erik Lins */

	/* check for three times exclamation mark pressed */
	else if(ch=='!') {
		ch = getch();
		if(ch=='!') {
		ch = getch();
		if(ch=='!') {
			PGM_P welcome = "";
			uint16_t extaddr;
			uint8_t addrl, addrh;

#ifdef CRUMB128
			welcome = "ATmegaBOOT / Crumb128 - (C) J.P.Kyle, E.Lins - 050815\n\r";
#elif defined PROBOMEGA128
			welcome = "ATmegaBOOT / PROBOmega128 - (C) J.P.Kyle, E.Lins - 050815\n\r";
#elif defined SAVVY128
			welcome = "ATmegaBOOT / Savvy128 - (C) J.P.Kyle, E.Lins - 050815\n\r";
#elif defined __AVR_ATmega1280__ 
			welcome = "ATmegaBOOT / Arduino Mega - (C) Arduino LLC - 090930\n\r";
#endif

			/* turn on LED */
			LED_DDR |= _BV(LED);
			LED_PORT &= ~_BV(LED);

			/* print a welcome message and command overview */
			for(i=0; welcome[i] != '\0'; ++i) {
				putch(welcome[i]);
			}

			/* test for valid commands */
			for(;;) {
				putch('\n');
				putch('\r');
				putch(':');
				putch(' ');

				ch = getch();
				putch(ch);

				/* toggle LED */
				if(ch == 't') {
					if(bit_is_set(LED_PIN,LED)) {
						LED_PORT &= ~_BV(LED);
						putch('1');
					} else {
						LED_PORT |= _BV(LED);
						putch('0');
					}
				} 

				/* read byte from address */
				else if(ch == 'r') {
					ch = getch(); putch(ch);
					addrh = gethex();
					addrl = gethex();
					putch('=');
					ch = *(uint8_t *)((addrh << 8) + addrl);
					puthex(ch);
				}

				/* write a byte to address  */
				else if(ch == 'w') {
					ch = getch(); putch(ch);
					addrh = gethex();
					addrl = gethex();
					ch = getch(); putch(ch);
					ch = gethex();
					*(uint8_t *)((addrh << 8) + addrl) = ch;
				}

				/* read from uart and echo back */
				else if(ch == 'u') {
					for(;;) {
						putch(getch());
					}
				}
				/* external bus loop  */
				else if(ch == 'b') {
					putch('b');
					putch('u');
					putch('s');
					MCUCR = 0x80;
					XMCRA = 0;
					XMCRB = 0;
					extaddr = 0x1100;
					for(;;) {
						ch = *(volatile uint8_t *)extaddr;
						if(++extaddr == 0) {
							extaddr = 0x1100;
						}
					}
				}

				else if(ch == 'j') {
					safe_start();
				}

			} /* end of monitor functions */

		}
		}
	}
	/* end of monitor */
#endif
	/*else if (++error_count == MAX_ERROR_COUNT) {
		safe_start();
	}*/
	} /* end of forever loop */

}


char gethexnib(void) {
	char a;
	a = getch(); putch(a);
	if(a >= 'a') {
		return (a - 'a' + 0x0a);
	} else if(a >= '0') {
		return(a - '0');
	}
	return a;
}


char gethex(void) {
	return (gethexnib() << 4) + gethexnib();
}


void puthex(char ch) {
	char ah;

	ah = ch >> 4;
	if(ah >= 0x0a) {
		ah = ah - 0x0a + 'a';
	} else {
		ah += '0';
	}
	
	ch &= 0x0f;
	if(ch >= 0x0a) {
		ch = ch - 0x0a + 'a';
	} else {
		ch += '0';
	}
	
	//putch(ah);
	//putch(ch);
   // output to uart for debugging
	putch_uart(ah);
	putch_uart(ch);
}


void putch(char ch)
{
   tx_buf[rfTxInfo.length] = ch;
   rfTxInfo.length++;
}

void rf_tx(void)
{
   _delay_ms(3);
   rf_tx_packet(&rfTxInfo);
}

void putch_uart(char ch)
{
	if(bootuart == 1) {
		while (!(UCSR0A & _BV(UDRE0)));
		UDR0 = ch;
	}
	else if (bootuart == 2) {
		while (!(UCSR1A & _BV(UDRE1)));
		UDR1 = ch;
	}
}


char getch(void)
{
   //uint32_t count = 0;

   while(1){
      if(chari < rfRxInfo.length){
         chari++;
         // print out received character
         //putch2(rx_buf[chari-1]);
         return rx_buf[chari-1];
      }

      if(rf_polling_rx_packet() == 1){
         chari = 0;
      }

      LED_PORT ^= _BV(LED);
	   asm volatile ("wdr\n\t");
	   time_count++;
		if(time_count > MAX_TIME_COUNT){
			//safe_start();
         return 0;
		}
   }
}


char getch_uart(void)
{
	uint32_t count = 0;
	if(bootuart == 1) {
		while(!(UCSR0A & _BV(RXC0))) {
			// Max fix: reset watchdog, just in case
			LED_PORT ^= _BV(LED);
			asm volatile ("wdr\n\t");
			count++;
			if (count > MAX_TIME_COUNT)
				safe_start();
			}

			return UDR0;
		}
	else if(bootuart == 2) {
		while(!(UCSR1A & _BV(RXC1))) {
			// Max fix: reset watchdog, just in case
			LED_PORT ^= _BV(LED);
			asm volatile ("wdr\n\t");
			count++;
			if (count > MAX_TIME_COUNT)
				safe_start();
		}

		return UDR1;
	}
	return 0;
}


void getNch(uint8_t count)
{
	while(count--) {
      getch();
	}
}


void byte_response(uint8_t val)
{
	if (getch() == ' ') {
		putch(0x14);
		putch(val);
		putch(0x10);
      rf_tx();
	} /*else {
		if (++error_count == MAX_ERROR_COUNT)
			safe_start();
	}*/
}


void nothing_response(void)
{
	if (getch() == ' ') {
		putch(0x14);
		putch(0x10);
      rf_tx();
	} /*else {
		if (++error_count == MAX_ERROR_COUNT)
			safe_start();
	}*/
}

void flash_led(uint8_t count)
{
	while (count--) {
		/* Max fix: reset watchdog, just in case */
		asm volatile ("wdr\n\t");
		LED_PORT |= _BV(LED);
		/* Set to negligible time of 1 ms */
		_delay_ms(1);
		asm volatile ("wdr\n\t");
		LED_PORT &= ~_BV(LED);
		_delay_ms(1);
	}
}


/* end of file ATmegaBOOT.c */
