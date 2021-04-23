
How to build:

make server
For reader, check: https://github.com/mihaidogaru2537/FirecrackerPlayground/tree/dpdk_component
Reader is the DPDK Secondary from firecracker.

----------------------------------------------
How to use:

First start server by doing: sudo ./server -l 1
Reader is supposed to be the DPDK Secondary from Firecracker. Ignore reader program from here.

-----------------------------------------------------
server.c

This is the DPDK primary which is comunicating with DPDK Secondary from Firecracker.

Received mbufs from shared ring.
Sends those packets to port.

Receives mbufs from port.
Puts those packets on shared ring.


Secondary -> Shared Ring -> Primary -> Port (Fully implemented)

Port -> Primary -> Shared Ring -> Secondary (Developing)

(Only port -> primary implemented so far)

---------------------------------------------------------------
reader.c

Not used, the reader is secondary dpdk form Firecracker.

------------------------------------------------------------
shmem.h

Just a few defines used in both server and reader.
Used the same macros inside secondary from Firecracker.
