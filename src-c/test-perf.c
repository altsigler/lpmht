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
** April-02-2023/Andrey Tsigler
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <sys/random.h>
#include <arpa/inet.h>
#include <assert.h>
#include <locale.h>
#include <pthread.h>

#include "lpmht-api.h"

/* Route Table Prefix Mode.
** The prefix mode controls what prefixes are populated in the route table 
** for the performance test.
** 
** The values apply to IPv4 and IPv6 prefixes, but the effects are different
** depending on whether IPv4 or IPv6 test is being conducted.
*/
typedef enum {
   PMODE_1 = 1, /*IPv4 Random Addresses, Fixed Prefix Length /32. */
                /*IPv6 Random Addresses, Fixed Prefix Length /128. */

   PMODE_2 = 2, /*IPv4 Random Addresses, Two Prefixes /32 and /24. */
                /*IPv6 Random Addresses, Two Prefixes /48 and /64. */

   PMODE_3 = 3, /*IPv4 Random Addresses, Four Prefixes /32, /24, /23, /22. */
                /*IPv6 Random Addresses, Four Prefixes /48, /64, /80, /100. */

   PMODE_4 = 4, /*IPv4 Random Addresses, Random Prefixes /0 to /24 */
                /*IPv6 Random Addresses, Random Prefixes /0 to /48 */

   PMODE_5 = 5  /*IPv4 Random Addresses, Random Prefixes /0 to /32 */
                /*IPv6 Random Addresses, Random Prefixes /0 to /128 */

} prefixMode_e;

typedef struct
{
  unsigned long next_hop;  
} routeEntry_t;

#define TEST_EXIT(rc,expected_rc) \
if ((rc) != (expected_rc))    \
{                                       \
  printf ("%s %d - Unexpected error %d (Expected %d)\n", \
                  __FUNCTION__, __LINE__,                \
                  (rc), (expected_rc)); \
  exit (0);                                             \
}

typedef struct 
{
  unsigned char prefix[4];
  unsigned int len; 
} ipv4List_t __attribute__ ((aligned (4))); 
static ipv4List_t *ipv4_list = 0;

typedef struct 
{
  unsigned char prefix[16];
  unsigned int len;
  unsigned int pad[3];
} ipv6List_t __attribute__ ((aligned (16)));
static ipv6List_t *ipv6_list = 0;

typedef struct
{
  lpmhtRouteTable_t *route_table;
  unsigned long long num_lookups; /* How many route lookups to perform */
  unsigned int task_num;
  unsigned int num_routes; /* Number of routes in the route table */
} lookupTaskParm_t;

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
** Generate IPv4 prefixes. 
** This function fills out the ipv4_list array. 
** The addresses in the ipv4 list can subsequently be used
** for inserting entries into the route tables and 
** performing lookups. 
**
** The function temporarily creates a Hash route table and inserts 
** the newly created addresses into that route table. This is done
** to eliminate duplicate prefixes. Note that during random
** prefix generation duplicates are likely to happen, 
** especially when the prefix length is smaller than /16.
**
*************************************************************/
static void ipv4_list_generate(prefixMode_e test_type, 
                                unsigned int max_routes)
{
  lpmhtRouteTable_t *test_tree;
  int rc;
  unsigned int prefix1;
  unsigned int net_prefix;
  routeEntry_t route_entry;
  unsigned int prefix_size;
  unsigned int i;
  unsigned int dup_count = 0;
  lpmhtTableProp_t prop;

  if (0 != ipv4_list)
                free (ipv4_list);

  ipv4_list = (ipv4List_t *) malloc (sizeof(ipv4List_t) * max_routes);
  assert(ipv4_list);

  printf ("\nGenerating %'u IPv4 Prefixes...\n", max_routes);

  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  test_tree = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                                        LPMHT_IPV4,
                                        LPMHT_HASH,
                                        &prop);
  assert (test_tree);

  srand(1);

  switch (test_type)
  {
    case 1:
        printf ("1 - IPv4 Random Addresses, Fixed Prefix Length /32.\n");
      break;
    case 2:
        printf ("2 - IPv4 Random Addresses, Two Prefixes /32 and /24.\n");
      break;
    case 3:
        printf ("3 - IPv4 Random Addresses, Four Prefixes /32, /24, /23, /22.\n");
      break;
    case 4:
        printf ("4 - IPv4 Random Addresses, Random Prefixes /0 to /24\n");
      break;
    case 5:
        printf ("5 - IPv4 Random Addresses, Random Prefixes /0 to /32\n");
      break;
    default:
      printf ("Unknown Test Type: %d\n", test_type);
      exit (-1);
  }
  /* Add random routes with prefix length of 32.
  */
  for (i = 0; i < max_routes; i++)
  {
    switch (test_type)
    {
     case 1:
        prefix1 = rand();
        prefix_size = 32;
      break;
     case 2:
        prefix1 = rand();
        prefix_size = (i < (max_routes / 2))?32:24;
      break;
     case 3:
        prefix1 = rand();
        if (i < (max_routes / 4)) prefix_size = 32;
          else if (i < (max_routes / 2)) prefix_size = 24;
           else if ((i < (max_routes - (max_routes / 4)))) prefix_size = 23;
            else prefix_size = 22;
      break;
     case 4:
        prefix1 = rand();
        prefix_size = rand();
        prefix_size %= 25;
      break;
     case 5:
        prefix1 = rand();
        prefix_size = rand();
        prefix_size %= 33;
      break;
     default:
      printf ("Unknown Test Type: %d\n", test_type);
      exit (-1);
    }

    route_entry.next_hop = i;
    net_prefix = htonl(prefix1);
    rc = lpmhtRouteAdd(test_tree, (unsigned char *) &net_prefix, 
                    prefix_size, route_entry.next_hop); 
    if (rc)
    {
      if (rc == -2)
      {
        /* Ignore duplicate error, and try a different route.
        */
        i--;
        dup_count++;
        continue;
      } else
      {
        printf ("Error:%d while inserting route %d, prefix_size:%d\n", 
                      rc, i, prefix_size);
        TEST_EXIT(rc,0);
      }
    }
    memcpy (ipv4_list[i].prefix , &net_prefix, sizeof (net_prefix));
    ipv4_list[i].len = prefix_size;
  }

  rc = lpmhtRouteTableDestroy(test_tree);
  TEST_EXIT(rc,0);

  printf ("Created %'u IPv4 Network Prefixes. Detected %'d duplicates.\n", 
                                max_routes, dup_count);
}

/*************************************************************
** Generate IPv6 prefixes. 
** This function fills out the ipv6_list array. 
** The addresses in the ipv6 list can subsequently be used
** for inserting entries into the route tables and 
** performing lookups. 
**
** The function temporarily creates a Hash route table and inserts 
** the newly created addresses into that route table. This is done
** to eliminate duplicate prefixes. Note that during random
** prefix generation duplicates are likely to happen, 
** especially when the prefix length is smaller than /16.
**
*************************************************************/
static void ipv6_list_generate(prefixMode_e test_type, 
                                unsigned int max_routes)
{
  lpmhtRouteTable_t *test_tree;
  int rc;
  unsigned char net_prefix[16];
  routeEntry_t route_entry;
  unsigned int prefix_size;
  unsigned int i;
  unsigned int dup_count = 0;
  lpmhtTableProp_t prop;

  if (0 != ipv6_list)
                free (ipv6_list);

  ipv6_list = (ipv6List_t *) malloc (sizeof(ipv6List_t) * max_routes);
  assert(ipv6_list);

  printf ("\nGenerating %'u IPv6 Prefixes...\n", max_routes);

  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  test_tree = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                                        LPMHT_IPV6,
                                        LPMHT_HASH,
                                        &prop);
  assert(test_tree);

  switch (test_type)
  {
    case 1:
        printf ("1 - IPv6 Random Addresses, Fixed Prefix Length /128.\n");
      break;
    case 2:
        printf ("2 - IPv6 Random Addresses, Two Prefixes /48 and /64.\n");
      break;
    case 3:
        printf ("3 - IPv6 Random Addresses, Four Prefixes /48, /64, /80, /100.\n");
      break;
    case 4:
        printf ("4 - IPv6 Random Addresses, Random Prefixes /0 to /48\n");
      break;
    case 5:
        printf ("5 - IPv6 Random Addresses, Random Prefixes /0 to /128\n");
      break;
    default:
      printf ("Unknown Test Type: %d\n", test_type);
      exit (-1);
  }
  srand(1);
  /* Add random routes with prefix length of 128.
  */
  for (i = 0; i < max_routes; i++)
  {
    *(unsigned int *) &net_prefix[0] = rand();
    *(unsigned int *) &net_prefix[4] = rand();
    *(unsigned int *) &net_prefix[8] = rand();
    *(unsigned int *) &net_prefix[12] = rand();
    switch (test_type)
    {
     case 1:
        prefix_size = 128;
      break;
     case 2:
        prefix_size = (i < (max_routes / 2))?48:64;
      break;
     case 3:
        if (i < (max_routes / 4)) prefix_size = 48;
          else if (i < (max_routes / 2)) prefix_size = 64;
           else if ((i < (max_routes - (max_routes / 4)))) prefix_size = 80;
            else prefix_size = 100;
      break;
     case 4:
        prefix_size = rand();
        prefix_size %= 49;
      break;
     case 5:
        prefix_size = rand();
        prefix_size %= 129;
      break;
     default:
      printf ("Unknown Test Type: %d\n", test_type);
      exit (-1);
    }

    route_entry.next_hop = i;
    rc = lpmhtRouteAdd(test_tree, (unsigned char *) net_prefix, 
                    prefix_size, route_entry.next_hop); 
    if (rc)
    {
      if (rc == -2)
      {
        /* Ignore duplicate error, and try a different route.
        */
        i--;
        dup_count++;
        continue;
      } else
      {
        printf ("Error:%d while inserting route %d, prefix_size:%d\n", 
                      rc, i, prefix_size);
        TEST_EXIT(rc,0);
      }
    }
    memcpy (ipv6_list[i].prefix , net_prefix, sizeof (net_prefix));
    ipv6_list[i].len = prefix_size;
  }

  rc = lpmhtRouteTableDestroy(test_tree);
  TEST_EXIT(rc,0);

  printf ("Created %'u IPv6 Network Prefixes. Detected %'u duplicates.\n", 
                                                max_routes, dup_count);
}


/*************************************************************
** IPv4 Route Lookup 
**
*************************************************************/
static void *route_lookup(void *parm)
{
  lookupTaskParm_t *task_parm = (lookupTaskParm_t *) parm;
  lpmhtRouteTable_t *route_table = task_parm->route_table;
  unsigned int task_num = task_parm->task_num;
  int rc;
  routeEntry_t route_entry;
  unsigned long long i;
  unsigned int j = 0;
  unsigned int prefix_size;

  /* Perform route lookups.
  */
  for (i = 0; i < task_parm->num_lookups; i++)
  {
    rc = lpmhtLPMatch(route_table,  ipv4_list[j].prefix, 
                    &prefix_size, &route_entry.next_hop); 
    TEST_EXIT(rc,0); /* All route lookups must be successful. */

    if (0 == (i % 10000000LLU))
                printf ("Task Num:%u - IPv4 Route Lookup:%'llu\n", task_num, i);

    /* Go back to the first route.
    */
    j++;
    if (j == task_parm->num_routes)
    {
        j = 0;
    }
  }

  return 0;
}

/****************************************************************
** Populate the IPv4 routing table.
****************************************************************/
void ipv4TableLoad(lpmhtRouteTable_t **test_tree, 
                        lpmhtTableMode_e table_mode,
                        lpmhtTableProp_t *prop,
                        unsigned int max_routes)
{
  int rc;
  routeEntry_t route_entry;
  unsigned int i;
  unsigned int start_time, end_time;
  unsigned long long average_modify_time = 0;
  unsigned long long max_modify_time = 0;
  unsigned long long start_modify_time, delta_modify_time;
  lpmhtTableInfo_t info;

  printf ("\n\nCreating IPv4 Route Table with %'u routes...\n\n", max_routes);

  start_time = sysUpTimeMillisecondsGet();
  /* Create private tree with hit count monitoring.
  */
  *test_tree = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                                        LPMHT_IPV4,
                                        table_mode,
                                        prop);
  assert(*test_tree);

  /* Add test prefixes.
  */
  for (i = 0; i < max_routes; i++)
  {
    start_modify_time = timeNanoGet();
    route_entry.next_hop = i;
    rc = lpmhtRouteAdd(*test_tree, ipv4_list[i].prefix, 
                    ipv4_list[i].len, route_entry.next_hop); 
    if (rc)
    {
      printf ("Error:%d while inserting route %'u, prefix_size:%d\n", 
                      rc, i, ipv4_list[i].len);
      TEST_EXIT(rc,0);
    }
    delta_modify_time = timeNanoGet() - start_modify_time;
    average_modify_time += delta_modify_time;
    if (delta_modify_time > max_modify_time)
                        max_modify_time = delta_modify_time;
  }
  average_modify_time /= max_routes;
  end_time = sysUpTimeMillisecondsGet();

  rc =  lpmhtRouteTableInfoGet (*test_tree, &info);
  TEST_EXIT(rc,0);

  printf ("Created IPv4 Route Table in %'ums. Longest Insert Time:%'lluns Average Insert Time:%'lluns\n", 
                  end_time - start_time,
                  max_modify_time, average_modify_time);
  printf ("Total Physical Mem:%'zuKB (Virtual Mem:%'zuKB) routes:%'u/%'u nodes:%'u\n\n", 
                  info.mem_size / (1024), info.virtual_mem_size / 1024,
                  info.num_routes, max_routes, info.num_nodes);

}

/*************************************************************
** IPv6 Route Lookup 
**
*************************************************************/
static void *ipv6_route_lookup(void *parm)
{
  lookupTaskParm_t *task_parm = (lookupTaskParm_t *) parm;
  lpmhtRouteTable_t *route_table = task_parm->route_table;
  unsigned int task_num = task_parm->task_num;
  int rc;
  routeEntry_t route_entry;
  unsigned long long i;
  unsigned int j = 0;
  unsigned int prefix_size;



  /* Perform route lookups.
  */
  for (i = 0; i < task_parm->num_lookups; i++)
  {
    rc = lpmhtLPMatch(route_table,  ipv6_list[j++].prefix, 
                    &prefix_size, &route_entry.next_hop); 
    TEST_EXIT(rc,0); /* All route lookups must be successful. */

    if (0 == (i % 10000000LLU))
                printf ("Task Num:%u - IPv6 Route Lookup:%'llu\n", task_num, i);
    /* Go back to the first route in the pseudo random sequence.
    ** If we don't reset the random number generator then we will
    ** start seeing routes that are not in the route table.
    */
    if (j == task_parm->num_routes)
                        j = 0;
  }

  return 0;

}
/****************************************************************
** Populate the IPv6 routing table.
****************************************************************/
void ipv6TableLoad(lpmhtRouteTable_t **test_tree, 
                        lpmhtTableMode_e table_mode,
                        lpmhtTableProp_t *prop,
                        unsigned int max_routes)
{
  int rc;
  routeEntry_t route_entry;
  unsigned int prefix_size = 0;
  unsigned int i;
  unsigned int start_time, end_time;
  unsigned long long average_modify_time = 0;
  unsigned long long max_modify_time = 0;
  unsigned long long start_modify_time, delta_modify_time;
  lpmhtTableInfo_t info;

  printf ("\n\nCreating IPv6 Route Table with %'u routes...\n\n", max_routes);

  start_time = sysUpTimeMillisecondsGet();
  /* Create private tree with hit count monitoring.
  */
  *test_tree = lpmhtRouteTableCreate(max_routes,  /* Max Routes */
                                        LPMHT_IPV6,
                                        table_mode,
                                        prop);
  assert(*test_tree);

  /* Add routes from ipv6_list.
  */
  for (i = 0; i < max_routes; i++)
  {
    start_modify_time = timeNanoGet();
    route_entry.next_hop = i;
    rc = lpmhtRouteAdd(*test_tree,  ipv6_list[i].prefix, 
                    ipv6_list[i].len, route_entry.next_hop); 
    if (rc)
    {
      printf ("Error:%d while inserting route %'d, prefix_size:%d\n", 
                      rc, i, prefix_size);
      TEST_EXIT(rc,0);
    }
    delta_modify_time = timeNanoGet() - start_modify_time;
    average_modify_time += delta_modify_time;
    if (delta_modify_time > max_modify_time)
                        max_modify_time = delta_modify_time;
  }
  average_modify_time /= max_routes;

  end_time = sysUpTimeMillisecondsGet();
  rc =  lpmhtRouteTableInfoGet (*test_tree, &info);
  TEST_EXIT(rc,0);

  printf ("Created IPv6 Table in %'ums. Longest Insert Time:%'lluns Average Insert Time:%'lluns\n", 
                  end_time - start_time,
                  max_modify_time, average_modify_time);
  printf ("Total Physical Mem:%'zuKB (Virtual Mem:%'zuKB) routes:%'u/%'u nodes:%'u\n\n", 
                  info.mem_size / (1024), info.virtual_mem_size / 1024, 
                  info.num_routes, max_routes, info.num_nodes);


}

/*********************
** Trie Lookup Tests
*********************/
void lpmht_test(lpmhtTableMode_e table_mode, 
                lpmhtIpMode_e ip_mode,
                lpmhtTableProp_t *prop,
                prefixMode_e prefix_mode,
                unsigned int num_routes,
                unsigned long long num_lookups,
                unsigned int num_tasks)
{
  lpmhtRouteTable_t *test_tree;
  unsigned int start_time, end_time;
  int rc;
  pthread_t task_list[num_tasks];
  lookupTaskParm_t task_parm[num_tasks];
  unsigned int i;
  char ip_mode_str[8];
  char table_mode_str[8];
  lpmhtTableInfo_t info;
  unsigned long long total_lookups;

  if ((ip_mode > LPMHT_IPV6) || (table_mode > LPMHT_HASH))
  {
    printf ("Error, invalid IP or route table mode.\n");
    exit(0);
  }

  if (ip_mode == LPMHT_IPV4)
               strcpy (ip_mode_str, "IPv4");
           else
               strcpy (ip_mode_str, "IPv6");

  if (table_mode == LPMHT_TRIE)
               strcpy (table_mode_str, "TRIE");
           else
               strcpy (table_mode_str, "HASH");

  printf ("\n\n*********** LPMHT Test - %s/%s\n", table_mode_str, ip_mode_str);
  printf ("  hit_count:%u next_get:%u mem_prealloc:%u ",
                prop->hit_count, prop->next_get, prop->mem_prealloc);
  if (table_mode == LPMHT_HASH)
  {
    printf ("hash_prealloc:%u ", prop->hash_prealloc);
    if (ip_mode == LPMHT_IPV4)
    {
      printf ("ipv4_rules:%u ", prop->ipv4_rules);
    } else
    {
      printf ("ipv6_flow:%u ", prop->ipv6_flow);
      if (prop->ipv6_flow)
      {
        printf ("ipv6_max_flows:%'u ipv6_flow_age_time:%u ", 
                        prop->ipv6_max_flows, prop->ipv6_flow_age_time);
      }
    }
  }
  printf ("\n");


  if (ip_mode == LPMHT_IPV4)
  {
    /* This function generates a list of IPv4 prefixes that are subsequently
    ** used for route table inserts and lookups.
    */
    ipv4_list_generate(prefix_mode, num_routes);

    /* Add routes to the IPv4 routing table.
    */
    ipv4TableLoad(&test_tree, table_mode, prop, num_routes);

    if (prop && (prop->ipv4_rules))
    {
      printf ("Waiting for the IPv4 rule table to be generated...\n");
      do
      {
        sleep (1);
        (void)  lpmhtRouteTableInfoGet (test_tree, &info);
      } while (0 == info.ipv4_rule_table_ready);
    }

    printf ("\nStarting %'u IPv4 lookup tasks with %'llu iterations per task...\n", 
                                num_tasks, num_lookups);
    start_time = sysUpTimeMillisecondsGet();

    for (i = 0; i < num_tasks; i++)
    {
      task_parm[i].route_table = test_tree;
      task_parm[i].task_num = i+1;
      task_parm[i].num_lookups = num_lookups;
      task_parm[i].num_routes = num_routes;
      if (0 > pthread_create (&task_list[i], 0, route_lookup, &task_parm[i]))
                  (assert(0), abort());
    }
  
    /* Wait until all threads exit.
    */
    for (i = 0; i < num_tasks; i++)
                  pthread_join(task_list[i], 0);

    end_time = sysUpTimeMillisecondsGet();

    total_lookups = num_lookups * num_tasks;
    printf (">>> Finished %'llu route lookups with %u tasks in %'ums\n", 
                    total_lookups, num_tasks, end_time - start_time);

    printf ("    Route Lookups Per Second: %'llu\n",
                (total_lookups / (end_time - start_time)) * 1000);


    rc = lpmhtRouteTableDestroy(test_tree);
    TEST_EXIT(rc,0); /* All route lookups must be successful. */
  } else
  {
    /* This function generates a list of IPv6 prefixes that are subsequently
    ** used for route table inserts and lookups.
    */
    ipv6_list_generate(prefix_mode, num_routes);

    /* Add routes to the IPv6 routing table.
    */
    ipv6TableLoad(&test_tree, table_mode, prop, num_routes);

    printf ("\nStarting %'u IPv6 lookup tasks with %'llu iterations per task...\n", 
                                        num_tasks, num_lookups);
    start_time = sysUpTimeMillisecondsGet();

    for (i = 0; i < num_tasks; i++)
    {
      task_parm[i].route_table = test_tree;
      task_parm[i].task_num = i+1;
      task_parm[i].num_lookups = num_lookups;
      task_parm[i].num_routes = num_routes;
      if (0 > pthread_create (&task_list[i], 0, ipv6_route_lookup, &task_parm[i]))
                (assert(0), abort());
    }

    /* Wait until all threads exit.
    */
    for (i = 0; i < num_tasks; i++)
                pthread_join(task_list[i], 0);

    end_time = sysUpTimeMillisecondsGet();

    total_lookups = num_lookups * num_tasks;
    printf (">>> Finished %'llu route lookups with %u tasks in %'ums\n", 
                  total_lookups, num_tasks, end_time - start_time);

    printf ("    Route Lookups Per Second: %'llu\n",
                (total_lookups / (end_time - start_time)) * 1000);

    if ((table_mode == LPMHT_HASH) && (prop->ipv6_flow))
    {
      (void)  lpmhtRouteTableInfoGet (test_tree, &info);
      printf ("    Total IPv6 Flow Misses:%'llu (%u%%)\n",
                        info.ipv6_flow_not_found,
                        (unsigned int)((info.ipv6_flow_not_found * 100LLU)/total_lookups));
    }

    rc = lpmhtRouteTableDestroy(test_tree);
    TEST_EXIT(rc,0); /* All route lookups must be successful. */
  }
}

/*********************************
** IPv4 Trie Tests
*********************************/
void ipv4_trie_tests(void)
{
  lpmhtTableProp_t prop;

#if 1
  /* PMODE_5 uses random prefix length from 0 to 32.
  ** Repeat the test with 1 task and two tasks.
  */
  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV4, &prop, /* Route table mode, IP version, properties */ 
                PMODE_5,  /* Route Prefix Test Mode */
                2000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV4, &prop, /* Route table mode, IP version, properties */ 
                PMODE_5,  /* Route Prefix Test Mode */
                2000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif

#if 1
  /* PMODE_1 uses fixed 32 bit prefix length. 
  ** Repeat the test with 1 task and two tasks.
  */
  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV4, &prop, /* Route table mode, IP version, properties */ 
                PMODE_1,  /* Route Prefix Test Mode */
                2000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV4, &prop, /* Route table mode, IP version, properties */ 
                PMODE_1,  /* Route Prefix Test Mode */
                2000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif

#if 1
  /* Run the PMODE_1 test with tiny route table. All routes should be in data cache,
  ** so we should see fast performance.
  ** Repeat the test with 1 task and two tasks.
  */
  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV4, &prop, /* Route table mode, IP version, properties */ 
                PMODE_1,  /* Route Prefix Test Mode */
                100,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV4, &prop, /* Route table mode, IP version, properties */ 
                PMODE_1,  /* Route Prefix Test Mode */
                100,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif
}

/*********************************
** IPv6 Trie Tests
*********************************/
void ipv6_trie_tests(void)
{
  lpmhtTableProp_t prop;

#if 1
  /* PMODE_5 uses random prefix length from 0 to 128.
  ** Repeat the test with 1 task and two tasks.
  */
  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV6, &prop, 
                PMODE_5,  /* Route Prefix Test Mode */
                2000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV6, &prop, 
                PMODE_5,  /* Route Prefix Test Mode */
                2000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif

#if 1
  /* PMODE_1 uses fixed 128 bit prefix length.
  ** Repeat the test with 1 task and two tasks.
  */
  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV6, &prop, 
                PMODE_1,  /* Route Prefix Test Mode */
                2000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV6, &prop, 
                PMODE_1,  /* Route Prefix Test Mode */
                2000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif

#if 1
  /* Run the PMODE_1 test with tiny route table. All routes should be in data cache,
  ** so we should see fast performance.
  ** Repeat the test with 1 task and two tasks.
  */
  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV6, &prop, 
                PMODE_1,  /* Route Prefix Test Mode */
                100,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  lpmht_test (LPMHT_TRIE, LPMHT_IPV6, &prop, 
                PMODE_1,  /* Route Prefix Test Mode */
                100,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif
}

/*********************************
** IPv4 Hash Tests
*********************************/
void ipv4_hash_tests(void)
{
  lpmhtTableProp_t prop;

#if 1
  /* PMODE_5 uses random prefix length from 0 to 32.
  ** Repeat the test with 1 task and two tasks.
  */
  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV4, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV4, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif

#if 1
  /* PMODE_5 uses random prefix length from 0 to 32.
  ** Run the test with IP4 rule table. We should see some performance 
  ** improvement, but since most routes match long prefixes, 
  ** the improvement is modest.
  ** Disable hash_prealloc on this test. We should observe longer 
  ** route table insertions.
  */
  memset (&prop, 0, sizeof(prop));
  prop.ipv4_rules = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV4, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  /* Repeat the test with a tiny route table. 
  ** We should see fast route lookups because routes are in the data cache.
  */
  memset (&prop, 0, sizeof(prop));
  prop.ipv4_rules = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV4, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                100,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */
#endif

#if 1
  /* PMODE_4 uses random prefix length from 0 to 24.
  ** This is an ideal environment for IPv4 rule table. 
  ** Repeat the test with 1 task and two tasks.
  ** Run the tests with and without the rule table.
  */
  memset (&prop, 0, sizeof(prop));
  prop.ipv4_rules = 0;
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV4, &prop,
                PMODE_4,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.ipv4_rules = 1;
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV4, &prop,
                PMODE_4,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.ipv4_rules = 0;
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV4, &prop,
                PMODE_4,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.ipv4_rules = 1;
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV4, &prop,
                PMODE_4,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif
}

/*********************************
** IPv6 Hash Tests
*********************************/
void ipv6_hash_tests(void)
{
  lpmhtTableProp_t prop;

#if 1
  /* PMODE_5 uses random prefix length from 0 to 128.
  ** Repeat the test with 1 task and two tasks.
  ** This test is very unfavorable to the hash table implementation 
  ** because there are 128 different prefix lengths routes in the route
  ** table, which may require up to 128 hash table lookups.
  */
  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif

#if 1
  /* PMODE_5 uses random prefix length from 0 to 128.
  ** Repeat the test with 1 task and two tasks.
  ** This test is similar to the previous test, except flow 
  ** table is enabled. The number of flows is four times 
  ** the number of different IP addresses that the route is 
  ** expecting to see. 
  ** Note that the number of iterations in this test is 
  ** 100,000,000 per task instead of 20,000,000. This is needed
  ** to get a better performance estimate because the first 
  ** 10,000,000 route lookups always miss the flow table.
  */
  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  prop.ipv6_flow = 1;
  prop.ipv6_max_flows = 40000000;
  prop.ipv6_flow_age_time = 30; 
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                100000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  prop.ipv6_flow = 1;
  prop.ipv6_max_flows = 40000000;
  prop.ipv6_flow_age_time = 30; 
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                100000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif

#if 1
  /* PMODE_4 uses random prefix length from 0 to 48.
  ** Repeat the test with 1 task and two tasks.
  ** This test is more realistic for a core router.
  */
  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_4,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_4,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif

#if 1
  /* PMODE_3 uses routes with four different prefix lengths.
  ** Repeat the test with 1 task and two tasks.
  ** This test is more realistic for an enterprise router. When using IPv6, customers
  ** don't really need a lot subnets with different prefix lengths. In fact
  ** many customers might just use a single subnet size, such as /96, for every subnet.
  */
  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_3,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_3,  /* Route Prefix Test Mode */
                10000000,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif
#if 1
  /* PMODE_5 uses random prefix length from 0 to 128.
  ** Use a small 128 route table for this test. The route lookups 
  ** should be fast because all routes are in the data cache.
  ** Run the same test with and without the flow table.
  */
  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                128,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */
  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                128,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */

  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  prop.ipv6_flow = 1;
  prop.ipv6_max_flows = 1000;
  prop.ipv6_flow_age_time = 30; 
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                128,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                1);    /* Number of route lookup tasks */
  memset (&prop, 0, sizeof(prop));
  prop.hash_prealloc = 1;
  prop.ipv6_flow = 1;
  prop.ipv6_max_flows = 1000;
  prop.ipv6_flow_age_time = 30; 
  lpmht_test (LPMHT_HASH, LPMHT_IPV6, &prop,
                PMODE_5,  /* Route Prefix Test Mode */
                128,  /* Number of routes in the route table */
                20000000LLU,  /* Number of route lookups per task */
                2);    /* Number of route lookup tasks */
#endif
}


/*********************************
**********************************
** Start of the test program.
**********************************
*********************************/
int main ()
{
  /* Enable printf() to format integers using comma separators. For example
  ** printf ("%'u", val) prints 100,000,000 instead of 100000000.
  ** Note the ' between % and u. This triggers printf() to use
  ** the country code formatting conventions.
  */
  setenv ("LC_ALL","en_US.UTF-8",1);
  setlocale (LC_NUMERIC, "");

  ipv4_trie_tests ();

  ipv6_trie_tests ();

  ipv4_hash_tests ();

  ipv6_hash_tests ();


  /* Test Cleanup.
  */
  if (ipv4_list)
                free (ipv4_list);

  if (ipv6_list)
                free (ipv6_list);

}
