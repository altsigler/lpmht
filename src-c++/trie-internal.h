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
#ifndef TRIE_INTERNAL_H_INCLUDED
#define TRIE_INTERNAL_H_INCLUDED
#include "lpmht-api.h"
#include <cstddef>

#include "lpmht-util.h"

class sharedTrie
{
public:
  sharedTrie(const unsigned int maximum_routes, const lpmhtIpMode_e mode,
             const lpmhtTableProp_t& prop);

  ~sharedTrie(void);

  int routeInsert(const unsigned char* const prefix, const unsigned int prefix_size,
                  const unsigned long user_data);

  int routeDelete(const unsigned char* const prefix, const unsigned int prefix_size);

  int set(const unsigned char* const prefix, const unsigned int prefix_size,
          const unsigned long user_data);

  int get(const unsigned char* const prefix, const unsigned int prefix_size,
          unsigned long *user_data, const unsigned int clear_hit_count,
          unsigned long long *hit_count);

  int LPMatch(const unsigned char* const prefix, unsigned int *prefix_size,
              unsigned long *user_data);

  void WRITE_LOCK(void)
  {
    trie_rwlock.wrLock();
  }
  void READ_LOCK(void)
  {
    trie_rwlock.rdLock();
  }
  void READ_UNLOCK(void)
  {
    trie_rwlock.rdUnlock();
  }
  void WRITE_UNLOCK(void)
  {
    trie_rwlock.rdUnlock();
  }

  unsigned int numNodesGet(void)
  {
    return num_nodes;
  };
  unsigned int numRoutesGet(void)
  {
    return num_routes;
  };
  size_t memorySizeGet(void)
  {
    return memory_size;
  };
  size_t virtualMemorySizeGet(void)
  {
    return virtual_memory_size;
  };

private:
  /* The control information for the Trie node.
  ** Note that we are referencing nodes and routes by index as
  ** opposed to a pointer, which cuts the memory requirement
  ** for nodes in half on 64 bit machines. This is
  ** because "unsigned int" type is 4 bytes and pointers
  ** are 8 bytes on 64 bit machines.
  */
  typedef struct alignas (4)
  {
    /* Index of the route entry.
    ** 0 means that there is no route entry associated with
    ** this node.
    */
    unsigned int route;

    /* Routes whose bit is 0.
    ** 0 means that there are no such routes.
    */
    unsigned int left;

    /* Routes whose bit is 1.
    ** 0 means that there are no such routes.
    */
    unsigned int right;

    /* Parent node.
    ** 0 means that there are no parent node.
    */
    unsigned int parent;
  } trieNode_t;

  typedef struct alignas(8)
  {
    /* Index of the node to which this route is attached.
     */
    unsigned int parent_node;
    unsigned long user_data;
    std::atomic <unsigned long long> hit_count;
  } trieRoute_t;

  void nodeFind(const unsigned char *prefix, const unsigned int prefix_size,
                unsigned int *match, unsigned int *prev,
                unsigned int *prev_height);

  void nodeHitGetClear(trieRoute_t *route, unsigned long long *hit_count,
                       const unsigned int clear_hit_count);

  void nodeLongestPrefixMatch(const unsigned char *prefix,
                              unsigned int *prefix_size, unsigned int *match);

  void nodeDelete(unsigned int node);

  void nodeInsert(const unsigned long user_data, const unsigned char *prefix,
                  const int prefix_size, unsigned int prev, const unsigned int prev_height,
                  const unsigned int match);

  /* Read/Write lock to protect access to the Trie routing table.
   */
  lpmhtRwlock trie_rwlock;

  unsigned int max_routes;
  unsigned int enable_hit_count;
  lpmhtIpMode_e ip_mode;

  /* Total number of nodes currently inserted in the tree.
   */
  unsigned int num_nodes;

  /* Total number of route entries currently inserted in the tree.
   */
  unsigned int num_routes;

  /* Total number of physical bytes allocated for this Trie tree.
  ** Note that on 64-bit systems the overall tree size can
  ** exceed 4GB.
  */
  size_t memory_size;

  /* Number of bytes of virtual memory reserved for the routing table
  ** constructs.
  */
  size_t virtual_memory_size;

  /* Index of the root node.
  ** If this value is zero then the tree is empty.
  */
  unsigned int root_node;

  /* When this flag is set to 1, all physical memory for the Trie tree is
  *allocated
  ** when the Trie tree is created.
  */
  unsigned int mem_prealloc;

  /* Memory allocated for Trie nodes.
   */
  trieNode_t *trie_node;
  lpmhtMemBlock node_mb;

  /* Memory allocated for route entries.
   */
  trieRoute_t *trie_route;
  lpmhtMemBlock route_mb;

}; // End of class sharedTrie

#endif /* TRIE_INTERNAL_H_INCLUDED */
