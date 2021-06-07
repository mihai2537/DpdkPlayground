Created this new branch in order to debug Strategy_2_Burst on the EC2 instance.

Packets are received by EC2 server from another server but when using microVM with DPDK, it is not working.

--------------------------------------------------------------------------------------------------

Going to try to add 2 more lcores which will do the transmit and receive logic.
One lcore will contain the main app.

----------------------------------------------------------------------------------------------------


Making DPDK as a loopback interface.

Packets received from Secondary will be sent back to secondary!
------------------------------------------------------------------