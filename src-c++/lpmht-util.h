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
#ifndef LPMHT_UTIL_H_INCLUDED
#define LPMHT_UTIL_H_INCLUDED
/******************************************************************************
** LATEST REVISION/AUTHOR
** January-09-2024/Andrey Tsigler
******************************************************************************/
/******************************************************************************
** This file contains utility functions to help with the lpmht library.
**
******************************************************************************/
#include <stdexcept>
#include <cstring>
#include <source_location>
#include <atomic>
#include <shared_mutex>

#include <sys/mman.h>
#include <sched.h>

#include <unistd.h> // Needed for _SC_PAGESIZE on MACOS 


/* The private locks improve performance.
** The private locks use atomic variables to implement the 
** read/write lock.
** Comment out this define to use the C++ shared_mutex 
** to implement the read/write lock.
*/
#define LPMHT_USE_PRIVATE_RDWR_LOCK

#ifdef __linux__
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
/* Either reader or writer is blocked, waiting on this lock.
 */
#define LPMHT_RWLOCK_WAIT_FLAG 0x80000000

/* Writer currently owns this lock.
 */
#define LPMHT_RWLOCK_OWNED_FLAG 0x40000000

/* The owner counter mask.
 */
#define LPMHT_OWNER_COUNT_MASK 0x3FFFFFFF
#endif /* LPMHT_USE_PRIVATE_RDWR_LOCK */

inline std::runtime_error ERR_MSG (const std::string message,
			const std::source_location& loc = std::source_location::current())
{
  return std::runtime_error(std::string(loc.file_name()) + 
		  ":" + std::to_string(loc.line()) + " - " + message);
}

/******************************************************************************
** Tell the system to use huge pages if the feature is supported.
**
**   addr -  Start of the address block for huge pages.
**   length - Number of bytes in the address block.
**
******************************************************************************/
class lpmhtUtil
{
public:
  void useHugePages(void *addr, size_t length)
  {
#ifdef LPMHT_USE_HUGE_PAGES
    (void)madvise(addr, length, MADV_HUGEPAGE);
#else
    // Avoid unused variable warning when the feature is not enabled.
    (void)addr;
    (void)length;
#endif
  }
}; // End of class lpmhtUtil

#ifndef LPMHT_USE_PRIVATE_RDWR_LOCK
class lpmhtRwlock
{
private:
  std::shared_mutex val;
public:
  void rdUnlock() { val.unlock_shared(); }
  void wrUnlock() { val.unlock(); }
  void rdLock() { val.lock_shared(); }
  void wrLock() { val.lock(); }
};
#else
class lpmhtRwlock
{
private:
  std::atomic<unsigned int> val = 0;

public:
  /******************************************************************************
  ** Unlock the Read lock.
  ** The Read/Write lock must be locked when calling this function.
  ** If this function is called for an unlocked lock the the lock state
  ** becomes corrupted.
  **
  ** Return Values:
  **   None
  ******************************************************************************/
  void rdUnlock()
  {
    if (0 != ((LPMHT_RWLOCK_WAIT_FLAG | LPMHT_RWLOCK_OWNED_FLAG) & 
			    std::atomic_fetch_sub(&val, 1)))
    {
      if (LPMHT_RWLOCK_WAIT_FLAG & std::atomic_fetch_and(&val, LPMHT_OWNER_COUNT_MASK))
      {
          val.notify_all();
      }
    }
  }

  /******************************************************************************
  ** Unlock the Write lock.
  ** The Read/Write lock must be locked when calling this function.
  ** If this function is called for an unlocked lock the the lock state
  ** becomes corrupted.
  **
  ** Return Values:
  **   None
  ******************************************************************************/
  void wrUnlock()
  {
    rdUnlock();
  }

  /******************************************************************************
  ** Lock the Read/Write lock in "Read" mode.
  **
  ** Multiple threads can obtain the lock in "Read" mode at the same time.
  **
  ** If another thread either already owns the lock in "Write" mode
  ** or is waiting for the "Write" lock then the caller blocks until the
  ** writer releases the lock.
  **
  ** If the lock is already locked in "Write" mode by the same thread 
  ** then the function blocks forever causing a deadlock. Therefore a thread
  ** should never try to obtain the read lock while already holding the
  ** write lock.
  **
  ** Return Values:
  **   None
  ******************************************************************************/
  void rdLock()
  {
    if (0 != ((LPMHT_RWLOCK_WAIT_FLAG | LPMHT_RWLOCK_OWNED_FLAG) & 
			    std::atomic_fetch_add(&val, 1)))
    {
      unsigned int rwlock_val = std::atomic_fetch_sub(&val, 1);
      if (--rwlock_val & LPMHT_RWLOCK_WAIT_FLAG)
      {
        rwlock_val = std::atomic_fetch_and(&val, ~LPMHT_RWLOCK_WAIT_FLAG);
        val.notify_all();
        rwlock_val &= ~LPMHT_RWLOCK_WAIT_FLAG;
      }
      do
      {
        if (rwlock_val & (LPMHT_RWLOCK_WAIT_FLAG | LPMHT_RWLOCK_OWNED_FLAG))
        {
          if (0 == (rwlock_val & LPMHT_RWLOCK_WAIT_FLAG))
          {
            if (0 == std::atomic_compare_exchange_strong(&val, &rwlock_val, 
				    rwlock_val | LPMHT_RWLOCK_WAIT_FLAG))
            {
              sched_yield();
              continue;
            }
          }
	  val.wait (rwlock_val | LPMHT_RWLOCK_WAIT_FLAG);
          rwlock_val = std::atomic_load(&val);
        }
        else
        {
          if (std::atomic_compare_exchange_strong(&val, 
				  &rwlock_val, (rwlock_val + 1)))
          {
            break;
          }
          sched_yield();
        }
      } while (1);
    }
  }

  /******************************************************************************
  ** Lock the Read/Write lock in "Write" mode.
  **
  ** Only one thread can obtain the lock in "Write" mode at any one time.
  **
  ** If another thread already owns the lock either in "Read" or
  ** "Write" mode or is waiting for the "Write" lock then the caller
  ** blocks until the writer/reader releases the lock.
  **
  ** A thread should never try to obtain the "Write" lock if at already owns
  ** the lock either in "Read" or "Write" mode. This operation causes a
  ** deadlock.
  **
  ** Return Values:
  **   None
  ******************************************************************************/
  void wrLock()
  {
    unsigned int rwlock_val = 0;

    do
    {
      if (rwlock_val)
      {
        if ((rwlock_val & LPMHT_RWLOCK_WAIT_FLAG) == 0)
        {
          if (0 == std::atomic_compare_exchange_strong(&val, &rwlock_val, 
				  rwlock_val | LPMHT_RWLOCK_WAIT_FLAG))
          {
            sched_yield();
            continue;
          }
        }
	val.wait (rwlock_val | LPMHT_RWLOCK_WAIT_FLAG);
        rwlock_val = std::atomic_load(&val);
      }
      else
      {
        if (std::atomic_compare_exchange_strong(&val, &rwlock_val, 
				(rwlock_val + 1) | LPMHT_RWLOCK_OWNED_FLAG))
        {
          break;
        }
        sched_yield();
      }
    } while (1);
  }
}; // End of class lpmhtRwlock
#endif  // LPMHT_USE_PRIVATE_RDWR_LOCK


/******************************************************************************
** The following APIs help manage memory.
******************************************************************************/
/* Memory Block.
 */
class lpmhtMemBlock
{
private:
  unsigned long _start_addr = 0;
  unsigned long _element_size = 0;
  unsigned long _max_elements = 0;
  unsigned long _os_page_size = 0;
  unsigned long _block_virtual_size = 0; /* virtual allocation size in bytes. */
  unsigned int _mem_prealloc = 0;

  unsigned int _next_free_element = 0;
  unsigned long _next_unalloc_page_addr = 0;

public:
  /******************************************************************************
  ** Initialize the memory block control structure and allocate memory
  ** for the memory block.
  ** The allocated memory is not initialized, so only virtual memory is reserved,
  ** while no physical memory is used.
  **
  ** If the virtual memory is not available then the code crashes.
  **
  **   element_size - Number of bytes in each element stored in the memory block.
  **   max_elements - Maximum number of elements that can be stored in the
  **                  memory block.
  **   mem_prealloc - Allocate all memory on MemBlockInit. Don't free memory
  **                  when elements are deleted.
  **
  ** Return Values:
  **   Pointer to the allocated memory block.
  ******************************************************************************/
  void *init(const unsigned long element_size, 
		  const unsigned long max_elements, 
		  const unsigned int mem_prealloc)
  {
    void *ptr;

    _os_page_size = sysconf(_SC_PAGESIZE);
    _element_size = element_size;
    _max_elements = max_elements;
    _mem_prealloc = mem_prealloc;

    _block_virtual_size = _element_size * _max_elements;
    if (_block_virtual_size % _os_page_size)
      _block_virtual_size = ((_block_virtual_size / _os_page_size) + 1) * _os_page_size;
    ptr = aligned_alloc(_os_page_size, _block_virtual_size);
    if (!ptr)
	    throw ERR_MSG ("aligned_alloc");
    _start_addr = (unsigned long)ptr;
    _next_unalloc_page_addr = _start_addr;

#ifdef LPMHT_USE_HUGE_PAGES
    (void)madvise(ptr, _block_virtual_size, MADV_HUGEPAGE);
#endif

    if (mem_prealloc)
      memset(ptr, 0, _block_virtual_size);

    return ptr;
  }

  /******************************************************************************
  ** Free memory associated with the memory block.
  **
  ** Return Values:
  **   None
  ******************************************************************************/
  void destroy(void)
  {
    free((void *)_start_addr);

    _start_addr = 0;
    _element_size = 0;
    _max_elements = 0;
    _os_page_size = 0;
    _block_virtual_size = 0;
    _mem_prealloc = 0;
    _next_free_element = 0;
    _next_unalloc_page_addr = 0;
  }

  /******************************************************************************
  ** Allocate an element number from the memory block.
  ** The first allocated element is 0, followed by 1, 2, 3...
  ** This continues until all the elements in the memory block are allocated.
  **
  **   next_free_element - (output) The next available element number.
  **
  ** Return Values:
  **   0 - OK
  **  -1 - The maximum number of elements have already been allocated.
  ******************************************************************************/
  int elementAlloc(unsigned int *next_free_element)
  {
    if (_next_free_element >= _max_elements)
      return -1; /* The memory is full. */

    *next_free_element = _next_free_element;
    _next_free_element++;

    while ((_next_unalloc_page_addr < (_start_addr + (_element_size * _next_free_element))) &&
           (_next_unalloc_page_addr < (_start_addr + _block_virtual_size)))
      _next_unalloc_page_addr += _os_page_size;

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
  ** Return Values:
  **   0 - OK
  **  -1 - All elements in the memory block are already free.
  ******************************************************************************/
  int elementFree(void)
  {
    if (0 == _next_free_element)
      return -1; /* Empty Memory Block */

    _next_free_element--;

    /* Check if we can free up physical pages.
     */
    while ((_next_unalloc_page_addr > _start_addr) &&
           ((_start_addr + (_element_size * _next_free_element)) < (_next_unalloc_page_addr - _os_page_size)))
    {
      _next_unalloc_page_addr -= _os_page_size;

      if (0 == _mem_prealloc)
      {
        /* The MADV_DONTNEED frees the pages immediately on Linux.
        ** Using this command instead of MADV_FREE makes it easier to confirm
        ** that the memory is freed when routes are deleted from the route
        ** table. Use the command "cat /proc/<pid>/status | grep RSS" to see
        ** the physical memory usage of a process.
        */
        if (madvise((void *)_next_unalloc_page_addr, _os_page_size, MADV_DONTNEED))
        {
          perror("madvise MADV_DONTNEED");
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
  **   last_used_element - (output) The most recently allocated element number.
  **
  ** Return Values:
  **   0 - OK
  **  -1 - All elements are unallocated in this memory block.
  ******************************************************************************/
  int lastElementGet(unsigned int *last_used_element)
  {
    if (0 == _next_free_element)
      return -1; /* The memory block is empty. */

    *last_used_element = _next_free_element - 1;

    return 0;
  }
  /******************************************************************************
  ** Get the size of the virtual block
  **
  **   0 - Virtual Block Size in bytes.
  ******************************************************************************/
  unsigned int blockVirtualSizeGet(void)
  {
    return _block_virtual_size;
  }
}; // End of class lpmhtMemBlock

#endif // LPMHT_UTIL_H_INCLUDED
