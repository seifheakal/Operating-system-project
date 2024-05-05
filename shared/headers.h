#pragma once

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "pri_queue.h"
#include <errno.h>

#ifndef _STD
#define _STD ::std::
#endif

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300
#define MSGKEY 400
#define QUANTUM_TIME 2

///==============================
// don't mess with this variable//
int* shmaddr; //
//===============================

int getClk()
{
	return *shmaddr;
}

/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
 */
void initClk()
{
	int shmid = shmget(SHKEY, 4, 0444);
	while ((int)shmid == -1)
	{
		// Make sure that the clock exists
		printf("Wait! The clock not initialized yet!\n");
		sleep(1);
		shmid = shmget(SHKEY, 4, 0444);
	}
	shmaddr = (int*)shmat(shmid, (void*)0, 0);
}

/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
 */

void destroyClk(bool terminateAll)
{
	shmdt(shmaddr);
	if (terminateAll)
	{

		killpg(getpgrp(), SIGINT);
	}
}

// our stuff

// process data as read from file
typedef struct process_data {
	int id;
	int arrival_time;
	int running_time;
	int priority;
} process_data;

typedef struct process_message_buffer {
	long type;
	struct process_data data;
} process_message_buffer;

#define SCHEDULING_ALGO_HPF 0
#define SCHEDULING_ALGO_SRTN 1
#define SCHEDULING_ALGO_RR 2