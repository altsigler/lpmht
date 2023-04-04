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
#ifndef LPMHT_UTIL_H_INCLUDED
#define LPMHT_UTIL_H_INCLUDED

/******************************************************************************
** The following APIs manage the Read/Write locks for
** threads very efficiently.
******************************************************************************/
#ifdef __linux__
#define LPMHT_USE_PRIVATE_RDWR_LOCK

/* Recommend to the kernel to use Huge Pages.
** Using huge pages reduces the number of TLB cache misses and
** improves performance by about 10% to 20% on the test platform.
** Use the "cat /proc/meminfo | grep AnonHuge" while the
** test is running to verify that the feature is working.
**
** Note that the Huge pages are used only when the
** Transparent Huge Pages (THP) feature is enabled on the device.
*/
#define LPMHT_USE_HUGE_PAGES
#endif /* __linux__ */

#ifdef LPMHT_USE_PRIVATE_RDWR_LOCK
typedef struct 
{
  unsigned int val;
} lpmhtRwlock_t;
#else /* Use the standard pthread read/write lock. */
#include <pthread.h>

typedef pthread_rwlock_t lpmhtRwlock_t;
#endif /* LPMHT_USE_PRIVATE_RDWR_LOCK */


void lpmhtRwlockInit(lpmhtRwlock_t *rwlock);
void lpmhtRwlockDestroy(lpmhtRwlock_t *rwlock);
void lpmhtRwlockUnlock(lpmhtRwlock_t *rwlock);
void lpmhtRwlockRdLock(lpmhtRwlock_t *rwlock);
void lpmhtRwlockWrLock(lpmhtRwlock_t *rwlock);

/******************************************************************************
** The following APIs help manage memory.
******************************************************************************/
/* Memory Block.
*/
typedef struct 
{
  unsigned long start_addr;
  unsigned long element_size;
  unsigned long max_elements;
  unsigned long os_page_size;
  unsigned long block_virtual_size; /* virtual allocation size in bytes. */

  unsigned int mem_prealloc; /* Allocate all physical memory during initialization. */

  unsigned int next_free_element;
  unsigned long next_unalloc_page_addr;
} memoryBlock_t;

void *lpmhtMemBlockInit (memoryBlock_t *mb, 
                unsigned long element_size, 
                unsigned long max_elements,
                unsigned int mem_prealloc);
void lpmhtMemBlockDestroy (memoryBlock_t *mb);
int lpmhtMemBlockLastElementGet (const memoryBlock_t *mb, unsigned int *last_used_element);
int lpmhtMemBlockElementAlloc(memoryBlock_t *mb, unsigned int *next_free_element);
int lpmhtMemBlockElementFree(memoryBlock_t *mb);


#endif /* LPMHT_UTIL_H_INCLUDED */
