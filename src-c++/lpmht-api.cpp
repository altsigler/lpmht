/******************************************************************************
** MIT License
** 
** Copyright (c) 2025 Andrey Tsigler
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
** January-09-2025/Andrey Tsigler
******************************************************************************/
/******************************************************************************
** This file contains APIs to manage routing tables. See comments in 
** lpmht-api.h for detailed API descriptions.
**
******************************************************************************/
#include "lpmht-api.h"
#include "trie-internal.h"
#include "hash-internal.h"

#include "avl-lib.h"


struct lpmhtRouteTable_s
{
  sharedTrie *trie_tree;   /* Pointer to the Trie control structure */
  sharedHash *hash_table; /* Pointer to the Hash Route Table */

  lpmhtTableMode_e table_mode;
  lpmhtIpMode_e ip_mode;

  unsigned int next_get_enable;
  lpmhtAvl *avl;
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
class lpmhtIpv4Avl : public lpmhtAvl 
{
public:
  lpmhtIpv4Avl (unsigned int max_nodes, 
		unsigned int node_size, 
		unsigned int key_size, 
		unsigned int mem_prealloc) : 
			lpmhtAvl(max_nodes, node_size, key_size, mem_prealloc) {};

  int key_compare (const void* const node1, const void* const node2)
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

};


/******************************************************************************
** Compare IPv6 addresses.
******************************************************************************/
class lpmhtIpv6Avl : public lpmhtAvl 
{
public:
  lpmhtIpv6Avl (unsigned int max_nodes, 
		unsigned int node_size, 
		unsigned int key_size, 
		unsigned int mem_prealloc) : 
			lpmhtAvl(max_nodes, node_size, key_size, mem_prealloc) {};

  int key_compare (const void* const node1, const void* const node2)
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
};

/***************************************************************************** 
** Create a new IPv4 or IPv6 routing table.
*****************************************************************************/
lpmhtRouteTable::lpmhtRouteTable (unsigned int max_routes,
                lpmhtIpMode_e ip_mode,
                lpmhtTableMode_e table_mode,
                lpmhtTableProp_t *prop)
{
  lpmhtTableProp_t default_prop;
  unsigned int mem_prealloc;

  memset (&default_prop, 0, sizeof(lpmhtTableProp_t));
  if (0 == prop)
          prop = &default_prop;

  if ((ip_mode != LPMHT_IPV4) && (ip_mode != LPMHT_IPV6))
  {
    throw ERR_MSG ("Invalid ip_mode");
  }

  if ((table_mode != LPMHT_TRIE) && 
                  (table_mode != LPMHT_HASH))
  {
    throw ERR_MSG ("Invalid table_mode");
  }

  if ((max_routes == 0) || 
      ((table_mode == LPMHT_TRIE) && (max_routes > LPMHT_MAX_TRIE_ROUTES)) ||
      (max_routes > LPMHT_MAX_HASH_ROUTES))
  {
    throw ERR_MSG ("Invalid max_routes");
  }

        
  route_table = (lpmhtRouteTable_t *) malloc(sizeof(lpmhtRouteTable_t));
  if (!route_table)
    throw ERR_MSG ("Can't allocate route_table");

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
    route_table->trie_tree = new sharedTrie(max_routes, ip_mode, *prop); 
  }
        
  if (table_mode == LPMHT_HASH) 
  {
    route_table->hash_table = new sharedHash (max_routes, ip_mode, *prop); 
  }

  if ((route_table->next_get_enable) &&
      (route_table->hash_table || route_table->trie_tree))
  {
    if (ip_mode == LPMHT_IPV4)
    {
      route_table->avl = new lpmhtIpv4Avl (max_routes, 
                         sizeof(avlIPv4Route_t), sizeof(avlIPv4Route_t),
                         mem_prealloc);
    } else
    {
      route_table->avl = new lpmhtIpv6Avl (max_routes,
                        sizeof(avlIPv6Route_t), sizeof(avlIPv6Route_t),
                        mem_prealloc);
    }
  }

}

/******************************************************************************
** Delete an existing route table. 
** The function is NOT thread-safe.
******************************************************************************/
lpmhtRouteTable::~lpmhtRouteTable (void)
{
  if (route_table->table_mode == LPMHT_TRIE)
  {
    delete route_table->trie_tree;
  }

  if (route_table->table_mode == LPMHT_HASH)
  {
    delete route_table->hash_table;
  }

  if (route_table->next_get_enable)
  {
    delete route_table->avl;
  }

  free (route_table);
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
void lpmhtRouteTable::avlRouteAdd (
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
    (void) route_table->avl->insert (&route);
  } else
  {
    avlIPv6Route_t route;

    memset (&route, 0, sizeof(route));
    memcpy (route.ip_addr, prefix, 16);
    route.prefix_size = (unsigned char) prefix_size;
    avlPrefixClean(route.ip_addr, 16, prefix_size);
    (void) route_table->avl->insert (&route);
  }
}

/******************************************************************************
** Add a route to the routing table.
******************************************************************************/
int lpmhtRouteTable::routeAdd (
                unsigned char *prefix,
                unsigned int  prefix_size,
                unsigned long user_data)
{
  int rc = -1;

  if (prefix == 0)
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
    route_table->trie_tree->WRITE_LOCK();

    rc = route_table->trie_tree->routeInsert(prefix, 
                                prefix_size, user_data);
    if (0 == rc)
                avlRouteAdd (prefix, prefix_size);

    route_table->trie_tree->WRITE_UNLOCK();
  }

  if (route_table->table_mode == LPMHT_HASH) 
  {
    route_table->hash_table->WRITE_LOCK();
    rc = route_table->hash_table->routeInsert(prefix, 
                                prefix_size, user_data);
    if (0 == rc)
                avlRouteAdd (prefix, prefix_size);

    route_table->hash_table->WRITE_UNLOCK();
  }

  return rc;
}

/******************************************************************************
** Delete route from the AVL table for NextGet feature.
******************************************************************************/
void lpmhtRouteTable::avlRouteDelete (
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
    (void) route_table->avl->nodeDelete (&route);
  } else
  {
    avlIPv6Route_t route;

    memset (&route, 0, sizeof(route));
    memcpy (route.ip_addr, prefix, 16);
    route.prefix_size = (unsigned char) prefix_size;
    avlPrefixClean(route.ip_addr, 16, prefix_size);
    (void) route_table->avl->nodeDelete (&route);
  }
}

/******************************************************************************
** Delete the specified route from the routing table.
******************************************************************************/
int lpmhtRouteTable::routeDelete (
                unsigned char *prefix,
                unsigned int prefix_size)
{
  int rc = -1;

  if (prefix == 0)
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
    route_table->trie_tree->WRITE_LOCK();

    rc = route_table->trie_tree->routeDelete (prefix, prefix_size);

    if (0 == rc)
                avlRouteDelete (prefix, prefix_size);

    route_table->trie_tree->WRITE_UNLOCK();
  }

  if (route_table->table_mode == LPMHT_HASH)
  {
    route_table->hash_table->WRITE_LOCK();
    rc = route_table->hash_table->routeDelete(prefix, 
                                prefix_size);

    if (0 == rc)
                avlRouteDelete (prefix, prefix_size);

    route_table->hash_table->WRITE_UNLOCK();
  }

  return rc;
}

/******************************************************************************
** Modify the content of the specified route.
******************************************************************************/
int lpmhtRouteTable::routeSet (
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long user_data)
{
  int rc = -1;

  if (prefix == 0)
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
    route_table->trie_tree->WRITE_LOCK();

    rc = route_table->trie_tree->set (prefix, prefix_size, user_data);

    route_table->trie_tree->WRITE_UNLOCK();
  }

  if (route_table->table_mode == LPMHT_HASH) 
  {
    route_table->hash_table->WRITE_LOCK();
    rc = route_table->hash_table->set(prefix, 
                                prefix_size, user_data);

    route_table->hash_table->WRITE_UNLOCK();
  }

  return rc;
}
/******************************************************************************
** Get the content of the specified route.
******************************************************************************/
int lpmhtRouteTable::routeGet (
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long *user_data,
                unsigned int clear_hit_count,
                unsigned long long *hit_count)
{
  int rc = -1;

  if ((prefix == 0) || (user_data == 0))
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
    route_table->trie_tree->READ_LOCK();
    rc = route_table->trie_tree->get (
                        prefix, prefix_size, 
                        user_data, clear_hit_count, hit_count);

    route_table->trie_tree->READ_UNLOCK();
  }

  if (route_table->table_mode == LPMHT_HASH) 
  {
    route_table->hash_table->READ_LOCK();
    rc = route_table->hash_table->get (
                        prefix, prefix_size, 
                        user_data, clear_hit_count, hit_count);

    route_table->hash_table->READ_UNLOCK();
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
int lpmhtRouteTable::avlRouteFirstGet (
                      unsigned char *prefix,
                      unsigned int *prefix_size)
{
  if (0 == route_table->next_get_enable)
                                   return -1;

  if (route_table->ip_mode == LPMHT_IPV4)
  {
    avlIPv4Route_t route;


    if (0 != route_table->avl->firstGet (&route))
                                                return -1;

    memcpy (prefix, route.ip_addr, 4);
    *prefix_size = route.prefix_size;
  } else
  {
    avlIPv6Route_t route;

    memset (&route, 0, sizeof(route));

    if (0 != route_table->avl->firstGet (&route))
                                                return -1;

    memcpy (prefix, route.ip_addr, 16);
    *prefix_size = route.prefix_size;
  }

  return 0;
}

/******************************************************************************
** Get the lowest node in the tree.
******************************************************************************/
int lpmhtRouteTable::routeFirstGet (
                unsigned char *prefix,
                unsigned int *prefix_size,
                unsigned long *user_data,
                unsigned int clear_hit_count,
                unsigned long long *hit_count)
{
  int rc = -1;

  if ((prefix == 0) || (prefix_size == 0) || (user_data == 0))
  {
    return rc;
  }


  rc = -2;
  if (route_table->table_mode == LPMHT_TRIE)
  {
    route_table->trie_tree->READ_LOCK();
    if (0 == avlRouteFirstGet (prefix, prefix_size))
    {
      rc = route_table->trie_tree->get (prefix, *prefix_size,
                       user_data, clear_hit_count, hit_count);
    }
    route_table->trie_tree->READ_UNLOCK();
  }

  if (route_table->table_mode == LPMHT_HASH) 
  {
    route_table->hash_table->READ_LOCK();
    if (0 == avlRouteFirstGet (prefix, prefix_size))
    {
      rc = route_table->hash_table->get (
                        prefix, *prefix_size, 
                        user_data, clear_hit_count, hit_count);
    }
    route_table->hash_table->READ_UNLOCK();
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
int lpmhtRouteTable::avlRouteNextGet (
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

    if (0 != route_table->avl->nextGet (&route, &next_route))
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

    if (0 != route_table->avl->nextGet (&route, &next_route))
                                                return -1;

    memcpy (next_prefix, next_route.ip_addr, 16);
    *next_prefix_size = next_route.prefix_size;
  }

  return 0;
}

/******************************************************************************
** Get the content of the next route.
******************************************************************************/
int lpmhtRouteTable::routeNextGet (
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long *user_data,
                unsigned char *next_prefix,
                unsigned int *next_prefix_size,
                unsigned int clear_hit_count,
                unsigned long long *hit_count)
{
  int rc = -1;

  if ((prefix == 0) || (user_data == 0) ||
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
    route_table->trie_tree->READ_LOCK();
    if (0 == avlRouteNextGet(prefix, prefix_size,
                             next_prefix, next_prefix_size))
    {
      rc = route_table->trie_tree->get (next_prefix, *next_prefix_size,
                       user_data, clear_hit_count, hit_count);
    }
    route_table->trie_tree->READ_UNLOCK();
  }

  if (route_table->table_mode == LPMHT_HASH)
  {
    route_table->hash_table->READ_LOCK();
    if (0 == avlRouteNextGet(prefix, prefix_size,
                             next_prefix, next_prefix_size))
    {
      rc = route_table->hash_table->get (next_prefix, *next_prefix_size,
                       user_data, clear_hit_count, hit_count);
    }
    route_table->hash_table->READ_UNLOCK();
  }
  
  return rc;
}

/******************************************************************************
** Get the number of routes currently inserted in the Trie tree.
******************************************************************************/
int lpmhtRouteTable::routeTableInfoGet (
                            lpmhtTableInfo_t *info)
{
  int rc = -1;

  if (info == 0)
  {
    return rc;
  }

  memset (info, 0, sizeof(lpmhtTableInfo_t));

  if (route_table->table_mode == LPMHT_TRIE)
  { 

    route_table->trie_tree->READ_LOCK();
    info->num_nodes = route_table->trie_tree->numNodesGet();
    info->num_routes = route_table->trie_tree->numRoutesGet();
    info->mem_size = route_table->trie_tree->memorySizeGet();
    info->virtual_mem_size = route_table->trie_tree->virtualMemorySizeGet();
    route_table->trie_tree->READ_UNLOCK();
  }

  if (route_table->table_mode == LPMHT_HASH) 
  { 

    route_table->hash_table->READ_LOCK();
    info->num_routes = route_table->hash_table->numRoutesGet();
    info->mem_size = route_table->hash_table->memorySizeGet();
    info->virtual_mem_size = route_table->hash_table->virtualMemorySizeGet();
    info->ipv6_flow_not_found = route_table->hash_table->ipv6FlowNotFoundGet();
    if (route_table->hash_table->ipv4RulesReadyGet())
                info->ipv4_rule_table_ready = 1;

                
    route_table->hash_table->READ_UNLOCK();

  }
  if (0 != route_table->next_get_enable)
  {
    unsigned int num_nodes = 0;
    size_t memory_size = 0;
    size_t virtual_memory_size = 0;

    (void) route_table->avl->nodeCountGet (&num_nodes, 
                                   &memory_size, &virtual_memory_size);

    info->mem_size += memory_size;
    info->virtual_mem_size += virtual_memory_size;
  }


  return 0;
}

/******************************************************************************
** Perform a longest prefix match on the specified prefix.
******************************************************************************/
int lpmhtRouteTable::LPMatch (
                unsigned char *prefix,
                unsigned int *prefix_size,
                unsigned long *user_data)
{
  int rc = -1;

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
    route_table->trie_tree->READ_LOCK();

    rc = route_table->trie_tree->LPMatch (prefix, prefix_size,
                        user_data);

    route_table->trie_tree->READ_UNLOCK();
  } else
  {
    unsigned char pref_size;

    route_table->hash_table->READ_LOCK();
    rc = route_table->hash_table->LPMatch (
                        prefix, &pref_size, 
                        user_data);

    *prefix_size = (unsigned int) pref_size;

    route_table->hash_table->READ_UNLOCK();
  }
  return rc;
}
