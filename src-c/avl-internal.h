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
#ifndef AVL_INTERNAL_H_INCLUDED
#define AVL_INTERNAL_H_INCLUDED
#include <sys/types.h>
#include "lpmht-util.h"
#include "avl-lib.h"

/* The control information for the AVL node. 
*/
typedef struct avlNode_s
{
  struct avlNode_s *parent;
  struct avlNode_s *left;
  struct avlNode_s *right;

  /* The level of this node from the bottom of the tree.
  */
  int height;

} avlNode_t;

/* This structure defines the AVL tree information. For shared AVL trees the
** structure is stored in shared memory.
*/
typedef struct 
{
  unsigned int max_nodes;
  unsigned int node_size;
  unsigned int key_size;

  /* Number of bytes allocated for every node.
  ** The nodes start at the 8-byte boundary.
  */
  unsigned int alloc_node_size;

  /* Total number of nodes currently inserted in the tree.
  */
  unsigned int num_nodes;

  /* Total number of bytes allocated for this AVL tree in physical memory.
  ** Note that on 64-bit systems the overall tree size can 
  ** exceed 4GB.
  */
  size_t memory_size;

  /* Number of bytes of virtual memory allocated for this tree.
  */
  size_t virtual_memory_size;

  /* When se to 1, all physical memory is allocated when the AVL tree is
  ** created, so virtual_memory_size is the same as memory_size.
  */
  unsigned int mem_prealloc;

  /* Pointer to the root node.
  */
  avlNode_t *root;

  /* Semaphore which is used internally by the AVL functions
  ** to protect access to the AVL tree.
  */
  lpmhtRwlock_t rwlock;

  /* Memory block for managing the node memory.
  */
  memoryBlock_t node_mb;

  /* Memory allocated for AVL nodes.
  */
  avlNode_t *avl_node; 

  /* Key comparison function. 
  */
  int  (*key_compare)(void *, void *);

} sharedAvl_t;

#endif /* AVL_INTERNAL_H_INCLUDED */
