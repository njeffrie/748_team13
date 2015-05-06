#ifndef _TDMA_CONSTANTS_H
#define _TDMA_CONSTANTS_H

#define TDMA_SLOT_LEN 200000//us
#define PKT_LEN 9
#define INTER_SYNC_CYCLES 100
#define NUM_NODES 5
/* we are group 13 */
#define CHAN 16

#define RF_POWER 10

#define IN_SLOT_TX_DELAY 2000

#define DEBUG

/* determines the number of transmissions per data slot... choose a value >= 1 */
/* !!! increases length of slot usage... can overflow if tdma slots are tight !!!*/
#define FLOOD_PROP_REDUNDANCY 1

uint8_t get_curr_tdma_cycle();
void wait_remainder_of_tdma_period(uint8_t old_cycle);

#endif
