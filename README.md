udpr
====

A simple UDP Relay program that I had written to solve some problem at Work.
I thought someone might find this useful.

The program can be compiled and linked with pthreads using

gcc *.c -lpthreads (Make sure you have pthread_barrier_t support)

and run as 

./a.out -p 2344 -i 192.168.1.103 -i 192.168.2.101 -i 192.168.3.105

The program listens for the incoming UDP messages on port 2344 and relays them to
the IP addresses given with the -i flag on port 2344.

NOTE: Please ensure you have routing sorted if the IP addresses that you are relaying
to are in different subnets.

I would have written this in a relatively higher level languages like Python or Ruby.
But I like C plus I really wanted to get started on multi threaded programming in C.

Please make this insanely simply program more interesting!
