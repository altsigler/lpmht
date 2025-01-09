# Longest Prefix Match Hash/Trie (LPMHT)
The lpmht application is a longest prefix match utility program that supports
IPv4 and IPv6 routing formats and scales to 10 million routes. 
The program is written in C and runs in the user space on POSIX-friendly 
operating systems. The application leverages the capabilities of modern 
64-bit CPUs and operating systems. The program has been tested on Linux/X86_64,
Linux/ARM64, and MacOS/ARM64. The program doesn't compile on 32 bit processors.

The program is also implemented in C++20. The C code is in the src-c directory 
and the C++ code is in the src-c++ directory.

Type "make" in the src-c directory to build the C code. The make file creates two 
executables: test-lpmht and test-perf. The test-lpmht focuses on code coverage
tests and the test-perf focuses on route lookup performance tests. The test 
platform should have at least 6GB of DRAM to run the test-perf application.

The lpmht is intended to be used as a control plane for
a hardware router or as a data plane for either a software router or 
a network appliance that requires knowledge of the routing table. 

The lpmht supports operations to create/destroy routing tables, add/delete
routes, perform longest prefix match route lookups while optionally keeping
track of hit count, perform get/set operations on the routes, and optionally 
maintain a sorted route list based on prefix length and prefix number accessible
via get-first/get-next functions. All operations except routing table 
creation and destruction are thread-safe. The thread protection mechanism 
uses read/write locks, which allow multiple route lookup threads to access
the route table concurrently, providing a significant performance boost on 
multi-core CPUs.

## Use Cases

The modern routers place multiple requirements on the route table 
application which impacts how memory is managed by the lpmht and how 
route lookups are performed in order to maximize throughput and 
minimize latency.

In terms of memory usage, some embedded applications prefer to allocate
all physical memory at boot time, so that there are no out of memory 
surprises when the maximum number of routes are learned. On the other hand
some applications work better when memory is allocated dynamically as 
the number of routes increases, and memory is freed as the routes are 
deleted from the route table. 

Some service provider routers require many route tables. These routers 
might have a large IPv4 and IPv6 route tables to connect to the public 
network and on the order of 100 small route tables to provide routing
services to individual customers. 

In terms of throughput, the hardware routers that only use the lpmht component
in the control plane don't require the fastest route lookup time, and 
instead prioritize memory utilization. On the other hand software routers
are very sensitive to how fast the route lookups are done.

In terms of latency, it is very important not to block route lookups
for any extended length of time while the route table is being updated. 
In fact, any interruptions of greater than 10ms could be catastrophic to the 
network stability, since they can cause time sensitive protocols, such as BFD,
to time out. In some networks a BGP neighbor failure can cause tens of 
thousands of routes to be added or deleted, and when problems arise in 
the network these route changes can stream continually, until the network
problem is fixed. The route table management software must be able to tolerate
continuous route additions and deletions without significant impact on 
the route lookups for traffic targeted to the stable routes.

## Implementation

The lpmht applications offers two mechanisms to organize the route table. 
The route table can be created as a Binary Trie Tree or a Hash Table. The route
table type is specified during route table creation in the 
lpmhtRouteTableCreate() function.

The Binary Trie Tree is intended for small route tables, such as one thousand
routes. The Binary Trie Tree route table type has been tested to 2 million
routes, but consumes a lot of memory and the route lookups are slower than the 
hash table in most cases.

The Hash table is the route table type that scales to 10 million routes and is the 
table type to use for any software router that needs very high throughput.

The following sections describe how the lpmht application manages memory, 
throughput, and latency to address the above use cases.

### Memory Management

When the route table is created, the caller must specify the maximum number of routes
supported in the table. The lpmht application allocates enough virtual memory 
to hold the route table of specified size. The 64 bit machines have a very large virtual 
memory space, so allocating a large virtual memory block is generally not an issue.

Allocating large virtual memory can be problematic in certain virtual machine environments or
where artificial restrictions are in place on per-process virtual memory usage. The user must
make sure that the operating system is configured to allow large virtual memory allocations.

The physical memory is allocated as needed when routes are added to the route table. The physical
memory is allocated one operating system page at a time, which is generally 4K on Linux, and 
freed one OS page at a time when the routes are deleted. Freeing memory using OS pages 
completely avoids memory fragmentation issues. 

When the route table is configured in Trie mode there is no physical memory allocation when 
the route table is empty. The memory is allocated only when routes are added. This makes 
Trie route table type useful for small routing tables, where smallest memory foot print is needed.

When the route table is configured in Hash mode there is a 400KB physical memory 
allocation to hold the initial hash table. The hash table only holds indexes to route entries, so the 
hash route table memory usage increases as routes are added. Also the hash table size itself is 
increased when enough routes are added to the route table. When hash table size is increased, all the 
routes are re-hashed. 

The Hash route table is more memory efficient for large number of routes than a Trie tree, particularly
for IPv6. The hash table with 10 million IPv4 routes needs about 573MB. The hash table with 10 million 
IPv6 routes needs about 914MB. The virtual memory and physical memory allocations are the same for 
the fully populated hash routing tables. On the other hand an IPv6 Trie routing table with 10 million
routes needs about 8GB of physical memory and 20GB of virtual memory. The actual physical memory usage 
depends on what routes are inserted into the Trie route table. The 8GB observation is for random prefixes
with random prefix length from 0 to 128. To avoid the large virtual and physical memory allocations,
the lpmht application limits the maximum Trie route table size to 2 million routes.

Certain route table options increase the initial physical memory allocated when the route table 
is created. For instance, the "mem_prealloc" option tells the lpmht to allocate all physical memory 
for the route table when the route table is created. The memory requirements for the various route
table options are described in lpmht-api.h file in the lpmhtTableProp_t type definition.

Since the route table uses large contiguous memory blocks for storing routes and other internal 
data, the lpmht application leverages the Transparent Huge Pages (THP) feature to accelerate 
memory accesses by reducing TLB cache misses. The THP feature is only available on Linux, and is not 
enabled for other operating systems. This feature can increase performance from 10% to 20% depending
on the use case.

### Throughput

Throughput is a measure of how many route lookups per second the lpmht application is able to do. 
The lpmht application throughput varies greatly depending on the use case. On a 4GHZ x86 system the 
throughput can vary from about 500,000 route lookups per second to 22 million route lookups per second.

In general, the best throughput is achieved when the route table is very small, such as 100 routes, or 
the route table is big, but the router is performing route lookups for only a few destination addresses. 
Both of these are valid use cases for client devices and hardware routers. The throughput is very good in 
these scenarios because the route information is loaded into the data cache, so memory accesses are very fast.

Software routers that require large route tables and that perform route lookups for millions of different 
destination IP addresses should set up the route table using the hash mode. The hash route table also 
has some options that can accelerate route lookups at the cost of additional memory.

The hash route table works by masking the destination IP address with the longest prefix size, hashing the 
resulting address into the hash table, and checking whether there is a matching route for this IP address.
If the route is not found then the destination IP address is masked with the next longest prefix and the 
hash is performed again. 

For example if the IPv4 hash route table contains routes with /32, /24, /16 /0 prefix lengths then four 
IP address masking and hashing operations may be required to resolve the IP address destination. The hash 
route tables work faster when there are only a few different prefix length routes in the route table, and 
work slower when the IPv4 route table has 32 different prefix length and IPv6 route table has 128 different
prefix lengths. Luckily IPv6 route tables tend to have fewer different length prefixes than IPv4 route tables
because the address space is so big that it is not necessary to fine tune the subnet size. For instance the
network administrator might use /96 subnet for every subnet in the enterprise. 

The lpmht application leverages hardware CPU features to achieve very fast hash computation. The lpmht uses 
hardware CRC 32 instructions to compute the hash index. These instructions are supported on X86_64 and ARM64
processors. The lpmht also uses the int128 data type to accelerate IPv6 address masking operation. 

To improve performance for IPv4 networks the lpmht application supports an optional IPv4 rule table feature. 
This feature requires 64MB of memory. The feature creates a rule table for routes with prefix length 0 to 24.
The rule table is indexed directly using the first three bytes of the IPv4 address. When the IPv4 rule feature
is enabled the route lookup operation uses the hash table to match for routes with prefixes 32 to 25, then 
if the IP address doesn't match any of the longer route prefixes then lpmht performs a direct access into the 
rule table using the first three bytes of the IPv4 address. The IPv4 rule tables offer the best performance
improvement when the network only has IPv4 routes with 0 to 24 bit mask lengths. 

The IPv4 rule table has to be recomputed every time a new route with prefix length 0 to 24 is added or an 
existing route is deleted. The rule table re-computation is akin to performing 16 million route lookups, so can
take a few seconds. While the rule table is recomputed the IPv4 forwarding is done using the hash table.

To improve performance for IPv6 networks the lpmht application supports an optional IPv6 flow based cache. 
When the IPv6 flow mode is enabled the router remembers certain number of destination IPv6 addresses and to which 
route the address resolved. The default flow table size is 2 million entries, which consumes 64MB. The flow table
size and the flow age time can be configured during route table creation.

The IPv6 flow based caching works best when the number of flows is at least 4 times the expected number of destination
IPv6 addresses. The use of this feature is situational, so the feature should not be enabled without first making 
sure that it actually helps. Having the feature enabled in a network where the number of IPv6 addresses seen within
the flow age time greatly exceeds the flow database capacity makes the route lookups slower than if the feature is 
disabled.

### Latency

The latency is a measure of how long it takes to perform a route lookup. Normally route lookups are very fast. However,
when many routes are added or deleted the route lookups can be delayed because the route table is locked while it is 
being modified.  

Suspending route lookups for even a few milliseconds can cause problems with time sensitive protocols and result in 
network instability. The lpmht application is susceptible to long table locking when using the hash mode with a large 
route table. The hash tables are affected because as the route table grows, the hash table also needs to be resized, 
which requires re-hashing all the routes. For example with 1 million routes the route re-hash can take on the order of 
10 milliseconds, while with 10 million routes re-hashing can take 150ms to 200ms.

In order to avoid re-hashing delays the lpmht hash route table supports an optional feature to pre-allocate memory 
for the maximum size hash table. The hash table size is 20 times the number of routes, so 1 million routes require 
about 20MB of DRAM, while 10 million routes require about 200MB of DRAM.
