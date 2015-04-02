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
char *tx_buf = "wazzzup";
RF_TX_INFO send_pkt;

char buf[100];

void main(){
	
	nrk_setup_ports();
	nrk_setup_uart(UART_BAUDRATE_115K2);
	nrk_init();
	pRRI.pPayload = buf;
	pRRI.ackRequest = 0;
	pRRI.max_length = 100;
	rf_power_up();
	rf_init(&pRRI, 13, 0xFFFF, 0x0);
	send_pkt.length = 8;
	send_pkt.pPayload = tx_buf;
	send_pkt.cca = 0;
	send_pkt.destAddr = 0;
	send_pkt.ackRequest = 0;
	while (1){
		nrk_led_toggle(GREEN_LED);
		rf_tx_packet(&send_pkt);
		
		int i;
		for (i=0; i<20; i++){
			nrk_spin_wait_us(50000);
		}
	};
}
