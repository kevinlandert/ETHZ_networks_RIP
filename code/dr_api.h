/**
 * Filename: dr_api.h
 * Purpose:  Define the API exported by the Dynamic Routing library.
 * Note:     All methods are implemented in a thread-safe manner.
 */

#ifndef _DR_API_H_
#define _DR_API_H_

#ifdef _LINUX_
#include <stdint.h>
#endif

#include "lvns_types.h"

/**
 * This function will be called before any other method here.  It may only be
 * called once.  The function pointer passed as an argument tells the DR API how
 * it can send packets.
 *     dst_ip         The ultimate desination of the packet.
 *     next_hop_ip    Next hop IP address (either a router or the ultimate dest)
 *     outgoing_intf  Index of the interface to send this packet out of
 *
 * This method initializes any data structures used internally by this library.
 * It may also start a thread to take care of periodic tasks.
 */
void dr_init(unsigned (*func_dr_interface_count)(),
             lvns_interface_t (*func_dr_get_interface)(unsigned index),
             void (*func_dr_send_payload)(uint32_t dst_ip,
                                          uint32_t next_hop_ip,
                                          uint32_t outgoing_intf,
                                          char* /* borrowed */,
                                          unsigned));

/**
 * Returns the next hop for packet destined to the specified (network-byte
 * order) IP.
 *
 * If and only if a next hop cannot be determined, then the dst_ip field of the
 * returned next_hop_t object will be 0xFFFFFFFF.
 */
next_hop_t dr_get_next_hop(uint32_t ip);

/**
 * Handles the payload of a dynamic routing packet (e.g. a RIP or OSPF payload).
 *
 * @param ip   The IP address which the dynamic routing packet is from.
 *
 * @param intf The index of the interface on which the packet arrived.
 *
 * @param buf  This is the payload of a packet in for the dynamic routing
 *             protocol.  The caller is reponsible for managing the memory
 *             associated with buf (e.g. this function will NOT free buf).
 *
 * @param len  The number of bytes in the payload.
 */
void dr_handle_packet(uint32_t ip, unsigned intf,
                      char* buf /* borrowed */, unsigned len);

/**
 * This method is called at a regular interval by a thread initialied by
 * dr_init.
 */

void dr_handle_periodic();

/**
 * This method is called when an interface is brought up or down and/or if its
 * cost is changed.
 *
 * @param intf             the index of the interface whose state has changed
 * @param state_changed    boolean; non-zero if the intf was brought up or down
 * @param cost_changed     boolean; non-zero if the cost was changed)
 */
void dr_interface_changed(unsigned intf, int state_changed, int cost_changed);





#endif /* _DR_API_H_ */
