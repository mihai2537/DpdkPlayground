/** Compilation: gcc -o memreader memreader.c -lrt -lpthread **/
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

caddr_t memptr;
sem_t *semReader;
sem_t *semWriter;
int fd;

void report_and_exit(const char *msg)
{
    perror(msg);
    exit(-1);
}

void clean_up()
{
    munmap(memptr, BYTE_SIZE);
    close(fd);
    sem_close(semReader);
    sem_close(semWriter);
    unlink(BACKING_FILE);
    sem_unlink(SEM_READER);
}

void wait_semaphore(sem_t *semptr)
{
    if (sem_wait(semptr) != 0)
    {
        //error
        clean_up();
        report_and_exit("Waiting at semaphore failed.\n");
    }
}

void init_stuff()
{
    // open the shared memory file
    fd = shm_open(BACKING_FILE, O_RDWR, ACCESSPERMS); /* empty to begin */
    if (fd < 0)
        report_and_exit("Can't get file descriptor...");

    /* get a pointer to memory */
    memptr = mmap(NULL,                   /* let system pick where to put segment */
                  BYTE_SIZE,              /* how many bytes */
                  PROT_READ | PROT_WRITE, /* access protections */
                  MAP_SHARED,             /* mapping visible to other processes */
                  fd,                     /* file descriptor */
                  0);                     /* offset: start at 1st byte */
    if ((caddr_t)-1 == memptr)
    {
        close(fd);
        unlink(BACKING_FILE);
        report_and_exit("Can't access segment...");
    }

    /* create a semaphore for reader */
    semReader = sem_open(SEM_READER,  /* name */
                         O_CREAT,     /* create the semaphore */
                         ACCESSPERMS, /* protection perms */
                         0);          /* initial value */
    if (semReader == (void *)-1)
    {
        close(fd);
        unlink(BACKING_FILE);
        munmap(memptr, BYTE_SIZE);
        report_and_exit("sem_open");
    }

    /* create a semaphore for reader */
    semWriter = sem_open(SEM_WRITER,  /* name */
                         O_CREAT,     /* create the semaphore */
                         ACCESSPERMS, /* protection perms */
                         0);          /* initial value */
    if (semWriter == (void *)-1)
    {
        close(fd);
        unlink(BACKING_FILE);
        munmap(memptr, BYTE_SIZE);
        sem_close(semReader);
        report_and_exit("sem_open");
    }
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

int main()
{

    init_stuff();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* use semaphore as a mutex (lock) by waiting for writer to increment it */
    wait_semaphore(semReader);
    int i;
    for (i = 0; i < strlen(MEM_CONTENTS); i++)
        write(STDOUT_FILENO, memptr + i, 1); /* one byte at a time */

    /* cleanup */
    clean_up();
    return 0;
}