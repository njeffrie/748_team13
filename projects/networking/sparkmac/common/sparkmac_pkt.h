#ifndef _SPARKMAC_PKT_H_
#define _SPARKMAC_PKT_H_

#include <stdint.h>




// SparkMAC packets
//
//   Notes:
//         0 is the LSB
//         1 is the MSB


#define SM_HEADER			19

#define SM_PKT_MCC_INDEX		0
#define SM_PKT_PROTO_TTL_INDEX		1
#define SM_PKT_SEQ_NUM_INDEX		2
#define SM_PKT_SRC_MAC_0_INDEX		3
#define SM_PKT_SRC_MAC_1_INDEX		4
#define SM_PKT_DST_MAC_0_INDEX		5
#define SM_PKT_DST_MAC_1_INDEX		6
#define SM_PKT_LAST_HOP_INDEX		7
#define SM_PKT_UNIX_TS_SEC_0_INDEX	8
#define SM_PKT_UNIX_TS_SEC_1_INDEX	9
#define SM_PKT_UNIX_TS_SEC_2_INDEX	10
#define SM_PKT_UNIX_TS_SEC_3_INDEX	11
#define SM_PKT_UNIX_TS_MS_0_INDEX	12
#define SM_PKT_UNIX_TS_MS_1_INDEX	13
#define SM_PKT_HEARTBEAT_SEC_0_INDEX	14
#define SM_PKT_HEARTBEAT_SEC_1_INDEX	15
#define SM_PKT_HEARTBEAT_SEC_2_INDEX	16
#define SM_PKT_HEARTBEAT_SEC_3_INDEX	17
#define SM_PKT_PAYLOAD_LEN_INDEX	18
#define SM_PKT_PAYLOAD_START_INDEX	19
#define SM_PKT_ROUTE_MODE_OFFSET	0
// PKT_ROUTE_MODE = PKT_DATA_LEN + *(PKT_DATA_LEN) + PKT_ROUTE_MODE_OFFSET
#define SM_PKT_ENCRYPTION_MAC_START_INDEX	20



#define MAX_PAYLOAD	100
#define MIN_PAYLOAD	3


typedef struct {
  uint8_t mac_contention_control;
  uint8_t proto_version;
  uint8_t ttl;
  uint8_t seq_num;
  uint32_t src_mac;
  uint32_t dst_mac;
  uint8_t last_hop;
  uint32_t unix_timestamp_sec;
  uint16_t unix_timestamp_ms;
  uint16_t heartbeat_timestamp_sec;
  uint16_t heartbeat_timestamp_ms;
  uint8_t payload_len;
  uint8_t payload[MAX_PAYLOAD];
  uint8_t routing_flags;
  // Add routing and footer structures...

   // Read from radio
  uint8_t last_hop_rssi;
} sm_pkt_t;

int sm_pkt_to_buf (sm_pkt_t * pkt, uint8_t * buf);
int sm_buf_to_pkt (uint8_t * buf, sm_pkt_t * pkt);




#endif
