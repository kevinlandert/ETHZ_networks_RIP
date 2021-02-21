/* Filename: dr_api.c */

/* include files */
#include <arpa/inet.h>  /* htons, ... */
#include <sys/socket.h> /* AF_INET */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "dr_api.h"
#include "rmutex.h"

/* internal data structures */
#define INFINITY 16

#define RIP_IP htonl(0xE0000009)

#define RIP_COMMAND_REQUEST  1
#define RIP_COMMAND_RESPONSE 2
#define RIP_VERSION          2

#define RIP_ADVERT_INTERVAL_SEC 10
#define RIP_TIMEOUT_SEC 20
#define RIP_GARBAGE_SEC 20

/** information about a route which is sent with a RIP packet */
typedef struct rip_entry_t {
    uint16_t addr_family;
    uint16_t pad;           /* just put zero in this field */
    uint32_t ip;
    uint32_t subnet_mask;
    uint32_t next_hop;
    uint32_t metric;
} __attribute__ ((packed)) rip_entry_t;

/** the RIP payload header */
typedef struct rip_header_t {
    char command;
    char version;
    uint16_t pad;        /* just put zero in this field */
    rip_entry_t entries[0];
} __attribute__ ((packed)) rip_header_t;

/** a single entry in the routing table */
typedef struct route_t {
    uint32_t subnet;        /* destination subnet which this route is for */
    uint32_t mask;          /* mask associated with this route */
    uint32_t next_hop_ip;   /* next hop on on this route */
    uint32_t outgoing_intf; /* interface to use to send packets on this route */
    uint32_t cost;
    long last_updated;   //CHANGED MYSELF

    int is_garbage; /* boolean which notes whether this entry is garbage */

    route_t *next;  /* pointer to the next route in a linked-list */
    route_t *previous; /*for double linked list */

} route_t;


/* internal variables */

/* a very coarse recursive mutex to synchronize access to methods */
static rmutex_t coarse_lock;

/** how mlong to sleep between periodic callbacks */
static unsigned secs_to_sleep_between_callbacks;
static unsigned nanosecs_to_sleep_between_callbacks;


/* these static functions are defined by the dr */

/*** Returns the number of interfaces on the host we're currently connected to.*/
static unsigned (*dr_interface_count)();

/*** Returns a copy of the requested interface.  All fields will be 0 if the an* invalid interface index is requested.*/
static lvns_interface_t (*dr_get_interface)(unsigned index);

/*** Sends specified dynamic routing payload.
 *** @param dst_ip   The ultimate destination of the packet.
 ** @param next_hop_ip  The IP of the next hop (either a router or the final dst).
 *** @param outgoing_intf  Index of the interface to send the packet from.
 ** @param payload  This will be sent as the payload of the DR packet.  The caller is reponsible for managing the memory associated with buf* (e.g. this function will NOT free buf).
 ** @param len      The number of bytes in the DR payload.*/
static void (*dr_send_payload)(uint32_t dst_ip,
                               uint32_t next_hop_ip,
                               uint32_t outgoing_intf,
                               char *buf /* borrowed */,
                               unsigned len);

/*own declarations*/

route_t *head;
route_t *tail;
unsigned int tablelength;
long lastsent;

//Own functions
static void makeroute_t(route_t *node, uint32_t ip, uint32_t subnet_mask, int cost, int interfnr, uint32_t next_hop_ip);

//static void clearup_table();
//static void addFirst(route_t* node);
static void addLast(route_t *node);

//static void getNode(route_t* node, int index);
//static void removeNode(route_t* node);
//static void removeFirst();
//static void removeLast();
//static void clear();
static void send_table();

void print_rippacket(uint32_t ip, unsigned intf, rip_entry_t *paket, int nrofentries);


/* internal functions */
long get_time();

void print_ip(int ip);

void print_routing_table(route_t *head);

/* internal lock-safe methods for the students to implement */
static next_hop_t safe_dr_get_next_hop(uint32_t ip);

static void safe_dr_handle_packet(uint32_t ip, unsigned intf,
                                  char *buf /* borrowed */, unsigned len);

static void safe_dr_handle_periodic();

static void safe_dr_interface_changed(unsigned intf,
                                      int state_changed,
                                      int cost_changed);

/*** This simple method is the entry point to a thread which will periodically* make a callback to your dr_handle_periodic method.*/
static void *periodic_callback_manager_main(void *nil) {
    struct timespec timeout;

    timeout.tv_sec = secs_to_sleep_between_callbacks;
    timeout.tv_nsec = nanosecs_to_sleep_between_callbacks;
    while (1) {
        nanosleep(&timeout, NULL);
        dr_handle_periodic();
    }

    return NULL;
}

next_hop_t dr_get_next_hop(uint32_t ip) {
    next_hop_t hop;
    rmutex_lock(&coarse_lock);
    hop = safe_dr_get_next_hop(ip);
    rmutex_unlock(&coarse_lock);
    return hop;
}

void dr_handle_packet(uint32_t ip, unsigned intf, char *buf /* borrowed */, unsigned len) {
    rmutex_lock(&coarse_lock);
    safe_dr_handle_packet(ip, intf, buf, len);
    rmutex_unlock(&coarse_lock);
}

void dr_handle_periodic() {
    rmutex_lock(&coarse_lock);
    safe_dr_handle_periodic();
    rmutex_unlock(&coarse_lock);
}

void dr_interface_changed(unsigned intf, int state_changed, int cost_changed) {
    rmutex_lock(&coarse_lock);
    safe_dr_interface_changed(intf, state_changed, cost_changed);
    rmutex_unlock(&coarse_lock);
}


/* ****** It is recommended that you only modify code below this line! ****** */


void dr_init(unsigned (*func_dr_interface_count)(),
             lvns_interface_t (*func_dr_get_interface)(unsigned index),
             void (*func_dr_send_payload)(uint32_t dst_ip,
                                          uint32_t next_hop_ip,
                                          uint32_t outgoing_intf,
                                          char * /* borrowed */,
                                          unsigned)) {
    pthread_t tid;

    /* save the functions the DR is providing for us */
    dr_interface_count = func_dr_interface_count;
    dr_get_interface = func_dr_get_interface;
    dr_send_payload = func_dr_send_payload;

    /* initialize the recursive mutex */
    rmutex_init(&coarse_lock);

    /* initialize the amount of time we want between callbacks */
    secs_to_sleep_between_callbacks = 1;
    nanosecs_to_sleep_between_callbacks = 0;

    /* start a new thread to provide the periodic callbacks */
    if (pthread_create(&tid, NULL, periodic_callback_manager_main, NULL) != 0) {
        exit(1);
    }

    /* do initialization of your own data structures here */

    head = NULL;
    tail = NULL;
    tablelength = 0;
    lastsent = 0;


    unsigned int intcount = dr_interface_count();


    //Create first routing table entries
    for (unsigned int i = 0; i < intcount; i++) {
        lvns_interface_t currInt = dr_get_interface(i);

        if (currInt.enabled) {
            route_t *node = (route_t *) malloc(sizeof(route_t));
            makeroute_t(node, currInt.ip, currInt.subnet_mask, currInt.cost, i, 0);
            node->last_updated = -1;
            addLast(node);
        }
    }

    printf("Routing table init");
    print_routing_table(head);


}

void makeroute_t(route_t *node, uint32_t ip, uint32_t subnet_mask, int cost, int interfnr, uint32_t next_hop_ip) {

    node->subnet = ip & subnet_mask;
    node->mask = subnet_mask;
    node->cost = cost;
    node->outgoing_intf = interfnr;
    node->is_garbage = 0;
    node->last_updated = get_time();
    node->next_hop_ip = next_hop_ip;   //
    node->next = NULL;
    node->previous = NULL;

}


next_hop_t safe_dr_get_next_hop(uint32_t ip) {



    /* determine the next hop in order to get to ip */

    /*
    if(ip == (uint32_t) 0x7B780000 ) {
        printf("Dodgy comparison\n\n\n\n");
        print_routing_table(head);
    }
    */


    uint32_t maxMlen = 0;
    route_t *lgst = NULL;
    route_t *currEntry = head;
    while (currEntry != NULL) {
        if (((currEntry->subnet & currEntry->mask) == ip) && currEntry->mask > maxMlen &&
            currEntry->cost < 16) {  //DARF ICH HIER OHNE 16 TEST
            maxMlen = currEntry->mask;
            lgst = currEntry;
        }
        currEntry = currEntry->next;

    }
    next_hop_t hophop;
    hophop.dst_ip = (maxMlen == 0) ? 0xFFFFFFFF : lgst->next_hop_ip;
    hophop.interface = (maxMlen == 0) ? 0 : lgst->outgoing_intf;
    return hophop;
}

void safe_dr_handle_packet(uint32_t ip, unsigned intf,
                           char *buf /* borrowed */, unsigned len) {
    /* handle the dynamic routing payload in the buf buffer */
    //ip = ntohl(ip);

    //Falls interface zu diesem router deaktiviert, table irrelevant
    if (!dr_get_interface(intf).enabled) {
        //free(buf);
        return;
    }

    bool tablechanged = false;
    unsigned intfc = dr_get_interface(intf).cost; //current interface cost


    rip_entry_t *payload = (rip_entry_t *) buf;


    int nrofentries = len / sizeof(rip_entry_t); //anzahl tabelleneitnräge

    rip_entry_t *entry = payload;

    printf("==============================\n");
    printf("Packet incomming...\n\n");

    //bool garbageset = false;


    for (int i = 0; i < nrofentries; i++) {


        route_t *current = head;
        bool addentry = true;


        while (current != NULL) {

            //Case 1: Entry in table
            if ((entry->subnet_mask & entry->ip) == (current->subnet & current->mask)) {
                addentry = false;

                //Case 1.1: If table received from interface which is outgoing interface of entry always adjust
                if (current->outgoing_intf == intf && current->next_hop_ip != 0) {

                    unsigned int oldcost = current->cost;

                    current->cost = (entry->metric + intfc >= 16) ? 16 : entry->metric + intfc; //aktualisiere kos
                    current->is_garbage = (current->cost == 16) ? 1 : 0;
                    //if(current->is_garbage)
                    //    garbageset = true;
                    current->last_updated = get_time();

                    if (oldcost != current->cost)
                        tablechanged = true; //eventuell too much

                }

                    //Case 1.2: If new route proposed than outgoing interface of current entry
                else if (current->outgoing_intf != intf) {

                    //Adjust only if route faster
                    if (entry->metric + intfc < current->cost) {
                        current->cost = entry->metric + intfc;
                        current->outgoing_intf = intf;
                        current->next_hop_ip = ip; //nicht immer nötig aber schadet nicht
                        current->last_updated = get_time();
                        current->is_garbage = 0;
                        tablechanged = true;

                    }

                }
                break;

            }
            current = current->next;
        }

        //Case 2:If destination not yet in table //nur anfügen falls total kosten <= 15
        if (addentry && (entry->metric + intfc <= 15)) {
            route_t *node = (route_t *) malloc(sizeof(route_t));
            makeroute_t(node, entry->ip, entry->subnet_mask, entry->metric + intfc, intf, ip);
            addLast(node);
            tablechanged = true;
        }
        entry++;

    }


    if (tablechanged) {
        //printf("==============================\n");
        printf("Table has changed!\n\n");
        printf("Packet that changed table: \n");
        print_rippacket(ip, intf, payload, nrofentries);
        printf("==============================\n");
        printf("Routing table after receiving paket:\n");
        print_routing_table(head);
        printf("==============================\n");
    } else {
        //printf("==============================\n");
        printf("Table has not changed!\n");
        printf("==============================\n");
    }

    //free(buf);
    //sende aktualisierten table
    if (tablechanged) {
        send_table();
    }


    //if(garbageset)
     //   clearup_table();

    //free((rip_entry_t*) buf);
    //buf = NULL;
}

//Implement
/*
void clearup_table() {
    printf("Clearup called\n");
    printf("Tablelength %d\n",tablelength);
    print_routing_table(head);
    int counter = 0;

    route_t* current = head;
    while (current != NULL) {
        if (current->is_garbage) {
            printf("Entry %d delete\n",counter);
            counter++;
            removeNode(current);
            current = head;
            counter = 0;
        } else {
            printf("Entry %d not delete\n",counter);
            counter++;
            current = current->next;
        }
    }

    printf("Clearup returned\n");

}
 */

/**
void addFirst(route_t* node) {
    if(tablelength == 0) {
        tail = node;
    } else {
        head->previous = node;
        node->next = head;
    }
    head = node;
    tablelength++;
}

**/

void addLast(route_t *node) {
    if (tablelength == 0) {
        head = node;

    } else {
        tail->next = node;
        node->previous = tail;
    }
    tail = node;
    tablelength++;
}

/**
void getNode(route_t* node, int index) {
    if(index >= tablelength);
        //assert(1 == 0);
    route_t* current = head;
    for (int i = 0; i < index; i++) {
        current = current->next;
    }
    node = current;
}

void removeNode(route_t* node) {
    if(tablelength == 1) {
        printf("Case 0\n");
        head = NULL;
        tail = NULL;
        tablelength = 0;
        free(node);
        node = NULL;
    } else if (node == head) {
        printf("Case 1\n");
        head = node->next;
        head->previous = NULL;
        free(node);
        node = head;
        tablelength--;

    } else if(node == tail) {
        printf("Case 2\n");
        tail = node->previous;
        tail->next = NULL;
        free(node);
        node = NULL;
        tablelength--;
    } else {
        printf("Case 3\n");
        node->previous->next = node->next;
        route_t* fonext = node->next;
        node->next->previous = node->previous;
        free(node);
        node = fonext;
        tablelength--;
    }
}

void removeFirst() {
    if (head == tail)
        clear();
    else {
        head = head->next;
        free(head->previous);
        head->previous = NULL;
        tablelength--;
    }
}
void removeLast() {
    if (head == tail)
        clear();
    else {
        tail = tail->previous;
        free(tail->next);
        tail->next = NULL;
        tablelength--;
    }
}

void clear() {
    free(head);
    head = NULL;
    tail = NULL;
    tablelength--;
}

*/


//Implement
static void send_table() {


    unsigned int intfcount = dr_interface_count();

    //Erstelle für jedes Interface das Routing table und schickt dieses raus
    for (unsigned int j = 0; j < intfcount; j++) {

        lvns_interface_t currInt = dr_get_interface(j);
        if (currInt.enabled) {

            rip_entry_t *payload = (rip_entry_t *) malloc(tablelength * sizeof(rip_entry_t));
            route_t *current = head;


            rip_entry_t *entry = payload;
            while (current != NULL) {
                entry->ip = current->subnet;
                entry->subnet_mask = current->mask;
                entry->metric = current->cost >= 16 ? 16 : current->cost; //+ currInt.cost;
                entry->next_hop = current->next_hop_ip;
                entry->pad = 0;


                //Poised reverse
                if (current->outgoing_intf == j && current->next_hop_ip != 0) {
                    entry->metric = 16;
                }


                entry++;  //funktioniert das so? wahsch memcpy
                current = current->next;
            }
            // print_routing_table(head);

            int size = tablelength * sizeof(rip_entry_t);


            dr_send_payload(RIP_IP, RIP_IP, j, (char *) payload, size);
            printf("Packet leaving\n\n");
            //print_rippacket(RIP_IP,j,payload,tablelength);
        }

    }

}

void safe_dr_handle_periodic() {
    /* handle periodic tasks for dynamic routing here */
    //printf("==============================\n");
    //printf("Periodic call!\n\n");



    bool send = false;
    //bool callclearup = false;

    //If timer run out set destination to unreachable
    route_t *curr = head;
    while (curr != NULL) {
        long now = get_time();

        //Timeout only if not directly connected ?
        if (curr->last_updated + RIP_TIMEOUT_SEC * 1000 < now && curr->last_updated != -1) {

            //Check if good way directly connected instead when timeout
            bool bad = true;
            int intcount = dr_interface_count();
            for (int i = 0; i < intcount; i++) {
                lvns_interface_t currInt = dr_get_interface(i);
                if (currInt.enabled && (currInt.subnet_mask & currInt.ip) == (curr->subnet & curr->mask) &&
                    currInt.cost < 16) {
                    curr->last_updated = -1;
                    curr->is_garbage = 0;
                    curr->outgoing_intf = i;
                    curr->next_hop_ip = 0;
                    bad = false;
                    break;
                }
            }
            if (bad) {
                curr->cost = 16;
                curr->last_updated = now;
                send = true;
            }
        }
        if (curr->cost == 16) {
            curr->is_garbage = 1;
            //callclearup = true;
        }


        curr = curr->next;
    }

    //If more than 10s passed since las periodic update
    long now = get_time();
    if (lastsent + RIP_ADVERT_INTERVAL_SEC * 1000 < now) {
        send = true;
        lastsent = now;
    }

    if (send) {
        printf("Periodic sending packet!\n\n");
        send_table();
        printf("Current table:!\n\n");
        print_routing_table(head);
    }

    /*
    if(callclearup)
        clearup_table();
    */


}

static void safe_dr_interface_changed(unsigned intf,
                                      int state_changed,
                                      int cost_changed) {
    /* handle an interface going down or being brought up */


    lvns_interface_t interfa = dr_get_interface(intf);

    bool send = false;
    bool addEntry = true;

    //Case 1: State Changed
    if (state_changed) {

        //Case 1.1: If now turned off
        if (!interfa.enabled) {
            addEntry = false;
            printf("Interface down - NR: %d IP: ", intf);
            print_ip(interfa.ip);

            route_t *node = head;
            for (unsigned int i = 0; i < tablelength; i++) {

                //Set all destinations that hat current interface as outgoing hop to unreachable
                if (node->outgoing_intf == intf) {
                    node->cost = 16;
                    node->is_garbage = 1;
                   // garbageset = true;
                    node->last_updated = get_time();
                    send = true;

                }
                node = node->next;

            }
            //Case 1.2: If now turned on
        } else {

            printf("Interface up - NR: %d IP: ", intf);
            print_ip(interfa.ip);

            route_t *node = head;
            for (unsigned int i = 0; i < tablelength; i++) {

                //Check for this interface if now faster with direct connection
                if ((node->subnet & node->mask) == (interfa.ip & interfa.subnet_mask)) {
                    addEntry = false; //Entry in table

                    if (interfa.cost < node->cost) {
                        node->outgoing_intf = intf;
                        node->cost = interfa.cost;
                        node->next_hop_ip = 0;
                        node->last_updated = get_time();
                        node->mask = interfa.subnet_mask;
                        send = true;
                        break;
                    }

                }
                node = node->next;

            }


        }
        //Case 2: Cost changed
    } else if (cost_changed) {
        route_t *node = head;
        unsigned int oldcost = 0;




        //get old cost to reach this subnet
        while (node != NULL) {

            if ((interfa.ip & interfa.subnet_mask) == (node->subnet & node->mask)) {
                oldcost = node->cost;
                addEntry = false;
                break;
            }
            node = node->next;

        }

        printf("Interface cost change - NR: %d IP: ", intf);
        print_ip(htonl(interfa.ip));
        printf("Oldcost:  %d NewCost: %d\n", oldcost, interfa.cost);



        //Go through all nodes that have the interface that has changed as outgoing and adjust costs
        node = head;
        while (node != NULL) {
            if (node->outgoing_intf == intf) {
                if (node->next_hop_ip != 0)
                    node->cost -= (oldcost - interfa.cost);//lower costs by difference between old costs and new costs
                else
                    node->cost = interfa.cost;
                node->last_updated = get_time();
                send = true;
                if (node->cost >= 16) {
                    node->cost = 16;
                    node->is_garbage = 1;
                    //garbageset = true;
                }
            }
            node = node->next;

        }

        //Go through all directly connected subnets and adjust costs
        unsigned int intcount = dr_interface_count();


        //Create first routing table entries
        for (unsigned int i = 0; i < intcount; i++) {
            lvns_interface_t currInt = dr_get_interface(i);

            if (currInt.enabled) {

                node = head;
                while (node != NULL) {
                    if (((currInt.ip & currInt.subnet_mask) == (node->subnet & node->mask)) &&
                        currInt.cost < node->cost) {
                        node->next_hop_ip = 0;
                        node->outgoing_intf = i;
                        node->cost = currInt.cost;
                        node->last_updated = -1;
                        node->is_garbage = 0;
                        send = true;
                        printf("Special case set");
                    }
                    node = node->next;
                }

            }
        }


    }

    //If not found in table then add
    if (addEntry) {
        route_t *node = (route_t *) malloc(sizeof(route_t));
        makeroute_t(node, interfa.ip, interfa.subnet_mask, interfa.cost, intf, 0);
        addLast(node);
        send = true;
    }
    print_routing_table(head);
    if (send) {
        send_table();
    }
    /*
    if(garbageset)
        clearup_table();
    */







}

/* definition of internal functions */

// gives current time in milliseconds
long get_time() {
    // Now in milliseconds
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000 + now.tv_usec / 1000;
}

// prints an ip address in the correct format
// this function is taken from: 
// https://stackoverflow.com/questions/1680365/integer-to-ip-address-c 
void print_ip(int ip) {
    unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
    printf("%d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
}

// prints the full routing table
void print_routing_table(route_t *head) {
    printf("==================================================================\nROUTING TABLE:\n==================================================================\n");
    int counter = 0;
    route_t *current = head;
    while (current != NULL) {
        printf("Entry %d:\n", counter);
        printf("\tSubnet: ");
        print_ip(htonl(current->subnet));
        printf("\tMask: ");
        print_ip(htonl(current->mask));
        printf("\tNext hop ip: ");
        print_ip(htonl(current->next_hop_ip));
        printf("\tOutgoing interface: ");
        print_ip(htonl(current->outgoing_intf));
        printf("\tCost: %d\n", current->cost);
        printf("\tLast updated (timestamp in microseconds): %li \n", current->last_updated);
        printf("\tGarbage: %d\n", current->is_garbage);

        printf("==============================\n");
        counter++;

        current = current->next;
    }
}

void print_rippacket(uint32_t ip, unsigned intf, rip_entry_t *paket, int nrofentries) {
    printf("==================================================================\nPackets:\n==================================================================\n");
    printf("Incomming IP: ");
    print_ip(htonl(ip));
    printf("Incomming Interface Nr: ");
    print_ip(htonl(intf));
    printf("\n");

    rip_entry_t *entry = paket;
    int counter = 0;

    for (int i = 0; i < nrofentries; i++) {

        printf("Entry %d:\n", counter);
        printf("Packet IP: ");
        print_ip(htonl(entry->ip));
        printf("Packet Mask: ");
        print_ip(htonl(entry->subnet_mask));
        printf("Packet NextHop IP: ");
        print_ip(htonl(entry->next_hop));
        printf("Packet Matric: %d\n\n", entry->metric);
        counter++;

        entry++;
    }


}

