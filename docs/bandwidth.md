<h3>Written: 2020-09-27</h3>

# Bandwidth assumptions

How much memory bandwidth do I have and how much of it can I use while garbage collecting?

Let's try the simplest test possible:
```c
for(uint64_t i=0; i < MEM_SIZE; i++) {
  to_area[i] = i;
}
```
```
Write naive: 6733 MiB/s
```

Hm, this is about half the advertised bandwidth of my memory. [Read For Ownership](https://en.wikipedia.org/wiki/MESI_protocol#Read_For_Ownership) says we have to read memory before we can write to it. Let's see if this is the cause of the above results.

```c
for(uint64_t i=0; i < MEM_SIZE; i++) {
  __builtin_nontemporal_store(i,to_area+i);
}
```
```
Write nontemporal: 14.6 GiB/s
```

Ah, much better. So my memory bandwidth is close to 15 GiB but writing to memory can unexpectedly mean reading from memory.

In a moving GC, we don't just write memory. We read memory, write it to a new location, and then overwrite the old data with a pointer to the new. Let's check our bandwidth intuition in this situation:

```c
for(uint64_t i=0; i < MEM_SIZE; i++) {
  to_area[i] = from_area[i];
  from_area[i] = i;
}
```
```
Copy naive: 3265 MiB/s
```

Alright, 3265 MiB/s is roughly 1/4 of total memory bandwidth. This makes sense since [RFO](https://en.wikipedia.org/wiki/MESI_protocol#Read_For_Ownership) means we're reading and writing both the source and target memory addresses. Four ops (two reads + two writes) per word means an effective copying speed 1/4 of total memory bandwidth.

So, if we disable RFO, we'll only have three ops (two reads + one write) per word and the effective speed should be 1/3 of total memory bandwidth. Let's see if that is true.

```c
for(uint64_t i=0; i < MEM_SIZE; i++) {
  __builtin_nontemporal_store(from_area[i],to_area+i);
  from_area[i] = i;
}
```
```
Copy nontempral: 4481 MiB/s
```

Yes, indeed, 4481 MiB/s is roughly 1/3 of 14.6 GiB/s. Great. So, does this mean we should expect garbage collectors to run at an effective rate of 4481 MiB/s? No, disabling RFO is rarely (but not never) useful for copying semi-space collectors. Does that mean we should expect 3265 MiB/s, then? No, garbage collectors typically spend most of their time moving small objects that do not appear linearly in memory.

Let's see how quickly we can copy objects that are spaced out randomly:

```c
for(uint64_t i=0; i < MEM_SIZE; i+=block_size) {
  uint64_t at = (wyrand(&seed) % (MEM_SIZE-block_size));
  for(uint64_t n=0;n<block_size;n++)
    to_area[i+n] = from_area[at+n];
  from_area[at] = i;
}
```
```
Copy random:  197 MiB/s (block size: 1)
```

Ouch, if we're copying tiny (1-word) objects, performance tanks and the effective rate is ~70 times lower than total memory bandwidth. Fortunately, objects tend to be a bit larger so let's look at performance as the block size increases:

```
Copy random 1:      197 MiB/s
Copy random 2:      328 MiB/s
Copy random 4:      612 MiB/s
Copy random 8:     1011 MiB/s
Copy random 16:    1569 MiB/s
Copy random 32:    1904 MiB/s
Copy random 64:    2506 MiB/s
Copy random 128:   3104 MiB/s
Copy random 256:   3575 MiB/s
Copy random 512:   3971 MiB/s
Copy random 1024:  4234 MiB/s
Copy random 2048:  4200 MiB/s
Copy random 4096:  4304 MiB/s
Copy random 8192:  4349 MiB/s
Copy random 16384: 4356 MiB/s
Copy random 32768: 4398 MiB/s
```

As the block size increases, the copy rate approaches 1/3 of total bandwidth. This is because the cost of writing the forwarding pointing trends towards zero. If we had used non-temporal stores then the copy rate would approach 1/2 of total bandwidth.

Now, block sizes between 2 and 4 are the most common and anything above 8 is so rare as to be inconsequential. All-in-all, if a (non-nursery) garbage collector copies at more than 500 MiB/s then it means it has a good cache layout.


# Appendix: mem_bench.c

```c
{!src/mem_bench.c!}
```
