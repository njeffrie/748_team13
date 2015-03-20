/**********************************************************/
/*                                                        */
/* Wireless Flasher for Atmel ATmega micros               */
/*                                                        */
/* 20140625: Originally implemented for and tested on     */
/*           the ATmega128RFA1                            */
/*                                                        */
/* author: Max Buevich, WiSE Lab, CMU                     */
/* http://nano-rk.org                                     */
/*                                                        */
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
#include <string.h>
#include <../basic_rf.h>

/* the current avr-libc eeprom functions do not support the ATmega168 */
/* own eeprom write/read functions are used instead */
//#if !defined(__AVR_ATmega168__) || !defined(__AVR_ATmega328P__)
#include <avr/eeprom.h>
//#endif

#define NUM_LED_FLASHES 1
#define MAX_TIME_COUNT 500000
#define SHORT_TIME_COUNT 100
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
#define LED      PIND7


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

uint8_t pagesz=0x80;

uint8_t i;
uint8_t bootuart = 0;

uint8_t error_count = 0;

/* RF variables */
RF_TX_INFO rfTxInfo;
RF_RX_INFO rfRxInfo;
uint8_t tx_buf[RF_MAX_PAYLOAD_SIZE];
uint8_t rx_buf[RF_MAX_PAYLOAD_SIZE];

/* set in the code */
uint8_t reset_val[16];

uint8_t chari;
uint32_t count;


/*
void appstart(void);
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
*/

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
	asm volatile ( "rjmp main");                               // jump to main()
}


int main(void) __attribute__ ((OS_main));

/* main program starts here */
int main(void)
{
	uint8_t ch, i;

   /* For bootloader code, global variable assignment must happen at runtime */
   reset_val[0] = 0x43;
   reset_val[1] = 0x15;
   reset_val[2] = 0xa6;
   reset_val[3] = 0xd9;
   reset_val[4] = 0x3d;
   reset_val[5] = 0x31;
   reset_val[6] = 0x82;
   reset_val[7] = 0xf1;
   reset_val[8] = 0x8c;
   reset_val[9] = 0xa7;
   reset_val[10] = 0x4f;
   reset_val[11] = 0xc5;
   reset_val[12] = 0x99;
   reset_val[13] = 0x97;
   reset_val[14] = 0x04;
   reset_val[15] = 0xac;


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
   

   rfRxInfo.pPayload = rx_buf;
   rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
   rfRxInfo.length = 0;
   rfRxInfo.ackRequest = 0;
   chari = 0;

   rf_init(&rfRxInfo, 13, 0x2420, 0x4315);
   rf_power_up();
   rf_rx_on();

   /* address of node to reset */
   rfTxInfo.destAddr = 0x1214;
   rfTxInfo.pPayload = tx_buf;
   rfTxInfo.cca = 0;

   set_tx_num(3);
   set_uart_check(1);
   rfTxInfo.length = 16;
   memcpy(tx_buf, reset_val, 16);
   rf_tx_packet(&rfTxInfo);

   
   
   /* link parameters in bootloader mode */
   rf_init(&rfRxInfo, 11, 0x0000, 0x4315);
   rfTxInfo.destAddr = 0xA6D9;
   set_tx_num(3);
   set_uart_check(1);
	
   /* forever loop */
   for (;;) {

      if(UCSR0A & _BV(RXC0)){
         tx_buf[rfTxInfo.length] = UDR0;
         rfTxInfo.length++;
         count = 0;
      }

      if(rfTxInfo.length > 0){
         if((rfTxInfo.length >= 64) || (count > SHORT_TIME_COUNT)){
            /* if flash data packet */
            if(rfTxInfo.length == 64 || rfTxInfo.length == 5)
               set_tx_num(3);
            else
               set_tx_num(3);
            rf_tx_packet(&rfTxInfo);
            LED_PORT ^= _BV(LED);	   
         }
      }

      LED_PORT &= ~_BV(LED);
      if(rf_polling_rx_packet() == 1){
         for(i=0; i<rfRxInfo.length; i++){
            putch(rx_buf[i]);
         }
      }
      LED_PORT |= _BV(LED);	

      count++;
	
   } /* end of forever loop */

}




void putch(char ch)
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
