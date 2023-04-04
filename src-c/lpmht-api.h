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
** March-29-2023/Andrey Tsigler
******************************************************************************/
#ifndef LPMHT_API_H_INCLUDED
#define LPMHT_API_H_INCLUDED
#include <stddef.h>

/* Maximum number of routes supported by lpmht.
** These are test limitations, and can be increased if needed.
** The Trie route table limit is lower because the Trie structure consumes a 
** lot of memory.
*/
#define LPMHT_MAX_TRIE_ROUTES 2000000
#define LPMHT_MAX_HASH_ROUTES 10000000

/* IPv4/IPv6 route table type selector.
*/
typedef enum
{
  LPMHT_IPV4 = 0,
  LPMHT_IPV6
} lpmhtIpMode_e;

/* Binary Trie Tree/Hash Table route management selector.
*/
typedef enum
{
  LPMHT_TRIE = 0,
  LPMHT_HASH
} lpmhtTableMode_e;

/* Route table diagnostic information returned by the lpmhtRouteTableInfoGet().
*/
typedef struct
{
  /********************************************************** 
  ** These fields are returned for all table types.
  */

  /* Total number of routes currently in the routing table.
  */
  unsigned int num_routes;

  /* Total Number of bytes allocated to all routing table 
  ** constructs in physical memory.
  */
  size_t mem_size;

  /* Total number of bytes allocated in virtual memory.
  */
  size_t virtual_mem_size;

  /*********************************************************** 
  ** These fields are only applicable to Trie route tables.
  */

  /* Number of nodes in the binary trie tree currently allocated in 
  ** physical memory. 
  */
  unsigned int num_nodes;

  /*********************************************************** 
  ** These fields are only applicable to Hash route tables.
  */

  /* When the IPv4 rule table feature is enabled the flag indicates
  ** whether the rule table is currently in use.
  */
  unsigned int ipv4_rule_table_ready:1;

  /* When the IPv6 flow table feature is enabled the counter reports
  ** the number of route lookups for which a flow was not found.
  */
  unsigned long long ipv6_flow_not_found;

} lpmhtTableInfo_t;

/* Route Table Properties. 
** This structure is passed on the lpmhtRouteTableCreate(). 
*/
typedef struct
{
    /* This property enables hit count tracking for routes. The feature is 
    ** supported for Trie and Hash route tables. The hit count is incremented
    ** every time the lpmhtLPMatch() function matches the route.
    ** The feature slows down route lookups by about 1%.
    */
    unsigned int hit_count:1;

    /* This property enables support for the lpmhtRouteFirstGet() and 
    ** lpmhtRouteNextGet() functions. The feature is supported for Trie 
    ** and Hash routing tables. For IPv4 the feature adds 40 bytes per route.
    ** For IPv6 the feature adds 56 bytes per route. The memory is allocated
    ** as the routes are added, and memory is freed as routes are deleted.
    ** When the feature is enabled, the routes are stored in an AVL tree 
    ** sorted by prefix length and prefix value. The routes with the longest
    ** prefix length and the lowest prefix value are reported first. 
    */
    unsigned int next_get:1;

    /* In some embedded systems the applications want to allocate all memory
    ** at start-up. This is useful in order to know how much memory a fully loaded 
    ** system will need, and to avoid any out of memory errors during system
    ** operation. Setting the mem_prealloc to 1 is supported for Trie and Hash
    ** routing tables and causes the physical memory for maximum routes to be 
    ** allocated when the routing table is created.
    */
    unsigned int mem_prealloc:1;

    /* This property pre-allocates the hash table for maximum number of routes. 
    ** The feature is supported only for hash routing tables. 
    ** The purpose of this feature is to eliminate interruptions in route
    ** lookups when the hash table size is increased and the routes must be re-hashed.
    ** With 1 million routes the rehash time can easily reach 10ms, while with 
    ** 100 million routes the rehash time is over 100ms. In some networks interrupting
    ** traffic forwarding for tens of milliseconds is not acceptable.
    ** The hash table size is LPMHT_HASH_FACTOR * 4 * max-routes, so for 1 million 
    ** routes the lpmht allocates about 20MB of physical memory when the route table
    ** is created.
    */
    unsigned int hash_prealloc:1;

    /* This property improves route lookup rate for IPv4 routes by using a rule table.
    ** This feature is supported only for IPv4 hash route tables.
    ** When this feature is enabled, a 64MB physical memory block is allocated when 
    ** the route table is created. Also a thread is created to maintain the rule table.
    ** When the rule table is active, the longest prefix match is done as usual for 
    ** routes with prefix lengths of 32 to 25. However, if the IP address doesn't
    ** match any of these routes then a direct lookup in the IPv4 rule table is 
    ** done using the first three bytes of the IP address. 
    ** The rule table must be recomputed every time a route is added or deleted. 
    ** The rule table recalculation is akin to performing 16 million route lookups,
    ** which can take several seconds. While the rule table is being recalculated
    ** the lpmht uses the hash table to make routing decisions, so the route lookups
    ** slow down, but are not stopped while the rule table is recreated.
    */
    unsigned int ipv4_rules:1;

    /* This property may improve performance in some IPv6 networks by creating a flow
    ** table based on the destination IPv6 address. The feature is supported only 
    ** for IPv6 hash tables. By default, the feature allocates 64MB of physical memory
    ** for 2 million flows when the route table is created. The number of flows can be 
    ** adjusted using the ipv6_max_flows property. The feature also creates a thread
    ** for ageing the flows. The age time is 30 seconds, but can be changed by the 
    ** ipv6_flow_age_time property. 
    **
    ** The feature provides best results in networks where the number of flows is at least
    ** four times larger than the number of destination IPv6 addresses transiting the 
    ** router. This is because the flow table is hashed using the destination IPv6 address,
    ** and the larger flow table avoids hash collisions. 
    ** Enabling this feature slows down route lookups for destination IP addresses that 
    ** don't match the flow table, so care should be taken to enable the feature only 
    ** in networks where it is actually helpful.
    */ 
    unsigned int ipv6_flow:1; 

    /* When the ipv6_flow is enabled, this property indicates the number 
    ** of flows the router supports. If the value is 0 then the default
    ** number of flows IPV6_DEFAULT_FLOW_COUNT is used.
    ** Each flow takes 32 bytes, and the physical memory is allocated when the 
    ** route table is created.
    */
    unsigned int ipv6_max_flows;

    /* When the ipv6_flow is enabled, this property indicates the number 
    ** of seconds between dispatches of the flow ageing loop. The flow
    ** ageing loop looks at every flow and clears the flow_detected flag.
    ** If the flow_detected flag is already cleated then the flow is marked
    ** as unused.
    ** If this property is set to zero then the IPV6_DEFAULT_FLOW_AGE_DISPATCH_TIME
    ** is used for ageing the flow table.
    */
    unsigned int ipv6_flow_age_time;
} lpmhtTableProp_t;


/* Externally visible Route Table type.
** This type is opaque, so variables of this type can only be 
** declared as pointers.
*/
typedef struct lpmhtRouteTable_s lpmhtRouteTable_t;

/******************************************************************************
** This file contains APIs to manage Longest Prefix Match (LPM) routing tables.
** The routing tables are designed to store IPv4 and IPv6 routes.
******************************************************************************/

/* Create a new IPv4 or IPv6 routing table.
** The user must create separate tables for storing IPv4 and IPv6 
** routes.
** Devices that support multiple router instances must create 
** a separate routing table for each router instance.
**
** max_routes - The maximum number of IP routes in the table.
**              The function allocates virtual memory for the routes,
**              but doesn't allocate physical memory until the route
**              is added to the routing table. Therefore this value
**              can be very big without impacting the physical memory usage.
** ip_mode - Select whether this table holds IPv4 or IPv6 routes.
** table_mode - Select whether this table keeps routes in a Trie tree,
**              or a hash table.
** prop - Route Table Properties. The caller may pass a NULL pointer
**        to the prop structure. This is equivalent to setting 
**        all properties to 0.
**        Properties that are not applicable to the specified route 
**        table type are silently ignored.
**
** Return Values:
**  Pointer to the newly created routing table.
**  0 - Table creation error.
*/
lpmhtRouteTable_t *lpmhtRouteTableCreate (unsigned int max_routes,
                lpmhtIpMode_e ip_mode,
                lpmhtTableMode_e table_mode,
                lpmhtTableProp_t  *prop);

/* Free all resources associated with the previously created routing table.
** This function is NOT thread safe. In other words the application must 
** make sure that all threads stop using the routing table before
** destroying the table.
**
**  route_table -  The pointer to the route table.
**
** Return Values:
**  0 - Tree is destroyed successfully.
** -1 - Error.
*/
int lpmhtRouteTableDestroy (lpmhtRouteTable_t *route_table);

/* Insert the specified route into the route table. 
**  route_table - The pointer to the route table.
**  prefix - The pointer to the route. The route must be in 
**          network byte order. 
**  prefix_size - Number of bits in the network mask for 
**               this route. 
**  user_data - The data to be associated with the route.
**
** Return Values:
**  0 - Route is inserted successfully.
** -1 - Error in input parameters.
** -2 - Route already exists.
** -3 - Exceeded the maximum number of route entries.
*/
int lpmhtRouteAdd (lpmhtRouteTable_t *route_table, 
                unsigned char *prefix,
                unsigned int  prefix_size,
                unsigned long user_data);

/* Delete the specified route from the route table.
**  route_table - The pointer to the route table.
**  prefix - The pointer to the route. The route must be in 
**          network byte order. 
**  prefix_size - Number of bits in the network mask for 
**               this route. 
**
** Return Values:
**  0 - Route is deleted successfully.
** -1 - Error in input parameters.
** -2 - Route doesn't exists.
*/
int lpmhtRouteDelete (lpmhtRouteTable_t *route_table, 
                unsigned char *prefix,
                unsigned int prefix_size);

/* Modify existing route in the route table.
**  route_table - The pointer to the route table.
**  prefix - The pointer to the route. The route must be in 
**          network byte order. 
**  prefix_size - Number of bits in the network mask for 
**               this route. 
**  user_data - The data to be associated with the route.
**
** Return Values:
**  0 - Route is modified successfully.
** -1 - Error in input parameters.
** -2 - Route doesn't exists.
*/ 
int lpmhtRouteSet (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long user_data);

/* Get the specified route in the route table.
**  route_table - The pointer to the route table.
**  prefix - The pointer to the route. The route must be in 
**          network byte order. 
**  prefix_size - Number of bits in the network mask for 
**               this route. 
**  user_data - (output) The data associated with the route.
**  clear_hit_count - 1-Reset the hit count to zero. 0-Don't
**                    clear the hit count.
**  hit_count - (output) The hit count for this entry. 
**              If the pointer is 0 then the hit count is not returned.
**
**
** Return Values:
**  0 - Route is found successfully.
** -1 - Error in input parameters.
** -2 - Route doesn't exists.
*/ 
int lpmhtRouteGet (lpmhtRouteTable_t *route_table, 
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long *user_data,
                unsigned int clear_hit_count,
                unsigned long long *hit_count);

/* Get the lowest route in the route table.
**  route_table - The pointer to the Trie tree.
**  prefix - (output) The pointer to the prefix. The returned prefix is 
**          in network byte order. 
**          The memory for the prefix is allocated by the caller and 
**          must be 4 bytes for IPv4 and 16 bytes for IPv6.
**  prefix_size - (output) Number of bits in the network mask for 
**               this route. 
**  user_data - (output) The data associated with the prefix.
**  clear_hit_count - 1-Reset the hit count to zero. 0-Don't
**                    clear the hit count.
**  hit_count - (output) The hit count for this entry.
**              If the pointer is 0 then the hit count is not returned.
**
** Return Values:
**  0 - Route is found successfully.
** -1 - Error in input parameters.
** -2 - The route table is empty.
*/ 
int lpmhtRouteFirstGet (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int *prefix_size,
                unsigned long *user_data,
                unsigned int clear_hit_count,
                unsigned long long *hit_count);

/* Get the next route in the route table.
**  route_table - The pointer to the route table.
**  prefix - The pointer to the route. The route must be in 
**          network byte order. 
**  prefix_size - Number of bits in the network mask for 
**               this route. 
**  user_data - (output) The data associated with the route.
**  next_prefix - (output) Next route in the route table.
**          The memory for the prefix is allocated by the caller and 
**          must be 4 bytes for IPv4 and 16 bytes for IPv6.
**  next_prefix_size - (output) - Next route prefix size.
**  clear_hit_count - 1-Reset the hit count to zero. 0-Don't
**                    clear the hit count.
**  hit_count - (output) The hit count for this entry.
**              If the pointer is 0 then the hit count is not returned.
**
** Return Values:
**  0 - Route is found successfully.
** -1 - Error in input parameters.
** -2 - The next route doesn't exist.
*/ 
int lpmhtRouteNextGet (lpmhtRouteTable_t *route_table, 
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long *user_data,
                unsigned char *next_prefix,
                unsigned int *next_prefix_size,
                unsigned int clear_hit_count,
                unsigned long long *hit_count);

/*  Perform the longest prefix match on the specified prefix.
**
**  route_table - The pointer to the route table.
**  prefix - The pointer to the route. The route must be in 
**          network byte order. 
**  prefix_size - (output) Number of bits in the network mask for 
**               the best matching route. 
**  user_data - (output) The data associated with the route.
**
** Return Values:
**  0 - Route is found successfully.
** -1 - Error in input parameters.
** -2 - Matching route doesn't exists.
*/ 
int lpmhtLPMatch (lpmhtRouteTable_t *route_table, 
                unsigned char *prefix,
                unsigned int *prefix_size,
                unsigned long *user_data);

/* Get the number of routes currently in the route table.
** See description of lpmhtTableInfo_t structure for details.
**
**  route_table - The pointer to the route table.
**  info - (output) Route table information.
**
** Return Values:
**  0 - Success.
** -1 - Error in input parameters.
*/
int lpmhtRouteTableInfoGet (lpmhtRouteTable_t *route_table, 
                lpmhtTableInfo_t *info);

#endif /* LPMHT_API_H_INCLUDED */
