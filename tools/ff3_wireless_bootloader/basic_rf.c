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
*  Maxim Buevich
*  Anthony Rowe
*******************************************************************************/

#include <avr/io.h>
#include <../basic_rf.h>
#include <string.h>
#include <util/delay.h>


#define OSC_STARTUP_DELAY	1000
//#define RADIO_CC2591
//#define GLOSSY_TESTING

//#define RADIO_VERBOSE
#ifdef RADIO_VERBOSE
	#define vprintf(...)		printf(__VA_ARGS__)
#else
	#define vprintf(...) 	
#endif


typedef struct ieee_mac_fcf{
	uint8_t frame_type: 		3;
	uint8_t sec_en: 			1;
	uint8_t frame_pending: 	1;
	uint8_t ack_request: 	1;
	uint8_t intra_pan: 		1;

	uint8_t res:				3;	

	uint8_t dest_addr_mode:	2;
	uint8_t frame_version: 	2;
	uint8_t src_addr_mode:	2;
} ieee_mac_fcf_t;

typedef struct ieee_mac_frame_header{	
	ieee_mac_fcf_t fcf;
	uint8_t seq_num;

	uint16_t dest_pan_id;
	uint16_t dest_addr;
	//uint16_t src_pan_id;
	uint16_t src_addr;
	/*uint16_t sec_header; */
} ieee_mac_frame_header_t;



static void rf_cmd(uint8_t cmd);

#ifdef GLOSSLY_TESTING
	void clear_packet_flags();
#endif

//uint8_t auto_ack_enable;
//uint8_t security_enable;
//uint8_t last_pkt_encrypted;
uint16_t mdmctrl0;
uint8_t tx_ctr[6];
uint8_t rx_ctr[4];

volatile RF_SETTINGS rfSettings;
uint8_t rf_ready;
volatile uint8_t rx_ready;
volatile uint8_t tx_done;
uint8_t use_glossy;

volatile void (*rx_start_func)(void) = 0;
volatile void (*rx_end_func)(void) = 0;

/* AES encryption and decryption key buffers */
uint8_t ekey[16];
uint8_t dkey[16];

/* Important note: One can declare globals in bootloader
** section, but not set their initial values. */
uint8_t tx_num;
uint8_t flasher_uart_check;

uint16_t prevSrcAddr;
uint8_t prevSeqNum;

void rf_enable_glossy(void)
{
	use_glossy = 1;
}

void rf_disable_glossy(void)
{
	use_glossy = 0;
}

void rf_power_down(void)
{
	uint8_t status;

	while((TRX_STATUS & 0x1F) == STATE_TRANSITION_IN_PROGRESS)
		continue;

	/* For some reason comparing to SLEEP doesn't work, but 0 does */
	status = (TRX_STATUS & 0x1F);
	if((status == 0) || (status == 0xF))
		return;
	/* Disable TRX if it is enabled */
	if((TRX_STATUS & 0x1F) != TRX_OFF){
		rf_cmd(TRX_OFF);
		do{
			status = (TRX_STATUS & 0x1F);
		}while(status != TRX_OFF);
	}

	TRXPR |= (1 << SLPTR);
	do{
		status = (TRX_STATUS & 0x1F);
	}while((status != 0) && (status != 0xF));
}

void rf_power_up()
{
	uint8_t status;

	while((TRX_STATUS & 0x1F) == STATE_TRANSITION_IN_PROGRESS)
		continue;
	/* For some reason comparing to SLEEP doesn't work, but 0 does */
	status = (TRX_STATUS & 0x1F);
	if((status != 0) && (status != 0xF))
		return;

	/* Wake up */
	TRXPR &= ~(1 << SLPTR);
	while((TRX_STATUS & 0x1F) != TRX_OFF)
		continue;
}


/* Safely change the radio state */
static void rf_cmd(uint8_t cmd)
{
	while((TRX_STATUS & 0x1F) == STATE_TRANSITION_IN_PROGRESS)
		continue;
	TRX_STATE = cmd;
}


void rf_tx_power(uint8_t pwr)
{
	PHY_TX_PWR &= 0xF0;
	PHY_TX_PWR |= (pwr & 0xF);
}

void rf_addr_decode_enable(void)
{
	XAH_CTRL_1 &= ~(1 << AACK_PROM_MODE);
}


void rf_addr_decode_disable(void)
{
	XAH_CTRL_1 |= (1 << AACK_PROM_MODE);
}


void rf_auto_ack_enable(void)
{
	CSMA_SEED_1 &= ~(1 << AACK_DIS_ACK);
}

void rf_auto_ack_disable(void)
{
	CSMA_SEED_1 |= (1 << AACK_DIS_ACK);
}


void rf_addr_decode_set_my_mac(uint16_t my_mac)
{
	/* Set short MAC address */
	SHORT_ADDR_0 = (my_mac & 0xFF); 
	SHORT_ADDR_1 = (my_mac >> 8);
	rfSettings.myAddr = my_mac;
}


void rf_set_rx(RF_RX_INFO *pRRI, uint8_t channel )
{
	rfSettings.pRxInfo = pRRI;
	PHY_CC_CCA &= ~(0x1F);
	PHY_CC_CCA |= (channel << CHANNEL0);
}



void rf_init(RF_RX_INFO *pRRI, uint8_t channel, uint16_t panId, uint16_t myAddr)
{ 
/*
	 uint8_t n;
   int8_t v;

#ifdef RADIO_PRIORITY_CEILING
    radio_sem = nrk_sem_create(1,RADIO_PRIORITY_CEILING);
    if (radio_sem == NULL)
      nrk_kernel_error_add (NRK_SEMAPHORE_CREATE_ERROR, nrk_get_pid ());

  v = nrk_sem_pend (radio_sem);
  if (v == NRK_ERROR) {
    nrk_kprintf (PSTR ("CC2420 ERROR:  Access to semaphore failed\r\n"));
  }
#endif
	

#ifdef RADIO_PRIORITY_CEILING
  v = nrk_sem_post (radio_sem);
  if (v == NRK_ERROR) {
    nrk_kprintf (PSTR ("CC2420 ERROR:  Release of semaphore failed\r\n"));
    _nrk_errno_set (2);
  }
#endif

*/


	/* Turn on auto crc calculation */
	TRX_CTRL_1 = (1 << TX_AUTO_CRC_ON);
	/* Set PA buffer lead time to 6 us and TX power to 3.0 dBm (maximum) */
	PHY_TX_PWR = (1 << PA_BUF_LT1) | (1 << PA_BUF_LT0) | (0 << TX_PWR0);
	/* CCA Mode and Channel selection */
	PHY_CC_CCA = (0 << CCA_MODE1) | (1 << CCA_MODE0) | (channel << CHANNEL0);
	/* Set CCA energy threshold */
	CCA_THRES = 0xC5;
	/* Start of frame delimiter */
	SFD_VALUE = 0xA7;
	/* Dynamic buffer protection on and data rate is 250 kb/s */
	TRX_CTRL_2 = (1 << RX_SAFE_MODE) | (0 << OQPSK_DATA_RATE1) | (0 << OQPSK_DATA_RATE0);
	
	/* Set short MAC address */
	SHORT_ADDR_0 = (myAddr & 0xFF); SHORT_ADDR_1 = (myAddr >> 8);
	/* Set PAN ID */
	PAN_ID_0 = (panId & 0xFF); PAN_ID_1 = (panId >> 8);
	
	/* 2-bit random value generated by radio hardware */
	#define RADIO_RAND ((PHY_RSSI >> RND_VALUE0) & 0x3)
	/* Set random csma seed */
	CSMA_SEED_0 = (RADIO_RAND << 6) | (RADIO_RAND << 4) 
			| (RADIO_RAND << 2) | (RADIO_RAND << 0);
	/* Will ACK received frames with version numbers of 0 or 1 */
	CSMA_SEED_1 = (0 << AACK_FVN_MODE1) | (1 << AACK_FVN_MODE0) 
			| (RADIO_RAND << CSMA_SEED_11) | (RADIO_RAND << CSMA_SEED_10);

	/* don't re-transmit frames or perform cca multiple times, slotted op is off */
	XAH_CTRL_0 = (0 << MAX_FRAME_RETRIES0) | (0 << MAX_CSMA_RETRIES0)
			| (0 << SLOTTED_OPERATION);
	/* Enable radio interrupts */
   /* Interrupts disabled */
	/* IRQ_MASK = (1 << AWAKE_EN) | (1 << TX_END_EN) | (1 << AMI_EN) | (1 << CCA_ED_DONE_EN)
			| (1 << RX_END_EN) | (1 << RX_START_EN) | (1 << PLL_UNLOCK_EN) | (1 << PLL_LOCK_EN); */

	/* Initialize settings struct */
	rfSettings.pRxInfo = pRRI;
	rfSettings.txSeqNumber = 0;
	rfSettings.ackReceived = 0;
	rfSettings.panId = panId;
	rfSettings.myAddr = myAddr;
	rfSettings.receiveOn = 0;

	rf_ready = 1;
	rx_ready = 0;
	tx_done = 0;

	use_glossy = 0;
   flasher_uart_check = 0;
   tx_num = 1;

} // rf_init() 


//-------------------------------------------------------------------------------------------------------
//  void rf_rx_on(void)
//
//  DESCRIPTION:
//      Enables the CC2420 receiver and the FIFOP interrupt. When a packet is received through this
//      interrupt, it will call rf_rx_callback(...), which must be defined by the application
//-------------------------------------------------------------------------------------------------------
void rf_rx_on(void) 
{
/*
#ifdef RADIO_PRIORITY_CEILING
	nrk_sem_pend (radio_sem);
#endif
	rfSettings.receiveOn = TRUE;

#ifdef RADIO_PRIORITY_CEILING
	nrk_sem_post(radio_sem);
#endif
*/

#ifdef RADIO_CC2591
	rf_cc2591_rx_on();
#endif
#ifdef GLOSSY_TESTING
	clear_packet_flags();
#endif
	rf_cmd(RX_AACK_ON);
}

void rf_polling_rx_on(void)
{
/*#ifdef RADIO_PRIORITY_CEILING
	nrk_sem_pend (radio_sem);
#endif
  rfSettings.receiveOn = TRUE;


#ifdef RADIO_PRIORITY_CEILING
	nrk_sem_post(radio_sem);
#endif
*/

#ifdef RADIO_CC2591
	rf_cc2591_rx_on();
#endif

	rf_cmd(RX_AACK_ON);
}


//-------------------------------------------------------------------------------------------------------
//  void rf_rx_off(void)
//
//  DESCRIPTION:
//      Disables the CC2420 receiver and the FIFOP interrupt.
//-------------------------------------------------------------------------------------------------------
void rf_rx_off(void)
{
/*
#ifdef RADIO_PRIORITY_CEILING
  nrk_sem_pend (radio_sem);
#endif
	// XXX
  //SET_VREG_INACTIVE();	
	rfSettings.receiveOn = FALSE;

#ifdef RADIO_PRIORITY_CEILING
  nrk_sem_post(radio_sem);
#endif
  //	DISABLE_FIFOP_INT();
*/
	rf_cmd(TRX_OFF);
	rx_ready = 0;
}



//-------------------------------------------------------------------------------------------------------
//  BYTE rf_tx_packet(RF_TX_INFO *pRTI)
//
//  DESCRIPTION:
//		Transmits a packet using the IEEE 802.15.4 MAC data packet format with short addresses. CCA is
//		measured only once before packet transmission (not compliant with 802.15.4 CSMA-CA).
//		The function returns:
//			- When pRTI->ackRequest is FALSE: After the transmission has begun (SFD gone high)
//			- When pRTI->ackRequest is TRUE: After the acknowledgment has been received/declared missing.
//		The acknowledgment is received through the FIFOP interrupt.
//
//  ARGUMENTS:
//      RF_TX_INFO *pRTI
//          The transmission structure, which contains all relevant info about the packet.
//
//  RETURN VALUE:
//		uint8_t
//			Successful transmission (acknowledgment received)
//-------------------------------------------------------------------------------------------------------


uint8_t rf_tx_packet(RF_TX_INFO *pRTI)
{
	/*
	#ifdef RADIO_PRIORITY_CEILING
	nrk_sem_pend(radio_sem);
	#endif

	#ifdef RADIO_PRIORITY_CEILING
	nrk_sem_post(radio_sem);
	#endif
	//return success;
	*/

	uint8_t trx_status, trx_error, *data_start, *frame_start = &TRXFBST;
	uint16_t i, j;

	if(!rf_ready) 
		return -1;
	
	ieee_mac_frame_header_t *machead = frame_start + 1;
	ieee_mac_fcf_t fcf;

	/* TODO: Setting FCF bits is probably slow. Optimize later. */
	fcf.frame_type = 1;
	fcf.sec_en = 0;
	fcf.frame_pending = 0;
	fcf.ack_request = pRTI->ackRequest;
	fcf.intra_pan = 1;
	fcf.res = 0;
	fcf.dest_addr_mode = 2;
	fcf.frame_version = 0;
	fcf.src_addr_mode = 2;
	
	/* Build the rest of the MAC header */
	rfSettings.txSeqNumber++;
	machead->fcf = fcf;
	if (use_glossy) {
		machead->seq_num = 0xFF;
		machead->src_addr = 0xAAAA;
		machead->dest_addr = 0xFFFF;
		machead->dest_pan_id = (PAN_ID_1 << 8) | PAN_ID_0;
	} else {
		machead->seq_num = rfSettings.txSeqNumber;
		machead->src_addr = (SHORT_ADDR_1 << 8) | SHORT_ADDR_0;
		machead->dest_addr = pRTI->destAddr;
		machead->dest_pan_id = (PAN_ID_1 << 8) | PAN_ID_0;
	}
	//machead->src_pan_id = (PAN_ID_1 << 8) | PAN_ID_0;
	
	/* Copy data payload into packet */
	data_start = frame_start + sizeof(ieee_mac_frame_header_t) + 1;
	memcpy(data_start, pRTI->pPayload, pRTI->length);
	/* Set the size of the packet */
	*frame_start = sizeof(ieee_mac_frame_header_t) + pRTI->length + 2;

  	/* Reset frame buffer protection. If there
   ** was a received packet, it's gone now. */
   IRQ_STATUS = (1 << RX_END);
	rx_ready = 0;
	TRX_CTRL_2 &= ~(1 << RX_SAFE_MODE);
	TRX_CTRL_2 |= (1 << RX_SAFE_MODE);


	vprintf("packet length: %d bytes\r\n", *frame_start);

	/* Wait for radio to be in a ready state */
	do{
		trx_status = (TRX_STATUS & 0x1F);
	}while((trx_status == BUSY_TX) || (trx_status == BUSY_RX)
			|| (trx_status == BUSY_RX_AACK) || (trx_status == BUSY_TX_ARET)
			|| (trx_status == STATE_TRANSITION_IN_PROGRESS));
	
	/* Return error if radio not in a tx-ready state */
	if((trx_status != TRX_OFF) && (trx_status != RX_ON) 
			&& (trx_status != RX_AACK_ON) && (trx_status != PLL_ON)){
		return -1;
	}


	rf_cmd(RX_AACK_ON);

	// Perform CCA if requested
	if(pRTI->cca){
		PHY_CC_CCA |= (1 << CCA_REQUEST);
		while(!(TRX_STATUS & (1 << CCA_DONE)))
			continue;
		if(!(TRX_STATUS & (1 << CCA_STATUS)))
			return -1;
	}


	rf_cmd(PLL_ON);
	if(pRTI->ackRequest)
		rf_cmd(TX_ARET_ON);
	

   pRTI->length = 0;

   for(j=0; j<tx_num; j++){
      
      /* wait a few hundred microseconds for spacing out packets */
      if(flasher_uart_check){
         for(i=0; i<400; i++){   
            if(UCSR0A & _BV(RXC0)){ pRTI->pPayload[pRTI->length] = UDR0; pRTI->length++;}
         }
      }
      else
         _delay_us(200);
      
      rf_cmd(0x2);
      for(i=0; i<65000; i++){   
         if(IRQ_STATUS & (1 << TX_END)){
            break;
         }
         if(flasher_uart_check){
            if(UCSR0A & _BV(RXC0)){ pRTI->pPayload[pRTI->length] = UDR0; pRTI->length++;}
         }

      }
      IRQ_STATUS = 1 << TX_END;
   
   }
   

	trx_error = ((pRTI->ackRequest && 
			(((TRX_STATE >> TRAC_STATUS0) & 0x7) != 0))
			|| (i == 65000)) ? -1 : 1;
	rf_cmd(trx_status);

#ifdef RADIO_CC2591
	if (trx_error == -1) rf_cc2591_rx_on();
#endif

	return trx_error;
}


void set_tx_num(uint8_t val)
{
   tx_num = val;
}

void set_uart_check(uint8_t val)
{
   flasher_uart_check = val;
}

int8_t rf_rx_packet_nonblock(void)
{
	/*
	#ifdef RADIO_PRIORITY_CEILING
	nrk_sem_pend(radio_sem);
	#endif
	
	#ifdef RADIO_PRIORITY_CEILING
	nrk_sem_post(radio_sem);
	#endif
	*/
	
	uint8_t *frame_start = &TRXFBST;

	if(!rf_ready)
		return -1;

	//if(!rx_ready)
	//	return 0;
   if(!(IRQ_STATUS & (1 << RX_END))){
      return 0;
   }
   if(!(PHY_RSSI & (1 << RX_CRC_VALID))){
      return 0;
   }
   if((TST_RX_LENGTH - 2) > rfSettings.pRxInfo->max_length)
		return -1;

   IRQ_STATUS = (1 << RX_END);

	ieee_mac_frame_header_t *machead = frame_start;

	rfSettings.pRxInfo->seqNumber = machead->seq_num;
	rfSettings.pRxInfo->srcAddr = machead->src_addr;
	rfSettings.pRxInfo->length = TST_RX_LENGTH - sizeof(ieee_mac_frame_header_t) - 2;

	if((rfSettings.pRxInfo->length > rfSettings.pRxInfo->max_length)
			|| (rfSettings.pRxInfo->length < 0)){
		rx_ready = 0;
		TRX_CTRL_2 &= ~(1 << RX_SAFE_MODE);
		TRX_CTRL_2 |= (1 << RX_SAFE_MODE);
		return -1;
	}

	if((rfSettings.pRxInfo->srcAddr == prevSrcAddr) &&
         (rfSettings.pRxInfo->seqNumber == prevSeqNum)){
		rx_ready = 0;
		TRX_CTRL_2 &= ~(1 << RX_SAFE_MODE);
		TRX_CTRL_2 |= (1 << RX_SAFE_MODE);
		return 0;
	}

   prevSrcAddr = rfSettings.pRxInfo->srcAddr;
   prevSeqNum = rfSettings.pRxInfo->seqNumber;

	memcpy(rfSettings.pRxInfo->pPayload, frame_start 
			+ sizeof(ieee_mac_frame_header_t), rfSettings.pRxInfo->length);
	
	/* I am assuming that ackRequest is supposed to
	 * be set, not read, by rf_basic */
	rfSettings.pRxInfo->ackRequest = machead->fcf.ack_request;
	//rfSettings.pRxInfo->rssi = *(frame_start + TST_RX_LENGTH);
	rfSettings.pRxInfo->rssi = PHY_ED_LEVEL;
	rfSettings.pRxInfo->actualRssi = PHY_RSSI >> 3;
	rfSettings.pRxInfo->energyDetectionLevel = PHY_ED_LEVEL;
	rfSettings.pRxInfo->linkQualityIndication = *(frame_start + TST_RX_LENGTH);

	/* Reset frame buffer protection */
	rx_ready = 0;
	TRX_CTRL_2 &= ~(1 << RX_SAFE_MODE);
	TRX_CTRL_2 |= (1 << RX_SAFE_MODE);

	return 1;
}





void rf_cc2591_tx_on(void)
{
	DPDS1	|= 0x3; 
	DDRG	|= 0x1;
	PORTG	|= 0x1;
	DDRE	|= 0xE0;
	PORTE	|= 0xE0;

    //nrk_spin_wait_us(12);
}

void rf_cc2591_rx_on()
{
	DPDS1	|= 0x3; 
	DDRG	|= 0x1;
	PORTG	&= ~(0x1);
	DDRE	|= 0xE0;
	PORTE	|= 0xE0;

    //nrk_spin_wait_us(12);
}




/* AES encryption and decryption */

void aes_setkey(uint8_t *key)
{
   uint8_t i;

   for(i=0; i<16; i++){
      ekey[i] = key[i];
      AES_KEY = key[i];
   }
   for(i=0; i<16; i++){
      AES_STATE = 0x00;
   }
   AES_CTRL = (1 << AES_REQUEST);

   while(!(AES_STATUS & (1 << AES_DONE))){
      continue;
   }
   for(i=0; i<16; i++){
      dkey[i] = AES_KEY;
   }
}


uint8_t aes_encrypt(uint8_t *data, uint8_t len)
{
   uint8_t i, j;

   if(len==0 || len%16!=0)
      return 1;

   for(i=0; i<16; i++)
      AES_KEY = ekey[i];

   for(i=0; 16*i<len; i++){ 
      if(i==0)
         AES_CTRL = (0 << AES_MODE) | (0 << AES_DIR);
      else
         AES_CTRL = (1 << AES_MODE) | (0 << AES_DIR);
      
      for(j=0; j<16; j++)
         AES_STATE = data[16*i+j];
      AES_CTRL |= (1 << AES_REQUEST);
      while(!(AES_STATUS & (1 << AES_DONE)))
         continue;
      for(j=0; j<16; j++)
         data[16*i+j] = AES_STATE;
   }
   return 0;
}

uint8_t aes_decrypt(uint8_t *data, uint8_t len)
{
   int8_t i;
   uint8_t j;

   if(len==1 || len%16!=0)
      return 1;

   for(i=0; i<16; i++)
      AES_KEY = dkey[i];

   for(i=(len/16)-1; i>=0; i--){ 
      AES_CTRL = (0 << AES_MODE) | (1 << AES_DIR);
      
      for(j=0; j<16; j++)
         AES_STATE = data[16*i+j];
      AES_CTRL |= (1 << AES_REQUEST);
      while(!(AES_STATUS & (1 << AES_DONE)))
         continue;
      for(j=0; j<16; j++){
         data[16*i+j] = AES_STATE;
         if(i!=0)
            data[16*i+j] ^= data[16*(i-1)+j];
      }
   }
   return 0;
}

