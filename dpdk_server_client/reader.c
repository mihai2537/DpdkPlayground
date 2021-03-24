/** Compilation: gcc -o memreader memreader.c -lrt -lpthread **/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>

#include <signal.h>
#include "shmem.h"

struct rte_ring *send_ring, *recv_ring;
struct rte_mempool *message_pool;

void report_and_exit(const char *msg)
{
    perror(msg);
    exit(-1);
}

static void
signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        printf("\n\nSignal %d received, preparing to exit...\n",
               signum);
        report_and_exit("Forcefully killed.\n");
        // force_quit = true;
    }
}

void main_logic()
{
    void *msg;

    for (;;)
    {
        // TO do
        if (rte_ring_dequeue(recv_ring, &msg) < 0)
        {
            printf("Nothing on the Queue so I'm going to sleep a lil'\n");
            usleep(5);
            continue;
        }
        // Means I got a packet from the queue.
        printf("Packet received, oh yeah.\n");
        rte_mempool_put(message_pool, msg);
    }
}

void init_stuff()
{
    recv_ring = rte_ring_lookup(PRI_2_SEC);
    if (recv_ring == ENOENT)
    {
        report_and_exit("Recv ring failed.\n");
    }

    send_ring = rte_ring_lookup(SEC_2_PRI);
    if (recv_ring == ENOENT)
    {
        report_and_exit("Recv ring failed.\n");
    }

    message_pool = rte_mempool_lookup(MSG_POOL);
    if (recv_ring == ENOENT)
    {
        report_and_exit("Recv ring failed.\n");
    }
}

int main()
{
    init_stuff();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* use semaphore as a mutex (lock) by waiting for writer to increment it */
    main_logic();

    return 0;
}