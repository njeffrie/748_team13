#ifndef _TDMA_CONSTANTS_H
#define _TDMA_CONSTANTS_H

#define TDMA_SLOT_LEN 2100//us
#define PKT_LEN 9
#define INTER_SYNC_CYCLES 10000
#define NUM_NODES 5
/* we are group 13 */
#define CHAN 13

/* determines the number of transmissions per data slot... choose a value >= 1 */
/* !!! increases length of slot usage... can overflow if tdma slots are tight !!!*/
#define FLOOD_PROP_REDUNDANCY 1

#endif
