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
/******************************************************************************
** This file contains APIs to manage routing tables. See comments in 
** lpmht-api.h for detailed API descriptions.
**
******************************************************************************/
#include <stdlib.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lpmht-api.h"
#include "trie-internal.h"
#include "hash-internal.h"
#include "lpmht-util.h"
#include "avl-lib.h"


struct lpmhtRouteTable_s
{
  sharedTrie_t *trie_tree;   /* Pointer to the Trie control structure */
  sharedHash_t *hash_table; /* Pointer to the Hash Route Table */

  lpmhtTableMode_e table_mode;
  lpmhtIpMode_e ip_mode;

  unsigned int next_get_enable;
  lpmhtAvl_t avl;
};

/* Structure for storing the route entries in the AVL tree.
** This structure is used when the prop.next_get is enabled.
** This structure supports the lpmhtRouteFirstGet() and
** lpmhtRouteNextGet() functions.
*/
typedef struct
{
  unsigned char ip_addr[4]; /* IPv4 address. */
  unsigned char prefix_size; /* Number of bits in the IP address. */
} avlIPv4Route_t;

typedef struct
{
  unsigned char ip_addr[16]; /* IPv6 address. */
  unsigned char prefix_size; /* Number of bits in the IP address. */
} avlIPv6Route_t;

/******************************************************************************
** Compare IPv4 addresses.
******************************************************************************/
static int lpmhtIpv4AvlCompare (void *node1, void *node2)
{
  avlIPv4Route_t *key1 = (avlIPv4Route_t *) node1;
  avlIPv4Route_t *key2 = (avlIPv4Route_t *) node2;
  int cmp;

  if (key1->prefix_size > key2->prefix_size)
                                return -1;

  if (key1->prefix_size < key2->prefix_size)
                                return 1; 

  cmp = memcmp (key1->ip_addr, key2->ip_addr, 4);

  if (cmp == 0)
  {
    return 0;
  }
  if (cmp < 0)
  {
    return -1;
  }
  return 1;
}

/******************************************************************************
** Compare IPv6 addresses.
******************************************************************************/
static int lpmhtIpv6AvlCompare (void *node1, void *node2)
{
  avlIPv6Route_t *key1 = (avlIPv6Route_t *) node1;
  avlIPv6Route_t *key2 = (avlIPv6Route_t *) node2;
  int cmp;

  if (key1->prefix_size > key2->prefix_size)
                                return -1;

  if (key1->prefix_size < key2->prefix_size)
                                return 1;

  cmp = memcmp (key1->ip_addr, key2->ip_addr, 16);
  
  if (cmp == 0)
  {
    return 0;
  }
  if (cmp < 0)
  {
    return -1;
  }
  return 1;
}

/***************************************************************************** 
** Create a new IPv4 or IPv6 routing table.
*****************************************************************************/
lpmhtRouteTable_t *lpmhtRouteTableCreate (unsigned int max_routes,
                lpmhtIpMode_e ip_mode,
                lpmhtTableMode_e table_mode,
                lpmhtTableProp_t *prop)
{
  lpmhtRouteTable_t *route_table;
  lpmhtTableProp_t default_prop;
  unsigned int mem_prealloc;

  memset (&default_prop, 0, sizeof(lpmhtTableProp_t));
  if (0 == prop)
          prop = &default_prop;

  if ((ip_mode != LPMHT_IPV4) && (ip_mode != LPMHT_IPV6))
  {
    return 0;
  }

  if ((table_mode != LPMHT_TRIE) && 
                  (table_mode != LPMHT_HASH))
  {
    return 0;
  }

  if ((max_routes == 0) || 
      ((table_mode == LPMHT_TRIE) && (max_routes > LPMHT_MAX_TRIE_ROUTES)) ||
      (max_routes > LPMHT_MAX_HASH_ROUTES))
  {
    return 0;
  }

        
  route_table = (lpmhtRouteTable_t *) malloc(sizeof(lpmhtRouteTable_t));
  if (!route_table)
          (assert(0), abort());

  memset (route_table, 0, sizeof(lpmhtRouteTable_t));

  route_table->table_mode = table_mode;
  route_table->ip_mode = ip_mode;

  if (prop->next_get)
                route_table->next_get_enable = 1;
       else
                route_table->next_get_enable = 0;

  if (prop->mem_prealloc)
                mem_prealloc = 1;
            else
                mem_prealloc = 0;


  if (table_mode == LPMHT_TRIE) 
  {
    route_table->trie_tree = trieCreate (max_routes, ip_mode, prop); 
  }
        
  if (table_mode == LPMHT_HASH) 
  {
    route_table->hash_table = hashCreate (max_routes, ip_mode, prop); 
  }

  if ((route_table->next_get_enable) &&
      (route_table->hash_table || route_table->trie_tree))
  {
    if (ip_mode == LPMHT_IPV4)
    {
      if (lpmhtAvlCreate(&route_table->avl, max_routes, 
                        sizeof(avlIPv4Route_t), sizeof(avlIPv4Route_t),
                        lpmhtIpv4AvlCompare, mem_prealloc))
                           (assert(0), abort());

    } else
    {
      if (lpmhtAvlCreate(&route_table->avl, max_routes, 
                        sizeof(avlIPv6Route_t), sizeof(avlIPv6Route_t),
                        lpmhtIpv6AvlCompare, mem_prealloc))
                           (assert(0), abort());
    }
  }

  return route_table;
}

/******************************************************************************
** Delete an existing route table. 
** The function is NOT thread-safe.
******************************************************************************/
int lpmhtRouteTableDestroy (lpmhtRouteTable_t *route_table)
{
  if (route_table == 0)
  {
    return -1;
  }
  
  if (route_table->table_mode == LPMHT_TRIE)
  {
    trieDestroy(route_table->trie_tree);
  }

  if (route_table->table_mode == LPMHT_HASH)
  {
    hashDestroy(route_table->hash_table);
  }

  if (route_table->next_get_enable)
  {
    (void) lpmhtAvlDestroy(&route_table->avl);
  }

  free (route_table);

  return 0;
}
/******************************************************************************
** Zero out masked bits in the prefix.
** 
**  prefix - IPv4 or IPv6 network prefix.
**  prefix_len - Number of bytes in the prefix (4-IPv4, 16-IPv6)
**  prefix_size - Number of significant bits in the prefix.
******************************************************************************/
static void avlPrefixClean (unsigned char *prefix,
                            unsigned int prefix_len,
                            unsigned int prefix_size)
{
  unsigned int byte;
  unsigned int bit;

  byte = prefix_size / 8;
  bit = prefix_size % 8;
  while (byte < prefix_len)
  {
    prefix[byte] &= ~(0x80>>bit);
    bit++;
    if (bit >= 8)
    {
      bit = 0;
      byte++;
    }
  }
}

/******************************************************************************
** Add a route to the AVL table for NextGet feature.
******************************************************************************/
static void avlRouteAdd (lpmhtRouteTable_t *route_table,
                        unsigned char *prefix,
                        unsigned int prefix_size)
{
  if (0 == route_table->next_get_enable)
                                   return;

  if (route_table->ip_mode == LPMHT_IPV4)
  {
    avlIPv4Route_t route;

    memset (&route, 0, sizeof(route));
    memcpy (route.ip_addr, prefix, 4);
    route.prefix_size = (unsigned char) prefix_size;
    avlPrefixClean(route.ip_addr, 4, prefix_size);
    (void) lpmhtAvlInsert (&route_table->avl, &route);
  } else
  {
    avlIPv6Route_t route;

    memset (&route, 0, sizeof(route));
    memcpy (route.ip_addr, prefix, 16);
    route.prefix_size = (unsigned char) prefix_size;
    avlPrefixClean(route.ip_addr, 16, prefix_size);
    (void) lpmhtAvlInsert (&route_table->avl, &route);
  }
}

/******************************************************************************
** Add a route to the routing table.
******************************************************************************/
int lpmhtRouteAdd (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int  prefix_size,
                unsigned long user_data)
{
  int rc = -1;

  if ((route_table == 0) || (prefix == 0))
  {
    return rc;
  }

  if ((route_table->ip_mode == LPMHT_IPV4) &&
        (prefix_size > 32))
                        return rc;
  if (prefix_size > 128)
                        return rc;


  if (route_table->table_mode == LPMHT_TRIE)
  {
    TRIE_TREE_WRITE_LOCK(route_table->trie_tree);

    rc = trieRouteInsert(route_table->trie_tree, prefix, 
                                prefix_size, user_data);
    if (0 == rc)
                avlRouteAdd (route_table, prefix, prefix_size);

    TRIE_TREE_UNLOCK(route_table->trie_tree);
  }

  if (route_table->table_mode == LPMHT_HASH) 
  {
    HASH_WRITE_LOCK(route_table->hash_table);
    rc = hashRouteInsert(route_table->hash_table, prefix, 
                                prefix_size, user_data);
    if (0 == rc)
                avlRouteAdd (route_table, prefix, prefix_size);

    HASH_UNLOCK(route_table->hash_table);
  }

  return rc;
}

/******************************************************************************
** Delete route from the AVL table for NextGet feature.
******************************************************************************/
static void avlRouteDelete (lpmhtRouteTable_t *route_table,
                        unsigned char *prefix,
                        unsigned int prefix_size)
{
  if (0 == route_table->next_get_enable)
                                   return;

  if (route_table->ip_mode == LPMHT_IPV4)
  {
    avlIPv4Route_t route;

    memset (&route, 0, sizeof(route));
    memcpy (route.ip_addr, prefix, 4);
    route.prefix_size = (unsigned char) prefix_size;
    avlPrefixClean(route.ip_addr, 4, prefix_size);
    (void) lpmhtAvlDelete (&route_table->avl, &route);
  } else
  {
    avlIPv6Route_t route;

    memset (&route, 0, sizeof(route));
    memcpy (route.ip_addr, prefix, 16);
    route.prefix_size = (unsigned char) prefix_size;
    avlPrefixClean(route.ip_addr, 16, prefix_size);
    (void) lpmhtAvlDelete (&route_table->avl, &route);
  }
}

/******************************************************************************
** Delete the specified route from the routing table.
******************************************************************************/
int lpmhtRouteDelete (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int prefix_size)
{
  int rc = -1;

  if ((route_table == 0) || (prefix == 0))
  {
    return rc;
  }

  if ((route_table->ip_mode == LPMHT_IPV4) &&
        (prefix_size > 32))
                        return rc;
  if (prefix_size > 128)
                        return rc;


  if (route_table->table_mode == LPMHT_TRIE) 
  {
    TRIE_TREE_WRITE_LOCK(route_table->trie_tree);

    rc = trieDelete (route_table->trie_tree, prefix, prefix_size);

    if (0 == rc)
                avlRouteDelete (route_table, prefix, prefix_size);

    TRIE_TREE_UNLOCK(route_table->trie_tree);
  }

  if (route_table->table_mode == LPMHT_HASH)
  {
    HASH_WRITE_LOCK(route_table->hash_table);
    rc = hashDelete(route_table->hash_table, prefix, 
                                prefix_size);

    if (0 == rc)
                avlRouteDelete (route_table, prefix, prefix_size);

    HASH_UNLOCK(route_table->hash_table);
  }

  return rc;
}

/******************************************************************************
** Modify the content of the specified route.
******************************************************************************/
int lpmhtRouteSet (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long user_data)
{
  int rc = -1;

  if ((route_table == 0) || (prefix == 0))
  {
    return rc;
  }

  if ((route_table->ip_mode == LPMHT_IPV4) &&
        (prefix_size > 32))
                        return rc;
  if (prefix_size > 128)
                        return rc;


  if (route_table->table_mode == LPMHT_TRIE)
  {
    TRIE_TREE_WRITE_LOCK(route_table->trie_tree);

    rc = trieSet (route_table->trie_tree, prefix, prefix_size, user_data);

    TRIE_TREE_UNLOCK(route_table->trie_tree);
  }

  if (route_table->table_mode == LPMHT_HASH) 
  {
    HASH_WRITE_LOCK(route_table->hash_table);
    rc = hashSet(route_table->hash_table, prefix, 
                                prefix_size, user_data);

    HASH_UNLOCK(route_table->hash_table);
  }

  return rc;
}
/******************************************************************************
** Get the content of the specified route.
******************************************************************************/
int lpmhtRouteGet (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long *user_data,
                unsigned int clear_hit_count,
                unsigned long long *hit_count)
{
  int rc = -1;

  if ((route_table == 0) || (prefix == 0) || (user_data == 0))
  {
    return rc;
  }


  if ((route_table->ip_mode == LPMHT_IPV4) &&
        (prefix_size > 32))
                        return rc;
  if (prefix_size > 128)
                        return rc;


  if (route_table->table_mode == LPMHT_TRIE)
  {
    TRIE_TREE_READ_LOCK(route_table->trie_tree);
    rc = trieGet (route_table->trie_tree,
                        prefix, prefix_size, 
                        user_data, clear_hit_count, hit_count);

    TRIE_TREE_UNLOCK(route_table->trie_tree);
  }

  if (route_table->table_mode == LPMHT_HASH) 
  {
    HASH_READ_LOCK(route_table->hash_table);
    rc = hashGet (route_table->hash_table,
                        prefix, prefix_size, 
                        user_data, clear_hit_count, hit_count);

    HASH_UNLOCK(route_table->hash_table);
  }

  return rc;
}

/******************************************************************************
** Get first route from the AVL Tree.
**
** Return Values
** 0 - First route found.
** -1 - Table is empty.
******************************************************************************/
static int avlRouteFirstGet (lpmhtRouteTable_t *route_table,
                      unsigned char *prefix,
                      unsigned int *prefix_size)
{
  if (0 == route_table->next_get_enable)
                                   return -1;

  if (route_table->ip_mode == LPMHT_IPV4)
  {
    avlIPv4Route_t route;


    if (0 != lpmhtAvlFirstGet (&route_table->avl, &route))
                                                return -1;

    memcpy (prefix, route.ip_addr, 4);
    *prefix_size = route.prefix_size;
  } else
  {
    avlIPv6Route_t route;

    memset (&route, 0, sizeof(route));

    if (0 != lpmhtAvlFirstGet (&route_table->avl, &route))
                                                return -1;

    memcpy (prefix, route.ip_addr, 16);
    *prefix_size = route.prefix_size;
  }

  return 0;
}

/******************************************************************************
** Get the lowest node in the tree.
******************************************************************************/
int lpmhtRouteFirstGet (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int *prefix_size,
                unsigned long *user_data,
                unsigned int clear_hit_count,
                unsigned long long *hit_count)
{
  int rc = -1;

  if (route_table == 0)
  {
    return rc;
  }

  if ((prefix == 0) || (prefix_size == 0) || (user_data == 0))
  {
    return rc;
  }


  rc = -2;
  if (route_table->table_mode == LPMHT_TRIE)
  {
    TRIE_TREE_READ_LOCK(route_table->trie_tree);
    if (0 == avlRouteFirstGet (route_table, prefix, prefix_size))
    {
      rc = trieGet (route_table->trie_tree, prefix, *prefix_size,
                       user_data, clear_hit_count, hit_count);
    }
    TRIE_TREE_UNLOCK(route_table->trie_tree);
  }

  if (route_table->table_mode == LPMHT_HASH) 
  {
    HASH_READ_LOCK(route_table->hash_table);
    if (0 == avlRouteFirstGet (route_table, prefix, prefix_size))
    {
      rc = hashGet (route_table->hash_table,
                        prefix, *prefix_size, 
                        user_data, clear_hit_count, hit_count);
    }
    HASH_UNLOCK(route_table->hash_table);
  }

  return rc;
}

/******************************************************************************
** Get next route from the AVL Tree.
**
** Return Values
** 0 - First route found.
** -1 - Table is empty.
******************************************************************************/
static int avlRouteNextGet (lpmhtRouteTable_t *route_table,
                      unsigned char *prefix,
                      unsigned int prefix_size,
                      unsigned char *next_prefix,
                      unsigned int *next_prefix_size)
{
  if (0 == route_table->next_get_enable)
                                   return -1;

  if (route_table->ip_mode == LPMHT_IPV4)
  {
    avlIPv4Route_t route, next_route;

    memset (&route, 0, sizeof(route));
    memcpy (route.ip_addr, prefix, 4);
    route.prefix_size = (unsigned char) prefix_size;
    avlPrefixClean(route.ip_addr, 4, prefix_size);

    if (0 != lpmhtAvlNextGet (&route_table->avl, &route, &next_route))
                                                return -1;

    memcpy (next_prefix, next_route.ip_addr, 4);
    *next_prefix_size = next_route.prefix_size;
  } else
  {
    avlIPv6Route_t route, next_route;

    memset (&route, 0, sizeof(route));
    memcpy (route.ip_addr, prefix, 16);
    route.prefix_size = (unsigned char) prefix_size;
    avlPrefixClean(route.ip_addr, 16, prefix_size);

    if (0 != lpmhtAvlNextGet (&route_table->avl, &route, &next_route))
                                                return -1;

    memcpy (next_prefix, next_route.ip_addr, 16);
    *next_prefix_size = next_route.prefix_size;
  }

  return 0;
}

/******************************************************************************
** Get the content of the next route.
******************************************************************************/
int lpmhtRouteNextGet (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long *user_data,
                unsigned char *next_prefix,
                unsigned int *next_prefix_size,
                unsigned int clear_hit_count,
                unsigned long long *hit_count)
{
  int rc = -1;

  if ((route_table == 0) || (prefix == 0) || (user_data == 0) ||
      (next_prefix == 0) || (next_prefix_size == 0))
  {
    return rc;
  }

  if ((route_table->ip_mode == LPMHT_IPV4) &&
        (prefix_size > 32))
                        return rc;
  if (prefix_size > 128)
                        return rc;

  rc = -2;
  if (route_table->table_mode == LPMHT_TRIE)
  { 
    TRIE_TREE_READ_LOCK(route_table->trie_tree);
    if (0 == avlRouteNextGet(route_table, prefix, prefix_size,
                             next_prefix, next_prefix_size))
    {
      rc = trieGet (route_table->trie_tree, next_prefix, *next_prefix_size,
                       user_data, clear_hit_count, hit_count);
    }
    TRIE_TREE_UNLOCK(route_table->trie_tree);
  }

  if (route_table->table_mode == LPMHT_HASH)
  {
    HASH_READ_LOCK(route_table->hash_table);
    if (0 == avlRouteNextGet(route_table, prefix, prefix_size,
                             next_prefix, next_prefix_size))
    {
      rc = hashGet (route_table->hash_table, next_prefix, *next_prefix_size,
                       user_data, clear_hit_count, hit_count);
    }
    HASH_UNLOCK(route_table->hash_table);
  }
  
  return rc;
}

/******************************************************************************
** Get the number of routes currently inserted in the Trie tree.
******************************************************************************/
int lpmhtRouteTableInfoGet (lpmhtRouteTable_t *route_table,
                            lpmhtTableInfo_t *info)
{
  int rc = -1;

  if ((route_table == 0) || (info == 0))
  {
    return rc;
  }

  memset (info, 0, sizeof(lpmhtTableInfo_t));

  if (route_table->table_mode == LPMHT_TRIE)
  { 

    TRIE_TREE_READ_LOCK(route_table->trie_tree);
    info->num_nodes = route_table->trie_tree->num_nodes;
    info->num_routes = route_table->trie_tree->num_routes;
    info->mem_size = route_table->trie_tree->memory_size;
    info->virtual_mem_size = route_table->trie_tree->virtual_memory_size;
    TRIE_TREE_UNLOCK(route_table->trie_tree);
  }

  if (route_table->table_mode == LPMHT_HASH) 
  { 

    HASH_READ_LOCK(route_table->hash_table);
    info->num_routes = route_table->hash_table->num_routes;
    info->mem_size = route_table->hash_table->memory_size;
    info->virtual_mem_size = route_table->hash_table->virtual_memory_size;
    info->ipv6_flow_not_found = *route_table->hash_table->ipv6_flow_not_found;
    if (route_table->hash_table->ipv4_rules_ready)
                info->ipv4_rule_table_ready = 1;

                
    HASH_UNLOCK(route_table->hash_table);

  }
  if (0 != route_table->next_get_enable)
  {
    unsigned int num_nodes = 0;
    size_t memory_size = 0;
    size_t virtual_memory_size = 0;

    (void) lpmhtAvlNodeCountGet (&route_table->avl , &num_nodes, 
                                   &memory_size, &virtual_memory_size);

    info->mem_size += memory_size;
    info->virtual_mem_size += virtual_memory_size;
  }


  return 0;
}

/******************************************************************************
** Perform a longest prefix match on the specified prefix.
******************************************************************************/
int lpmhtLPMatch (lpmhtRouteTable_t *route_table,
                unsigned char *prefix,
                unsigned int *prefix_size,
                unsigned long *user_data)
{
  int rc = -1;

  if (route_table == 0)
  {
    return rc;
  }

  if (user_data == 0)
  {
    return rc;
  }

  if ((prefix == 0) || (prefix_size == 0))
  {
    return rc;
  }


  if (route_table->table_mode == LPMHT_TRIE) 
  { 
    TRIE_TREE_READ_LOCK(route_table->trie_tree);

    rc = trieLPMatch (route_table->trie_tree, prefix, prefix_size,
                        user_data);

    TRIE_TREE_UNLOCK(route_table->trie_tree);
  } else
  {
    unsigned char pref_size;

    HASH_READ_LOCK(route_table->hash_table);
    rc = hashLPMatch (route_table->hash_table,
                        prefix, &pref_size, 
                        user_data);

    *prefix_size = (unsigned int) pref_size;

    HASH_UNLOCK(route_table->hash_table);
  }
  return rc;
}
