/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

//shared mem libs
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <signal.h>

#include <rte_gso.h>
#include "shmem.h"

#define RX_RING_SIZE  1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

struct rte_ring *send_ring, *recv_ring;
struct rte_mempool *message_pool;

const unsigned flags = 0;
const unsigned ring_size = 2048;
const unsigned pool_size = 1024;
const unsigned pool_cache = 32;
const unsigned priv_data_sz = 0;

void report_and_exit(const char *msg)
{
	perror(msg);
	exit(-1);
}

void clean_up()
{
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM)
	{
		printf("\n\nSignal %d received, preparing to exit...\n",
			   signum);
		clean_up();
		report_and_exit("Forcefully killed.\n");
		// force_quit = true;
	}
}

// end of shared mem funcs

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
	{
		printf("No valid port.\n");
		return -1;
	}

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0)
	{
		printf("Error during getting device (port %u) info: %s\n",
			   port, strerror(-retval));
		return retval;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) {
		printf("Am setat Checksum pt TX\n");
		port_conf.txmode.offloads |= DEV_TX_OFFLOAD_TCP_CKSUM;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_TSO) {
		printf("Am setat TSO pt TX\n");
		port_conf.txmode.offloads |= DEV_TX_OFFLOAD_TCP_TSO;
	} else {
		printf("NU am setat TSO pt TX\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) {
		printf("Am setat Checksum pe IPV4\n");
		port_conf.txmode.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;
	}

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
	{
		printf("problem at: rte_eth_dev_configure\n");
		return retval;
	}

	printf("Max RX descriptors: %d\n", dev_info.rx_desc_lim.nb_max);
	printf("Max TX descriptors: %d\n", dev_info.tx_desc_lim.nb_max);

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
	{

		printf("Problem at: rte_eth_dev_adjust_nb_rx_tx_desc\n");
		return retval;
	}

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++)
	{
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
										rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
		{
			printf("%i %i %i %i\n", -EIO, -ENODEV, -EINVAL, -ENOMEM);
			printf("Problem at: rte_eth_rx_queue_setup, code: %d\n", retval);
			return retval;
		}
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;

	printf("Txconf for txqueue are: %ld\n", txconf.offloads);

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++)
	{
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
										rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
		{
			printf("Problem at: rte_eth_x_queue_setup\n");
			return retval;
		}
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
	{
		printf("Problem at: rte_eth_dev_start\n");
		return retval;
	}

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval != 0)
	{
		printf("Problem at: rte_eth_macaddr_get\n");
		return retval;
	}

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
		   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
		   port,
		   addr.addr_bytes[0], addr.addr_bytes[1],
		   addr.addr_bytes[2], addr.addr_bytes[3],
		   addr.addr_bytes[4], addr.addr_bytes[5]);


	/* Enable RX in promiscuous mode for the Ethernet device. */
	// THis is not working on EC2
	// retval = rte_eth_promiscuous_enable(port);
	// if (retval != 0)
	// {
	// 	printf("Problem at: rte_eth_promiscuous_enable\n");
	// 	printf("%s\n", rte_strerror(rte_errno));
	// 	printf("Error code: %d\n", retval);
	// 	// ENOTSUP
	// 	return retval;
	// }

	// added
	// retval = rte_eth_dev_set_mtu(port, (uint16_t)9000);
	// if (retval != 0) {
	// 	printf("Problem with setting MTU: %d\n", retval);
	// 	// Eroarea e -22 care inseamna EINVAL
	// 	return retval;
	// }

	return 0;
}

static void
print_buf_packet(struct rte_mbuf* mbuf)
{
	unsigned char* my_packet;
	uint16_t data_len;
	uint16_t i = 0;

	my_packet = ((unsigned char *)(*(struct rte_mbuf *)mbuf).buf_addr) + ((struct rte_mbuf *)mbuf)->data_off;
	data_len = ((struct rte_mbuf *)mbuf)->data_len;

	for (i = 0; i < data_len; i++) {
		printf("%02x ", (uint8_t)my_packet[i]);
	}
	printf("\n");
}

/*
	Receives from secondary and sends back to secondary.
*/
static int lcore_loopback(void* arg)
{
	printf("\nCore %u forwarding packets in loopback. [Ctrl+C to quit]\n",
		   rte_lcore_id());

	for (;;) {

		unsigned int count = 0;
		// Get data from port and send it to secondary
		struct rte_mbuf *received_bufs[BURST_SIZE];
		uint16_t nb_rx = rte_ring_dequeue_burst(recv_ring, (void **)received_bufs, BURST_SIZE, NULL);
		uint16_t total_recv = 0;

		if (unlikely(nb_rx == 0))
			continue;
		// printf("Received burst from port: %d\n", nb_rx);

		count = rte_ring_enqueue_burst(send_ring, (void * const *)received_bufs, nb_rx, NULL);
		// printf("Sent to secondary: %d\n", count);
		printf("Receive / Sent back: %d : %d\n", nb_rx, count);

		total_recv += count;

		// So after 10MB throughput on UDP this will start to loop.
		while (count < nb_rx) {
			nb_rx = nb_rx - count;
			count = rte_ring_enqueue_burst(send_ring, (void * const *)(received_bufs + total_recv), nb_rx, NULL);
			total_recv += count;
			// if (count < nb_rx) {
			// 	printf("Count: %d, nb_rx: %d, total_recv: %d, iter: %d\n", count, nb_rx, total_recv, iteration);
			// }
		}
	}

}

/*
	This function will receive from secondary and transmit to port.
*/
static int lcore_transmit_to_port(void * arg)
{
	uint16_t port;
	uint16_t i = 0;
	struct rte_mbuf *mbuf;

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	RTE_ETH_FOREACH_DEV(port)
	if (rte_eth_dev_socket_id(port) > 0 &&
		rte_eth_dev_socket_id(port) !=
			(int)rte_socket_id())
		printf("WARNING, port %u is on remote NUMA node to "
			   "polling thread.\n\tPerformance will "
			   "not be optimal.\n",
			   port);

	printf("\nCore %u forwarding packets to port. [Ctrl+C to quit]\n",
		   rte_lcore_id());

	port = rte_eth_find_next_owned_by(0, RTE_ETH_DEV_NO_OWNER);

	int count_failed_enq = 0;
	/* Run until the application is quit or killed. */
	for (;;)
	{	
		uint16_t i = 0;
		void *mbuf;
		struct rte_mbuf *bufs[BURST_SIZE];
		unsigned int count = rte_ring_dequeue_burst(recv_ring, (void **)bufs, BURST_SIZE, NULL);

		if (count != 0) {
			// printf("Count: %d\n", count);
			// for (i = 0 ; i < count; i++) {
			// 	bufs[i]->ol_flags = PKT_TX_TCP_CKSUM;
			// }
			// printf("Received burst from secondary: %d\n", count);

			const uint16_t nb_tx = rte_eth_tx_burst(port, 0, bufs, count);
			// printf("Sent to port: %d\n", nb_tx);
			if (unlikely(nb_tx < count)) {
				for (i = nb_tx; i < count; i++) {
					printf("Nu s-a trimis pachetul.\n");
					rte_pktmbuf_free(bufs[i]);
				}
			}
		}
	}
	return 0;
}

/*
	This function will receive from port and send to secondary.
*/
static int lcore_receive_from_port(void * arg)
{
	uint16_t port;
	uint16_t i = 0;
	struct rte_mbuf *mbuf;

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	RTE_ETH_FOREACH_DEV(port)
	if (rte_eth_dev_socket_id(port) > 0 &&
		rte_eth_dev_socket_id(port) !=
			(int)rte_socket_id())
		printf("WARNING, port %u is on remote NUMA node to "
			   "polling thread.\n\tPerformance will "
			   "not be optimal.\n",
			   port);

	printf("\nCore %u forwarding packets to firecracker. [Ctrl+C to quit]\n",
		   rte_lcore_id());

	port = rte_eth_find_next_owned_by(0, RTE_ETH_DEV_NO_OWNER);

	for (;;)
	{	
		unsigned int count = 0;
		// Get data from port and send it to secondary
		struct rte_mbuf *received_bufs[BURST_SIZE];
		uint16_t nb_rx = rte_eth_rx_burst(port, 0, received_bufs, BURST_SIZE);
		uint16_t total_recv = 0;

		if (unlikely(nb_rx == 0))
			continue;
		// printf("Received burst from port: %d\n", nb_rx);


		count = rte_ring_enqueue_burst(send_ring, (void * const *)received_bufs, nb_rx, NULL);
		// printf("Sent to secondary: %d\n", count);
		total_recv += count;

		// So after 10MB throughput on UDP this will start to loop.
		while (count < nb_rx) {
			nb_rx = nb_rx - count;
			count = rte_ring_enqueue_burst(send_ring, (void * const *)(received_bufs + total_recv), nb_rx, NULL);
			total_recv += count;
			// if (count < nb_rx) {
			// 	printf("Count: %d, nb_rx: %d, total_recv: %d, iter: %d\n", count, nb_rx, total_recv, iteration);
			// }
		}
	}
	return 0;
}


/*
 * The lcore main. This is the main thread that does the work.
 * Packet comes on RX buffer then it is sent to reader app.
 */

static void
lcore_main(void)
{
	unsigned lcore_id;
	int counter = 0;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_remote_launch(lcore_loopback, NULL, lcore_id);
	}

	rte_eal_mp_wait_lcore();
}

void init_stuff()
{
	send_ring = rte_ring_create(PRI_2_SEC, ring_size, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (send_ring == NULL)
	{
		report_and_exit("Send ring failed.\n");
	}

	recv_ring = rte_ring_create(SEC_2_PRI, ring_size, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (recv_ring == NULL)
	{
		report_and_exit("Recv ring failed.\n");
	}

	message_pool = rte_pktmbuf_pool_create(MSG_POOL,				  // name of the pool
										   NUM_MBUFS * 1,			  // number of elements
										   MBUF_CACHE_SIZE,			  // cache size
										   0,						  // priv size
										   65500, // size of data buffer including headroom
										   rte_socket_id());		  // socket where the memory should be allocated

	// message_pool = rte_pktmbuf_pool_create(MSG_POOL,				  // name of the pool
	// 									   NUM_MBUFS * 1,			  // number of elements
	// 									   MBUF_CACHE_SIZE,			  // cache size
	// 									   0,						  // priv size
	// 									   RTE_MBUF_DEFAULT_BUF_SIZE, // size of data buffer including headroom
	// 									   rte_socket_id());		  // socket where the memory should be allocated

	//why this DID NOT WORK?

	// message_pool = rte_mempool_create(MSG_POOL,					 //name of the pool
	// 								  NUM_MBUFS * 1,			 //number of elements
	// 								  RTE_MBUF_DEFAULT_BUF_SIZE, //size of each elem
	// 								  MBUF_CACHE_SIZE,			 // cache size
	// 								  0,						 //size of private data appended after the mempool struct
	// 								  NULL,						 // function pointer to init priv data
	// 								  NULL,						 // opaque pointer to data that can be used in the mempool constructor func
	// 								  NULL,						 // func pointer for each object init
	// 								  NULL,						 // opaque pointer to data that can be used as argument for each call to the object constructor function
	// 								  rte_socket_id(),			 // socket...
	// 								  flags);					 // some flags

	if (message_pool == NULL)
	{
		report_and_exit("Message pool ring failed.\n");
	}
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[])
{
	unsigned nb_ports;
	uint16_t portid;

	/* FIRST DO THIS */
	/* Initialize the Environment Abstraction Layer (EAL).*/
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	init_stuff();

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	argc -= ret;
	argv += ret;

	// I do not need ports for this loopback.

	// /* Check that there is at least a port available */
	// nb_ports = rte_eth_dev_count_avail();
	// if (nb_ports < 1)
	// 	rte_exit(EXIT_FAILURE, "Error: number of ports must be at least 1\n");

	// /* Initialize ONE port only. */
	// portid = rte_eth_find_next_owned_by(0, RTE_ETH_DEV_NO_OWNER);
	// if (port_init(portid, message_pool) != 0)
	// 	rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
	// 			 portid);

	if (rte_lcore_count() > 2)
		printf("\nWARNING: Too many lcores enabled. Only 2 used.\n");

	/* Call lcore_main on the main core only. */
	lcore_main();

	return 0;
}

// /* Send burst of TX packets, to second port of pair. */
// const uint16_t nb_tx = rte_eth_tx_burst(port, 0,
// 										bufs, nb_rx);

// /* Free any unsent packets. */
// if (unlikely(nb_tx < nb_rx))
// {
// 	uint16_t buf;
// 	for (buf = nb_tx; buf < nb_rx; buf++)
// 		rte_pktmbuf_free(bufs[buf]);
// }
