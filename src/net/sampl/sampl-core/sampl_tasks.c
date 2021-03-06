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
#include <bmac.h>
#include <nrk_error.h>
#include <nrk_events.h>
#include <nrk_reserve.h>
#include <sampl.h>
#include <pkt_packer.h>
#include <aggregate.h>
#include <generate.h>
#include <p2p_handler.h>
#include <globals.h>
#include <ff_basic_sensor.h>
#include <nrk_eeprom.h>
#include <route_table.h>
#include <neighbor_list.h>
#include <debug.h>
#include <sampl_tasks.h>


// Operating States
#define IDLE_STATE 0
#define FLOOD_ACTIVE_STATE 1
#define TX_ACTIVE_STATE 1

#define MAX_TTL		15

#define TX_PKTS_PER_MINUTE_DEFAULT	1000
#define MOBILE_PKTS_PER_MINUTE_DEFAULT	1000


#ifndef SAMPL_STACKSIZE
#define SAMPL_STACKSIZE NRK_APP_STACKSIZE	
#endif

uint8_t sampl_init;

int8_t check_subnet (uint8_t * in, uint8_t * my);

nrk_task_type SAMPL_RX_TASK;
NRK_STK ff_rx_task_stack[SAMPL_STACKSIZE];
void ff_rx_task (void);


nrk_task_type SAMPL_TX_TASK;
NRK_STK ff_tx_task_stack[SAMPL_STACKSIZE];
void ff_tx_task (void);

void nrk_create_taskset ();

SAMPL_DOWNSTREAM_PKT_T ds_pkt;
SAMPL_UPSTREAM_PKT_T us_pkt;
SAMPL_UPSTREAM_PKT_T us_pkt_in;
SAMPL_PEER_2_PEER_PKT_T p2p_pkt;
SAMPL_PEER_2_PEER_PKT_T p2p_pkt_in;


uint8_t p2p_pending;
uint8_t us_pending;
uint8_t my_depth;
uint8_t secs_per_level;
uint8_t ff_state;
uint8_t us_state;

uint8_t last_flood_pkt;
uint8_t last_flood_seq_num;
uint8_t last_flood_check_rate;
uint8_t last_flood_ctrl;
uint8_t last_flood_rssi;

nrk_time_t check_period;
nrk_time_t timeout;
nrk_time_t now;
nrk_time_t uptime;

uint8_t aes_key[16] =
  { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
0x0c, 0x0d, 0x0e, 0x0f };
uint8_t CTR_buf[4];
nrk_sig_t flood_pkt_sig;
nrk_sig_t flood_pkt_done_sig;
nrk_sig_t p2p_pkt_sig;


void print_ds_pkt (SAMPL_DOWNSTREAM_PKT_T * ds_pkt)
{
  nrk_kprintf (PSTR ("Downstream Packet:\r\n"));
  nrk_kprintf (PSTR ("  PKT TYPE:       "));
  printf ("%d", ds_pkt->pkt_type);
  nrk_kprintf (PSTR ("\r\n  CTRL FLAGS:  "));
  printf ("%d", ds_pkt->ctrl_flags);
  if (ds_pkt->mac_filter_num != 0) {
    nrk_kprintf (PSTR ("\r\n  MAC MASK NUM:  "));
    printf ("%d", ds_pkt->mac_filter_num);
  }
  nrk_kprintf (PSTR ("\r\n  HOP CNT:        "));
  printf ("%d", ds_pkt->hop_cnt);
  nrk_kprintf (PSTR ("\r\n  HOP MAX:        "));
  printf ("%d", ds_pkt->hop_max);
  nrk_kprintf (PSTR ("\r\n  NAV:            "));
  printf ("%d", ds_pkt->nav);
  nrk_kprintf (PSTR ("\r\n  LEVEL DELAY:    "));
  printf ("%d", ds_pkt->delay_per_level);
  nrk_kprintf (PSTR ("\r\n  CHECK RATE:     "));
  printf ("%d", ds_pkt->mac_check_rate);
  nrk_kprintf (PSTR ("\r\n  RSSI THRESHOLD: "));
  printf ("%d", (int8_t) ds_pkt->rssi_threshold);
  nrk_kprintf (PSTR ("\r\n  SEQ NUM:        "));
  printf ("%d", ds_pkt->seq_num);
  nrk_kprintf (PSTR ("\r\n  PRIORITY:       "));
  printf ("%d", ds_pkt->priority);
  nrk_kprintf (PSTR ("\r\n  LAST HOP MAC:   "));
  printf ("%d", ds_pkt->last_hop_mac);
  nrk_kprintf (PSTR ("\r\n"));
}

void ff_rx_task ()
{
  uint8_t i,j, upstream, downstream, p2p_flag;
  int8_t rssi, val;
  uint8_t *local_rx_buf;
  nrk_sig_mask_t sig_ret;
  nrk_sig_t pkt_rx_sig;

  flood_pkt_done_sig = nrk_signal_create ();
  flood_pkt_sig = nrk_signal_create ();
  p2p_pkt_sig = nrk_signal_create ();
  val = nrk_signal_register (flood_pkt_done_sig);
  if (val == NRK_ERROR)
    nrk_kprintf (PSTR ("Error registering flood signal\r\n"));

  last_flood_seq_num = 250;
  local_epoch=0;
  // initialize the routing table
  route_table_init ();

  // initialize the neighbor list 
  neighbor_list_init ();

// Default neighbor list TTL is 10 minutes
  neighborlist_ttl = 600;

// 100 ms by default
  last_flood_check_rate = DEFAULT_CHECK_RATE;
  // Setup some default values
  ff_state = IDLE_STATE;
  my_depth = 10;
  secs_per_level = 1;
  p2p_pending = 0;

  val = read_eeprom_mac_address (&mac_address);
  if (val == NRK_OK) {
    my_mac = (mac_address & 0xff);
    my_subnet_mac[0] = ((mac_address >> 8) & 0xff);
    my_subnet_mac[1] = ((mac_address >> 16) & 0xff);
    my_subnet_mac[2] = ((mac_address >> 24) & 0xff);
  }
  else {
    while (1) {
      nrk_led_toggle(RED_LED);
      nrk_kprintf (PSTR
                   ("* ERROR reading MAC address from EEPROM run eeprom-config utility\r\n"));
      //for(i=0; i<100; i++ ) nrk_wait_until_next_period ();
      nrk_wait_until_next_period ();
    }
  }

  val = read_eeprom_channel (&i);
  val = read_eeprom_aes_key ((uint8_t *) & aes_key);
  nrk_kprintf( PSTR("\r\nNano-RK Version: " ));
  printf( "%d",NRK_VERSION );
  nrk_kprintf( PSTR("\r\nSAMPL Version: " ));
  printf( "%d.1",SAMPL_VERSION );
  nrk_kprintf( PSTR("\r\nNetmask: 0x " ));
  printf ("%x", my_subnet_mac[2]);
  printf (" %x ", my_subnet_mac[1]);
  printf ("%x", my_subnet_mac[0]);
  nrk_kprintf( PSTR("\r\nMAC: 0x " ));
  printf ("%x", my_mac);
  nrk_kprintf( PSTR("\r\nChannel: " ));
  printf ("%d", i);

// 30 minute timer
timeout.secs=30*60;
timeout.nano_secs=0;
val=nrk_sw_wdt_init(1, &timeout, NULL);
nrk_sw_wdt_start(1);

  us_state=IDLE_STATE;
  // init bmac on channel 26 
  bmac_init (i);

  val = read_eeprom_load_img_pages (&i);
  nrk_kprintf( PSTR("\r\nImage Size: " ));
  printf ("%d", i);
  nrk_kprintf( PSTR("\r\nAES Checksum: " ));
  j=0;
  for(i=0; i<16; i++ )
	{
	j+=aes_key[i];
	}
  printf ("%d", j);
  nrk_kprintf( PSTR("\r\n" ));

  //printf("New Version!");

  bmac_encryption_set_key (aes_key, 16);
  bmac_encryption_enable ();

  val =
    bmac_addr_decode_set_my_mac (((uint16_t) my_subnet_mac[0] << 8) | my_mac);
  val = bmac_addr_decode_dest_mac (0xffff);     // broadcast by default
  bmac_addr_decode_enable ();

  // The default Clear Channel Assement RSSI threshold is -45
  // Setting this value higher means that you will only trigger
  // receive with a very strong signal.  Setting this lower means
  // bmac will try to receive fainter packets.  If the value is set
  // too high or too low performance will suffer greatly.
  bmac_set_cca_thresh (-39);


  //if (val == NRK_ERROR)
  //  nrk_kprintf (PSTR ("ERROR setting bmac rate\r\n"));
  // This sets the next RX buffer.
  // This can be called at anytime before releaseing the packet
  // if you wish to do a zero-copy buffer switch
  bmac_rx_pkt_set_buffer (rx_buf, RF_MAX_PAYLOAD_SIZE);
  pkt_rx_sig = bmac_get_rx_pkt_signal ();

  if (pkt_rx_sig == NRK_ERROR) {
    nrk_kprintf (PSTR ("Error getting bmac RX signal\r\n"));
  }

  while (1) {
    nrk_led_clr (GREEN_LED);
    task_got_pkt_flag = 0;
    nrk_sw_wdt_update(1);
    // Wait until an RX packet is received
    if (bmac_rx_pkt_ready () == 0)
      val = bmac_wait_until_rx_pkt ();
    else
      val = NRK_OK;
    task_got_pkt_flag = 1;
    debug_stats.rx_pkts++;
    if (val == NRK_ERROR) {
      bmac_rx_pkt_release ();
      continue;
    }
    downstream = 0;
    upstream = 0;
    p2p_flag = 0;
    // Get the RX packet 
    if (admin_debug_flag == 1)
      nrk_led_set (GREEN_LED);
    local_rx_buf = bmac_rx_pkt_get ((uint8_t *) & rx_buf_len, &rssi);
/*
    // agr XXX
    if(local_rx_buf==NULL)
    {
	nrk_kprintf( PSTR("bad packet\r\n") );
    }
    else
    {
	printf( "RX %d %d %d [",my_mac, rx_buf_len,rssi );
	for(i=0; i<rx_buf_len; i++ )
	{
	printf( "%d ",local_rx_buf[i] );
	}
	nrk_kprintf( PSTR("]\n"));

    }
*/


#ifdef DEBUG_TXT
    printf ("\r\nGot pkt len=%d, rssi=%d\r\n", rx_buf_len, rssi);
    for(i=0; i<rx_buf_len; i++ )
	    printf( "%x ",rx_buf[i] );
    nrk_kprintf( PSTR("\r\n" ));
#endif

    // A little sanity check to stop really bad packets from doing horrible things
    if (rx_buf_len <= 0 || rx_buf_len > 120 || rssi > 30
        || local_rx_buf == NULL) {
      bmac_rx_pkt_release ();
      continue;
    }


// Determine Packet Type

    // If P2P packet 
    if ((local_rx_buf[CTRL_FLAGS] & (US_MASK | DS_MASK)) == 0
        && p2p_pending == 0) {
      // Packet the gp packet data structure
      p2p_flag = 1;
      p2p_pkt_in.buf = local_rx_buf;
      p2p_pkt_in.buf_len = rx_buf_len;
      p2p_pkt_in.rssi = (int8_t) rssi;
      unpack_peer_2_peer_packet (&p2p_pkt_in);
    }
    // If downstream packet, parse out common parameters
    else if ((local_rx_buf[CTRL_FLAGS] & DS_MASK) != 0
             && ff_state == IDLE_STATE) {
      // Packet the downstream packet data structure
      downstream = 1;
      ds_pkt.buf = local_rx_buf;
      ds_pkt.buf_len = rx_buf_len;
      ds_pkt.rssi = (int8_t) rssi;
      unpack_downstream_packet (&ds_pkt, my_mac);
#ifdef DEBUG_TXT
      print_ds_pkt (&ds_pkt);
#endif
    }
    else if ((local_rx_buf[CTRL_FLAGS] & US_MASK) != 0) {
      // Packet the upstream packet data structure
      upstream = 1;
      us_pkt_in.buf = local_rx_buf;
      us_pkt_in.buf_len = rx_buf_len;
//      us_pkt_in.payload=us_pkt_in.buf[US_PAYLOAD_START];
//      us_pkt_in.payload_len=us_pkt_in.buf_len-US_PAYLOAD_START;
      us_pkt_in.rssi = (int8_t) rssi;
      unpack_upstream_packet (&us_pkt_in);
    }
#ifdef DEBUG_TXT
    else {
      nrk_kprintf (PSTR ("Packet Rejected\r\n"));
      nrk_kprintf (PSTR ("P2P pending: "));
      printf( "%u\r\n",p2p_pending );
    }
#endif

// Service Packet 

#ifdef DEBUG_TXT
    printf ("p2p = %d\r\n", p2p_flag);
    printf ("ff_state = %d\r\n", ff_state);
    printf ("downstream = %d\r\n", downstream);
#endif

    // P2P packet 
    if (p2p_flag == 1) {
#ifdef DEBUG_TXT
      nrk_kprintf (PSTR ("Got P2P packet\r\n"));
#endif
      if (nrk_reserve_consume (mobile_reserve) == NRK_ERROR) {
        bmac_rx_pkt_release ();
        continue;
      }
      // Check if it is a P2P packet 
      // Case 1: P2P packet with dst subnet matching my subnet (handle pkt)
      // Case 2: P2P packet with dst subnet not matching, but mobile flag set which
      //         indicates that the packet should be forwarded.
      if (p2p_pkt_in.ttl > 0 && p2p_pkt_in.ttl < MAX_TTL
          && (check_subnet (p2p_pkt_in.dst_subnet_mac, my_subnet_mac) == 1
              || (p2p_pkt_in.ctrl_flags & MOBILE_MASK) != 0)
          && p2p_pending == 0) {
        p2p_pending = 0;
#ifdef DEBUG_TXT
        nrk_kprintf (PSTR ("Got P2P packet: ttl "));
        printf ("%u\r\n", p2p_pkt_in.ttl);
        nrk_kprintf (PSTR ("payload len: "));
        printf ("%u\r\n", p2p_pkt_in.payload_len);
	
        nrk_kprintf (PSTR ("p2p pkt:"));
        for(i=0; i<p2p_pkt_in.payload_len; i++ ) printf ("%u ", p2p_pkt_in.payload[i]);
	nrk_kprintf( PSTR("\r\n" ));
#endif
        p2p_pkt.buf = p2p_buf;
        p2p_pkt.buf_len = 0;
        p2p_pkt.buf_len = P2P_PAYLOAD_START;
        p2p_pkt.payload = &(p2p_buf[P2P_PAYLOAD_START]);
        p2p_pkt.payload_len = 0;
        p2p_pkt.payload_start = P2P_PAYLOAD_START;
        p2p_pending = handle_peer_2_peer_pkt (&p2p_pkt_in, &p2p_pkt);
        if (p2p_pending)
          val = nrk_event_signal (p2p_pkt_sig);
      }
#ifdef DEBUG_TXT
      else {
        nrk_kprintf (PSTR ("Rejecting P2P packet: ttl "));
        printf ("%d ", p2p_pkt_in.ttl);
        printf ("mm %d\r\n", (p2p_pkt_in.ctrl_flags & MOBILE_MASK));
      }
#endif
    }
    // This is a NEW downstream flood packet
    else if (downstream == 1
             && check_subnet (ds_pkt.subnet_mac, my_subnet_mac) == 1) {
      // Got downstream message from someone in subnet, so add to neighbor list 
      val =
        neighbor_list_add (ds_pkt.last_hop_mac, ds_pkt.rssi,
                           neighborlist_ttl);
      if (val == NRK_ERROR)
        nrk_kprintf (PSTR ("Neighbor list add failed 1\r\n"));
      // We actually need to service this downstream message
      if (ff_state == IDLE_STATE && ds_pkt.seq_num != last_flood_seq_num) {
        last_flood_pkt = ds_pkt.pkt_type;
        last_flood_check_rate = ds_pkt.mac_check_rate;
        last_flood_ctrl = ds_pkt.ctrl_flags;
        if (check_subnet (ds_pkt.subnet_mac, my_subnet_mac) == 1) {
          if (ds_pkt.hop_cnt < ds_pkt.hop_max
              && ds_pkt.rssi > ds_pkt.rssi_threshold) {
            // Set the reply address for the gateway (mac=0) to last-hop with no ttl timeout
            val = route_table_set (GATEWAY_MAC, ds_pkt.last_hop_mac, 0);
            if (val == NRK_ERROR)
              nrk_kprintf (PSTR ("Route Table set Error\r\n"));
	
	    // Update local epoch
	    local_epoch=(uint32_t)local_rx_buf[DS_EPOCH_TIME_3]<<24 |
	    		(uint32_t)local_rx_buf[DS_EPOCH_TIME_2]<<16 |
	    		(uint32_t)local_rx_buf[DS_EPOCH_TIME_1]<<8 |
	    		(uint32_t)local_rx_buf[DS_EPOCH_TIME_0];
	    // Check if epoch was set
	    if(local_epoch!=0)
	    {
	    	nrk_time_get(&uptime);
	    	local_epoch-=uptime.secs;
	    }
	    //printf( "local epoch=%lu\r\n",local_epoch );
            if ((ds_pkt.ctrl_flags & ENCRYPT) != 0) {
              // FIXME:  Why is this tx_buf and not rx_buf???
              CTR_buf[0] = local_rx_buf[DS_AES_CTR_0];
              CTR_buf[1] = local_rx_buf[DS_AES_CTR_1];
              CTR_buf[2] = local_rx_buf[DS_AES_CTR_2];
              CTR_buf[3] = local_rx_buf[DS_AES_CTR_3];
              bmac_encryption_set_ctr_counter ((uint8_t *) & CTR_buf, 4);
            }
            last_flood_seq_num = ds_pkt.seq_num;
            // copy the flood packet to tx buffer
            for (i = 0; i < rx_buf_len; i++)
              tx_buf[i] = local_rx_buf[i];
            tx_buf_len = rx_buf_len;
            val = nrk_event_signal (flood_pkt_sig);
            if (val == NRK_ERROR) {
#ifdef DEBUG_TXT
              nrk_kprintf (PSTR ("Flood pkt signal failed\r\n"));
              printf ("errno: %u \r\n", nrk_errno_get ());
#endif
            }
            // Wait for first flood packet to transmit
            sig_ret = nrk_event_wait (SIG (flood_pkt_done_sig));
            if (sig_ret == 0) {
#ifdef DEBUG_TXT
              nrk_kprintf (PSTR ("Error waiting on flood done signal\r\n"));
              printf ("errno: %u \r\n", nrk_errno_get ());
#endif
            }
#ifdef DEBUG_TXT
            nrk_kprintf (PSTR ("RSSI Accepted!\r\n"));
#endif
          }
          else {
#ifdef DEBUG_TXT
            printf ("RSSI reject: %d < %d\r\n", ds_pkt.rssi,
                    ds_pkt.rssi_threshold);
#endif
          }
        }
      }
    }
    // Upstream Packet to Aggregate
    else if ( /*ff_state == FLOOD_ACTIVE_STATE && */ upstream == 1) {

      if(ff_state!=FLOOD_ACTIVE_STATE && ((us_pkt_in.ctrl_flags & TREE_FILTER) ==0) &&
	((us_pkt_in.ctrl_flags & LINK_ACK) ==0)) 
	{
	// This appears to be an asynchonous upstream pkt with no flood supression of any sort
	// that is outside of the normal flood period.  
    		bmac_rx_pkt_release ();
		continue;
	}

	// Too late, we are already trying to Transmit an upstream packet
	// try to set an error flag and punt
	if(us_state==TX_ACTIVE_STATE)
	{
		us_pkt.buf[US_ERROR_CODE] |=DELAY_PER_LEVEL_ERROR_MASK;
    		bmac_rx_pkt_release ();
		continue;
	}



      // Add neighbor, with timeout
      val =
        neighbor_list_add (us_pkt_in.last_hop_src_mac, us_pkt_in.rssi,
                           neighborlist_ttl);
      if (val == NRK_ERROR)
        nrk_kprintf (PSTR ("Neighbor list add failed 2\r\n"));

	// Check for active state if late packets have come since the up stream tx started
      if (us_pkt_in.seq_num == last_flood_seq_num
          && check_subnet (us_pkt_in.subnet_mac, my_subnet_mac) == 1) {
        if ((last_flood_ctrl & TREE_FILTER) != 0) {
          if (us_pkt_in.next_hop_dst_mac == my_mac) {
#ifdef DEBUG_TXT
            nrk_kprintf (PSTR ("Aggregating pkt 1\r\n"));
#endif
	    	aggregate_upstream_data (&us_pkt_in, &us_pkt);
            if (ff_state != FLOOD_ACTIVE_STATE) {
	      // If not in the flooding aggregation state, then this is an asynchronous upstream
	      // message that arrived later but has flooding supression  
              us_pending = 1;
              val = nrk_event_signal (p2p_pkt_sig);
              if (val == NRK_ERROR) {
#ifdef DEBUG_TXT
                nrk_kprintf (PSTR ("p2p pkt signal failed\r\n"));
                printf ("errno: %u \r\n", nrk_errno_get ());
#endif
              }
            }
          }
        }
        else {
#ifdef DEBUG_TXT
          nrk_kprintf (PSTR ("Aggregating pkt 2\r\n"));
#endif
          	aggregate_upstream_data (&us_pkt_in, &us_pkt);
          if (ff_state != FLOOD_ACTIVE_STATE) {
            us_pending = 1;
            val = nrk_event_signal (p2p_pkt_sig);
            if (val == NRK_ERROR) {
#ifdef DEBUG_TXT
              nrk_kprintf (PSTR ("p2p pkt signal failed\r\n"));
              printf ("errno: %u \r\n", nrk_errno_get ());
#endif
            }
          }
        }
      }
    }
    else {

#ifdef DEBUG_TXT
      nrk_kprintf (PSTR ("No Packet CASE matched!\r\n"));
#endif
    }


    // Release the RX buffer so future packets can arrive 
    //while(flood_sig_sent) 
//              nrk_wait_until_next_period();    
    bmac_rx_pkt_release ();

  }
}


void ff_tx_task ()
{
  uint8_t retry, i,ack_retry;
  uint16_t reply_mac;
  int8_t val;
  nrk_sig_mask_t sig_ret;

  //printf ("ff tx task PID=%d\r\n", nrk_get_pid ());
  admin_debug_flag = 0;
  // Wait until the tx_task starts up bmac
  // This should be called by all tasks using bmac that
  // do not call bmac_init()...
  while (!bmac_started ())
    nrk_wait_until_next_period ();

timeout.secs=30*60;
timeout.nano_secs=0;
val=nrk_sw_wdt_init(2, &timeout, NULL);
nrk_sw_wdt_start(2);

  check_period.secs = 0;
  check_period.nano_secs = DEFAULT_CHECK_RATE * NANOS_PER_MS;
  val = bmac_set_rx_check_rate (check_period);

  // Set the low-level packet transmit reserve
  timeout.secs = 60;
  timeout.nano_secs = 0;
  val = bmac_tx_reserve_set (&timeout, TX_PKTS_PER_MINUTE_DEFAULT);
  if (val == NRK_ERROR)
    nrk_kprintf (PSTR
                 ("Error setting b-mac tx reservation (is NRK_MAX_RESERVES defined?)\r\n"));

  mobile_reserve = nrk_reserve_create ();
  if (mobile_reserve == NRK_ERROR)
    nrk_kprintf (PSTR ("Error creating mobile node reserve\r\n"));
  val = nrk_reserve_set (mobile_reserve, &timeout, MOBILE_PKTS_PER_MINUTE_DEFAULT, NULL);
  if (val == NRK_ERROR)
    nrk_kprintf (PSTR ("Error setting mobile node reserve\r\n"));

  val = nrk_signal_register (flood_pkt_sig);
  if (val == NRK_ERROR)
    nrk_kprintf (PSTR ("Error registering flood signal\r\n"));

  val = nrk_signal_register (p2p_pkt_sig);
  if (val == NRK_ERROR)
    nrk_kprintf (PSTR ("Error registering idle signal\r\n"));

  sampl_startup();
  while (1) {
    sampl_init=1;
    nrk_sw_wdt_update(2);
    // Wait until Flood signal or explicit TX signal
    sig_ret = nrk_event_wait (SIG (flood_pkt_sig) | SIG (p2p_pkt_sig));
#ifdef DEBUG_TXT
    nrk_kprintf (PSTR ("Got signal\r\n"));
#endif
    if (sig_ret != 0 /*&& tx_buf_len != 0 */ ) {
      if (sig_ret & SIG (flood_pkt_sig)) {
        // TX Flood Packet Forward
        // Set reply mac address
        tx_buf[DS_LAST_HOP_MAC] = my_mac;
        // Increase Hop count and forward
        tx_buf[DS_HOP_CNT] = ds_pkt.hop_cnt + 1;
        // if((ds_pkt.ctrl_flags & LINK_ACK)!=0) bmac_addr_decode_enable();
        if ((ds_pkt.ctrl_flags & DEBUG_FLAG) != 0
            && ds_pkt.is_mac_selected == 1)
          nrk_led_set (BLUE_LED);
        if ((ds_pkt.ctrl_flags & ENCRYPT) == 0)
          bmac_encryption_disable ();
        bmac_set_cca_active (false);

        //while(bmac_rx_pkt_ready()) nrk_wait_until_next_period();      
        val = bmac_tx_pkt (tx_buf, tx_buf_len);
        debug_stats.tx_pkts++;
        bmac_set_cca_active (true);
#ifdef DEBUG_TXT
        nrk_kprintf (PSTR ("Tx flood forwarded!\r\n"));
#endif
        bmac_encryption_enable ();
        check_period.secs = 0;
        check_period.nano_secs = last_flood_check_rate * NANOS_PER_MS;
        val = bmac_set_rx_check_rate (check_period);

        // Build Reply Packet 
#ifdef DEBUG_TXT
        nrk_kprintf (PSTR ("Generate Upstream\r\n"));
#endif
        us_pkt.buf = tx_buf;
        us_pkt.buf_len = 0;


	nrk_time_get(&now);

	reply_mac = route_table_get (GATEWAY_MAC);
        create_upstream_data_packet (&ds_pkt, &us_pkt, (reply_mac & 0xff));


	timeout.secs = now.secs +
          (ds_pkt.delay_per_level * (ds_pkt.hop_max - ds_pkt.hop_cnt));
        timeout.nano_secs = now.nano_secs;
	//timeout.secs =
        //  (ds_pkt.delay_per_level * (ds_pkt.hop_max - ds_pkt.hop_cnt));
        //timeout.nano_secs = 0;

	

	// This was from a flood signal, so it was waiting...
        // send a done signal to rx task waiting to release the ds packet 
        val = nrk_event_signal (flood_pkt_done_sig);
        if (val == NRK_ERROR) {
#ifdef DEBUG_TXT
          nrk_kprintf (PSTR ("flood pkt done signal failed\r\n"));
          printf ("errno: %u \r\n", nrk_errno_get ());
#endif
        }

        // Set this to active to aggregate incomming packets


        ff_state = FLOOD_ACTIVE_STATE;
        // Wait for depth timeout
#ifdef DEBUG_TXT
        nrk_kprintf (PSTR ("Flood start waiting...\r\n"));
#endif
        //val = nrk_wait (timeout);
	// do a wait_until to compensate for time it takes to create upstream packet...
        val = nrk_wait_until (timeout);
#ifdef DEBUG_TXT
        nrk_kprintf (PSTR ("Flooding period over\r\n"));
#endif
        us_pending = 1;
      }

      if (us_pending == 1) {

        // Send Upstream Packet
        if (us_pkt.pkt_type != EMPTY_PKT) {
          if ((us_pkt.ctrl_flags & LINK_ACK) != 0) {
            val = bmac_addr_decode_dest_mac (((uint16_t) us_pkt.subnet_mac[0] << 8) | us_pkt.next_hop_dst_mac); // broadcast by default
            val = bmac_auto_ack_enable ();
          }
          else
            us_pkt.ack_retry = 0;
          if ((us_pkt.ctrl_flags & ENCRYPT) == 0)
            bmac_encryption_disable ();
          us_pkt.last_hop_src_mac = my_mac;
  	  us_state=TX_ACTIVE_STATE;
          pack_upstream_packet (&us_pkt);
          retry = 0;
          debug_stats.tx_pkts++;
          do {
            val = bmac_tx_pkt (us_pkt.buf, us_pkt.buf_len);
	    retry++;
          } while (val != 1 && retry < us_pkt.ack_retry);
          debug_stats.tx_retry += retry - 1;
          if ((us_pkt.ctrl_flags & LINK_ACK) != 0) {
            val = bmac_auto_ack_disable ();
            val = bmac_addr_decode_dest_mac (0xffff);
          }
          bmac_encryption_enable ();
          //bmac_addr_decode_disable();
  	  us_state=IDLE_STATE;
#ifdef DEBUG_TXT
          nrk_kprintf (PSTR ("Sent Upstream packet!\r\n"));
#endif
        }
#ifdef DEBUG_TXT
        else
          nrk_kprintf (PSTR ("EMPTY Upstream packet!\r\n"));
#endif

        nrk_led_clr (BLUE_LED);

        check_period.secs = 0;
        check_period.nano_secs = DEFAULT_CHECK_RATE * NANOS_PER_MS;
        val = bmac_set_rx_check_rate (check_period);

        us_pending = 0;
      }


      if ( /*(sig_ret & SIG (p2p_pkt_sig)) || */ p2p_pending == 1) {
#ifdef DEBUG_TXT
        nrk_kprintf (PSTR ("Send P2P packet...\r\n"));
#endif
        if (admin_debug_flag == 1)
          nrk_led_set (RED_LED);
        check_period.secs = 0;
        check_period.nano_secs = p2p_pkt.check_rate * NANOS_PER_MS;
        val = bmac_set_rx_check_rate (check_period);

	ack_retry=p2p_pkt.ack_retry;
        if ((p2p_pkt.ctrl_flags & ENCRYPT) == 0)
          bmac_encryption_disable ();
	// Enable link acks on p2p if the flag is set and not broadcast address
	// Only enable ACK for packets routed to GW, otherwise we have no routing data
        if (((p2p_pkt.ctrl_flags & LINK_ACK) != 0) && (p2p_pkt.next_hop_mac!=255) ) {
          val = bmac_addr_decode_dest_mac (((uint16_t) p2p_pkt.dst_subnet_mac[0] << 8) | p2p_pkt.next_hop_mac);     // broadcast by default
          val = bmac_auto_ack_enable ();
        }
        else
	{
#ifdef DEBUG_TXT
	nrk_kprintf( PSTR( "P2P Disable link ACK\r\n" ));
#endif
	  ack_retry=0;	  
          //p2p_pkt.ack_retry = 0;
	}
        p2p_pkt.last_hop_mac = my_mac;
        pack_peer_2_peer_packet (&p2p_pkt);

#ifdef DEBUG_TXT
        printf ("ctrl_flag =%x\r\n", p2p_pkt.ctrl_flags);
        printf ("dst addr[0] =%d\r\n", p2p_pkt.dst_subnet_mac[0]);
        printf ("dst addr[1] =%d\r\n", p2p_pkt.dst_subnet_mac[1]);
        printf ("dst addr[2] =%d\r\n", p2p_pkt.dst_subnet_mac[2]);
        printf ("dst mac =%d\r\n", p2p_pkt.dst_mac);
        printf ("next hop mac =%d\r\n", p2p_pkt.next_hop_mac);
        printf ("ack retry=%d\r\n", p2p_pkt.ack_retry);
        printf ("ttl=%d\r\n", p2p_pkt.ttl);
        printf ("p2p reply=%d: ", p2p_pkt.buf_len);
        for (val = 0; val < p2p_pkt.buf_len; val++)
          printf ("%d ", p2p_pkt.buf[val]);
        printf ("\r\n");
#endif
        debug_stats.tx_pkts++;
        retry = 0;
        do {
          val = bmac_tx_pkt (p2p_pkt.buf, p2p_pkt.buf_len);
          retry++;
        } while (val != 1 && retry < ack_retry);
        debug_stats.tx_retry += retry - 1;
        if ((p2p_pkt.ctrl_flags & LINK_ACK) != 0) {
          val = bmac_auto_ack_disable ();
          val = bmac_addr_decode_dest_mac (0xffff);
        }
        bmac_encryption_enable ();

        p2p_pending = 0;
        check_period.secs = 0;
        check_period.nano_secs = DEFAULT_CHECK_RATE * NANOS_PER_MS;
        val = bmac_set_rx_check_rate (check_period);
        nrk_led_clr (RED_LED);
      }

      ff_state = IDLE_STATE;
    }
    else
      nrk_kprintf (PSTR ("flood wait failed\r\n"));

  }

}


int8_t check_subnet (uint8_t * in, uint8_t * my)
{
  if (in[0] == 255 && in[1] == 255 && in[2] == 255)
    return NRK_OK;
  if (my[0] == in[0] && my[1] == in[1] && my[2] == in[2])
    return NRK_OK;

  return NRK_ERROR;
}

int8_t push_p2p_pkt(char *payload, uint8_t size, uint8_t pkt_type, uint8_t dst_mac )
{
int8_t val;
uint8_t i;
static uint8_t cntr;
if(p2p_pending==1) return 0;
	p2p_pending=1;
	p2p_pkt.pkt_type = pkt_type;
	p2p_pkt.seq_num = cntr;
	cntr++;
	p2p_pkt.ctrl_flags = ENCRYPT | LINK_ACK;
	p2p_pkt.ttl = 6;
  	p2p_pkt.ack_retry = 0x05;
  	p2p_pkt.check_rate = DEFAULT_CHECK_RATE;
    	p2p_pkt.dst_subnet_mac[0] = my_subnet_mac[0];
    	p2p_pkt.dst_subnet_mac[1] = my_subnet_mac[1];
    	p2p_pkt.dst_subnet_mac[2] = my_subnet_mac[2];
    	p2p_pkt.src_subnet_mac[0] = my_subnet_mac[0];
    	p2p_pkt.src_subnet_mac[1] = my_subnet_mac[1];
    	p2p_pkt.src_subnet_mac[2] = my_subnet_mac[2];
    	p2p_pkt.src_mac = my_mac;
    	p2p_pkt.dst_mac = dst_mac;
	p2p_pkt.buf = p2p_buf;
        p2p_pkt.buf_len = 0;
        p2p_pkt.buf_len = P2P_PAYLOAD_START;
        p2p_pkt.payload_start = P2P_PAYLOAD_START;
        p2p_pkt.payload = &(p2p_buf[P2P_PAYLOAD_START]);
	for(i=0; i<size; i++ )
		p2p_pkt.payload[i]=payload[i];
        p2p_pkt.payload_len = size;
	val = nrk_event_signal (p2p_pkt_sig);
return val;
}

int8_t sampl_started()
{

return sampl_init;
}

void sampl_config ()
{

  sampl_init=0;
  SAMPL_RX_TASK.task = ff_rx_task;
  nrk_task_set_stk (&SAMPL_RX_TASK, ff_rx_task_stack, SAMPL_STACKSIZE);
  SAMPL_RX_TASK.prio = 5;
  SAMPL_RX_TASK.FirstActivation = TRUE;
  SAMPL_RX_TASK.Type = BASIC_TASK;
  SAMPL_RX_TASK.SchType = PREEMPTIVE;
  SAMPL_RX_TASK.period.secs = 1;
  SAMPL_RX_TASK.period.nano_secs = 0;
  SAMPL_RX_TASK.cpu_reserve.secs = 5;
  SAMPL_RX_TASK.cpu_reserve.nano_secs = 0;      // 500 * NANOS_PER_MS;
  SAMPL_RX_TASK.offset.secs = 0;
  SAMPL_RX_TASK.offset.nano_secs = 0;
  nrk_activate_task (&SAMPL_RX_TASK);

  SAMPL_TX_TASK.task = ff_tx_task;
  nrk_task_set_stk (&SAMPL_TX_TASK, ff_tx_task_stack, SAMPL_STACKSIZE);
  SAMPL_TX_TASK.prio = 10;
  SAMPL_TX_TASK.FirstActivation = TRUE;
  SAMPL_TX_TASK.Type = BASIC_TASK;
  SAMPL_TX_TASK.SchType = PREEMPTIVE;
  SAMPL_TX_TASK.period.secs = 1;
  SAMPL_TX_TASK.period.nano_secs = 0;
  SAMPL_TX_TASK.cpu_reserve.secs = 5;
  SAMPL_TX_TASK.cpu_reserve.nano_secs = 0;      // 500 * NANOS_PER_MS;
  SAMPL_TX_TASK.offset.secs = 0;
  SAMPL_TX_TASK.offset.nano_secs = 0;
  nrk_activate_task (&SAMPL_TX_TASK);




}
