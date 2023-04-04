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
#ifndef AVL_LIB_H_INCLUDED
#define AVL_LIB_H_INCLUDED
#include <stddef.h>

/* Externally visible AVL tree pointer.
** The user should not directly manipulate the structure elements.
*/
typedef struct 
{
  void *avl_tree;   /* Pointer to the tree control structure */
} lpmhtAvl_t;

/******************************************************************************
** This file contains APIs to manage AVL Trees.
******************************************************************************/

/* Create a new AVL tree with an optional custom key compare function. 
**
** The default key comparison function is memcmp().
**
** avl - (output) Pointer to the AVL tree structure. 
** max_nodes - Maximum number of nodes in the AVL tree.
** node_size - Size of the AVL tree node. This includes the key. 
** key_size - Size of the AVL tree node key. 
** key_compare - The key comparison function. 0 - Use the memcmp() key compare. 
** mem_prealloc - 1 - Pre-allocate all physical memory when the tree is created.
**                0 - Allocate physical memory when node are added, free physical 
**                    memory when nodes are deleted.
**
** Return Values:
**  0 - Tree is created successfully.
** -1 - Error in the input parameters.
** -2 - Tree with specified name already exists.
*/
int lpmhtAvlCreate (lpmhtAvl_t *avl,
                unsigned int max_nodes,
                unsigned int node_size,
                unsigned int key_size,
                int          key_compare(void *, void *),
                unsigned int mem_prealloc);

/* Free all resources associated with the previously created AVL tree.
**
**  avl -  The pointer to the AVL tree.
**
** Return Values:
**  0 - Tree is destroyed successfully.
** -1 - Error.
*/
int lpmhtAvlDestroy (lpmhtAvl_t *avl);

/* Insert the specified node into the AVL tree. 
**  avl - The pointer to the AVL tree.
**  node - The pointer to the node, including the initialized key.
**
** Return Values:
**  0 - Node is inserted successfully.
** -1 - Error in input parameters.
** -2 - Node with the specified key already exists.
*/
int lpmhtAvlInsert (lpmhtAvl_t *avl, void *node);

/* Delete the specified node from the AVL tree. 
**  avl - The pointer to the AVL tree.
**  node - The pointer to the node. Only the key 
**         needs to be initialized.
**
** Return Values:
**  0 - Node is deleted successfully.
** -1 - Error in input parameters.
** -2 - Node with the specified doesn't exists.
*/
int lpmhtAvlDelete (lpmhtAvl_t *avl, void *node);

/* Get the lowest node in the AVL tree.
**  avl - The pointer to the AVL tree.
**  node (input/output) - The pointer to the node.
**        The A{I returns the key and the data.
**
** Return Values:
**  0 - Node is found successfully.
** -1 - Error in input parameters.
** -2 - The tree is empty.
*/ 
int lpmhtAvlFirstGet (lpmhtAvl_t *avl, void *node);

/* Get the next node in the AVL tree.
**  avl - The pointer to the AVL tree.
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
int lpmhtAvlNextGet (lpmhtAvl_t *avl, void *node, void *next_node);

/* Get the number of nodes currently in the AVL tree.
**
**  avl - The pointer to the AVL tree.
**  num_nodes - (output) The current number of nodes.
**  memory_size - (output) Number of physical memory bytes used by the tree.
**  virtual_memory_size - (output) Number of bytes used by the tree.
**
** Return Values:
**  0 - Success.
** -1 - Error in input parameters.
*/
int lpmhtAvlNodeCountGet (lpmhtAvl_t *avl, 
                                unsigned int *num_nodes,
                                size_t *memory_size,
                                size_t *virtual_memory_size);

#endif /* AVL_LIB_H_INCLUDED */
