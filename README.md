
Concurrency primitives and concurrent algorithms/data structures
==========

  

This repo contains some of the tasks involving concurrency I made during the Advanced C++ course at Higher School of Economics.

  

## List of stuff presented here:

- hash-table - separate chaining hash table that enables to work with a hash table concurrently more effective than a simple mutex + std::unordered_map

- buffered-channel - buffered channel from Go language

- rw-lock - syncronization primitive that enables "reading" without locking, implemented using condition variables

- semaphore - implementation of semaphore using condition variables

- timerqueue - a priority queue for objects scheduled to perform actions at clock times

- unbuffered-channel - unbuffered channel from Go language

- fast-queue - Multiple-reader multiple-writer lock-free bounded queue

- futex - Mutex using Linux' "futex" and specific syscalls, implemented using lock-free paradigm

- hazard-ptr - Hazard pointer primitive for working with dynamically allocated objects in lock-free algorithms

- mpsc-stack - Multiple-reader single-writer lock-free stack

- rw-spinlock - Like rw-lock, but is lock-free
