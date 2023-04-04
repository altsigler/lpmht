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
** April-01-2023/Andrey Tsigler
******************************************************************************/
#ifndef TRIE_INTERNAL_H_INCLUDED
#define TRIE_INTERNAL_H_INCLUDED
#include <sys/types.h>
#include "lpmht-api.h"
#include "lpmht-util.h"

/* The control information for the Trie node. 
** Note that we are referencing nodes and routes by index as 
** opposed to a pointer, which cuts the memory requirement
** for nodes in half on 64 bit machines. This is 
** because "unsigned int" type is 4 bytes and pointers
** are 8 bytes on 64 bit machines.
*/
typedef struct 
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
} trieNode_t __attribute__ ((aligned (4)));


typedef struct 
{
  /* Index of the node to which this route is attached.
  */
  unsigned int parent_node; 
  unsigned long user_data;
  unsigned long long hit_count;
} trieRoute_t __attribute__ ((aligned (8)));

/* This structure defines the Trie tree information. 
*/
typedef struct 
{
  /* Read/Write lock to protect access to the Trie routing table.
  */
  lpmhtRwlock_t trie_rwlock;
#define TRIE_TREE_WRITE_LOCK(trie_tree)  lpmhtRwlockWrLock(&(trie_tree)->trie_rwlock)
#define TRIE_TREE_READ_LOCK(trie_tree)  lpmhtRwlockRdLock(&(trie_tree)->trie_rwlock)
#define TRIE_TREE_UNLOCK(trie_tree)  lpmhtRwlockUnlock(&(trie_tree)->trie_rwlock)


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

  /* When this flag is set to 1, all physical memory for the Trie tree is allocated
  ** when the Trie tree is created.
  */
  unsigned int mem_prealloc;

  /* Memory allocated for Trie nodes.
  */
  trieNode_t *trie_node; 
  memoryBlock_t node_mb;

  /* Memory allocated for route entries.
  */
  trieRoute_t *trie_route;
  memoryBlock_t route_mb;

} sharedTrie_t;


sharedTrie_t *trieCreate (unsigned int max_routes,
                          lpmhtIpMode_e ip_mode,
                          lpmhtTableProp_t *prop);

void trieDestroy (sharedTrie_t *trie);

int trieRouteInsert (sharedTrie_t *trie_tree,
                unsigned char *prefix,
                unsigned int  prefix_size,
                unsigned long user_data);

int trieDelete (sharedTrie_t *trie_tree,
                unsigned char *prefix,
                unsigned int prefix_size);

int trieSet (sharedTrie_t *trie_tree,
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long user_data);

int trieGet (sharedTrie_t *trie_tree,
                unsigned char *prefix,
                unsigned int prefix_size,
                unsigned long *user_data,
                unsigned int clear_hit_count,
                unsigned long long *hit_count);

int trieLPMatch (sharedTrie_t *trie_tree,
                unsigned char *prefix,
                unsigned int *prefix_size,
                unsigned long *user_data);


#endif /* TRIE_INTERNAL_H_INCLUDED */
