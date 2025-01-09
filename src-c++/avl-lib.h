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
#ifndef AVL_LIB_H_INCLUDED
#define AVL_LIB_H_INCLUDED
#include <cstddef>
#include <cstring>

#include "lpmht-util.h"

/******************************************************************************
** This file contains APIs to manage AVL Trees.
******************************************************************************/

class lpmhtAvl
{
public:
  /* Create a new AVL tree.
  **
  ** The default key comparison function is memcmp().
  **
  ** max_nodes - Maximum number of nodes in the AVL tree.
  ** node_size - Size of the AVL tree node. This includes the key.
  ** key_size - Size of the AVL tree node key.
  ** mem_prealloc - 1 - Pre-allocate all physical memory when the tree is created.
  **                0 - Allocate physical memory when node are added, free physical
  **                    memory when nodes are deleted.
  **
  */
  lpmhtAvl(const unsigned int max_nodes, 
		 const unsigned int node_size, 
		 const unsigned int key_size, const unsigned int mem_prealloc);

  /* Free all resources associated with the previously created AVL tree.
  **
  */
  virtual ~lpmhtAvl(void);

  /* Compare keys. The default key comparison function is memcmp().
  ** The users of the lpmtAvl class are intended to derive a new
  ** class when a different key comparison function is needed.
  **
  ** Return Values:
  **  0 - Keys are equal.
  **  Positive Integer - k1 > k2
  **  Negative Integer - k1 < k2
  */
  virtual int key_compare(const void* const k1, const void* const k2)
  {
    return memcmp(k1, k2, avl_tree->key_size);
  };

  /* Insert the specified node into the AVL tree.
  **  node - The pointer to the node, including the initialized key.
  **
  ** Return Values:
  **  0 - Node is inserted successfully.
  ** -1 - Error in input parameters.
  ** -2 - Node with the specified key already exists.
  */
  int insert(void *node);

  /* Delete the specified node from the AVL tree.
  **  node - The pointer to the node. Only the key
  **         needs to be initialized.
  **
  ** Return Values:
  **  0 - Node is deleted successfully.
  ** -1 - Error in input parameters.
  ** -2 - Node with the specified doesn't exists.
  */
  int nodeDelete(void *node);

  /* Get the lowest node in the AVL tree.
  **  node (input/output) - The pointer to the node.
  **        The API returns the key and the data.
  **
  ** Return Values:
  **  0 - Node is found successfully.
  ** -1 - Error in input parameters.
  ** -2 - The tree is empty.
  */
  int firstGet(void *node);

  /* Get the next node in the AVL tree.
  **  node - (input) The pointer to the current node.
  **        The key part of the node must be initialized by the caller.
  **        It is OK if the node with the specified key doesn't exist.
  **  next_node - (output) The pointer to the next node.
  **        The key and the value are filled in by the API function.
  **
  ** Return Values:
  **  0 - Node is found successfully.
  ** -1 - Error in input parameters.
  ** -2 - The next node doesn't exist.
  */
  int nextGet(void *node, void *next_node);

  /* Get the number of nodes currently in the AVL tree.
  **
  **  num_nodes - (output) The current number of nodes.
  **  memory_size - (output) Number of physical memory bytes used by the tree.
  **  virtual_memory_size - (output) Number of bytes used by the tree.
  **
  ** Return Values:
  **  0 - Success.
  ** -1 - Error in input parameters.
  */
  int nodeCountGet(unsigned int *num_nodes, std::size_t *memory_size, std::size_t *virtual_memory_size);

private:
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
    lpmhtRwlock rwlock;

    /* Memory block for managing the node memory.
     */
    lpmhtMemBlock node_mb;

    /* Memory allocated for AVL nodes.
     */
    avlNode_t *avl_node;

  } sharedAvl_t;

  sharedAvl_t *avl_tree;

  int avlNodeWeightCompute(avlNode_t *avl_node);

  void avlNodeNextFind(const unsigned char* const user_node_key, avlNode_t **next);

  void avlNodeFind(const unsigned char* const user_node_key, 
		  	avlNode_t **match, avlNode_t **prev, int *comp_result);

  void avlNodeLowestFind(avlNode_t **match);

  void avlNodeHeightUpdate(avlNode_t *node);

  void avlTreeBalance(avlNode_t *node);

  void avlNodeDelete(avlNode_t *node);

  void avlNodeInsert(const unsigned char* const user_node, avlNode_t *prev, const int comp_result);
};

#endif /* AVL_LIB_H_INCLUDED */
