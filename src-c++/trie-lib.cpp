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
** This file contains code to manage Binary Trie trees.
******************************************************************************/
#include <cstring>
#include <stdexcept>

#include "lpmht-api.h"
#include "trie-internal.h"


/* Map bit number to bit mask.
 */
static const unsigned char bitMask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
#define LPMHT_IS_BIT_SET(mask, bit) ((mask)[(bit) / 8] & bitMask[(bit) % 8])

#define LPMHT_BIT_SET(mask, bit) (mask)[(bit) / 8] |= bitMask[(bit) % 8]

/******************************************************************************
** Find the specified node in the Trie tree.
** If the node is found then the match pointer is set to the node address.
** If the node is not found then the match pointer is set to 0.
** The prev pointer points to the previous entry in the
** Trie tree.
**
** prefix - Network prefix in network byte order.
** prefix_size - Number of bits in the network prefix.
** match - (output) - Index of the matching tree node. 0 - if there is no match.
** prev - (output) - Index of the last examined node before finding or
**                   not finding the matching node. 0 - if the matching node
**                   is the root node or the tree only has the root node,
**                   or the tree is empty.
** prev_height - (output) - If prev pointer is non-zero then this is the
**               height of the prev node in the tree.
**
******************************************************************************/
void sharedTrie::nodeFind(const unsigned char *prefix, 
			  const unsigned int prefix_size, 
			  unsigned int *match, unsigned int *prev,
                          unsigned int *prev_height)
{
  unsigned int p;
  unsigned int i;

  *match = *prev = 0;
  *prev_height = 0;

  if (root_node == 0)
  {
    /* Special case when the tree is empty.
     */
    return;
  }

  p = root_node;
  for (i = 0; i < prefix_size; i++)
  {
    *prev = p;
    *prev_height = i;
    if (LPMHT_IS_BIT_SET(prefix, i))
    {
      p = trie_node[p].right;
    }
    else
    {
      p = trie_node[p].left;
    }
    __builtin_prefetch(&trie_node[trie_node[p].left]);
    __builtin_prefetch(&trie_node[trie_node[p].right]);
    if (p == 0)
    {
      break;
    }
  }
  if (p)
  {
    *match = p;
  }
}

/******************************************************************************
** Get/Clear hit count.
**
** route - Pointer to the route entry whose hit count needs to be obtained.
** hit_count - (output) - Hit count.
** clear_hit_count - Flag indicating whether to clear the hit count.
**
******************************************************************************/
void sharedTrie::nodeHitGetClear(trieRoute_t *route, 
			unsigned long long *hit_count, 
			const unsigned int clear_hit_count)
{
  if (enable_hit_count)
  {
    if (hit_count || clear_hit_count)
    {
      if (clear_hit_count)
      {
        if (hit_count)
        {
          *hit_count =  std::atomic_exchange_explicit(&route->hit_count, 0, 
			  		std::memory_order_relaxed);
        }
        else
        {
          (void)std::atomic_exchange_explicit(&route->hit_count, 0, 
			  		std::memory_order_relaxed);
        }
      }
      else
      {
        if (hit_count)
        {
          *hit_count = std::atomic_load_explicit(&route->hit_count, 
			  		std::memory_order_relaxed);
        }
      }
    }
  }
  else if (hit_count)
  {
    *hit_count = 0;
  }
}

/******************************************************************************
** Find the longest prefix match for the specified prefix.
** If the node is found then the match pointer is set to the node address.
** If the node is not found then the match pointer is set to 0.
**
** prefx - (output) Network prefix.
** prefix_size - (output) Number of bits in the network prefix.
** match - (output) - Index of the matching tree node. 0 - if there is no match.
**
******************************************************************************/
void sharedTrie::nodeLongestPrefixMatch(const unsigned char *prefix, unsigned int *prefix_size, unsigned int *match)
{
  unsigned int p = root_node;
  unsigned int i = 0;
  unsigned int prev = 0;
  unsigned int prev_height = 0;
  unsigned int route_index;

  __builtin_prefetch(&trie_node[trie_node[p].left]);
  __builtin_prefetch(&trie_node[trie_node[p].right]);
  while (p)
  {
    if (trie_node[p].route)
    {
      __builtin_prefetch(&trie_route[trie_node[p].route]);
      prev = p;
      prev_height = i;
    }
    if (LPMHT_IS_BIT_SET(prefix, i))
    {
      p = trie_node[p].right;
    }
    else
    {
      p = trie_node[p].left;
    }
    __builtin_prefetch(&trie_node[trie_node[p].left]);
    __builtin_prefetch(&trie_node[trie_node[p].right]);
    i++;
  };

  if (prev)
  {
    *match = prev;
    *prefix_size = prev_height;
    if (enable_hit_count)
    {
      route_index = trie_node[prev].route;
      auto hit_count_p = &trie_route[route_index].hit_count;
      (void)std::atomic_fetch_add_explicit(hit_count_p, 1, std::memory_order_relaxed);
    }
  }
  else
  {
    *match = 0;
  }
}

/******************************************************************************
** Delete the node from the Trie tree.
**
** node - Index of the node that needs to be deleted.
**
******************************************************************************/
void sharedTrie::nodeDelete(unsigned int node)
{
  unsigned int parent, last_alloc_node = 0;
  trieRoute_t *r;
  trieNode_t *n;
  unsigned int route, last_alloc_route = 0;

  route = trie_node[node].route;
  trie_node[node].route = 0;

  (void)route_mb.lastElementGet(&last_alloc_route);
  if (route != last_alloc_route)
  {
    /* We are only allowed to free the most recently allocated route.
    ** Therefore we need to copy information from the most recently
    ** allocated route to the route which is now available.
    */
    trie_route[route].hit_count.store(trie_route[last_alloc_route].hit_count.load());
    trie_route[route].parent_node = trie_route[last_alloc_route].parent_node;
    trie_route[route].user_data = trie_route[last_alloc_route].user_data;

    /* Now update the Trie tree to point to the new route.
     */
    r = &trie_route[route];
    trie_node[r->parent_node].route = route;
  }

  (void)route_mb.elementFree();
  num_routes--;
  if (0 == mem_prealloc)
    memory_size -= sizeof(trieRoute_t);
  do
  {
    /* If this node doesn't have an associated route and
    ** this node doesn't have any child nodes then delete it.
    */
    if ((trie_node[node].route == 0) && (trie_node[node].left == 0) && (trie_node[node].right == 0))
    {
      n = &trie_node[node];
      parent = n->parent;

      if (parent)
      {
        if (trie_node[parent].left == node)
        {
          trie_node[parent].left = 0;
        }
        else
        {
          trie_node[parent].right = 0;
        }
      }
      else
      {
        /* This must be the root node.
         */
        root_node = 0;
      }

      (void)node_mb.lastElementGet(&last_alloc_node);
      if (node != last_alloc_node)
      {
        /* We are only allowed to free the most recently allocated node.
        ** Therefore we need to copy information from the most recently
        ** allocated node to the node which is now available.
        */
        memcpy(n, &trie_node[last_alloc_node], sizeof(trieNode_t));

        /* Now update the tree to point to the new node.
         */
        if (n->route)
          trie_route[n->route].parent_node = node;

        if (n->right)
          trie_node[n->right].parent = node;

        if (n->left)
          trie_node[n->left].parent = node;

        if (trie_node[n->parent].left == last_alloc_node)
          trie_node[n->parent].left = node;
        else
          trie_node[n->parent].right = node;

        if (parent == last_alloc_node)
          parent = node;
      }

      (void)node_mb.elementFree();
      if (0 == mem_prealloc)
        memory_size -= sizeof(trieNode_t);
      num_nodes--;
    }
    else
    {
      break;
    }
    node = parent;
  } while (node);
}

/******************************************************************************
** Insert the new node into the Trie tree.
** The prev points to the Trie node after which the new node needs to be
** inserted.
** If the node already exists then the match points to the node where the
** route needs to be inserted.
**
** user_data - Data associated with this route.
** prefx - Network prefix.
** prefix_size - Number of bits in the network prefix.
** prev - Index of the previous node.
** prev_height - Prefix length of the previous node.
** match - Index of the matching tree node. 0 - if there is no match.
**
******************************************************************************/
void sharedTrie::nodeInsert(const unsigned long user_data, 
		const unsigned char *prefix, 
		const int prefix_size, unsigned int prev,
                            const unsigned int prev_height, 
			    const unsigned int match)
{
  unsigned int p;
  trieRoute_t *r;
  int i;
  unsigned int route;

  /* Get route entry from the free list.
   */
  if (route_mb.elementAlloc(&route))
    throw ERR_MSG("Route Allocation Failed");

  if (0 == mem_prealloc)
    memory_size += sizeof(trieRoute_t);
  r = &trie_route[route];
  num_routes++;
  r->user_data = user_data;
  r->hit_count = 0;

  /* Handle the special case when the node is already in the tree.
   */
  if (match)
  {
    trie_node[match].route = route;
    r->parent_node = match;
    return;
  }

  /* Special case when the tree is empty.
   */
  if (root_node == 0)
  {
    /* Get node from the free list. Since we already performed error
    ** checking, assume that the free list is not empty.
    */
    if (node_mb.elementAlloc(&p))
      throw ERR_MSG("Node Allocation Failed");

    if (0 == mem_prealloc)
      memory_size += sizeof(trieNode_t);
    num_nodes++;

    trie_node[p].left = 0;
    trie_node[p].right = 0;
    trie_node[p].route = 0;
    trie_node[p].parent = 0;

    root_node = p;
  }

  p = root_node;

  /* Special case when the prefix size is 0.
   */
  if (prefix_size == 0)
  {
    trie_node[p].route = route;
    r->parent_node = p;
    return;
  }

  /* Special case when prev is 0. This only happens when the tree is empty
  ** or the tree only has the root node. At this point we already made the
  ** tree non-empty, and established that the prefix doesn't match the
  ** root node. Therefore set prev to the root node.
  */
  if (prev == 0)
  {
    prev = root_node;
  }

  for (i = prev_height; i < prefix_size; i++)
  {
    (void)node_mb.elementAlloc(&p);
    if (0 == mem_prealloc)
      memory_size += sizeof(trieNode_t);
    num_nodes++;

    trie_node[p].left = 0;
    trie_node[p].right = 0;
    trie_node[p].route = 0;
    trie_node[p].parent = prev;

    if (LPMHT_IS_BIT_SET(prefix, i))
    {
      trie_node[prev].right = p;
    }
    else
    {
      trie_node[prev].left = p;
    }
    prev = p;
  }

  trie_node[p].route = route;
  r->parent_node = p;

  return;
}

/******************************************************************************
*******************************************************************************
** API Functions
*******************************************************************************
******************************************************************************/

/******************************************************************************
** Create a new Trie tree.
******************************************************************************/
sharedTrie::sharedTrie(const unsigned int maximum_routes, 
		       const lpmhtIpMode_e mode, const lpmhtTableProp_t& prop)
{
  unsigned int idx;
  unsigned int max_nodes;

  enable_hit_count = 0;
  memory_size = 0;
  virtual_memory_size = 0;

  num_nodes = 0;
  num_routes = 0;
  root_node = 0;

  ip_mode = mode;

  if (prop.mem_prealloc)
    mem_prealloc = 1;
  else
    mem_prealloc = 0;

  if (prop.hit_count)
    enable_hit_count = 1;

  max_routes = maximum_routes;

  if (mode == LPMHT_IPV4)
    max_nodes = max_routes * 32;
  else
    max_nodes = max_routes * 128;

  trie_node = (trieNode_t *)node_mb.init(sizeof(trieNode_t), max_nodes + 1, mem_prealloc);
  trie_route = (trieRoute_t *)route_mb.init(sizeof(trieRoute_t), max_routes + 1, mem_prealloc);

  virtual_memory_size += node_mb.blockVirtualSizeGet();
  virtual_memory_size += route_mb.blockVirtualSizeGet();

  if (mem_prealloc)
    memory_size = virtual_memory_size;

  /* Allocate the first element from the node tree and the route tree.
  ** The first element has index 0, which we are not going to use because
  ** the Trie tree algorithms assume that 0 means "not used".
  ** Note that the above tables contain "max_nodes + 1" and
  ** "max_routes + 1" elements to account for the discarded element.
  */
  if (node_mb.elementAlloc(&idx))
    throw ERR_MSG("Node Allocation Failed");

  if (route_mb.elementAlloc(&idx))
    throw ERR_MSG("Route Allocation Failed");
}

/******************************************************************************
** Delete an existing Trie tree.
** The function is NOT thread-safe.
******************************************************************************/
sharedTrie::~sharedTrie(void)
{
  /* Free the memory previously allocated for the Trie tree.
   */
  node_mb.destroy();
  route_mb.destroy();
}

/******************************************************************************
** Insert the specified Route into the Trie tree.
**  trie_tree - The pointer to the Trie tree.
**  prefix - The pointer to the route. The route must be in
**          network byte order.
**  prefix_size - Number of bits in the network mask for
**               this route.
**  user_data - The data to be associated with the route.
**
** Return Values:
**  0 - Route is inserted successfully.
** -2 - Route already exists.
** -3 - Exceeded the maximum number of route entries.
******************************************************************************/
int sharedTrie::routeInsert(const unsigned char* const prefix, 
			const unsigned int prefix_size, 
			const unsigned long user_data)
{
  unsigned int match, prev;
  unsigned int prev_height;

  if (num_routes == max_routes)
  {
    return -3;
  }

  /* Search the tree and find the matching node and the previous node.
   */
  nodeFind(prefix, prefix_size, &match, &prev, &prev_height);

  /* If this route is already in the tree then return an error.
   */
  if ((match) && (trie_node[match].route))
  {
    return -2;
  }

  /* Insert the new route into the Trie tree.
   */
  nodeInsert(user_data, prefix, prefix_size, prev, prev_height, match);

  return 0;
}

/******************************************************************************
** Delete the specified node from the Trie tree.
******************************************************************************/
int sharedTrie::routeDelete(const unsigned char* const prefix, const unsigned int prefix_size)
{
  int rc = -1;
  unsigned int match, prev;
  unsigned int prev_height;

  do /* Single-iteration loop - Used for error checking. */
  {
    if (num_nodes == 0)
    {
      /* The tree is empty.
       */
      rc = -2;
      break;
    }

    /* Search the tree and find the matching node and the previous node.
     */
    nodeFind(prefix, prefix_size, &match, &prev, &prev_height);

    /* If this node doesn't exist then return an error.
     */
    if (match == 0)
    {
      rc = -2;
      break;
    }

    /* If this node doesn't have an associated route then
    ** return an error.
    */
    if (trie_node[match].route == 0)
    {
      rc = -2;
      break;
    }

    /* Delete the node from the Trie tree.
     */
    nodeDelete(match);

    rc = 0;
  } while (0);

  return rc;
}

/******************************************************************************
** Modify the content of the specified route.
******************************************************************************/
int sharedTrie::set(const unsigned char* const prefix, 
			const unsigned int prefix_size, 
			const unsigned long user_data)
{
  int rc = -1;
  unsigned int match, prev;
  unsigned int prev_height;
  unsigned int route;

  do /* Single-iteration loop - Used for error checking. */
  {
    /* Search the tree and find the matching node and the previous node.
     */
    nodeFind(prefix, prefix_size, &match, &prev, &prev_height);

    /* If this node is not found then return an error.
     */
    if (0 == match)
    {
      rc = -2;
      break;
    }

    route = trie_node[match].route;
    if (0 == route)
    {
      rc = -2;
      break;
    }

    /* Copy the content of the new node.
     */
    trie_route[route].user_data = user_data;

    rc = 0;
  } while (0);

  return rc;
}

/******************************************************************************
** Get the content of the specified route.
******************************************************************************/
int sharedTrie::get(const unsigned char* const prefix, 
		const unsigned int prefix_size, unsigned long *user_data,
                    const unsigned int clear_hit_count, unsigned long long *hit_count)
{
  int rc = -1;
  unsigned int match, prev;
  unsigned int prev_height;
  unsigned int route;

  do /* Single-iteration loop - Used for error checking. */
  {
    /* Search the tree and find the matching node and the previous node.
     */
    nodeFind(prefix, prefix_size, &match, &prev, &prev_height);

    /* If this node is not found then return an error.
     */
    if (0 == match)
    {
      rc = -2;
      break;
    }

    route = trie_node[match].route;
    if (0 == route)
    {
      rc = -2;
      break;
    }

    /* Retrieve the content of the node.
     */
    *user_data = trie_route[route].user_data;

    /* Get/Clear hit count.
     */
    nodeHitGetClear(&trie_route[route], hit_count, clear_hit_count);

    rc = 0;
  } while (0);

  return rc;
}

/******************************************************************************
** Perform a longest prefix match on the specified prefix.
******************************************************************************/
int sharedTrie::LPMatch(const unsigned char* const prefix, 
		       unsigned int *prefix_size, unsigned long *user_data)
{
  int rc = -1;
  unsigned int match;
  unsigned int route;

  do /* Single-iteration loop - Used for error checking. */
  {
    /* Search the tree and find the lowest node.
     */
    nodeLongestPrefixMatch(prefix, prefix_size, &match);

    /* If this node is not found then return an error.
     */
    if (0 == match)
    {
      rc = -2;
      break;
    }

    /* Retrieve the content of the node.
     */
    route = trie_node[match].route;
    *user_data = trie_route[route].user_data;

    rc = 0;
  } while (0);

  return rc;
}
