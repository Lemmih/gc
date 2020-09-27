<h3>Written: 2020-09-22</h3>

# Threads and blocking calls

I've written a little script to cement my understanding of GHC's rts w.r.t. threading. The script can be run
in a few different configuration and will report which OS threads (aka capabilities) are blocked.

For example, calling a blocking C function marked as "safe" should not block other threads:

```
$ ./blocking safe +RTS -N4
Cap 3 is NOT blocked.
Cap 2 is NOT blocked.
Cap 1 is NOT blocked.
Cap 0 is NOT blocked.
```

If we accidentally mark a blocking C function as "unsafe", though, this will block the calling thread but
not other threads:

```
$ ./blocking unsafe +RTS -N4
Cap 3 is NOT blocked.
Cap 2 is NOT blocked.
Cap 1 is NOT blocked.
Cap 0 is blocked.
```

(Note that `main` is not always on capability 0.)

GHC's uses a stop-the-world GC for nurseries and the minor generation. Accordingly, triggering a GC while
any thread is blocked should block all threads:

```
$ ./blocking unsafe gc +RTS -N4
Cap 3 is blocked.
Cap 2 is blocked.
Cap 1 is blocked.
Cap 0 is blocked.
```

# Local heaps

Having local heaps (ie nurseries that are collected independently of each other) would eliminate the cost of
synchronizing every garbage collect. Local heaps were implemented a long time ago by Simon Marlow and Simon
Peyton Jones but the results weren't good enough for mainline GHC. It might be time to test this approach
again since maintaining the right invariants is a lot easier with a fixed-size nursery than for an entire
minor generation.

# Appendix: blocking.hs

```haskell
{!src/blocking.hs!}
```
