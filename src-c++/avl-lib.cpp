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
** This file contains APIs to manage AVL trees. See comments in
** avl-api.h for detailed API descriptions.
**
******************************************************************************/
#include <cassert>
#include <cstdlib>
#include <stdexcept>

#include "avl-lib.h"

#if 0
/* The LPMHT uses external locks before calling the AVL functions, 
** so the internal locks are not needed.
*/

/* These locks may be commented out to improve performance, but only if 
** the AVL library is used in a single-process/single-thread environment
** or locking is implemented in the application around all calls to the 
** AVL functions.
*/
#define AVL_TREE_READ_LOCK(avl_tree) (avl_tree->rwlock.rdLock())
#define AVL_TREE_WRITE_LOCK(avl_tree) (avl_tree->rwlock.wrLock())
#define AVL_TREE_READ_UNLOCK(avl_tree) (avl_tree->rwlock.rdUnlock())
#define AVL_TREE_WRITE_UNLOCK(avl_tree) (avl_tree->rwlock.wrUnlock())
#else
#define AVL_TREE_READ_LOCK(avl_tree)
#define AVL_TREE_WRITE_LOCK(avl_tree)
#define AVL_TREE_READ_UNLOCK(avl_tree)
#define AVL_TREE_WRITE_UNLOCK(avl_tree)
#endif

/******************************************************************************
** Figure out the balance factor of the tree node.
**
** avl_node - AVL Tree control structure.
**
******************************************************************************/
int lpmhtAvl::avlNodeWeightCompute(avlNode_t *avl_node)
{
  int left_height;
  int right_height;

  if (avl_node->left)
  {
    left_height = avl_node->left->height + 1;
  }
  else
  {
    left_height = 0;
  }

  if (avl_node->right)
  {
    right_height = avl_node->right->height + 1;
  }
  else
  {
    right_height = 0;
  }

  return (right_height - left_height);
}

/******************************************************************************
** Find the next highest node after the specified key.
** The specified key may match one of the nodes, but it can also contain
** value which is not in the AVL tree.
**
** user_node_key - Pointer to the first byte of the key for which to search.
** next - (output) - Pointer to the next higher node than the search key.
**                   0 - if there are no higher keys in the AVL tree.
**
******************************************************************************/
void lpmhtAvl::avlNodeNextFind(const unsigned char* const user_node_key, 
			avlNode_t **next)
{
  avlNode_t *p;
  int result = 0;
  unsigned char *tree_node_key;

  *next = 0;

  if (avl_tree->root == 0)
  {
    /* Special case when the tree is empty.
     */
    return;
  }

  p = avl_tree->root;
  do
  {
    tree_node_key = ((unsigned char *)p) + sizeof(avlNode_t);

    result = key_compare(user_node_key, tree_node_key);
    if (result < 0)
    {
      if (p->left == 0)
      {
        *next = p;
        break;
      }
      p = p->left;
    }
    else
    {
      if (p->right == 0)
      {
        while (p->parent)
        {
          if (p == p->parent->left)
          {
            *next = p->parent;
            break;
          }
          p = p->parent;
        }
        break;
      }
      p = p->right;
    }
  } while (p);
}

/******************************************************************************
** Find the specified node in the AVL tree.
** If the node is found then the match pointer is set to the node address.
** If the node is not found then the match pointer is set to 0.
** The prev pointer points to the previous entry in the
** AVL tree.
**
** user_node_key - Pointer to the first byte of the key for which to search.
** match - (output) - Pointer to matching tree node. 0 - if there is no match.
** prev - (output) - Pointer to the last examined node before finding or
**                   not finding the matching node. 0 - if the matching node
**                   is the root node or the tree only has the root node,
**                   or the tree is empty.
** comp_result (output) - Result of the last compare operation.
**                         0 - Equal or tree is empty.
**                        -1 - User data is less than tree data,
**                         1 - User data is greater than tree data.
**
******************************************************************************/
void lpmhtAvl::avlNodeFind(const unsigned char* const user_node_key, 
		avlNode_t **match, avlNode_t **prev, int *comp_result)
{
  avlNode_t *p;
  int result = 0;
  unsigned char *tree_node_key;

  *match = *prev = 0;

  if (avl_tree->root == 0)
  {
    /* Special case when the tree is empty.
     */
    *comp_result = 0;
    return;
  }

  p = avl_tree->root;
  do
  {
    tree_node_key = ((unsigned char *)p) + sizeof(avlNode_t);

    result = key_compare(user_node_key, tree_node_key);
    if (result == 0)
    {
      *match = p;
      break;
    }
    *prev = p;
    if (result < 0)
    {
      p = p->left;
    }
    else
    {
      p = p->right;
    }
  } while (p);

  *comp_result = result;
}

/******************************************************************************
** Find the lowest node in the AVL tree.
** If the node is found then the match pointer is set to the node address.
** If the node is not found then the match pointer is set to 0.
**
** user_node_key - Pointer to the first byte of the key for which to search.
** match - (output) - Pointer to matching tree node. 0 - if there is no match.
**
******************************************************************************/
void lpmhtAvl::avlNodeLowestFind(avlNode_t **match)
{
  avlNode_t *p = avl_tree->root;

  *match = 0;
  if (p == 0)
  {
    return;
  }
  while (p->left)
  {
    p = p->left;
  }
  *match = p;
}

/******************************************************************************
** Compute the height of the specified node and set its height attribute.
**
******************************************************************************/
void lpmhtAvl::avlNodeHeightUpdate(avlNode_t *node)
{
  if (node->left == 0)
  {
    if (node->right == 0)
    {
      node->height = 0;
    }
    else
    {
      node->height = node->right->height + 1;
    }
  }
  else if (node->right == 0)
  {
    node->height = node->left->height + 1;
  }
  else
  {
    /* Node has right and left child nodes, so select the
    ** larger height.
    */
    if (node->left->height > node->right->height)
    {
      node->height = node->left->height + 1;
    }
    else
    {
      node->height = node->right->height + 1;
    }
  }
}

/******************************************************************************
** Re-Balance the AVL tree.
**
******************************************************************************/
void lpmhtAvl::avlTreeBalance(avlNode_t *node)
{
  int weight;
  avlNode_t *p = 0, *q;

  while (node)
  {
    avlNodeHeightUpdate(node);
    weight = avlNodeWeightCompute(node);
    if ((weight >= -1) && (weight <= 1))
    {
      /* The sub-tree below this node is balanced,
      ** so go to the parent node.
      */
      node = node->parent;
      continue;
    }

    /* Balance the Sub-tree. There are four cases that need to be handled.
     */
    do /* Single-iteration loop  */
    {
      /* Case-1 - Single Right Rotation
       */
      if ((weight < 0) && (0 >= avlNodeWeightCompute(node->left)))
      {
        p = node->left;
        node->left = p->right;
        if (node->left)
        {
          node->left->parent = node;
        }
        p->right = node;
        avlNodeHeightUpdate(node);
        avlNodeHeightUpdate(p);
        break;
      }

      /* Case-2 - Single Left Rotation
       */
      if ((weight > 0) && (0 <= avlNodeWeightCompute(node->right)))
      {
        p = node->right;
        node->right = p->left;
        if (node->right)
        {
          node->right->parent = node;
        }
        p->left = node;
        avlNodeHeightUpdate(node);
        avlNodeHeightUpdate(p);
        break;
      }

      /* Case-3 - Left-Right Rotation
       */
      if ((weight < 0) && (0 < avlNodeWeightCompute(node->left)))
      {
        /* Rotate left subtree to the left.
         */
        p = node->left;
        q = p->right;
        p->right = q->left;
        if (p->right)
        {
          p->right->parent = p;
        }
        node->left = q;
        q->parent = node;
        q->left = p;
        p->parent = q;
        avlNodeHeightUpdate(q);
        avlNodeHeightUpdate(p);
        avlNodeHeightUpdate(node);

        /* Rotate to the right.
         */
        p = node->left;
        node->left = p->right;
        if (node->left)
        {
          node->left->parent = node;
        }
        p->right = node;
        avlNodeHeightUpdate(node);
        avlNodeHeightUpdate(p);
        break;
      }

      /* Case-4 - Right-Left Rotation
       */
      if ((weight > 0) && (0 > avlNodeWeightCompute(node->right)))
      {
        /* Rotate right subtree to the right.
         */
        p = node->right;
        q = p->left;
        p->left = q->right;
        if (p->left)
        {
          p->left->parent = p;
        }
        node->right = q;
        q->parent = node;
        q->right = p;
        p->parent = q;
        avlNodeHeightUpdate(q);
        avlNodeHeightUpdate(p);
        avlNodeHeightUpdate(node);

        /* Rotate to the left.
         */
        p = node->right;
        node->right = p->left;
        if (node->right)
        {
          node->right->parent = node;
        }
        p->left = node;
        avlNodeHeightUpdate(node);
        avlNodeHeightUpdate(p);
        break;
      }

      /* If we didn't hit one of the above cases then there is an error
      ** in the tree caused by memory corruption or a logic error in the AVL
      ** library.
      */
      throw ERR_MSG ("AVL Tree Error");

    } while (0);

    if (avl_tree->root == node)
    {
      avl_tree->root = p;
    }
    else
    {
      if (node->parent->right == node)
      {
        node->parent->right = p;
      }
      else
      {
        node->parent->left = p;
      }
    }
    assert(p);
    p->parent = node->parent;
    node->parent = p;

    /* Move up the tree to the next node.
     */
    node = p->parent;
  }
}

/******************************************************************************
** Delete the node from the AVL tree.
**
******************************************************************************/
void lpmhtAvl::avlNodeDelete(avlNode_t *node)

{
  avlNode_t *parent = 0, *child;

  do /* Single-iteration loop  */
  {
    /* Case-1 - The node has left and right child.
     */

    /* Find the lowest child in the right subtree and replace
    ** the node with that child.
    */
    if ((node->left) && (node->right))
    {
      child = node->right;
      while (child->left)
      {
        child = child->left;
      }
      memcpy(((unsigned char *)node) + sizeof(avlNode_t), ((unsigned char *)child) + sizeof(avlNode_t),
             avl_tree->node_size);

      /* Set the node to the lowest child and delete that node
      ** using either Case-2 or Case-3 below.
      */
      node = child;
    }

    /* Case-2 - The node doesn't have any child nodes.
     */
    if ((node->left == 0) && (node->right == 0))
    {
      if (node == avl_tree->root)
      {
        avl_tree->root = 0;
        parent = 0;
      }
      else
      {
        parent = node->parent;
        if (parent->left == node)
        {
          parent->left = 0;
        }
        else
        {
          parent->right = 0;
        }
      }
      break;
    }

    /* Case-3 - The node has has either left or right child, but
    **          not both.
    */
    if (node->left)
    {
      child = node->left;
    }
    else
    {
      child = node->right;
    }
    if (node == avl_tree->root)
    {
      parent = 0;
      avl_tree->root = child;
    }
    else
    {
      parent = node->parent;
      if (parent->left == node)
      {
        parent->left = child;
      }
      else
      {
        parent->right = child;
      }
    }
    child->parent = parent;
  } while (0);

  /* Update height of all the parent nodes.
   */
  avlTreeBalance(parent);

  /* Free the node.
  ** We can only free the last allocated node, so if this node is
  ** not the last node then we need to copy the content of the last
  ** node into this node, and free the last node.
  */
  {
    avlNode_t *p;
    unsigned int idx;

    if (avl_tree->node_mb.lastElementGet(&idx))
      (assert(0), abort());

    p = (avlNode_t *)(((unsigned char *)avl_tree->avl_node) + (idx * avl_tree->alloc_node_size));

    if (p != node)
    {
      /* We are not freeing the last allocated node, so need to
      ** copy the content of the last node into this node, and
      ** update the tree structure.
      */
      memcpy(node, p, avl_tree->alloc_node_size);
      if (node->right)
        node->right->parent = node;

      if (node->left)
        node->left->parent = node;

      if (node->parent)
      {
        if (node->parent->right == p)
          node->parent->right = node;
        else
          node->parent->left = node;
      }
      if (avl_tree->root == p)
        avl_tree->root = node;
    }
    (void)avl_tree->node_mb.elementFree();

    if (0 == avl_tree->mem_prealloc)
      avl_tree->memory_size -= avl_tree->alloc_node_size;
  }
}

/******************************************************************************
** Insert the new node into the AVL tree.
** The prev points to the AVL node after which the new node needs to be
** inserted.
**
******************************************************************************/
void lpmhtAvl::avlNodeInsert(const unsigned char*  const user_node, 
				avlNode_t *prev, const int comp_result)
{
  avlNode_t *p;
  unsigned char *tree_node; /* Start of user data in the node. */
  unsigned int idx;

  /* Get node from the free list. Since we already performed error
  ** checking, assume that the free list is not empty.
  */
  if (avl_tree->node_mb.elementAlloc(&idx))
       throw std::runtime_error("Empty free list.");

  p = (avlNode_t *)(((unsigned char *)avl_tree->avl_node) + (idx * avl_tree->alloc_node_size));
  memset(p, 0, avl_tree->alloc_node_size);

  if (0 == avl_tree->mem_prealloc)
    avl_tree->memory_size += avl_tree->alloc_node_size;

  p->left = 0;
  p->right = 0;
  p->height = 0;
  tree_node = ((unsigned char *)p) + sizeof(avlNode_t);

  /* Copy user data into the new node.
   */
  memcpy(tree_node, user_node, avl_tree->node_size);

  if (avl_tree->root == 0)
  {
    /* Special case when the tree is empty.
     */
    avl_tree->root = p;
    p->parent = 0;
    return;
  }

  p->parent = prev;
  if (comp_result < 0)
  {
    assert(prev);
    prev->left = p;
  }
  else
  {
    assert(prev);
    prev->right = p;
  }

  avlTreeBalance(p);

  return;
}

/******************************************************************************
*******************************************************************************
** API Functions
*******************************************************************************
******************************************************************************/

/******************************************************************************
** Create a new AVL tree.
******************************************************************************/
lpmhtAvl::lpmhtAvl(const unsigned int max_nodes, 
		const unsigned int node_size, 
		const unsigned int key_size, unsigned int mem_prealloc)
{
  unsigned int alloc_node_size;
  avlNode_t *avl_node;

  /* Check input parameters.
   */
  if (node_size < key_size)
  {
    throw std::runtime_error("AVL Key Size is larger than Node Size.");
  }
  if (node_size == 0)
  {
    throw std::runtime_error("AVL Node Size is 0.");
  }
  if (key_size == 0)
  {
    throw std::runtime_error("AVL Key Size is 0.");
  }

  do /* Single-iteration loop - Used for error checking. */
  {
    /* Make sure that AVL nodes are aligned on an 8-byte boundary.
     */
    alloc_node_size = node_size + sizeof(avlNode_t);
    if (alloc_node_size % 8)
      alloc_node_size = ((alloc_node_size / 8) + 1) * 8;

    avl_tree = (sharedAvl_t *)calloc(1, sizeof(sharedAvl_t));
    assert(avl_tree);

    avl_node = (avlNode_t *)avl_tree->node_mb.init(alloc_node_size, max_nodes, mem_prealloc);

    avl_tree->mem_prealloc = mem_prealloc;
    avl_tree->avl_node = avl_node;
    avl_tree->virtual_memory_size = avl_tree->node_mb.blockVirtualSizeGet();
    if (mem_prealloc)
      avl_tree->memory_size = avl_tree->virtual_memory_size;
    else
      avl_tree->memory_size = 0;
    avl_tree->key_size = key_size;
    avl_tree->node_size = node_size;
    avl_tree->alloc_node_size = alloc_node_size;
    avl_tree->max_nodes = max_nodes;

  } while (0);
}
/******************************************************************************
** Delete an existing AVL tree.
******************************************************************************/
lpmhtAvl::~lpmhtAvl(void)
{
  avl_tree->node_mb.destroy();

  free(avl_tree);
}

/******************************************************************************
** Insert the specified node into the AVL tree.
******************************************************************************/
int lpmhtAvl::insert(void *node)
{
  int rc = -1;
  avlNode_t *match, *prev;
  int comp_result;

  if (node == 0)
  {
    return rc;
  }

  AVL_TREE_WRITE_LOCK(avl_tree);
  do /* Single-iteration loop - Used for error checking. */
  {
    if (avl_tree->num_nodes == avl_tree->max_nodes)
    {
      break;
    }

    /* Search the tree and find the matching node and the previous node.
     */
    avlNodeFind((unsigned char *)node, &match, &prev, &comp_result);

    /* If this node is already in the tree then return an error.
     */
    if (match)
    {
      rc = -2;
      break;
    }

    /* Insert the new node into the AVL tree.
     */
    avlNodeInsert((unsigned char *)node, prev, comp_result);

    avl_tree->num_nodes++;

    rc = 0;
  } while (0);
  AVL_TREE_WRITE_UNLOCK(avl_tree);

  return rc;
}

/******************************************************************************
** Delete the specified node from the AVL tree.
******************************************************************************/
int lpmhtAvl::nodeDelete(void *node)
{
  int rc = -1;
  avlNode_t *match, *prev;
  int comp_result;

  if (node == 0)
  {
    return rc;
  }

  AVL_TREE_WRITE_LOCK(avl_tree);
  do /* Single-iteration loop - Used for error checking. */
  {
    if (avl_tree->num_nodes == 0)
    {
      /* The tree is empty.
       */
      rc = -2;
      break;
    }

    /* Search the tree and find the matching node and the previous node.
     */
    avlNodeFind((unsigned char *)node, &match, &prev, &comp_result);

    /* If this node doesn't exist then return an error.
     */
    if (match == 0)
    {
      rc = -2;
      break;
    }

    /* Delete the node from the AVL tree.
     */
    avlNodeDelete(match);

    avl_tree->num_nodes--;

    rc = 0;
  } while (0);
  AVL_TREE_WRITE_UNLOCK(avl_tree);

  return rc;
}

/******************************************************************************
** Get the lowest node in the tree.
******************************************************************************/
int lpmhtAvl::firstGet(void *node)
{
  int rc = -1;
  avlNode_t *match;
  unsigned char *tree_node_data;
  unsigned char *user_node_data;

  if (node == 0)
  {
    return rc;
  }

  AVL_TREE_READ_LOCK(avl_tree);
  do /* Single-iteration loop - Used for error checking. */
  {
    /* Search the tree and find the lowest node.
     */
    avlNodeLowestFind(&match);

    /* If this node is not found then return an error.
     */
    if (0 == match)
    {
      rc = -2;
      break;
    }

    /* Retrieve the content of the next node.
    ** We have to return the key and data.
    */
    user_node_data = (unsigned char *)node;
    tree_node_data = ((unsigned char *)match) + sizeof(avlNode_t);
    memcpy(user_node_data, tree_node_data, avl_tree->node_size);

    rc = 0;
  } while (0);
  AVL_TREE_READ_UNLOCK(avl_tree);

  return rc;
}

/******************************************************************************
** Get the content of the next node.
******************************************************************************/
int lpmhtAvl::nextGet(void *node, void *next_node)
{
  int rc = -1;
  avlNode_t *match;
  unsigned char *tree_node_data;
  unsigned char *user_node_data;

  if ((node == 0) || (next_node == 0))
  {
    return rc;
  }

  AVL_TREE_READ_LOCK(avl_tree);
  do /* Single-iteration loop - Used for error checking. */
  {
    /* Search the tree and find the next node.
     */
    avlNodeNextFind((unsigned char *)node, &match);

    /* If the next node is not found then return an error.
     */
    if (0 == match)
    {
      rc = -2;
      break;
    }

    /* Retrieve the content of the next node.
    ** We have to return the key and data.
    */
    user_node_data = (unsigned char *)next_node;
    tree_node_data = ((unsigned char *)match) + sizeof(avlNode_t);
    memcpy(user_node_data, tree_node_data, avl_tree->node_size);

    rc = 0;
  } while (0);
  AVL_TREE_READ_UNLOCK(avl_tree);

  return rc;
}

/******************************************************************************
** Get the number of nodes currently inserted in the AVL tree.
******************************************************************************/
int lpmhtAvl::nodeCountGet(unsigned int *num_nodes, size_t *memory_size, size_t *virtual_memory_size)
{
  int rc = -1;

  if (num_nodes == 0)
  {
    return rc;
  }

  AVL_TREE_READ_LOCK(avl_tree);
  do /* Single-iteration loop - Used for error checking. */
  {
    *num_nodes = avl_tree->num_nodes;
    *memory_size = avl_tree->memory_size;
    *virtual_memory_size = avl_tree->virtual_memory_size;

    rc = 0;
  } while (0);
  AVL_TREE_READ_UNLOCK(avl_tree);

  return rc;
}
