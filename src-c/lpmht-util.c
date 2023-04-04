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
** March-30-2023/Andrey Tsigler
******************************************************************************/
/******************************************************************************
** This file contains utility functions to help with the lpmt library.
**
******************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "lpmht-util.h"

#ifdef LPMHT_USE_PRIVATE_RDWR_LOCK
#include <linux/futex.h>
#include <sys/syscall.h>
#include <limits.h>
#include <sched.h>

/* Either reader or writer is blocked, waiting on this lock.
*/
#define LPMHT_RWLOCK_WAIT_FLAG 0x80000000

/* Writer currently owns this lock.
*/
#define LPMHT_RWLOCK_OWNED_FLAG 0x40000000

/* The owner counter mask.
*/
#define LPMHT_OWNER_COUNT_MASK 0x3FFFFFFF

#define lpmht_futex(a,b,c,d,e,f) syscall (SYS_futex, a, b, c, d, e, f)

/* Static initializer for the private Read/Write lock.
*/
#define LPMHT_RWLOCK_INIT_PRIVATE {0}
#endif /* LPMHT_USE_PRIVATE_RDWR_LOCK */

/******************************************************************************
** Initialize the RW lock.
** This function doesn't allocate any memory, so there is no corresponding
** function to delete the RW lock.
**
** The caller must allocate memory for the lock before calling this function.
**
**   rwlock - (output) Pointer to the RW lock.
**
******************************************************************************/
void lpmhtRwlockInit(lpmhtRwlock_t *rwlock)     
{                                               
#ifdef LPMHT_USE_PRIVATE_RDWR_LOCK
  (rwlock)->val = 0;                             
#else
  (void) pthread_rwlock_init (rwlock, 0);
#endif
}

/******************************************************************************
** Destroy the RW lock.
**
**   rwlock - (output) Pointer to the RW lock.
**
******************************************************************************/
void lpmhtRwlockDestroy(lpmhtRwlock_t *rwlock)     
{                                               
#ifdef LPMHT_USE_PRIVATE_RDWR_LOCK
  /* Nothing to do for lpmht locks. */
#else
  (void) pthread_rwlock_destroy (rwlock);
#endif
}

/******************************************************************************
** Unlock the Read/Write lock.
** The Read/Write lock must be locked when calling this function.
** If this function is called for an unlocked lock the the lock state
** becomes corrupted.
**
**   rwlock - (input) Pointer to the RW lock.
**
** Return Values:
**   None
******************************************************************************/
void lpmhtRwlockUnlock(lpmhtRwlock_t *rwlock)                                              
{                                                                              
#ifdef LPMHT_USE_PRIVATE_RDWR_LOCK
  if (0 != ((LPMHT_RWLOCK_WAIT_FLAG | LPMHT_RWLOCK_OWNED_FLAG) &              
                          __atomic_fetch_sub(&(rwlock)->val,  1, __ATOMIC_SEQ_CST)))                
  {                                                                             
    if (LPMHT_RWLOCK_WAIT_FLAG &                                               
                    __atomic_fetch_and (&(rwlock)->val, LPMHT_OWNER_COUNT_MASK, __ATOMIC_SEQ_CST)) 
    {                                                                           
      (void) lpmht_futex (&(rwlock)->val,                                      
                      FUTEX_WAKE_PRIVATE,         
                      INT_MAX, 0, 0, 0);                                        
    }                                                                           
  }                                                                             
#else
  (void)  pthread_rwlock_unlock(rwlock);
#endif
}

/******************************************************************************
** Lock the Read/Write lock in "Read" mode.
**
** Multiple threads can obtain the lock in "Read" mode at the same time.
**
** If another thread or process either already owns the lock in "Write" mode
** or is waiting for the "Write" lock then the caller blocks until the 
** writer releases the lock.
**
** If the lock is already locked in "Write" mode by the same thread or process
** then the function blocks forever causing a deadlock. Therefore a thread 
** should never try to obtain the read lock while already holding the 
** write lock.
**
**   rwlock - (input) Pointer to the RW lock.
**
** Return Values:
**   None
******************************************************************************/
void  lpmhtRwlockRdLock(lpmhtRwlock_t *rwlock)                                              
{                                                                              
#ifdef LPMHT_USE_PRIVATE_RDWR_LOCK
  if (0 != ((LPMHT_RWLOCK_WAIT_FLAG | LPMHT_RWLOCK_OWNED_FLAG) &              
                          __atomic_fetch_add(&(rwlock)->val,  1, __ATOMIC_SEQ_CST)))                
  {                                                                             
    unsigned int rwlock_val = __atomic_fetch_sub(&(rwlock)->val,  1, __ATOMIC_SEQ_CST);             
    if (--rwlock_val & LPMHT_RWLOCK_WAIT_FLAG)                                 
    {                                                                           
     rwlock_val = __atomic_fetch_and(&(rwlock)->val, ~LPMHT_RWLOCK_WAIT_FLAG, __ATOMIC_SEQ_CST);   
     (void) lpmht_futex (&(rwlock)->val,                                       
                      FUTEX_WAKE_PRIVATE,         
                      INT_MAX, 0, 0, 0);                                        
     rwlock_val &= ~LPMHT_RWLOCK_WAIT_FLAG;                                    
    }                                                                           
    do                                                                          
    {                                                                           
      if (rwlock_val & (LPMHT_RWLOCK_WAIT_FLAG | LPMHT_RWLOCK_OWNED_FLAG))    
      {                                                                         
        if (0 == (rwlock_val & LPMHT_RWLOCK_WAIT_FLAG))                        
        {                                                                       
          if (0 == __atomic_compare_exchange_n(&(rwlock)->val, &rwlock_val,  
                                rwlock_val | LPMHT_RWLOCK_WAIT_FLAG,
				0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))          
          {                                                                     
            sched_yield();                                                      
            continue;                                                           
          }                                                                     
        }                                                                       
        (void) lpmht_futex (&(rwlock)->val,                                    
                 FUTEX_WAIT_PRIVATE,                
                 rwlock_val | LPMHT_RWLOCK_WAIT_FLAG,                          
                 0, 0, 0);                                                      
        rwlock_val = __atomic_load_n(&(rwlock)->val, __ATOMIC_SEQ_CST);                               
      } else                                                                    
      {                                                                         
        if (__atomic_compare_exchange_n(&(rwlock)->val, &rwlock_val,         
                                (rwlock_val + 1),
				0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))                              
        {                                                                       
          break;                                                                
        }                                                                       
        sched_yield();                                                          
      }                                                                         
    } while (1);                                                                
  }                                                                             
#else
  (void) pthread_rwlock_rdlock(rwlock);
#endif
}


/******************************************************************************
** Lock the Read/Write lock in "Write" mode.
**
** Only one thread can obtain the lock in "Write" mode at any one time.
**
** If another thread or process already owns the lock either in "Read" or
** "Write" mode or is waiting for the "Write" lock then the caller 
** blocks until the writer/reader releases the lock.
**
** A thread should never try to obtain the "Write" lock if at already owns
** the lock either in "Read" or "Write" mode. This operation causes a 
** deadlock.
**
**   rwlock - (input) Pointer to the RW lock.
**
** Return Values:
**   None
******************************************************************************/
void lpmhtRwlockWrLock(lpmhtRwlock_t *rwlock)                                              
{                                                                              
#ifdef LPMHT_USE_PRIVATE_RDWR_LOCK
  unsigned int rwlock_val = 0;                                                  

  do                                                                            
  {                                                                             
    if (rwlock_val)                                                             
    {                                                                           
      if ((rwlock_val & LPMHT_RWLOCK_WAIT_FLAG) == 0)                          
      {                                                                         
        if (0 == __atomic_compare_exchange_n(&(rwlock)->val,                 
                           &rwlock_val, rwlock_val | LPMHT_RWLOCK_WAIT_FLAG,
				0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))  
        {                                                                       
          sched_yield();                                                        
          continue;                                                            
        }                                                                       
      }                                                                         
      (void) lpmht_futex (&(rwlock)->val,                                      
                FUTEX_WAIT_PRIVATE,                 
                rwlock_val | LPMHT_RWLOCK_WAIT_FLAG,                           
                0, 0, 0);                                                       
      rwlock_val = __atomic_load_n(&(rwlock)->val, __ATOMIC_SEQ_CST);                                 
    } else                                                                     
    {                                                                           
      if (__atomic_compare_exchange_n(&(rwlock)->val, &rwlock_val,           
                              (rwlock_val + 1) | LPMHT_RWLOCK_OWNED_FLAG,
		      		0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))     
      {                                                                         
        break;                                                                  
      }                                                                         
      sched_yield();                                                            
    }                                                                           
  } while (1);                                                                  
#else
  (void) pthread_rwlock_wrlock(rwlock);
#endif
}

/******************************************************************************
** Initialize the memory block control structure and allocate memory
** for the memory block. 
** The allocated memory is not initialized, so only virtual memory is reserved, 
** while no physical memory is used.
**
** If the virtual memory is not available then the code crashes.
**
**   mb - (output) Pointer to the Memory Block Control Structure. The memory
**		   for the control structure is allocated by the caller.
**   element_size - Number of bytes in each element stored in the memory block.
**   max_elements - Maximum number of elements that can be stored in the 
**                  memory block. 
**   mem_prealloc - Allocate all memory on MemBlockInit. Don't free memory
**                  when elements are deleted.
**
** Return Values:
**   Pointer to the allocated memory block.
******************************************************************************/
void *lpmhtMemBlockInit (memoryBlock_t *mb, 
		         unsigned long element_size, 
			 unsigned long max_elements,
                         unsigned int mem_prealloc)
{
  void *ptr;

  memset (mb, 0, sizeof(memoryBlock_t));
  mb->os_page_size = sysconf(_SC_PAGESIZE);
  mb->element_size = element_size;
  mb->max_elements = max_elements;
  mb->mem_prealloc = mem_prealloc;
  
  mb->block_virtual_size = element_size * max_elements; 
  if (mb->block_virtual_size % mb->os_page_size)
	  mb->block_virtual_size = ((mb->block_virtual_size / 
				          mb->os_page_size) + 1) *
		  			            mb->os_page_size;
  ptr = aligned_alloc(mb->os_page_size, mb->block_virtual_size);
  if (!ptr) 
	  (perror("aligned_alloc"), assert(0), abort());
  mb->start_addr = (unsigned long) ptr;
  mb->next_unalloc_page_addr = mb->start_addr;

#ifdef LPMHT_USE_HUGE_PAGES
  (void) madvise (ptr, mb->block_virtual_size, MADV_HUGEPAGE);
#endif

  if (mem_prealloc)
        memset (ptr, 0, mb->block_virtual_size);

  return ptr;
}

/******************************************************************************
** Free memory associated with the memory block.
**
**   mb -  Pointer to the Initialized Memory Block Control Structure.
**
** Return Values:
**   None
******************************************************************************/
void lpmhtMemBlockDestroy (memoryBlock_t *mb)
{
  free ((void *) mb->start_addr);
  memset (mb, 0, sizeof(memoryBlock_t));
}

/******************************************************************************
** Allocate an element number from the memory block.
** The first allocated element is 0, followed by 1, 2, 3...
** This continues until all the elements in the memory block are allocated.
**
**   mb -  Pointer to the Initialized Memory Block Control Structure.
**   next_free_element - (output) The next available element number.
**
** Return Values:
**   0 - OK
**  -1 - The maximum number of elements have already been allocated.
******************************************************************************/
int lpmhtMemBlockElementAlloc(memoryBlock_t *mb, unsigned int *next_free_element)
{
  if (mb->next_free_element >= mb->max_elements)
	  	return -1;   /* The memory is full. */

  *next_free_element = mb->next_free_element;
  mb->next_free_element++;

  while ((mb->next_unalloc_page_addr < 
		  (mb->start_addr + (mb->element_size * mb->next_free_element))) &&
         (mb->next_unalloc_page_addr < (mb->start_addr + mb->block_virtual_size)))
	                             mb->next_unalloc_page_addr += mb->os_page_size;

  return 0;
}

/******************************************************************************
** Return a no longer needed element to the memory block.
**
** Note that the function doesn't accept the element number to be returned.
** This is because only the most recently allocated element can be returned 
** to the memory block. This design point puts the responsibility on the 
** caller to copy data from the last allocated element to the lower numbered
** element which is now available, prior to calling this function.
**
** When enough elements have been freed to make up one OS page, tell the 
** kernel to free up that page. 
**
**   mb -  Pointer to the Initialized Memory Block Control Structure.
**
** Return Values:
**   0 - OK
**  -1 - All elements in the memory block are already free.
******************************************************************************/
int lpmhtMemBlockElementFree(memoryBlock_t *mb)
{
  if (0 == mb->next_free_element)
	  	return -1;   /* Empty Memory Block */

  mb->next_free_element--;

  /* Check if we can free up physical pages.
  */
  while ((mb->next_unalloc_page_addr > mb->start_addr) &&
	  ((mb->start_addr + (mb->element_size * mb->next_free_element)) < 
		  (mb->next_unalloc_page_addr - mb->os_page_size)))
  {
    mb->next_unalloc_page_addr -= mb->os_page_size;

    if (0 == mb->mem_prealloc)
    {
      /* The MADV_DONTNEED frees the pages immediately on Linux.
      ** Using this command instead of MADV_FREE makes it easier to confirm
      ** that the memory is freed when routes are deleted from the route
      ** table. Use the command "cat /proc/<pid>/status | grep RSS" to see
      ** the physical memory usage of a process.
      */
      if (madvise ((void *) mb->next_unalloc_page_addr, 
			      mb->os_page_size, MADV_DONTNEED))
      {
        perror ("madvise MADV_DONTNEED");
      }
    }
  }

  return 0;
}

/******************************************************************************
** Get the last allocated element number.
** This function may be used to figure out which elements need to be swapped
** prior to calling the ElementFree function.
**
**   mb -  Pointer to the Initialized Memory Block Control Structure.
**   last_used_element - (output) The most recently allocated element number.
**
** Return Values:
**   0 - OK
**  -1 - All elements are unallocated in this memory block.
******************************************************************************/
int lpmhtMemBlockLastElementGet (const memoryBlock_t *mb, 
		unsigned int *last_used_element)
{
  if (0 == mb->next_free_element)
	  	return -1;   /* The memory block is empty. */

  *last_used_element = mb->next_free_element - 1;

  return 0;
}



