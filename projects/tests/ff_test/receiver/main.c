#include <basic_rf.h>
#include <nrk.h>
#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <avr/sleep.h>
#include <hal.h>
#include <nrk_error.h>
#include <nrk_time.h>
#include <avr/eeprom.h>
#include <nrk_eeprom.h>
#include <string.h>
#include <flash.h>

RF_RX_INFO pRRI;
char buf[100];

void rf_start_callback()
{
	printf("received packet!\r\n");
	nrk_led_toggle(BLUE_LED);
}

void rf_finish_callback()
{
	if (rf_rx_packet_nonblock() != NRK_OK)
		printf("failed to receive packet\r\n");
	rf_rx_off();
	rf_rx_on();
}

void main(){
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);
	nrk_init();
	pRRI.ackRequest = 0;
	pRRI.max_length = 100;
	pRRI.pPayload = buf;
	rf_power_up();
	rf_init(&pRRI, 13, 0xFFFF, 0x0);
	rx_start_callback(rf_start_callback);
	rx_end_callback(rf_finish_callback);
	rf_rx_on();
	printf("finished initialization\r\n");
	while (1){
		continue;
	}
}
