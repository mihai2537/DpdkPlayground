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
#include "shmem.h"

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

struct rte_ring *send_ring, *recv_ring;
struct rte_mempool *message_pool;

const unsigned flags = 0;
const unsigned ring_size = 64;
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

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
	{
		printf("problem at: rte_eth_dev_configure\n");
		return retval;
	}

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
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
	{
		printf("Problem at: rte_eth_promiscuous_enable\n");
		return retval;
	}

	return 0;
}

/*
 * The lcore main. This is the main thread that does the work.
 * Packet comes on RX buffer then goes back on TX buffer.
 */
static __rte_noreturn void
lcore_main(void)
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

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
		   rte_lcore_id());

	port = rte_eth_find_next_owned_by(0, RTE_ETH_DEV_NO_OWNER);

	/* Run until the application is quit or killed. */
	for (;;)
	{
		/*
		 * Receive packets on a port, print it, then put it on the TX buffer.
		 */

		/* Get burst of RX packets, from first port of pair. */
		struct rte_mbuf *bufs[BURST_SIZE];
		const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
												bufs, BURST_SIZE);

		if (unlikely(nb_rx == 0))
			continue;

		printf("Number of packets received: %d\n", nb_rx);
		// Now do something with the received data.
		for (i = 0; i < nb_rx; i++)
		{
			mbuf = bufs[i];
			if (rte_ring_enqueue(send_ring, (void *)mbuf) == 0)
			{
				printf("Packet was put into queue.\n");
			}
			else
			{
				printf("Couldn't put packet into queue.\n");
			}
		}
	}
}

void init_stuff()
{
	send_ring = rte_ring_create(PRI_2_SEC, ring_size, rte_socket_id(), flags);
	if (send_ring == NULL)
	{
		report_and_exit("Send ring failed.\n");
	}

	recv_ring = rte_ring_create(SEC_2_PRI, ring_size, rte_socket_id(), flags);
	if (recv_ring == NULL)
	{
		report_and_exit("Recv ring failed.\n");
	}
	message_pool = rte_pktmbuf_pool_create(MSG_POOL, NUM_MBUFS * 1,
										   MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	// message_pool = rte_mempool_create(MSG_POOL, NUM_MBUFS,
	// 								  sizeof(struct rte_mbuf), MBUF_CACHE_SIZE, priv_data_sz,
	// 								  NULL, NULL, NULL, NULL,
	// 								  rte_socket_id(), flags);
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

	/* Check that there is at least a port available */
	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports < 1)
		rte_exit(EXIT_FAILURE, "Error: number of ports must be at least 1\n");

	/* Initialize ONE port only. */
	portid = rte_eth_find_next_owned_by(0, RTE_ETH_DEV_NO_OWNER);
	if (port_init(portid, message_pool) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
				 portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

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
