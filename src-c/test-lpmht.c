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
/******************************************************************************
** Test the Trie library APIs.
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <sys/random.h>
#include <arpa/inet.h>
#include <assert.h>
#include <locale.h>
#include <pthread.h>

#include "lpmht-api.h"

/* Unique name for this tree.
*/
#define TRIE_TREE_NAME "Tree-1"

/* Maximum number of routes in the Trie tree.
*/
#define TRIE_MAX_ROUTES 1000000

/* Number of LPM matches to perform.
*/
#define NUM_LPM_MATCHES 5000000

/* List of prefixes used for Add/Delete/Lookup test.
*/
static unsigned int ipv4_prefix[TRIE_MAX_ROUTES];
static unsigned char ipv4_prefix_size[TRIE_MAX_ROUTES];

/* The Trie tree used for Add/Delete/Lookup test.
*/
static lpmhtRouteTable_t *ipv4_tree;

typedef struct
{
  unsigned int next_hop;  
} routeEntry_t;

#define TEST_EXIT(rc,expected_rc) \
if ((rc) != (expected_rc))    \
{                                       \
  printf ("%s %d - Unexpected error %d (Expected %d)\n", \
                  __FUNCTION__, __LINE__,                \
                  (rc), (expected_rc)); \
  exit (0);                                             \
}


/****************************************************************
** Get Physical Memory Usage.
****************************************************************/
void physMemShow(void)
{
#if defined(__linux__)
  int my_pid = (int) getpid();
  char cmd[1024];

  sprintf (cmd, "cat /proc/%u/status | grep RSS", my_pid);
  if (system (cmd))
          perror ("system");

  printf ("\n");
#else
  printf ("Unsupported\n\n");
#endif 
}

/********************************************************************
** Get system up time in milliseconds. 
** The up time is based on when the process started running.
** The time wraps approximately every 49 days.
**
** Return Value:
** Time in milliseconds. 
********************************************************************/
unsigned int sysUpTimeMillisecondsGet(void)
{
  unsigned int time_ms;
  struct timespec time;
  int rc;
  static int first_time = 1;
  static int first_time_ms;

  rc = clock_gettime (CLOCK_MONOTONIC, &time);
  if (rc < 0)
  {
    perror ("clock_gettime CLOCK_MONOTONIC");
    exit (0);
  }
  time_ms = time.tv_sec * 1000;
  time_ms += time.tv_nsec / 1000000;

  if (first_time)
  {
    first_time = 0;
    first_time_ms = time_ms;
  }
  time_ms -=  first_time_ms;

  return time_ms;
}

/********************************************************************
** Get time in nanoseconds.. 
**
** Return Value:
** Time in nanoseconds. 
********************************************************************/
unsigned long long timeNanoGet(void)
{
  unsigned long long time_ns;
  struct timespec time;
  int rc;

  rc = clock_gettime (CLOCK_MONOTONIC, &time);
  if (rc < 0)
  {
    perror ("clock_gettime CLOCK_MONOTONIC");
    exit (0);
  }
  time_ns = time.tv_sec * 1000000000LLU;
  time_ns += time.tv_nsec;

  return time_ns;
}

/*************************************************************
** Sleep for specified number of milliseconds.
*************************************************************/
static void msleep(unsigned int wait_ms)
{
  struct timespec req;
  int rv;

  req.tv_sec = wait_ms / 1000;
  req.tv_nsec = (wait_ms % 1000) * 1000000;

  do 
  {
    rv = nanosleep(&req, &req);
  } while ((rv == -1) && (errno == EINTR));
} 


/*************************************************************
** add_delete_task
**
** This task continuously adds and deletes routes in the 
** Trie tree.
**
*************************************************************/
static void * add_delete_task (void *parm)
{
  int rc;
  unsigned int i;

  do 
  {
    /* Delete Routes.
    ** We always leave the default route in the table.
    */
    for (i = 1; i < TRIE_MAX_ROUTES; i++)
    {
      rc = lpmhtRouteDelete(ipv4_tree, 
                      (unsigned char *) &ipv4_prefix[i], 
                      ipv4_prefix_size[i]);
      TEST_EXIT(rc,0);

      pthread_testcancel();
    }
    printf ("Physical memory after deleting %lu routes from IPv4 table:\n", 
                    (unsigned long) TRIE_MAX_ROUTES);
    physMemShow();

    pthread_testcancel();
    /* Add Routes.
    */
    for (i = 1; i < TRIE_MAX_ROUTES; i++)
    {
      rc = lpmhtRouteAdd (ipv4_tree, 
                    (unsigned char *) &ipv4_prefix[i], 
                    ipv4_prefix_size[i],
                    i);
      TEST_EXIT(rc,0);

      pthread_testcancel();
    }
    printf ("Physical memory after adding %lu routes to IPv4 table:\n", 
                    (unsigned long) TRIE_MAX_ROUTES);
    physMemShow();
    pthread_testcancel();
  } while (1);

  return 0;
}

/*************************************************************
** Perform LPM route lookups.
**
*************************************************************/
void route_lookup (void)
{
  int rc;
  unsigned int start_time, end_time;
  unsigned long route_entry;
  unsigned int prefix_size;
  unsigned int prefix_index = 0;
  unsigned int i;
  unsigned long long max_lookup_time = 0;
  unsigned long long average_lookup_time = 0;
  unsigned long long start_lookup_time, delta_lookup_time;

  start_time = sysUpTimeMillisecondsGet();

  for (i = 0; i < NUM_LPM_MATCHES; i++)
  {
    start_lookup_time = timeNanoGet();
    rc = lpmhtLPMatch (ipv4_tree, (unsigned char *) &ipv4_prefix[prefix_index], 
                    &prefix_size, &route_entry);
    delta_lookup_time = timeNanoGet() - start_lookup_time;
    if (delta_lookup_time > max_lookup_time)
    {
      max_lookup_time = delta_lookup_time;
    }
    average_lookup_time += delta_lookup_time;

    TEST_EXIT(rc,0);
    prefix_index++;
    if (prefix_index >= TRIE_MAX_ROUTES)
    {
      prefix_index = 0;
    }
  } 
  end_time = sysUpTimeMillisecondsGet();

  printf ("Finished %'u LPMatches on Route Table in %'ums. Max Lookup Time:%'lluns Average Lookup Time:%'lluns\n", 
                  NUM_LPM_MATCHES, end_time - start_time,
                  max_lookup_time, average_lookup_time / NUM_LPM_MATCHES);

}

/****************************************************************
** Test code coverage for lpmhtRouteTableCreate()/Destroy()
****************************************************************/
void createTest1(void)
{
  int rc;
  lpmhtRouteTable_t *t1, *t2, *t3;


  /* Create successful IPv4 route table.
  */
  t1 = lpmhtRouteTableCreate(3,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             0);
 
  assert (t1);

  /* Create successful IPv6 route table.
  */
  t3 = lpmhtRouteTableCreate(1000000,  /* Max Routes */
                             LPMHT_IPV6,
                             LPMHT_TRIE,
                             0);
 
  assert (t3);

  /* Create Trie route table with too many routes.
  */
  t2 = lpmhtRouteTableCreate(LPMHT_MAX_TRIE_ROUTES+1,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             0);
  assert (!t2);

  /* Create Hash route table with too many routes.
  */
  t2 = lpmhtRouteTableCreate(LPMHT_MAX_HASH_ROUTES+1,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_HASH,
                             0);
  assert (!t2);

  /* Create with invalid IP version.
  */
  t2 = lpmhtRouteTableCreate(3,  /* Max Routes */
                             (lpmhtIpMode_e) 3,
                             LPMHT_TRIE,
                             0);
  assert (!t2);

  /* Create with Invalid Table Type
  */
  t2 = lpmhtRouteTableCreate(3,  /* Max Routes */
                             LPMHT_IPV4,
                             (lpmhtTableMode_e) 3,
                             0);
  assert (!t2);

  /* Destroy the tree.
  */
  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

  rc = lpmhtRouteTableDestroy(t3);
  TEST_EXIT(rc,0);

  /* Invoke with NULL pointer.
  */
  rc = lpmhtRouteTableDestroy(0);
  TEST_EXIT(rc,-1);

}

/****************************************************************
** Test code coverage for lpmhtRouteAdd()
****************************************************************/
void insertTest1(void)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix1[4];
  unsigned char prefix2[16];
  unsigned long route_entry = 1;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  /* Create successful Trie routing table
  */
  t1 = lpmhtRouteTableCreate(3,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             0);
 
  assert (t1);

  /* Try to insert with tree set to NULL.
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteAdd(0, prefix1, 1, route_entry); 
  TEST_EXIT(rc,-1);

  /* Insert IPv4 prefix with prefix length > 32.
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  rc = lpmhtRouteAdd(t1, prefix1, 33, route_entry); 
  TEST_EXIT(rc,-1);

  /* Insert 0x80/1 
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  rc = lpmhtRouteAdd(t1, prefix1, 1, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert duplicate route 0x80/1 
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  rc = lpmhtRouteAdd(t1, prefix1, 1, route_entry); 
  TEST_EXIT(rc,-2);

  /* Insert route 0x80/2
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  rc = lpmhtRouteAdd(t1, prefix1, 2, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert route 0x0/0
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteAdd(t1, prefix1, 0, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert too many routes.
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x40;
  rc = lpmhtRouteAdd(t1, prefix1, 2, route_entry); 
  TEST_EXIT(rc,-3);

  (void) lpmhtRouteTableDestroy(t1);

  /* Create successful Hash IPv4 routing table
  */
  t1 = lpmhtRouteTableCreate(3,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_HASH,
                             0);
 
  assert (t1);

  /* Insert 0x80.0xa5/15
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  prefix1[1] = 0xa5;
  rc = lpmhtRouteAdd(t1, prefix1, 15, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert duplicate 0x80.0xa5/15
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  prefix1[1] = 0xa5;
  rc = lpmhtRouteAdd(t1, prefix1, 15, route_entry); 
  TEST_EXIT(rc,-2);

  /* Insert route 0x80,0xa5/16
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  prefix1[1] = 0xa5;
  rc = lpmhtRouteAdd(t1, prefix1, 16, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert route 0x0/0
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteAdd(t1, prefix1, 0, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert too many routes.
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x40;
  rc = lpmhtRouteAdd(t1, prefix1, 2, route_entry); 
  TEST_EXIT(rc,-3);

  /* Delete route with NULL route table and prefix.
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteDelete(0, 0, 0); 
  TEST_EXIT(rc,-1);

  /* Delete existing route 0x0/0
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteDelete(t1, prefix1, 0); 
  TEST_EXIT(rc,0);

  /* Re-Insert route 0/0
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteAdd(t1, prefix1, 0, route_entry); 
  TEST_EXIT(rc,0);

  (void) lpmhtRouteTableDestroy(t1);

  /* Create successful IPv6 Hash routing table
  */
  prop.hit_count = 1;
  t1 = lpmhtRouteTableCreate(3,  /* Max Routes */
                             LPMHT_IPV6,
                             LPMHT_HASH,
                             &prop);
 
  assert (t1);

  /* Insert IPv6 with prefix length greater than 128.
  */
  memset (prefix2, 0, sizeof(prefix2));
  prefix2[0] = 0x80;
  prefix2[1] = 0xa5;
  rc = lpmhtRouteAdd(t1, prefix2, 129, route_entry); 
  TEST_EXIT(rc,-1);

  /* Insert 0x80.0xa5/50 
  */
  memset (prefix2, 0, sizeof(prefix2));
  prefix2[0] = 0x80;
  prefix2[1] = 0xa5;
  rc = lpmhtRouteAdd(t1, prefix2, 50, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert duplicate route 0x80.0xa5/50 
  */
  memset (prefix2, 0, sizeof(prefix2));
  prefix2[0] = 0x80;
  prefix2[1] = 0xa5;
  rc = lpmhtRouteAdd(t1, prefix2, 50, route_entry); 
  TEST_EXIT(rc,-2);

  /* Insert route 0x80.0xa5/51
  */
  memset (prefix2, 0, sizeof(prefix2));
  prefix2[0] = 0x80;
  prefix2[1] = 0xa5;
  rc = lpmhtRouteAdd(t1, prefix2, 51, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert route 0x0/0
  */
  memset (prefix2, 0, sizeof(prefix2));
  rc = lpmhtRouteAdd(t1, prefix2, 0, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert too many routes.
  */
  memset (prefix2, 0, sizeof(prefix2));
  prefix2[0] = 0x40;
  rc = lpmhtRouteAdd(t1, prefix2, 2, route_entry); 
  TEST_EXIT(rc,-3);

  (void) lpmhtRouteTableDestroy(t1);

}

/****************************************************************
** Test code coverage for hash-based route tables 
** Insert/Get.
****************************************************************/
void insertTest3(void)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix2[16];
  unsigned long route_entry;
  unsigned int max_ipv4_routes = 100001;
  unsigned int max_ipv6_routes = 200001;
  unsigned int i;
  unsigned int ipv4_prefix;
  unsigned long long hit_count;
  unsigned int ipv6_prefix_size;
  lpmhtTableInfo_t info;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  /* Create successful Hash routing table
  */
  prop.hit_count = 1;
  t1 = lpmhtRouteTableCreate(max_ipv4_routes,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_HASH,
                             &prop);
  assert (t1);

  for (i = 0; i < max_ipv4_routes; i++)
  {
    ipv4_prefix = htonl(i);
    rc = lpmhtRouteAdd (t1, (unsigned char *) &ipv4_prefix, 32, i);
    TEST_EXIT(rc,0);
  }
 
  rc =  lpmhtRouteTableInfoGet (t1, &info);
  TEST_EXIT(rc,0);

  printf ("Added %'u routes to IPv4 Hash Table. Physical Mem:%'zuKB  Virtual Mem:%'zuKB\n",
                info.num_routes, info.mem_size/1024, info.virtual_mem_size/1024);

  /* Perform Set on IPv4 route with prefix > 32.
  */
  rc = lpmhtRouteSet (t1, (unsigned char *) &ipv4_prefix, 33, 0);
  TEST_EXIT(rc,-1);

  /* Perform Get on IPv4 route with prefix > 32.
  */
  rc = lpmhtRouteGet (t1, (unsigned char *) &ipv4_prefix, 33, &route_entry, 0, &hit_count);
  TEST_EXIT(rc,-1);

  /* Perform a get/set on each route.
  */
  for (i = 0; i < max_ipv4_routes; i++)
  {
    ipv4_prefix = htonl(i);
    rc = lpmhtRouteGet (t1, (unsigned char *) &ipv4_prefix, 32, &route_entry, i%2, &hit_count);
    if (rc)
        printf ("i=%d\n", i);
    TEST_EXIT(rc,0);
    assert(route_entry == i);
    assert(hit_count == 0);

    rc = lpmhtRouteSet (t1, (unsigned char *) &ipv4_prefix, 32, i + 5);
    TEST_EXIT(rc,0);

    rc = lpmhtRouteGet (t1, (unsigned char *) &ipv4_prefix, 32, &route_entry, i%2, 0);
    TEST_EXIT(rc,0);
    TEST_EXIT((unsigned int) route_entry, i + 5);
  }
  /* Get non-existing route.
  */
  ipv4_prefix = htonl(i);
  rc = lpmhtRouteGet (t1, (unsigned char *) &ipv4_prefix, 32, &route_entry, i%2, &hit_count);
  TEST_EXIT(rc,-2);

  printf ("Perfomed RouteGet on %u IPv4 routes.\n", max_ipv4_routes);

  /* Delete IPv4 prefix with prefix length > 32.
  */
  rc = lpmhtRouteDelete (t1, (unsigned char *) &ipv4_prefix, 33);
  TEST_EXIT(rc,-1);

  /* Delete non-existent route.
  */
  rc = lpmhtRouteDelete (t1, (unsigned char *) &ipv4_prefix, 32);
  TEST_EXIT(rc,-2);

  for (i = 0; i < max_ipv4_routes; i++)
  {
    ipv4_prefix = htonl(i);
    rc = lpmhtRouteDelete (t1, (unsigned char *) &ipv4_prefix, 32);
    TEST_EXIT(rc,0);
  }
 
  /* Delete non-existent route with empty table.
  */
  rc = lpmhtRouteDelete (t1, (unsigned char *) &ipv4_prefix, 32);
  TEST_EXIT(rc,-2);

  rc =  lpmhtRouteTableInfoGet (t1, &info);
  TEST_EXIT(rc,0);

  printf ("Deleted %'u routes from IPv4 Hash Table. Physical Mem:%'zuKB  Virtual Mem:%'zuKB\n",
                max_ipv4_routes - info.num_routes, info.mem_size/1024, info.virtual_mem_size/1024);

  (void) lpmhtRouteTableDestroy(t1);

  /* Create successful IPv6 Hash routing table
  */
  prop.hit_count = 1;
  t1 = lpmhtRouteTableCreate(max_ipv6_routes,  /* Max Routes */
                             LPMHT_IPV6,
                             LPMHT_HASH,
                             &prop);
  assert (t1);

  ipv6_prefix_size = 64;

  memset (prefix2, 0, sizeof(prefix2));
  for (i = 0; i < max_ipv6_routes; i++)
  {
    ipv4_prefix = htonl(i);
    *(unsigned int *) &prefix2[4] = ipv4_prefix;
    rc = lpmhtRouteAdd (t1,  prefix2, ipv6_prefix_size, i);
    TEST_EXIT(rc,0);
  }
 
  rc =  lpmhtRouteTableInfoGet (t1, &info);
  TEST_EXIT(rc,0);

  printf ("Added %'u routes to IPv6 Hash Table. Mem:%'zuKB\n",
                info.num_routes, info.mem_size/1024);

  /* Perform Set on IPv6 route with prefix > 128.
  */
  rc = lpmhtRouteSet (t1, prefix2, 129, 0);
  TEST_EXIT(rc,-1);

  /* Perform Get on IPv6 route with prefix > 128.
  */
  rc = lpmhtRouteGet (t1, prefix2, 129, &route_entry, 0, &hit_count);
  TEST_EXIT(rc,-1);

  ipv6_prefix_size = 64;
  /* Perform a get/set on each route.
  */
  for (i = 0; i < max_ipv6_routes; i++)
  {
    ipv4_prefix = htonl(i);
    *(unsigned int *) &prefix2[4] = ipv4_prefix;
    rc = lpmhtRouteGet (t1, prefix2, ipv6_prefix_size, &route_entry, i%2, &hit_count);
    TEST_EXIT(rc,0);
    TEST_EXIT((unsigned int)route_entry, i);
    TEST_EXIT((unsigned int) hit_count, 0);

    rc = lpmhtRouteSet (t1, prefix2, ipv6_prefix_size, i + 5);
    TEST_EXIT(rc,0);

    rc = lpmhtRouteGet (t1, prefix2, ipv6_prefix_size, &route_entry, i%2, 0);
    TEST_EXIT(rc,0);
    TEST_EXIT((unsigned int) route_entry, i + 5);
  }
  /* Get non-existing route.
  */
  ipv4_prefix = htonl(i);
  *(unsigned int *) &prefix2[4] = ipv4_prefix;
  rc = lpmhtRouteGet (t1, prefix2, ipv6_prefix_size, &route_entry, i%2, &hit_count);
  TEST_EXIT(rc,-2);

  printf ("Perfomed RouteGet on %u IPv6 routes.\n", max_ipv6_routes);

  /* Delete Non-Existing route.
  */
  rc = lpmhtRouteDelete (t1, prefix2, ipv6_prefix_size);
  TEST_EXIT(rc,-2);

  /* Delete IPv6 prefix with prefix length > 128.
  */
  rc = lpmhtRouteDelete (t1, prefix2, 129);
  TEST_EXIT(rc,-1);

  ipv6_prefix_size = 64;
  memset (prefix2, 0, sizeof(prefix2));
  for (i = 0; i < max_ipv6_routes; i++)
  {
    ipv4_prefix = htonl(i);
    *(unsigned int *) &prefix2[4] = ipv4_prefix;
    rc = lpmhtRouteDelete (t1,  prefix2, ipv6_prefix_size);
    TEST_EXIT(rc,0);
  }
 
  /* Delete Non-Existing route from empty table.
  */
  rc = lpmhtRouteDelete (t1, prefix2, ipv6_prefix_size);
  TEST_EXIT(rc,-2);

  rc =  lpmhtRouteTableInfoGet (t1, &info);
  TEST_EXIT(rc,0);

  printf ("Deleted %'u routes from IPv6 Hash Table. Mem:%'zuKB\n",
                max_ipv6_routes - info.num_routes, info.mem_size/1024);


  (void) lpmhtRouteTableDestroy(t1);
}
/****************************************************************
** Test code coverage for lpmhtRouteAdd()
****************************************************************/
void insertTest2(lpmhtTableMode_e table_mode)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix1[4];
  unsigned long route_entry = 1;

  /* Create successful private tree.
  */
  t1 = lpmhtRouteTableCreate(3,  /* Max Routes */
                             LPMHT_IPV4,
                             table_mode,
                             0);
  assert (t1);

  /* Insert route 0x0/0
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteAdd(t1, prefix1, 0, route_entry); 
  TEST_EXIT(rc,0);

  /* Insert route 0x40/2
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x40;
  rc = lpmhtRouteAdd(t1, prefix1, 2, route_entry); 
  TEST_EXIT(rc,0);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);


}

/****************************************************************
** Test code coverage for lpmhtRouteDelete()
****************************************************************/
void deleteTest1(lpmhtTableMode_e table_mode)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix1[4];
  unsigned long route_entry = 1;

  memset (prefix1, 0, sizeof(prefix1));

  /* Create successful private tree.
  */
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             table_mode,
                             0);
  assert (t1);

  /* Delete with trie tree set to 0.
  */
  rc = lpmhtRouteDelete(0,prefix1,1);
  TEST_EXIT(rc,-1);

  /* Delete an element from an empty tree.
  */
  rc = lpmhtRouteDelete(t1,prefix1,1);
  TEST_EXIT(rc,-2);

  /* Insert route 0x0/0
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteAdd(t1, prefix1, 0, route_entry); 
  TEST_EXIT(rc,0);

  /* Delete element 0x40/2, which doesn't exist.
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x40;
  rc = lpmhtRouteDelete(t1,prefix1,2);
  TEST_EXIT(rc,-2);

  /* Insert route 0x40/2
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x40;
  rc = lpmhtRouteAdd(t1, prefix1, 2, route_entry); 
  TEST_EXIT(rc,0);


  /* Delete route 0x0/0
  */
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteDelete(t1, prefix1, 0); 
  TEST_EXIT(rc,0);

  /* Delete element 0x40/2, which is the last element in the tree.
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x40;
  rc = lpmhtRouteDelete(t1,prefix1,2);
  TEST_EXIT(rc,0);

  /* Insert 0x80/1 and 0xE0/3. 
  ** Delete 0xC0/2.
  ** Delete 0xE0/3.
  ** Delete 0x80/1.
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  rc = lpmhtRouteAdd(t1, prefix1, 1, route_entry); 
  TEST_EXIT(rc,0);

  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xE0;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xC0;
  rc = lpmhtRouteDelete(t1,prefix1,2);
  TEST_EXIT(rc,-2);

  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xE0;
  rc = lpmhtRouteDelete(t1,prefix1,3);
  TEST_EXIT(rc,0);

  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  rc = lpmhtRouteDelete(t1,prefix1,1);
  TEST_EXIT(rc,0);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

}

/****************************************************************
** Test code coverage for lpmhtRouteFirstGet()
****************************************************************/
void firstGetTest1(void)
{
  int rc;
  lpmhtRouteTable_t *t1, *t2;
  unsigned char prefix1[4];
  unsigned char prefix2[16];
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  unsigned long long hit_count;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));

  /* Create IPv6 hash route table with next_get disabled.
  */
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV6,
                             LPMHT_HASH,
                             0);

  /* Attempt firstGet on route table with next_get property disabled.
  */
  rc = lpmhtRouteFirstGet(t1, prefix2, &prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-2);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

  /* Create IPv6 hash route table with next_get enabled.
  */
  prop.next_get = 1;
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV6,
                             LPMHT_HASH,
                             &prop);

  /* Attempt firstGet on empty IPv6 table.
  */
  rc = lpmhtRouteFirstGet(t1, prefix2, &prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-2);

  /* Add two routes with prefix length 70 and 80.
  ** Make sure that FirstGet returns the route with prefix length 80.
  */
  memset (prefix2, 0xff, sizeof(prefix2));
  rc = lpmhtRouteAdd(t1, prefix2, 80, route_entry); 
  TEST_EXIT(rc,0);

  memset (prefix2, 0, sizeof(prefix2));
  rc = lpmhtRouteAdd(t1, prefix2, 70, route_entry); 
  TEST_EXIT(rc,0);

  rc = lpmhtRouteFirstGet(t1, prefix2, &prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size, 80);


  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);


  /* Create route table with no hit count monitoring.
  */
  memset (&prop, 0, sizeof(prop));
  prop.next_get = 1;
  prop.hit_count = 0;
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             &prop);
  assert (t1);

  /* Create tree with hit count monitoring.
  */
  prop.next_get = 1;
  prop.hit_count = 1;
  t2 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             &prop);
  assert (t2);

  /* Invoke FirstGet with NULL tree.
  */
  rc = lpmhtRouteFirstGet(0, prefix1, &prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-1);

  /* Invoke FirstGet with prefix set to 0.
  */
  rc = lpmhtRouteFirstGet(t1, 0, &prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-1);

  /* Invoke FirstGet on empty tree.
  */
  rc = lpmhtRouteFirstGet(t1, prefix1, &prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-2);

  /* Insert route 0xa0/3 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xa0;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke FirstGet on t1
  */
  memset (&prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteFirstGet(t1, prefix1, &prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0xa0);

  /* Invoke FirstGet on t1 with non-NULL hit_count.
  */
  memset (&prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteFirstGet(t1, prefix1, &prefix_size, &route_entry, 
                  0, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0xa0);
  TEST_EXIT((unsigned int) hit_count,0);

  /* Insert route 0x60/3 into t2
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x60;
  rc = lpmhtRouteAdd(t2, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke FirstGet on t2 with NULL-hit count and 0-clear_hit_count.
  */
  memset (&prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteFirstGet(t2, prefix1, &prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0x60);

  /* Invoke FirstGet on t2 with 0-clear_hit_count.
  */
  memset (&prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteFirstGet(t2, prefix1, &prefix_size, &route_entry, 
                  0, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0x60);
  TEST_EXIT((unsigned int) hit_count,0);

  /* Invoke FirstGet on t2 with clear_hit_count and NULL hit_count.
  */
  memset (&prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteFirstGet(t2, prefix1, &prefix_size, &route_entry, 
                  1, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0x60);

  /* Invoke FirstGet on t2 with 1-clear_hit_count.
  */
  memset (&prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteFirstGet(t2, prefix1, &prefix_size, &route_entry, 
                  1, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0x60);
  TEST_EXIT((unsigned int) hit_count,0);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

  rc = lpmhtRouteTableDestroy(t2);
  TEST_EXIT(rc,0);

}

/****************************************************************
** Test code coverage for lpmhtRouteGet()/lpmhtRouteSet()
****************************************************************/
void getSetTest1(void)
{
  int rc;
  lpmhtRouteTable_t *t1, *t2;
  unsigned char prefix1[4];
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  unsigned long long hit_count;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));
  prefix_size = 0;

  /* Create routing table with no hit count monitoring.
  */
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             0);
  assert (t1);

  /* Create routing table with hit count monitoring.
  */
  prop.hit_count = 1;
  t2 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             &prop);
  assert (t2);

  /* Invoke Get with NULL tree.
  */
  rc = lpmhtRouteGet(0, prefix1, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-1);

  /* Invoke Set with NULL tree.
  */
  rc = lpmhtRouteSet(0, prefix1, prefix_size, route_entry);
  TEST_EXIT(rc,-1);

  /* Invoke Get on empty tree.
  */
  rc = lpmhtRouteGet(t1, prefix1, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-2);

  /* Invoke Set on empty tree.
  */
  rc = lpmhtRouteSet(t1, prefix1, prefix_size, route_entry);
  TEST_EXIT(rc,-2);

  /* Insert route 0xa0/3 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xa0;
  route_entry = 3;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke Get on t1
  */
  prefix1[0] = 0xa0;
  prefix_size = 3;
  rc = lpmhtRouteGet(t1, prefix1, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) route_entry,3);

  /* Invoke Get on an existing node which doesn't have a route entry.
  */
  prefix1[0] = 0xa0;
  prefix_size = 2;
  rc = lpmhtRouteGet(t1, prefix1, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-2);

  /* Invoke Set on an existing node which doesn't have a route entry.
  */
  prefix1[0] = 0xa0;
  prefix_size = 2;
  rc = lpmhtRouteSet(t1, prefix1, prefix_size, route_entry);
  TEST_EXIT(rc,-2);

  /* Invoke Set on an existing node.
  */
  prefix1[0] = 0xa0;
  prefix_size = 3;
  route_entry = 4;
  rc = lpmhtRouteSet(t1, prefix1, prefix_size, route_entry);
  TEST_EXIT(rc,0);


  /* Invoke Get on t1 with non-NULL hit_count.
  */
  prefix1[0] = 0xa0;
  prefix_size = 3;
  rc = lpmhtRouteGet(t1, prefix1, prefix_size, &route_entry, 
                  0, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) route_entry,4);
  TEST_EXIT((unsigned int) hit_count,0);

  /* Insert route 0x60/3 into t2
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x60;
  prefix_size = 3;
  rc = lpmhtRouteAdd(t2, prefix1, prefix_size, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke Get on t2 with NULL-hit count and 0-clear_hit_count.
  */
  rc = lpmhtRouteGet(t2, prefix1, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,0);

  /* Invoke Get on t2 with 0-clear_hit_count.
  */
  rc = lpmhtRouteGet(t2, prefix1, prefix_size, &route_entry, 
                  0, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) hit_count,0);

  /* Invoke Get on t2 with clear_hit_count and NULL hit_count.
  */
  rc = lpmhtRouteGet(t2, prefix1, prefix_size, &route_entry, 
                  1, 0);
  TEST_EXIT(rc,0);

  /* Invoke Get on t2 with 1-clear_hit_count.
  */
  rc = lpmhtRouteGet(t2, prefix1, prefix_size, &route_entry, 
                  1, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) hit_count,0);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

  rc = lpmhtRouteTableDestroy(t2);
  TEST_EXIT(rc,0);

}

/****************************************************************
** Hash Route Table Tests.
** Test code coverage for lpmhtRouteGet()/lpmhtRouteSet()
****************************************************************/
void getSetTest2(void)
{
  int rc;
  lpmhtRouteTable_t *t1, *t2;
  unsigned char prefix1[4];
  unsigned char prefix2[16];
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  unsigned long long hit_count;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));
  memset (prefix2, 0, sizeof(prefix2));
  prefix_size = 0;

  /* Create IPv4 routing table with no hit count monitoring.
  */
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_HASH,
                             0);
  assert (t1);

  /* Create IPv6 routing table with hit count monitoring.
  */
  prop.hit_count = 1;
  t2 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV6,
                             LPMHT_HASH,
                             &prop);
  assert (t2);

  /* Invoke Get on empty tree.
  */
  rc = lpmhtRouteGet(t1, prefix1, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-2);

  /* Invoke Set on empty tree.
  */
  rc = lpmhtRouteSet(t1, prefix1, prefix_size, route_entry);
  TEST_EXIT(rc,-2);

  /* Insert route 0xa0/3 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xa0;
  route_entry = 3;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke Get on t1
  */
  prefix1[0] = 0xa0;
  prefix_size = 3;
  rc = lpmhtRouteGet(t1, prefix1, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) route_entry,3);

  /* Invoke Get on an existing node which doesn't have a route entry.
  */
  prefix1[0] = 0xa0;
  prefix_size = 2;
  rc = lpmhtRouteGet(t1, prefix1, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,-2);

  /* Invoke Set on an existing node which doesn't have a route entry.
  */
  prefix1[0] = 0xa0;
  prefix_size = 2;
  rc = lpmhtRouteSet(t1, prefix1, prefix_size, route_entry);
  TEST_EXIT(rc,-2);

  /* Invoke Set on an existing node.
  */
  prefix1[0] = 0xa0;
  prefix_size = 3;
  route_entry = 4;
  rc = lpmhtRouteSet(t1, prefix1, prefix_size, route_entry);
  TEST_EXIT(rc,0);


  /* Invoke Get on t1 with non-NULL hit_count.
  */
  prefix1[0] = 0xa0;
  prefix_size = 3;
  rc = lpmhtRouteGet(t1, prefix1, prefix_size, &route_entry, 
                  0, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) route_entry,4);
  TEST_EXIT((unsigned int) hit_count,0);

  /* Insert route 0x0060::/16 into t2
  */
  memset (prefix2, 0, sizeof(prefix2));
  prefix1[1] = 0x60;
  prefix_size = 16;
  rc = lpmhtRouteAdd(t2, prefix2, prefix_size, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke Set on an existing route.
  */
  route_entry = 4;
  rc = lpmhtRouteSet(t2, prefix2, prefix_size, route_entry);
  TEST_EXIT(rc,0);

  /* Invoke Get on t2 with NULL-hit count and 0-clear_hit_count.
  */
  rc = lpmhtRouteGet(t2, prefix2, prefix_size, &route_entry, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) route_entry,4);

  /* Invoke Get on t2 with 0-clear_hit_count.
  */
  rc = lpmhtRouteGet(t2, prefix2, prefix_size, &route_entry, 
                  0, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) hit_count,0);

  /* Invoke Get on t2 with clear_hit_count and NULL hit_count.
  */
  rc = lpmhtRouteGet(t2, prefix2, prefix_size, &route_entry, 
                  1, 0);
  TEST_EXIT(rc,0);

  /* Invoke Get on t2 with 1-clear_hit_count.
  */
  rc = lpmhtRouteGet(t2, prefix2, prefix_size, &route_entry, 
                  1, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) hit_count,0);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

  rc = lpmhtRouteTableDestroy(t2);
  TEST_EXIT(rc,0);

}
/****************************************************************
** Test code coverage for lpmhtRouteNextGet()
****************************************************************/
void getNextTest1(void)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix1[4];
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  unsigned char next_prefix[4];
  unsigned int next_prefix_size;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));
  prefix_size = 0;

  memset (next_prefix, 0, sizeof(next_prefix));
  next_prefix_size = 0;

  /* Create route table with next_get disabled.
  */
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             0);
  assert (t1);

  /* Invoke NextGet on route table with next_get feature disabled.
  */
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,-2);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

  /* Create route table with no hit count monitoring.
  */
  prop.next_get = 1;
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             &prop);
  assert (t1);

  /* Invoke NextGet with NULL tree.
  */
  rc = lpmhtRouteNextGet(0, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,-1);

  /* Invoke NextGet on empty tree.
  */
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,-2);

  /* Invoke NextGet with invalid prefix length.
  */
  rc = lpmhtRouteNextGet(t1, prefix1, 33, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,-1);

  /* Insert route 0xa0/3 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xa0;
  route_entry = 3;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke NextGet on t1 with element we just inserted.
  */
  prefix1[0] = 0xa0;
  prefix_size = 3;
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,-2);

  /* Invoke NextGet starting with 0/32.
  */
  prefix1[0] = 0;
  prefix_size = 32;
  memset (next_prefix, 0, sizeof(next_prefix));
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT(next_prefix_size,3);
  TEST_EXIT(next_prefix[0],0xa0);

  /* Invoke NextGet starting with 0x80/3.
  */
  prefix1[0] = 0x80;
  prefix_size = 3;
  memset (next_prefix, 0, sizeof(next_prefix));
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT(next_prefix_size,3);
  TEST_EXIT(next_prefix[0],0xa0);

  /* Insert route 0x80/2 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x80;
  route_entry = 100;
  rc = lpmhtRouteAdd(t1, prefix1, 2, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke NextGet starting with 0x80/3.
  */
  prefix1[0] = 0x80;
  prefix_size = 3;
  memset (next_prefix, 0, sizeof(next_prefix));
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT(next_prefix_size,3);
  TEST_EXIT(next_prefix[0],0xa0);
  TEST_EXIT((unsigned int) route_entry,3);

  /* Invoke GetNext starting with 0xa0/3
  */
  memcpy (prefix1, next_prefix, sizeof(prefix1));
  prefix_size = next_prefix_size;
  memset (next_prefix, 0, sizeof(next_prefix));
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,0);
  TEST_EXIT(next_prefix_size,2);
  TEST_EXIT(next_prefix[0],0x80);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

}

/****************************************************************
** Test code coverage for lpmhtRouteNextGet()
****************************************************************/
void getNextTest2(void)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix1[4];
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  unsigned char next_prefix[4];
  unsigned int next_prefix_size;
  unsigned long long hit_count;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));
  prefix_size = 0;

  memset (next_prefix, 0, sizeof(next_prefix));
  next_prefix_size = 0;

  /* Create routing table with hit count monitoring.
  */
  prop.hit_count = 1;
  prop.next_get = 1;
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_HASH,
                             &prop);
  assert (t1);

  /* Insert route 0x40/3 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x20;
  route_entry = 3;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke NextGet on t1 with element we just inserted.
  */
  prefix1[0] = 0x20;
  prefix_size = 3;
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,-2);

  /* Insert route 0x0/1 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x0;
  route_entry = 3;
  rc = lpmhtRouteAdd(t1, prefix1, 1, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke NextGet starting with 0x20/3.
  */
  prefix1[0] = 0x20;
  prefix_size = 3;
  memset (next_prefix, 0, sizeof(next_prefix));
  rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT(next_prefix_size,1);
  TEST_EXIT(next_prefix[0],0);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

}

/****************************************************************
** Test lpmhtRouteNextGet() with many IPv6 routes.
****************************************************************/
void getNextTest3(lpmhtTableMode_e table_mode, unsigned int mem_prealloc)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix1[16];
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  unsigned char next_prefix[16];
  unsigned int next_prefix_size;
  unsigned long long hit_count;
  unsigned int max_routes = 2000;
  unsigned int i, j;
  unsigned int delete_route;
  lpmhtTableInfo_t info;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));
  prefix_size = 0;

  printf ("\n***** %s - IPv6 - %s - mem_prealloc:%d\n", __FUNCTION__,
                (table_mode == LPMHT_TRIE)?"TRIE":"HASH", mem_prealloc);

  /* Create routing table with hit count monitoring.
  */
  prop.hit_count = 1;
  prop.next_get = 1;
  if (mem_prealloc)
        prop.mem_prealloc = 1;
  t1 = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                             LPMHT_IPV6,
                             table_mode,
                             &prop);
  assert (t1);

  /* Invoke NextGet with invalid prefix size.
  */
  memset (next_prefix, 0, sizeof(next_prefix));
  rc = lpmhtRouteNextGet(t1, prefix1, 129, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
  TEST_EXIT(rc,-1);


  for (i = 0; i < max_routes; i++)
  {
    *(unsigned int *) &prefix1[0] = rand();
    *(unsigned int *) &prefix1[4] = rand();
    *(unsigned int *) &prefix1[8] = rand();
    *(unsigned int *) &prefix1[12] = rand();
    prefix_size = (rand() % 65) + 64;
    route_entry = i;
    rc = lpmhtRouteAdd(t1, prefix1, prefix_size, route_entry); 
    if (rc)
    {
      printf ("Error:%d while inserting route %d\n", rc, i);
    }
    TEST_EXIT(rc,0);
  }

  rc =  lpmhtRouteTableInfoGet (t1, &info);
  TEST_EXIT(rc,0);

  printf ("Route Table - mem:%'zu virtual mem:%'zu routes:%'u/%'u nodes:%'u\n", 
                  info.mem_size, info.virtual_mem_size, info.num_routes, max_routes, info.num_nodes);

  printf ("First 10 elements:\n");
  memset (prefix1, 0, sizeof(prefix1));
  rc = lpmhtRouteFirstGet(t1, prefix1, &prefix_size, &route_entry, 
                  1, &hit_count);
  TEST_EXIT(rc,0);
  for (i = 0; i < 10; i++)
  {
    printf ("%d - ", i);
    printf ("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                    prefix1[0],prefix1[1],prefix1[2],prefix1[3],
                    prefix1[4],prefix1[5],prefix1[6],prefix1[7],
                    prefix1[8],prefix1[9],prefix1[10],prefix1[11],
                    prefix1[12],prefix1[13],prefix1[14],prefix1[15]);
    printf ("/%d\n", prefix_size);

    memset (next_prefix, 0, sizeof(next_prefix));
    rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
    TEST_EXIT(rc,0);

    memcpy (prefix1, next_prefix, sizeof(prefix1));
    prefix_size = next_prefix_size;
  }
  printf ("\n\n");

  /* Verify that the NextGet finds all inserted elements.
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix_size = 128;
  i = 0;
  do 
  {
    memset (next_prefix, 0, sizeof(next_prefix));
    rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                  next_prefix, &next_prefix_size, 0, 0);
    if (rc)
    {
      break;
    }
    memcpy (prefix1, next_prefix, sizeof(prefix1));
    prefix_size = next_prefix_size;
    i++;
  } while (rc == 0);
  printf ("lpmhtRouteNextGet() found %d routes. Expected %d routes.\n\n", 
                  i, max_routes);

  /* Delete elements at random from the tree.
  */ 
  printf ("Deleting random routes...\n");
  for (i = 0; i < max_routes; i++)
  {
    delete_route = rand() % (max_routes - i);
  //  printf ("%d - Delete Route:%d\n", i, delete_route);
    memset (prefix1, 0, sizeof(prefix1));
    rc = lpmhtRouteFirstGet(t1, prefix1, &prefix_size, &route_entry, 
                    1, &hit_count);
    TEST_EXIT(rc,0);

    for (j = 0; j < delete_route; j++)
    {
      memset (next_prefix, 0, sizeof(next_prefix));
      rc = lpmhtRouteNextGet(t1, prefix1, prefix_size, &route_entry, 
                    next_prefix, &next_prefix_size, 0, 0);
      TEST_EXIT(rc,0);

      memcpy (prefix1, next_prefix, sizeof(prefix1));
      prefix_size = next_prefix_size;
    }
    rc = lpmhtRouteDelete(t1,prefix1,prefix_size);
    TEST_EXIT(rc,0);
  }

  rc =  lpmhtRouteTableInfoGet (t1, &info);
  TEST_EXIT(rc,0);

  printf ("Route Table - mem:%'zu virtual mem:%'zu routes:%'u/%'u nodes:%'u\n", 
                  info.mem_size, info.virtual_mem_size, info.num_routes, max_routes, info.num_nodes);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);
}

/****************************************************************
** Test lpmhtRouteNextGet() with many IPv4 routes.
****************************************************************/
void getNextTest4(lpmhtTableMode_e table_mode, unsigned int mem_prealloc)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned int prefix1;
  unsigned int net_prefix;
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  unsigned int next_prefix;
  unsigned int next_prefix_size;
  unsigned long long hit_count;
  unsigned int max_routes = 1000000;
  unsigned int i;
  unsigned int j;
  lpmhtTableInfo_t info;
  lpmhtTableProp_t prop;

  printf ("\n***** %s - IPv4 - %s - mem_prealloc:%d\n", __FUNCTION__,
                (table_mode == LPMHT_TRIE)?"TRIE":"HASH", mem_prealloc);

  memset (&prop, 0, sizeof(prop));

  prefix1 = 0;
  prefix_size = 0;


  /* Create routing table with hit count monitoring.
  */
  prop.hit_count = 1;
  prop.next_get = 1;
  if (mem_prealloc)
        prop.mem_prealloc = 1;
  t1 = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_TRIE,
                             &prop);
  assert (t1);

  /* A typical internet router will see every prefix with 
  ** prefix length 0 to 16. Therefore add all prefixes to the table for 
  ** this test.
  */
  i = 0;
  for (prefix_size = 0; prefix_size <= 16; prefix_size++)
  {
    for (j = 0; j < (1U<<prefix_size); j++)
    {
      if ((j == 0) && (prefix_size != 0))
      {
        /* Don't insert routes with network 0 and prefix size of non-zero.
        ** For example 0.0.0.0/8 is not a valid route.
        */
        continue;
      }
      prefix1 =  (prefix_size == 0)?0:j << (32 - prefix_size);
      route_entry = i;
      net_prefix = htonl(prefix1);
      rc = lpmhtRouteAdd(t1, (unsigned char *) &net_prefix, 
                    prefix_size, route_entry); 
#if 0      
      printf ("%d - ", i);
      printf ("%02x.%02x.%02x.%02x",
                    (prefix1 >> 24) & 0xff,
                    (prefix1 >> 16) & 0xff,
                    (prefix1 >> 8) & 0xff,
                    prefix1 & 0xff);
      printf ("/%d  rc:%d\n", prefix_size, rc);
#endif      
      i++;
    }
  }
  printf ("Inserted %d prefixes with prefix lengths 0 to 16\n", i);

  /* Add random routes with prefix length of 16 to 32.
  */
  for (; i < max_routes; i++)
  {
    prefix1 = rand();
    prefix_size = (rand() % 17) + 16;
    route_entry = i;
    net_prefix = htonl(prefix1);
    rc = lpmhtRouteAdd(t1, (unsigned char *) &net_prefix, 
                    prefix_size, route_entry); 
    if (rc)
    {
      if (rc == -2)
      {
        /* Ignore duplicate error, and try a different route.
        */
        i--;
        continue;
      } else
      {
        printf ("Error:%d while inserting route %d, prefix_size:%d\n", 
                      rc, i, prefix_size);
        TEST_EXIT(rc,0);
      }
    }
  }

  rc =  lpmhtRouteTableInfoGet (t1, &info);
  TEST_EXIT(rc,0);

  printf ("tree-1 - mem:%'zu virtual mem:%'zu routes:%'u/%'u nodes:%'u\n", 
                  info.mem_size, info.virtual_mem_size, info.num_routes, max_routes, info.num_nodes);

  printf ("First 10 elements:\n");
  net_prefix = 0;
  rc = lpmhtRouteFirstGet(t1, (unsigned char *) &net_prefix, 
                  &prefix_size, &route_entry, 
                  1, &hit_count);
  TEST_EXIT(rc,0);
  prefix1 = ntohl(net_prefix);
  for (i = 0; i < 10; i++)
  {
    printf ("%d - ", i);
    printf ("%02x.%02x.%02x.%02x",
                    (prefix1 >> 24) & 0xff,
                    (prefix1 >> 16) & 0xff,
                    (prefix1 >> 8) & 0xff,
                    prefix1 & 0xff);
    printf ("/%d\n", prefix_size);

    next_prefix = 0;
    rc = lpmhtRouteNextGet(t1, (unsigned char *) &net_prefix, 
                    prefix_size, &route_entry, 
                  (unsigned char *) &next_prefix, &next_prefix_size, 0, 0);
    TEST_EXIT(rc,0);

    net_prefix = next_prefix;
    prefix1 = htonl(net_prefix);
    prefix_size = next_prefix_size;
  }
  printf ("\n\n");

  /* Verify that the NextGet finds all inserted elements.
  */
  net_prefix = 0;
  prefix_size = 32;
  i = 0;
  do 
  {
    next_prefix = 0;
    rc = lpmhtRouteNextGet(t1, (unsigned char *) &net_prefix, 
                    prefix_size, &route_entry, 
                  (unsigned char *) &next_prefix, &next_prefix_size, 0, 0);
    if (rc)
    {
      break;
    }
    net_prefix = next_prefix;
    prefix_size = next_prefix_size;
    i++;
  } while (rc == 0);
  printf ("lpmhtRouteNextGet() found %d routes. Expected %d routes.\n\n", 
                  i, max_routes);

  /* Delete elements at random from the tree.
  */ 
  printf ("Deleting random routes...\n");
  for (i = 0; i < max_routes; i++)
  {
    prefix1 = rand();
    prefix_size = rand() % 33;

    next_prefix = 0;
    net_prefix = htonl(prefix1);
    rc = lpmhtRouteNextGet(t1, (unsigned char *) &net_prefix, 
                      prefix_size, &route_entry, 
                    (unsigned char *) &next_prefix, &next_prefix_size, 0, 0);
    if (rc != 0)
    {
      net_prefix = 0;
      rc = lpmhtRouteFirstGet(t1, (unsigned char *) &net_prefix, 
                    &prefix_size, &route_entry, 
                    1, &hit_count);
      TEST_EXIT(rc,0);
    } else
    {
      net_prefix = next_prefix;
      prefix_size = next_prefix_size;
    }

    rc = lpmhtRouteDelete(t1,(unsigned char *) &net_prefix,prefix_size);
//    TEST_EXIT(rc,0);
    if (rc != 0)
    {
      prefix1 = ntohl(net_prefix);
      printf ("%d - Delete Error:%d - ", i, rc);
      printf ("%02x.%02x.%02x.%02x",
                    (prefix1 >> 24) & 0xff,
                    (prefix1 >> 16) & 0xff,
                    (prefix1 >> 8) & 0xff,
                    prefix1 & 0xff);
      printf ("/%d\n", prefix_size);
    }
  }

  rc =  lpmhtRouteTableInfoGet (t1, &info);
  TEST_EXIT(rc,0);

  printf ("tree-1 - mem:%'zu virtual mem:%'zu routes:%'u/%'u nodes:%'u\n", 
                  info.mem_size, info.virtual_mem_size, info.num_routes, max_routes, info.num_nodes);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

}
/****************************************************************
** Test code coverage for lpmhtRouteTableInfoGet()/
****************************************************************/
void routeTableInfoGetTest1(lpmhtTableMode_e table_mode)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix1[4];
  unsigned long route_entry = 2;
  lpmhtTableInfo_t info;
  lpmhtTableProp_t prop;
  unsigned int max_routes = 2000;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));

  printf ("%s Route Table InfoGet test. Max Route Capacity:%'u\n",
                (table_mode == LPMHT_TRIE)?"Trie":"Hash", max_routes);

  /* Create routing table with hit count monitoring.
  */
  prop.hit_count = 1;
  t1 = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                             LPMHT_IPV4,
                             table_mode,
                             &prop);
  assert (t1);

  /* Invoke lpmhtRouteTableInfoGet with NULL trie tree.
  */
  rc = lpmhtRouteTableInfoGet(0, &info); 
  TEST_EXIT(rc,-1);

  /* Invoke lpmhtRouteTableInfoGet with empty tree.
  */
  rc = lpmhtRouteTableInfoGet(t1, &info); 
  TEST_EXIT(rc,0);
  printf ("  Empty IPv4 Route Table - mem_size:%'zu virtual_mem_size:%'zu  num_routes:%'u num_nodes:%'u\n",
                  info.mem_size, info.virtual_mem_size, info.num_routes, info.num_nodes);

  /* Insert route 0x40/3 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x20;
  route_entry = 3;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke lpmhtRouteTableInfoGet with one route.
  */
  rc = lpmhtRouteTableInfoGet(t1, &info); 
  TEST_EXIT(rc,0);
  printf ("  IPv4 Route Table with One Route - mem_size:%'zu virtual_mem_size:%'zu  num_routes:%'u num_nodes:%'u\n",
                  info.mem_size, info.virtual_mem_size, info.num_routes, info.num_nodes);


  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);
}


/****************************************************************
** Test code coverage for lpmhtLPMatch()
****************************************************************/
void lpMatchTest1(lpmhtTableMode_e table_mode)
{
  int rc;
  lpmhtRouteTable_t *t1, *t2;
  unsigned char prefix1[4];
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));

  /* Create routing table with hit count monitoring.
  */
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             table_mode,
                             0);
  assert (t1);

  /* Create route table with hit count monitoring.
  */
  prop.hit_count = 1;
  t2 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV4,
                             table_mode,
                             &prop);
  assert (t2);

  /* Invoke LPMatch with NULL tree.
  */
  rc = lpmhtLPMatch(0, prefix1, &prefix_size, &route_entry);
  TEST_EXIT(rc,-1);

  /* Invoke LPMatch with prefix set to 0.
  */
  rc = lpmhtLPMatch(t1, 0, &prefix_size, &route_entry);
  TEST_EXIT(rc,-1);

  /* Invoke LPMatch with prefix_size set to 0.
  */
  rc = lpmhtLPMatch(t1, prefix1, 0, &route_entry);
  TEST_EXIT(rc,-1);

  /* Invoke LPMatch with user_data set to 0.
  */
  rc = lpmhtLPMatch(t1, prefix1, &prefix_size, 0);
  TEST_EXIT(rc,-1);

  /* Invoke LPMatch on empty tree.
  */
  rc = lpmhtLPMatch(t1, prefix1, &prefix_size, &route_entry);
  TEST_EXIT(rc,-2);

  /* Insert route 0xa0/3 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xa0;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke LPMatch on t1 to exactly match the inserted route.
  */
  rc = lpmhtLPMatch(t1, prefix1, &prefix_size, &route_entry);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0xa0);

  /* Invoke LPMatch on t1 with non-NULL hit_count.
  */
  rc = lpmhtLPMatch(t1, prefix1, &prefix_size, &route_entry); 
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0xa0);

  /* Insert route 0x60/3 into t2
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x60;
  rc = lpmhtRouteAdd(t2, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke LPMatch on t2 with prefix of 0x61. 
  ** The lookup should match the 0x60 route.
  */
  prefix1[0] = 0x61;
  rc = lpmhtLPMatch(t2, prefix1, &prefix_size, &route_entry); 
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);

  /* Invoke LPMatch on t2 with prefix of 0x81. 
  ** The lookup should fail.
  */
  prefix1[0] = 0x81;
  rc = lpmhtLPMatch(t2, prefix1, &prefix_size, &route_entry); 
  TEST_EXIT(rc,-2);

  /* Invoke LPMatch on t2 with prefix of 0.1.0.0
  ** The lookup should fail.
  */
  prefix1[0] = 0;
  prefix1[1] = 1;
  rc = lpmhtLPMatch(t2, prefix1, &prefix_size, &route_entry); 
  TEST_EXIT(rc,-2);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

  rc = lpmhtRouteTableDestroy(t2);
  TEST_EXIT(rc,0);

}

/****************************************************************
** Test code coverage for lpmhtLPMatch() on IPv6
****************************************************************/
void lpMatchTest2(lpmhtTableMode_e table_mode)
{
  int rc;
  lpmhtRouteTable_t *t1, *t2;
  unsigned char prefix1[16];
  unsigned long route_entry = 2;
  unsigned int prefix_size;
  unsigned long user_data;
  unsigned long long hit_count;
  lpmhtTableProp_t prop;

  memset (&prop, 0, sizeof(prop));

  memset (prefix1, 0, sizeof(prefix1));

  /* Create routing table with hit count monitoring.
  */
  t1 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV6,
                             table_mode,
                             0);
  assert (t1);

  /* Create route table with hit count monitoring.
  */
  prop.hit_count = 1;
  t2 = lpmhtRouteTableCreate(20,  /* Max Routes */
                             LPMHT_IPV6,
                             table_mode,
                             &prop);
  assert (t2);

  /* Invoke LPMatch with NULL tree.
  */
  rc = lpmhtLPMatch(0, prefix1, &prefix_size, &route_entry);
  TEST_EXIT(rc,-1);

  /* Invoke LPMatch with prefix set to 0.
  */
  rc = lpmhtLPMatch(t1, 0, &prefix_size, &route_entry);
  TEST_EXIT(rc,-1);

  /* Invoke LPMatch on empty tree.
  */
  rc = lpmhtLPMatch(t1, prefix1, &prefix_size, &route_entry);
  TEST_EXIT(rc,-2);

  /* Insert route 0xa0/3 into t1
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0xa0;
  rc = lpmhtRouteAdd(t1, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke LPMatch on t1 to exactly match the inserted route.
  */
  rc = lpmhtLPMatch(t1, prefix1, &prefix_size, &route_entry);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0xa0);


  /* Invoke LPMatch on t1 with non-NULL hit_count.
  */
  rc = lpmhtLPMatch(t1, prefix1, &prefix_size, &route_entry); 
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);
  TEST_EXIT(prefix1[0],0xa0);

  /* Insert route 0x60/3 into t2
  */
  memset (prefix1, 0, sizeof(prefix1));
  prefix1[0] = 0x60;
  rc = lpmhtRouteAdd(t2, prefix1, 3, route_entry); 
  TEST_EXIT(rc,0);

  /* Invoke LPMatch on t2 with prefix of 0x61. 
  ** The lookup should match the 0x60 route.
  */
  prefix1[0] = 0x61;
  rc = lpmhtLPMatch(t2, prefix1, &prefix_size, &route_entry); 
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,3);

  /* Get the route information to verify hit count.
  */
  rc = lpmhtRouteGet (t2, prefix1, prefix_size, &user_data, 0, &hit_count);
  TEST_EXIT(rc,0);
  TEST_EXIT((unsigned int) hit_count,1);

  /* Invoke LPMatch on t2 with prefix of 0x81. 
  ** The lookup should fail.
  */
  prefix1[0] = 0x81;
  rc = lpmhtLPMatch(t2, prefix1, &prefix_size, &route_entry); 
  TEST_EXIT(rc,-2);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

  rc = lpmhtRouteTableDestroy(t2);
  TEST_EXIT(rc,0);
}

/****************************************************************
** Test code coverage for IPv4 rule table.
****************************************************************/
void ipv4RuleTest1(void)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix[4];
  unsigned int prefix_size;
  unsigned long user_data;
  lpmhtTableProp_t prop;
  unsigned int max_routes = 10000000;
  unsigned int prefix32;
  int i;
  lpmhtTableInfo_t info;

  printf ("\n***** %s\n", __FUNCTION__);

  memset (&prop, 0, sizeof(prop));

  memset (prefix, 0, sizeof(prefix));

  /* Create an IPv4 route table with hit count monitoring, IPv4 rules,
  ** and hash pre-allocation.
  ** These are the modes likely to be used for a software-based 
  ** router with a large route table.
  */
  prop.hit_count = 1;
  prop.ipv4_rules = 1;
  prop.hash_prealloc = 1;
  t1 = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                             LPMHT_IPV4,
                             LPMHT_HASH,
                             &prop);
  assert (t1);

  printf ("   Adding 10 routes, waiting 500ms between route adds...\n");
  /* Add 10 routes with 500ms delay between route additions.
  ** This causes the IPv4 rule generation to be restarted several times.
  */
  prefix32 = 0x01010101;
  for (i = 0; i < 10; i++)
  {
    *(unsigned int *) prefix = htonl(prefix32);
    prefix32 += 0x100;
    rc = lpmhtRouteAdd(t1, prefix, 24, i); 
    TEST_EXIT(rc,0);
    msleep (500);
  }

  /* Wait for the IPv4 rule table to be generated.
  */
  printf ("   Waiting for the IPv4 rule table to be generated...\n");
  do
  {
    sleep (1);
    (void)  lpmhtRouteTableInfoGet (t1, &info);
  } while (0 == info.ipv4_rule_table_ready);


  /* Invoke LPMatch for a route that doesn't match any of the 
  ** 24-bit routes.
  */
  prefix32 = 0x02010101;
  *(unsigned int *) prefix = htonl(prefix32);
  rc = lpmhtLPMatch(t1, prefix, &prefix_size, &user_data);
  TEST_EXIT(rc,-2);

  /* Add default route to the route table.
  */
  memset (prefix, 0, sizeof(prefix));
  rc = lpmhtRouteAdd(t1, prefix, 0, 999); 
  TEST_EXIT(rc,0);

  /* Wait for the rule table to be re-constructed.
  */
  printf ("   Waiting for the IPv4 rule table to be generated...\n");
  do
  {
    sleep (1);
    (void)  lpmhtRouteTableInfoGet (t1, &info);
  } while (0 == info.ipv4_rule_table_ready);

  /* Now we should get a match.
  */
  prefix32 = 0x02010101;
  *(unsigned int *) prefix = htonl(prefix32);
  rc = lpmhtLPMatch(t1, prefix, &prefix_size, &user_data);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size,0);
  TEST_EXIT(999, (unsigned int) user_data);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

}

/****************************************************************
** Test code coverage for IPv6 flow table.
****************************************************************/
void ipv6FlowTest1(void)
{
  int rc;
  lpmhtRouteTable_t *t1;
  unsigned char prefix[16];
  unsigned int prefix_size;
  unsigned long user_data;
  lpmhtTableProp_t prop;
  unsigned int max_routes = 1000;
  unsigned int prefix32;
  int i;

  printf ("\n***** %s\n", __FUNCTION__);


  memset (prefix, 0, sizeof(prefix));

  /* Create an IPv6 route table with hit count monitoring, IPv6 flows,
  ** and hash pre-allocation.
  */
  memset (&prop, 0, sizeof(prop));
  prop.hit_count = 1;
  prop.hash_prealloc = 1;
  prop.ipv6_flow = 1;
  prop.ipv6_max_flows = 75;
  prop.ipv6_flow_age_time = 2;
  t1 = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                             LPMHT_IPV6,
                             LPMHT_HASH,
                             &prop);
  assert (t1);

  printf ("   Adding 10 routes.\n");
  /* Add 10 routes.
  */
  prefix32 = 0x01010101;
  for (i = 0; i < 10; i++)
  {
    memset (prefix, 0, sizeof(prefix));
    *(unsigned int *) &prefix[4] = htonl(prefix32);
    prefix32++;
    rc = lpmhtRouteAdd(t1, prefix, 64, i); 
    TEST_EXIT(rc,0);
  }

  /* Invoke LPMatch for a route that doesn't match any of the 
  ** routes. This operation should not create any flows.
  */
  memset (prefix, 0, sizeof(prefix));
  prefix32 = 0x02010101;
  *(unsigned int *) &prefix[4] = htonl(prefix32);
  rc = lpmhtLPMatch(t1, prefix, &prefix_size, &user_data);
  TEST_EXIT(rc,-2);

  /* Invoke LPMatch for a matching route.
  ** This operation should create a flow.
  */
  memset (prefix, 0, sizeof(prefix));
  prefix32 = 0x01010101;
  *(unsigned int *) &prefix[4] = htonl(prefix32);
  *(unsigned int *) &prefix[8] = htonl(0x55555555);
  rc = lpmhtLPMatch(t1, prefix, &prefix_size, &user_data);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size, 64);
  TEST_EXIT((unsigned int) user_data, 0);

  /* Invoke LPMatch for a matching route.
  ** This operation should create a new flow.
  */
  memset (prefix, 0, sizeof(prefix));
  prefix32 = 0x01010101;
  *(unsigned int *) &prefix[4] = htonl(prefix32);
  *(unsigned int *) &prefix[8] = htonl(0x66666666);
  rc = lpmhtLPMatch(t1, prefix, &prefix_size, &user_data);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size, 64);
  TEST_EXIT((unsigned int) user_data, 0);

  /* Invoke LPMatch for a matching route. 
  ** Since the flow already exists, it should be matched.
  */
  memset (prefix, 0, sizeof(prefix));
  prefix32 = 0x01010101;
  *(unsigned int *) &prefix[4] = htonl(prefix32);
  *(unsigned int *) &prefix[8] = htonl(0x55555555);
  rc = lpmhtLPMatch(t1, prefix, &prefix_size, &user_data);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size, 64);
  TEST_EXIT((unsigned int) user_data, 0);

  /* Add default route to the route table.
  ** Adding routes should invalidate all flows.
  */
  memset (prefix, 0, sizeof(prefix));
  rc = lpmhtRouteAdd(t1, prefix, 0, 999); 
  TEST_EXIT(rc,0);

  /* Invoke LPMatch for a matching route. 
  ** Since the flow is invalid, it should be re-learned.
  */
  memset (prefix, 0, sizeof(prefix));
  prefix32 = 0x01010101;
  *(unsigned int *) &prefix[4] = htonl(prefix32);
  *(unsigned int *) &prefix[8] = htonl(0x55555555);
  rc = lpmhtLPMatch(t1, prefix, &prefix_size, &user_data);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size, 64);
  TEST_EXIT((unsigned int) user_data, 0);

  printf ("Waiting 5 seconds for IPv6 flows to be aged out...\n");
  sleep(5);

  /* Invoke LPMatch for a matching route. 
  ** Since the flow is aged out, it should be re-learned.
  */
  memset (prefix, 0, sizeof(prefix));
  prefix32 = 0x01010101;
  *(unsigned int *) &prefix[4] = htonl(prefix32);
  *(unsigned int *) &prefix[8] = htonl(0x55555555);
  rc = lpmhtLPMatch(t1, prefix, &prefix_size, &user_data);
  TEST_EXIT(rc,0);
  TEST_EXIT(prefix_size, 64);
  TEST_EXIT((unsigned int) user_data, 0);

  rc = lpmhtRouteTableDestroy(t1);
  TEST_EXIT(rc,0);

}
/****************************************************************
** Test performance and memory usage for IPv4 packet forwarding
** while routes are added/deleted.
****************************************************************/
void ipv4Perf1(lpmhtTableMode_e table_mode)
{
  unsigned int i;
  int rc;
  lpmhtTableProp_t prop;
  pthread_t route_add_delete_task;
  lpmhtTableInfo_t info;

  memset (&prop, 0, sizeof(prop));


  /* Create the IPv4 Trie tree for the Add/Delete/Modify test
  ** and populate it with IPv4 routes.
  */
  printf ("\n\nCreating IPv4 %s route table with %'u routes.\n", 
                  (table_mode == LPMHT_TRIE)?"Trie":"Hash",
                  TRIE_MAX_ROUTES);

  printf ("Physical Memory Before Table Creation:\n"); 
  physMemShow();


  /* Create route table with hit count monitoring.
  */
  prop.hit_count = 1;
//  prop.hash_prealloc = 1;
  ipv4_tree = lpmhtRouteTableCreate(TRIE_MAX_ROUTES,  /* Max Routes */
                             LPMHT_IPV4,
                             table_mode,
                             &prop);
  assert (ipv4_tree);

  printf ("Physical Memory After Table Creation:\n");
  physMemShow();

  /* Add default route to the table.
  */
  i = 0;
  ipv4_prefix[i] = 0;
  ipv4_prefix_size[i] = 0;
  rc = lpmhtRouteAdd (ipv4_tree, (unsigned char *) &ipv4_prefix[i], 
                              ipv4_prefix_size[i], i);
  for (i = 1; i < TRIE_MAX_ROUTES; i++)
  {
    ipv4_prefix[i] = rand();
    ipv4_prefix_size[i] = rand() % 33;
    rc = lpmhtRouteAdd (ipv4_tree, (unsigned char *) &ipv4_prefix[i], 
                                ipv4_prefix_size[i], i);
    if (rc == -2)
    {
      /* If duplicate entry then try again.
      */
      i--; 
    }
  }

  rc =  lpmhtRouteTableInfoGet (ipv4_tree, &info);
  TEST_EXIT(rc,0);

  printf ("Added %'u routes to IPv4 Route Table. Physical Mem:%'zuKB  Virtual Mem:%'zuKB\n",
                info.num_routes, info.mem_size/1024, info.virtual_mem_size/1024);

  printf ("Physical Memory After Populating The Route Table:\n");
  physMemShow();

  /* Start task which continuously deletes and adds entries.
  */
  (void) pthread_create (&route_add_delete_task, 0, add_delete_task, 0);

  /* Perform tree lookups while the routes are added and deleted.
  */
  printf ("Performing Longest Prefix match Test with %'u iterations while adding/deleting routes...\n",
                NUM_LPM_MATCHES);

  route_lookup();

  pthread_cancel (route_add_delete_task);
  pthread_join(route_add_delete_task, 0);

  rc = lpmhtRouteTableDestroy(ipv4_tree);
  TEST_EXIT(rc,0);

}
/****************************************************************
** Test performance and memory usage for IPv6 packet forwarding
** while routes are added/deleted.
**
** This test creates two threads that add/delete routes and 
** two threads that perform longest prefix match lookups.
**
****************************************************************/
/* Flag which tells IPv6 route add/delete tasks and route lookup 
** tasks that the test is done.
*/
lpmhtRouteTable_t *ipv6_route_table;

typedef struct {
  unsigned char ipv6_addr[16];
  unsigned char prefix_length;
  unsigned char pad[3]; /* Align to 4 bytes */
} ipv6Prefix_t;

ipv6Prefix_t *ipv6_prefix_list;
unsigned int num_ipv6_routes = 0;

/* Configuration/Status structure for route add/delete task. 
*/
typedef struct {
  /* Index in the ipv6_prefix_list where the route add/delete task 
  ** looks for routes to be added/deleted.
  */
  unsigned int first_route; 
  unsigned int num_add_delete; /* Number of routes to add/delete */

  /* Status Parameters.
  */
  unsigned long long total_route_adds;
  unsigned long long total_route_deletes;

  unsigned long long longest_route_add_ns;
  unsigned long long average_route_add_ns;

  unsigned long long longest_route_delete_ns;
  unsigned long long average_route_delete_ns;

} taskInfo_t;


typedef struct {
  unsigned int num_routes;

  unsigned long long total_route_lookups;

  unsigned long long longest_route_lookup_ns;
  unsigned long long average_route_lookup_ns;

  /* Number of lookups that took longer than 1 millisecond.
  */
  unsigned long long num_lookups_longer_than_ms;

} lookupInfo_t;

volatile unsigned int end_ipv6_test = 0;

/*************************************************************
** ipv6_add_delete_task
**
** This task continuously adds and deletes routes in the 
** route table.
**
*************************************************************/
static void * ipv6_add_delete_task (void *parm)
{
  int rc;
  unsigned int i;
  unsigned long long num_delete = 0, num_add = 0;
  taskInfo_t *tinfo = (taskInfo_t *) parm;
  unsigned long long max_add_time = 0;
  unsigned long long average_add_time = 0;
  unsigned long long max_delete_time = 0;
  unsigned long long average_delete_time = 0;
  unsigned long long start_time, delta_time;

//  printf ("Starting IPv6 Add Delete. First:%'u Count:%'u\n",
//        tinfo->first_route, 
//        tinfo->num_add_delete);

  do 
  {
    /* Delete Routes.
    ** We always leave the default route in the table.
    */
    for (i = tinfo->first_route; 
              i < tinfo->first_route + tinfo->num_add_delete; i++)
    {
      start_time = timeNanoGet();
      rc = lpmhtRouteDelete(ipv6_route_table, 
                       ipv6_prefix_list[i].ipv6_addr, 
                      ipv6_prefix_list[i].prefix_length);
      delta_time = timeNanoGet() - start_time;
      if (delta_time > max_delete_time)
      {
        max_delete_time = delta_time;
      }
      average_delete_time += delta_time;
      TEST_EXIT(rc,0);

      num_delete++;

      if (end_ipv6_test)
                        break;
    }

    if (end_ipv6_test)
                        break;

    /* Add Routes.
    */
    for (i = tinfo->first_route; 
                i < tinfo->first_route + tinfo->num_add_delete; i++)
    {
      start_time = timeNanoGet();
      rc = lpmhtRouteAdd (ipv6_route_table, 
                     ipv6_prefix_list[i].ipv6_addr, 
                    ipv6_prefix_list[i].prefix_length,
                    i);
      delta_time = timeNanoGet() - start_time;
      if (delta_time > max_add_time)
      {
        max_add_time = delta_time;
      }
      average_add_time += delta_time;
      TEST_EXIT(rc,0);

      num_add++;

      if (end_ipv6_test)
                        break;
    }
    if (end_ipv6_test)
                        break;
  } while (1);

  tinfo->total_route_adds = num_add;
  tinfo->total_route_deletes = num_delete;

  if (num_add)
  {
    tinfo->longest_route_add_ns = max_add_time;
    tinfo->average_route_add_ns = average_add_time / num_add;
  } else
  {
    tinfo->longest_route_add_ns = 0;
    tinfo->average_route_add_ns = 0;
  }

  tinfo->longest_route_delete_ns = max_delete_time;
  tinfo->average_route_delete_ns = average_delete_time / num_delete;
  return 0;
}

/*************************************************************
** Perform LPM route lookups.
**
*************************************************************/
static void *ipv6_route_lookup_task (void *parm)
{
  lookupInfo_t *tinfo = (lookupInfo_t *) parm;
  int rc;
  unsigned long route_entry;
  unsigned int prefix_size;
  unsigned int i;
  unsigned long long max_lookup_time = 0;
  unsigned long long average_lookup_time = 0;
  unsigned long long start_lookup_time, delta_lookup_time;

  i = 0;
  do {
    start_lookup_time = timeNanoGet();
    rc = lpmhtLPMatch (ipv6_route_table,  ipv6_prefix_list[i].ipv6_addr, 
                    &prefix_size, &route_entry);
    delta_lookup_time = timeNanoGet() - start_lookup_time;
    if (delta_lookup_time > max_lookup_time)
    {
      max_lookup_time = delta_lookup_time;
    }
    if (delta_lookup_time > 1000000)
          tinfo->num_lookups_longer_than_ms++;

    average_lookup_time += delta_lookup_time;

    TEST_EXIT(rc,0);

    i++;
    if (i == tinfo->num_routes)
                        i = 0;

    tinfo->total_route_lookups++;

    if (end_ipv6_test)
                        break;

  } while (1);

  tinfo->longest_route_lookup_ns = max_lookup_time;
  tinfo->average_route_lookup_ns = average_lookup_time / tinfo->total_route_lookups; 

  return 0;
}
/****************************************************************
** Test Entry Point.
****************************************************************/
void ipv6Perf1(lpmhtTableMode_e table_mode, 
                        unsigned int num_routes,
                        unsigned int test_duration_seconds,
                        unsigned int num_add_delete_tasks,
                        unsigned int num_lookup_tasks)
{
  unsigned int i;
  int rc;
  lpmhtTableInfo_t info;
  lpmhtTableProp_t prop;
  taskInfo_t task_info[num_add_delete_tasks];
  pthread_t  add_delete_pthread[num_add_delete_tasks];
  lookupInfo_t lookup_info[num_lookup_tasks];
  pthread_t  lookup_pthread[num_lookup_tasks];

  memset (&prop, 0, sizeof(prop));
  memset (task_info, 0, sizeof(task_info));
  memset (lookup_info, 0, sizeof(lookup_info));

  end_ipv6_test = 0;

  /* Create the IPv6 routing table for the Add/Delete/Modify test
  ** and populate it with IPv6 routes.
  */
  printf ("********* IPv6 >>%s<< Performance Test. Duration:%'u seconds.\n\n",
                  (table_mode == LPMHT_TRIE)?"TRIE":"HASH",
                  test_duration_seconds
                );

  ipv6_prefix_list = (ipv6Prefix_t *) malloc(num_routes * sizeof(ipv6Prefix_t));
  assert(ipv6_prefix_list);

//  printf ("Physical Memory Before Table Creation:\n"); 
//  physMemShow();

  printf ("Creating IPv6 %s routing table with %'u routes...\n", 
                  (table_mode == LPMHT_TRIE)?"TRIE":"HASH",
                  num_routes);

  /* Create route table with hash table pre-allocation.
  */
  prop.hash_prealloc = 1;
  ipv6_route_table = lpmhtRouteTableCreate(num_routes,  /* Max Routes */
                             LPMHT_IPV6,
                             table_mode,
                             &prop);
  assert (ipv6_route_table);



  /* Add default route to the table.
  */
  i = 0;
  memset (ipv6_prefix_list, 0, num_routes * sizeof (ipv6Prefix_t));
  rc = lpmhtRouteAdd (ipv6_route_table, ipv6_prefix_list[i].ipv6_addr, 
                              ipv6_prefix_list[i].prefix_length, i);

  rc =  lpmhtRouteTableInfoGet (ipv6_route_table, &info);
  TEST_EXIT(rc,0);
  printf ("Allocated physical memory for table with one route:%'zuKB\n",
                                        info.mem_size / 1024);
  for (i = 1; i < num_routes; i++)
  {
    *(unsigned int *) &ipv6_prefix_list[i].ipv6_addr[0] = rand();
    *(unsigned int *) &ipv6_prefix_list[i].ipv6_addr[4] = rand();
    *(unsigned int *) &ipv6_prefix_list[i].ipv6_addr[8] = rand();
    *(unsigned int *) &ipv6_prefix_list[i].ipv6_addr[12] = rand();
    ipv6_prefix_list[i].prefix_length = rand() % 129;

    rc = lpmhtRouteAdd (ipv6_route_table, ipv6_prefix_list[i].ipv6_addr, 
                                ipv6_prefix_list[i].prefix_length, i);
    if (rc == -2)
    {
      /* If duplicate entry then try again.
      */
      i--; 
    }
  }
  num_ipv6_routes = i;

  rc =  lpmhtRouteTableInfoGet (ipv6_route_table, &info);
  TEST_EXIT(rc,0);
  printf ("Allocated physical memory for table with %'u routes:%'zuKB\n", info.num_routes,
                                        info.mem_size / 1024);
//  printf ("Physical Memory After loading %'u routes into the IPv6 table.\n", num_ipv6_routes); 
//  physMemShow();

  if (num_add_delete_tasks)
    printf ("Creating %'u route add/delete tasks...\n", num_add_delete_tasks);

  /* Start route add/delete tasks. Tell each task which routes to add/delete from 
  ** the prefix list. The default route, which is the first route is in the 
  ** prefix list, is never deleted. This makes sure that the route lookups always 
  ** succeed.
  */
  for (i = 0; i < num_add_delete_tasks; i++)
  {
    task_info[i].first_route = 1 + (i * ((num_routes - 1) / num_add_delete_tasks));
    task_info[i].num_add_delete = (num_routes - 1) / num_add_delete_tasks;

    (void) pthread_create (&add_delete_pthread[i], 0, ipv6_add_delete_task, &task_info[i]);
  }

  if (num_lookup_tasks)
    printf ("Creating %'u route lookup tasks...\n", num_lookup_tasks);
  /* Start route lookup tasks.
  */
  for (i = 0; i < num_lookup_tasks; i++)
  {
    lookup_info[i].num_routes = num_routes;
    (void) pthread_create (&lookup_pthread[i], 0, ipv6_route_lookup_task, &lookup_info[i]);
  }

  printf ("Waiting %'u seconds for the test to run...\n", test_duration_seconds);

  sleep (test_duration_seconds);

  /* Tell the threads to end the test.
  */
  end_ipv6_test = 1;
  for (i = 0; i < num_add_delete_tasks; i++)
  {
//    (void) pthread_cancel (add_delete_pthread[i]);
    (void) pthread_join (add_delete_pthread[i], 0);
  }
  for (i = 0; i < num_lookup_tasks; i++)
  {
    (void) pthread_cancel (lookup_pthread[i]);
    (void) pthread_join (lookup_pthread[i], 0);
  }

  lpmhtRouteTableDestroy(ipv6_route_table);

  free (ipv6_prefix_list);
//  printf ("Physical Memory After destroying the IPv6 table.\n"); 
//  physMemShow();

  for (i = 0; i < num_add_delete_tasks; i++)
  {
    printf ("Add/Delete Task:%d - Adds:%'llu Deletes:%'llu\n",
                i+1, task_info[i].total_route_adds, task_info[i].total_route_deletes);
    printf ("    Longest Add:%'lluns  Average Add:%'lluns   Longest Delete:%'lluns  Average Delete:%'lluns\n\n",
                task_info[i].longest_route_add_ns, task_info[i].average_route_add_ns,
                task_info[i].longest_route_delete_ns, task_info[i].average_route_delete_ns);

  }
  printf ("\n\n");
  for (i = 0; i < num_lookup_tasks; i++)
  {
    printf ("Route Lookup Task:%d - Num Lookups:%'llu\n",
                i+1, lookup_info[i].total_route_lookups);
    printf ("    Longest Lookup:%'lluns  Average Lookup:%'lluns Num. lookups longer than 1 millisecond:%'llu\n\n",
                lookup_info[i].longest_route_lookup_ns, lookup_info[i].average_route_lookup_ns,
                                lookup_info[i].num_lookups_longer_than_ms); 
  }

}
/*********************************
**********************************
** Start of the test program.
**********************************
*********************************/
int main ()
{
 struct timespec res;

  /* Enable printf() to format integers using comma separators. For example
  ** printf ("%'d", val) prints 100,000,000 instead of 100000000.
  ** Note the ' between % and d. This triggers printf() to use
  ** the country code formatting conventions.
  */
  setenv ("LC_ALL","en_US.UTF-8",1);
  setlocale (LC_NUMERIC, "");

 if (0 == clock_getres(CLOCK_MONOTONIC, &res))
 {
  printf ("CLOCK_MONOTONIC Clock Resolution: tv_sec:%lu tv_nsec:%lu\n",
                  res.tv_sec, res.tv_nsec);
 } else perror ("CLOCK_MONOTONIC");


#if 1   
  createTest1();
  insertTest1();
  insertTest3();
  getSetTest2();
  insertTest2(LPMHT_TRIE);
  insertTest2(LPMHT_HASH);
  deleteTest1(LPMHT_TRIE);
  deleteTest1(LPMHT_HASH);
  firstGetTest1();
  getSetTest1();
  getNextTest1();
  getNextTest2();
  routeTableInfoGetTest1(LPMHT_TRIE);
  routeTableInfoGetTest1(LPMHT_HASH);
  lpMatchTest1(LPMHT_TRIE);
  lpMatchTest1(LPMHT_HASH);
  lpMatchTest2(LPMHT_TRIE);
  lpMatchTest2(LPMHT_HASH);
  ipv4RuleTest1();
  ipv6FlowTest1();
#endif
#if 1
  getNextTest3(LPMHT_TRIE, 0);
  getNextTest3(LPMHT_HASH, 0);
  getNextTest3(LPMHT_TRIE, 1);
  getNextTest3(LPMHT_HASH, 1);

  getNextTest4(LPMHT_TRIE, 0);
  getNextTest4(LPMHT_HASH, 0);
  getNextTest4(LPMHT_TRIE, 1);
  getNextTest4(LPMHT_HASH, 1);

  ipv4Perf1(LPMHT_TRIE);
  ipv4Perf1(LPMHT_HASH);
#endif  
#if 1
  ipv6Perf1(LPMHT_TRIE, 
              1000000,  /* Number of routes */
              10,  /* Number of seconds to run this test */
              2,   /* Number of route Add/Delete tasks */
              2);  /* Number of route lookup tasks */
  ipv6Perf1(LPMHT_HASH, 
              2000000,  /* Number of routes */
              40,  /* Number of seconds to run this test */
              2,   /* Number of route Add/Delete tasks */
              2);  /* Number of route lookup tasks */
#endif

}
