/******************************************************************************
** MIT License
**
** Copyright (c) 2023 Andrey Tsigler
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
******************************************************************************/
/******************************************************************************
** LATEST REVISION/AUTHOR
** March-30-2023/Andrey Tsigler
******************************************************************************/
#ifndef HASH_INTERNAL_H_INCLUDED
#define HASH_INTERNAL_H_INCLUDED
#include <sys/types.h>
#include <pthread.h>
#include "lpmht-api.h"
#include "lpmht-util.h"

/* The hash implementation creates a single hash table, with the masked IP address and 
** prefix size used to compute the hash key. Each bucket in the hash table is 
** 4 bytes, and contains an index into IPv4 or IPv6 route database. To reduce 
** collisions, the number of buckets in the hash table is five times the 
** the current number of routes in the hash table. To conserve memory the 
** number of buckets per route can be reduced. 
*/
#define LPMHT_HASH_FACTOR 5

/* The longest prefix match algorithm first examines the hash table with IP
** address masked with the longest prefix. If the route is not found then
** the algorithm examines the next longest prefix, and so on until the 
** route is found or the algorithm reaches prefix length 0.
**
** This algorithm is very fast when the routing table contains prefixes with
** only a few prefix lengths, but is significantly slowed down when a lot of 
** different prefix lengths routes are present in the route table, potentially 
** requiring the algorithm to check 33 times for IPv4 and 129 times for IPv6.
*/

/* The prefix is hashed using the FNV hash algorithm,
** or hardware assisted CRC function on platforms that support it.
** The hardware assisted hash function tends to cause more collisions than 
** the FNV function, but the performance is still much better, especially 
** for IPv6, since the IPv6 address is 16 bytes.
**
** The x86 and the ARM64 support hardware assisted CRC computation. 
** To disable the hardware assisted hash function on platforms that support it,
** set the below compiler define to 0. To enable hardware assist, set 
** the value to 1.
*/
#define LPMHT_HASH_HARDWARE 1

/* Each entry in the hash table contains a 4-byte index pointing to 
** the route entry. The hash table size varies depending on the total 
** number of routes in the routing table. Initially the hash table size is set 
** to the below compiler define. The value refers to the number of route
** indexes that can fit into the hash, so when the size is set to 100,000
** the number of bytes allocated in the block is 4x100,000 = 400,000.
*/
#define LPMHT_HASH_BLOCK_SIZE (20000 * LPMHT_HASH_FACTOR)

/* When the number of routes exceeds the block size, the hash table is 
** increased in size by another block, and all the routes are re-hashed.
** When the number of routes is reduced by 
** two times the hash block size, the hash table size is reduced and the
** routes are re-hashed. This approach optimizes the use of physical 
** memory to only allocate enough for the required routes. The virtual memory
** is pre-allocated to accommodate the maximum number of routes.
**
** Note that rehashing the hash table can take 15 milliseconds
** when one million routes are in the route table, and 150 milliseconds
** when there are 10 million routes in the route table. The 
** route table lookups are blocked while the hash table is updated,
** which can cause some packets to have 150 millisecond latency 
** with 10 million routes in the route table. The latency can 
** vary based on the platform CPU speed and memory speed.
**
** Applications that can't tolerate 150ms interruptions in packet flow
** can pass the LPMHT_OPT_HASH_PREALLOC flag during route table creation.
** This flag causes the hash table for the maximum routes to be allocated 
** in physical memory when the route table is created, so no re-hashing is
** ever necessary. The memory required for the pre-allocated hash table
** is the maximum number of routes multiplied by the LPMHT_HASH_FACTOR
** multiplied by 4. For example for 10 million routes it is 10M * 5 * 4,
** which is about 200MB.
** Pre-allocating the hash table also has the benefit of faster 
** route insertion. The insertion time can stretch to tens of seconds
** with 10 million routes without hash table pre allocation.
*/

/* Number of flows in the IPv6 flow table. 
** Each flow entry is 32 bytes.
*/
#define IPV6_DEFAULT_FLOW_COUNT  (2*1024*1024)

/* Flow Age Time is the number of seconds the flow age thread sleeps between
** checking whether any flows need to be removed.
** The flow removal time is twice the Flow Age Time.
*/
#define IPV6_DEFAULT_FLOW_AGE_DISPATCH_TIME 30

typedef struct {
  /* Atomic Lock. The lock is obtained for all operations (lookup/learning/ageing).
  ** The callers never block on the lock. If the lock is not 
  ** available then the callers simply skip whatever operation they were
  ** planning to do.
  ** This approach avoids deadlocks, which can happen with blocking spin locks
  ** when different priority real time threads (SCHED_RR/SCHED_FIFO) attempt
  ** to access the lock. 
  */
  unsigned char entry_lock; 
  unsigned char flow_detected; /* Detected route lookup for this entry. */
  unsigned char pad1; /* Unused */
  unsigned char pad2; /* Unused */
  unsigned int  pad3; /* Unused */
  unsigned int  route_index; /* Route entry corresponding to the flow. (0-Unused) */
  unsigned int  route_flow_correlator; /* Must match route table correlator for flow to be used */
  unsigned __int128 ipv6_addr; /* Destination IPv6 address */
} ipv6FlowTable_t __attribute__ ((aligned (sizeof (unsigned __int128))));

/* This structure defines IPv4 route info. 
** Note that multiple routes can hash into the same hash table index, 
** so the next/prev manage the linked list of entries with the same hash index.
*/
typedef struct
{
  unsigned char prefix_size;
  unsigned int ipv4_addr;
  unsigned int prev; 
  unsigned int next;


  unsigned long long hit_count;
  unsigned long user_data;

} hashIpv4Route_t __attribute__ ((aligned (sizeof(unsigned long long))));

/* This structure defines IPv6 route info. 
*/
typedef struct
{
  unsigned __int128 ipv6_addr;
  unsigned char prefix_size;
  unsigned int prev; 
  unsigned int next;


  unsigned long long hit_count;
  unsigned long user_data;

} hashIpv6Route_t __attribute__ ((aligned (sizeof(unsigned __int128))));

/* The hash route table. 
*/
typedef struct 
{
  /* Read/Write lock to protect access to the Hash routing table.
  */
  lpmhtRwlock_t hash_rwlock;
#define HASH_WRITE_LOCK(hash)  lpmhtRwlockWrLock(&(hash)->hash_rwlock)
#define HASH_READ_LOCK(hash)  lpmhtRwlockRdLock(&(hash)->hash_rwlock)
#define HASH_UNLOCK(hash)  lpmhtRwlockUnlock(&(hash)->hash_rwlock)


  unsigned int max_routes;
  unsigned int enable_hit_count;
  lpmhtIpMode_e ip_mode;

  /* Total number of route entries currently inserted in the hash
  ** route table.
  */
  unsigned int num_routes;

  /* When this flag is not zero, the physical memory for the whole hash table is
  ** allocated when the table is created.
  */
  unsigned int mem_prealloc;

  /* Total number of bytes of physical memory allocated for this routing table.
  ** This includes routes, lookup tables, and the hash table.
  */
  size_t memory_size;

  /* Number of bytes of virtual memory reserved for the routing table 
  ** constructs.
  */
  size_t virtual_memory_size;

  /* Number of routes for each prefix. We reserve 129 entries, but 
  ** only use 33 for IPv4.
  */
  unsigned int num_routes_in_prefix[129];

  /* Sorted list of prefixes for which there are routes present.
  ** The highest prefix size is in element 0.
  */
  unsigned char active_prefix_list[129];

  /* Number of active prefixes.
  */
  unsigned int num_active_prefixes;

  /* Memory allocated for IPv4 route table.
  */
  hashIpv4Route_t *ipv4_route;

  /* Memory allocated for IPv6 route table.
  */
  hashIpv6Route_t *ipv6_route;

  /* Memory block to help manage the route table.
  */
  memoryBlock_t route_mb;

  /* Hash Table. Each entry in this table is an integer containing an 
  ** index of the appropriate route in the route table.
  */
  unsigned int *hash_table;

  /* Memory block to help manage the hash table.
  */
  memoryBlock_t hash_mb;

  /* Flag indicating that the memory for all routes should be pre-allocated in the
  ** hash table when the routing table is created. This avoids re-hashing the table
  ** and eliminates the route lookup delays due to re-hashing.
  ** Note that only the hash table memory is pre-allocated. The memory management
  ** scheme for the route table is not affected by this flag. 
  */
  unsigned int reserve_hash_memory;

  /* maximum number of blocks in hash.
  */
  unsigned int max_blocks_in_hash;

  /* current number of blocks in hash.
  */
  unsigned int num_blocks_in_hash;

  /* Maximum route indexes that can currently fit into the hash_table.
  ** This value is equal to num_blocks_in_hash multiplied by 
  ** LPMHT_HASH_BLOCK_SIZE.
  */
  unsigned int hash_table_size;

  /***************************************************************************
  ** IPv4 Rule Table Management Data
  ***************************************************************************/
  /* Flag indicating that IPv4 rule generation feature is enabled.
  */
  unsigned int ipv4_rules_enabled;
  pthread_t ipv4_rules_generator; /* Thread ID of the rule generator thread. */

  /* This flag is set to 1 whenever a new route is added or an existing
  ** route is deleted.
  */
  unsigned int ipv4_new_rules_needed;

  /* This flag is set to 1 when the rule generation is done, and the rules
  ** are ready to be used for routing decisions. The flag is cleared to 0
  ** whenever a new route is added or an existing route is deleted.
  */
  unsigned int ipv4_rules_ready;

  /* The rule table is a 64MB memory block containing an entry for every 24-bit
  ** network mask. When the IPv4 rules feature is enabled, the longest prefix 
  ** match function first checks the hash tables for prefix lengths 32 through 
  ** 25. If the incoming IPv4 address doesn't match any of the 25-32 bit routes
  ** then the algorithm performs a direct lookup in the IPv4 rule table using 
  ** the first three bytes of the IP address. The rule table contains either 
  ** a 0, which means there is no matching route, or an index of the matching
  ** IPv4 route entry.
  */
  unsigned int *ipv4_rule_table;
  
  /***************************************************************************
  ** IPv6 Flow Table Management Data
  ***************************************************************************/
  /* Flag indicating that IPv6 flow table is enabled.
  */
  unsigned int ipv6_flow_enabled;
  pthread_t ipv6_flow_ageing_thread;

  /* Maximum number of flows that can be added to the flow table.
  */
  unsigned int ipv6_max_flows;

  /* The number of seconds between flow ageing passes through the flow table.
  */
  unsigned int flow_age_dispatch_time;

  /* This flag is incremented whenever a new route is added to the routing 
  ** table. 
  */
  unsigned int route_flow_correlator;

  /* This counter is incremented whenever a flow lookup doesn't find
  ** a matching flow.
  */
  unsigned long long *ipv6_flow_not_found;

  /* Memory for the flow table. If the LPMHT_OPT_IPV6_FLOW option is enabled
  ** then The flow table is allocated in physical memory when the routing
  ** table is created.
  */
  ipv6FlowTable_t *ipv6_flow_table;

} sharedHash_t;


sharedHash_t *hashCreate (unsigned int max_routes,
                          lpmhtIpMode_e ip_mode,
                          lpmhtTableProp_t *prop);

void hashDestroy (sharedHash_t *route_table);

int hashRouteInsert (sharedHash_t *route_table,
                unsigned char *prefix,
                unsigned char  prefix_size,
                unsigned long user_data);

int hashDelete (sharedHash_t *route_table,
                unsigned char *prefix,
                unsigned char prefix_size);

int hashSet (sharedHash_t *route_table,
                unsigned char *prefix,
                unsigned char prefix_size,
                unsigned long user_data);

int hashGet (sharedHash_t *route_table,
                unsigned char *prefix,
                unsigned char prefix_size,
                unsigned long *user_data,
                unsigned int clear_hit_count,
                unsigned long long *hit_count);

int hashLPMatch (const sharedHash_t *route_table,
                const unsigned char *prefix,
                unsigned char *prefix_size,
                unsigned long *user_data);


#endif /* HASH_INTERNAL_H_INCLUDED */
