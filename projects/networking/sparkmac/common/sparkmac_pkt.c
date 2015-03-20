#include <sparkmac_pkt.h>
#include <stdint.h>
#include <stdio.h>


int sm_pkt_to_buf (sm_pkt_t * pkt, uint8_t * buf)
{
uint8_t i;

  if (buf == NULL || pkt == NULL)
    return -1;
  buf[SM_PKT_MCC_INDEX] = pkt->mac_contention_control;
  buf[SM_PKT_PROTO_TTL_INDEX] = (pkt->proto_version<<4) | (pkt->ttl & 0xf);
  buf[SM_PKT_SEQ_NUM_INDEX] = pkt->seq_num;

  buf[SM_PKT_SRC_MAC_0_INDEX] = pkt->src_mac & 0xff;
  buf[SM_PKT_SRC_MAC_1_INDEX] = (pkt->src_mac >> 8) & 0xff;

  buf[SM_PKT_DST_MAC_0_INDEX] = pkt->dst_mac & 0xff;
  buf[SM_PKT_DST_MAC_1_INDEX] = (pkt->dst_mac >> 8) & 0xff;

  buf[SM_PKT_LAST_HOP_INDEX] = pkt->last_hop;

  buf[SM_PKT_UNIX_TS_SEC_0_INDEX] = pkt->unix_timestamp_sec & 0xff;
  buf[SM_PKT_UNIX_TS_SEC_1_INDEX] = (pkt->unix_timestamp_sec >> 8) & 0xff;
  buf[SM_PKT_UNIX_TS_SEC_2_INDEX] = (pkt->unix_timestamp_sec >> 16) & 0xff;
  buf[SM_PKT_UNIX_TS_SEC_3_INDEX] = (pkt->unix_timestamp_sec >> 24) & 0xff;

  buf[SM_PKT_UNIX_TS_MS_0_INDEX] = pkt->unix_timestamp_ms & 0xff;
  buf[SM_PKT_UNIX_TS_MS_1_INDEX] = (pkt->unix_timestamp_ms >> 8) & 0xff;

  buf[SM_PKT_HEARTBEAT_SEC_0_INDEX] = pkt->heartbeat_timestamp_sec & 0xff;
  buf[SM_PKT_HEARTBEAT_SEC_1_INDEX] = (pkt->heartbeat_timestamp_sec >> 8) & 0xff;
  buf[SM_PKT_HEARTBEAT_SEC_2_INDEX] = (pkt->heartbeat_timestamp_sec >> 16) & 0xff;
  buf[SM_PKT_HEARTBEAT_SEC_3_INDEX] = (pkt->heartbeat_timestamp_sec >> 24) & 0xff;

  buf[SM_PKT_PAYLOAD_LEN_INDEX] = pkt->payload_len;

  for (i = 0; i < pkt->payload_len; i++) {
    buf[SM_PKT_PAYLOAD_START_INDEX + i] = pkt->payload[i];
  }

  return (SM_HEADER+pkt->payload_len);
}


int sm_buf_to_pkt (uint8_t *buf, sm_pkt_t *pkt)
{
  uint8_t i;

  if (buf == NULL || pkt == NULL)
    return -1;


  pkt->mac_contention_control= buf[SM_PKT_MCC_INDEX];
  pkt->proto_version = buf[SM_PKT_PROTO_TTL_INDEX]>>4;
  pkt->ttl = buf[SM_PKT_PROTO_TTL_INDEX] & 0xf;
  pkt->seq_num = buf[SM_PKT_SEQ_NUM_INDEX];

  pkt->src_mac = ((uint32_t) buf[SM_PKT_SRC_MAC_1_INDEX] << 8) | buf[SM_PKT_SRC_MAC_0_INDEX];

  pkt->dst_mac = ((uint32_t) buf[SM_PKT_DST_MAC_1_INDEX] << 8) | buf[SM_PKT_DST_MAC_0_INDEX];

  pkt->last_hop = buf[SM_PKT_LAST_HOP_INDEX];


  pkt->unix_timestamp_sec = ((uint32_t) buf[SM_PKT_UNIX_TS_SEC_3_INDEX] << 24) |
    ((uint32_t) buf[SM_PKT_UNIX_TS_SEC_2_INDEX] << 16) |
    ((uint32_t) buf[SM_PKT_UNIX_TS_SEC_1_INDEX] << 8) | buf[SM_PKT_UNIX_TS_SEC_0_INDEX];


  pkt->unix_timestamp_ms = ((uint32_t) buf[SM_PKT_UNIX_TS_MS_1_INDEX] << 8) | buf[SM_PKT_UNIX_TS_MS_0_INDEX];

  pkt->heartbeat_timestamp_sec = ((uint32_t) buf[SM_PKT_HEARTBEAT_SEC_3_INDEX] << 24) |
    ((uint32_t) buf[SM_PKT_HEARTBEAT_SEC_2_INDEX] << 16) |
    ((uint32_t) buf[SM_PKT_HEARTBEAT_SEC_1_INDEX] << 8) | buf[SM_PKT_HEARTBEAT_SEC_0_INDEX];

  pkt->payload_len = buf[SM_PKT_PAYLOAD_LEN_INDEX];
  if(pkt->payload_len>MAX_PAYLOAD) return -1;

  for (i = 0; i < pkt->payload_len; i++) {
    pkt->payload[i] = buf[SM_PKT_PAYLOAD_START_INDEX + i];
  }


  return (SM_HEADER+pkt->payload_len);

}
