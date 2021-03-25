
How to build:

make reader
make writer
----------------------------------------------
How to use:

First start server by doing: sudo ./server -l 1
Second: sudo ./reader -l 1 --proc-type=secondary

Now send packets to the NIC used by server, you shall see that the reader is able
to receive them too.
-----------------------------------------------------
server.c

Here the DPDK App is polling the ETH RX buffer.
Received packets are sent to the reader app.
---------------------------------------------------------------
reader.c

Here the reader app is polling the receive ring buffer for upcoming
packets from the DPDK app.
------------------------------------------------------------
shmem.h

Just a few defines used in both server and reader.
