/*
 * Filename: lvns_types.h
 * Purpose: Various LVNS data structures.
 */

#ifndef _LVNS_TYPES_H_
#define _LVNS_TYPES_H_

#ifdef _LINUX_
#include <stdint.h>
#endif

/** holds info about a router's interface */
typedef struct lvns_interface_t {
    uint32_t ip;            /* IP address of the interface */
    uint32_t subnet_mask;   /* subnet mask of the link     */
    char enabled;           /* whether the interface is on */
    unsigned cost;          /* cost of sending a packet from this interface */
} lvns_interface_t;

/** information about how to route a packet */
typedef struct next_hop_t {
    /** the index of the interface to send the packet out; e.g. 0 => eth0 */
    uint32_t interface;

    /** IP address of the next hop */
    uint32_t dst_ip;
} __attribute__ ((packed)) next_hop_t;

#endif /* _LVNS_TYPES_H_ */
