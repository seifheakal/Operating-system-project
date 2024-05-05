#include "headers.h"
#include "pri_queue.h"
#include "doubly_linked_list.h"
#include "pcb.h"

#include <math.h>

int initialize_message_queue();
int register_process_control_block(process_data* data, int algorithm, /*out*/ process_control_block** pcbEntry);
int fork_process(process_control_block* pcb);
process_control_block* process_table_find_pcb_from_system(int systemPid);

// schedule algos
void sched_hpf();
void sched_srtn();
void sched_rr(int);

// sig handlers
void process_termination_handler(int);
void process_running_time_handler(int);

void log_data(process_control_block*);
void log_perf();

key_t process_msgq_id;

// all processes reside here
doubly_linked_list process_table;

int terminated_processes_count;

pri_queue process_queue;
process_control_block* running_process;

int last_rr_change_time;

// doubly_linked_list rr_seq;

// output stuff

int main(int argc, char** argv) {
	int algorithm = atoi(argv[1]);
	int quantum = atoi(argv[2]); // rr quantum only
	int processesCount = atoi(argv[3]); // total num of processes

	// process termination handler
	signal(SIGUSR1, process_termination_handler);
	signal(SIGUSR2, process_running_time_handler);

	printf("[Scheduler] Starting with algo=%d, q=%d, procCount=%d\n", algorithm, quantum, processesCount);

	if (algorithm < 0 || algorithm > 2) {
		perror("Invalid algorithm");
		exit(EXIT_FAILURE);
	}

	if (algorithm == SCHEDULING_ALGO_RR && quantum < 1) {
		perror("Invalid RR quantum time");
		exit(EXIT_FAILURE);
	}

	void(*algorithmHandler)(int) = 0;

	switch (algorithm) {
	case SCHEDULING_ALGO_HPF:
		algorithmHandler = sched_hpf;
		break;

	case SCHEDULING_ALGO_SRTN:
		algorithmHandler = sched_srtn;
		break;

	case SCHEDULING_ALGO_RR:
		algorithmHandler = sched_rr;
		break;

	default:
		// how did we end up here :)?
		perror("This should never happen lol");
		exit(EXIT_FAILURE);
		break;
	}

	initClk();

	// delete old log files
	remove("scheduler.log");
	remove("scheduler.perf");

	// init queue & table
	doubly_linked_list_init(&process_table);
	pri_queue_init(&process_queue);

	// doubly_linked_list_init(&rr_seq);

	if (!initialize_message_queue()) {
		perror("Msg queue init failed");
		goto exit;
	}

	// initially 0
	terminated_processes_count = 0;

	// initially -1
	last_rr_change_time = -1;

	// when do we terminate?
	// terminatedProcessesCount = processesCount

	process_message_buffer msgBuffer;
	while (terminated_processes_count < processesCount) {
		// check for arrivals
		int canSkip = 0;

		do {
			if (msgrcv(process_msgq_id, &msgBuffer, sizeof(msgBuffer.data), 1, IPC_NOWAIT) == -1) {
				if (errno != ENOMSG) {
					// something went wrong
					perror("msgrcv failure");
					goto exit;
				}

				// we're fine
				canSkip = 1;
			}
			else {
				// we have a new process
				// enqueue process!

				printf("[Scheduler] %d - Received new proc, pid=%d, at=%d, rt=%d\n", getClk(), msgBuffer.data.id, msgBuffer.data.arrival_time, msgBuffer.data.running_time);

				process_control_block* pcb;
				if (!register_process_control_block(&msgBuffer.data, algorithm, &pcb)) {
					// failed
					perror("Cannot register pcb");
					goto exit;
				}

				// schedule algo continues the process

				if (!fork_process(pcb)) {
					perror("Cannot run process");
					goto exit;
				}
			}
		} while (canSkip == 0);

		if (algorithmHandler) {
			algorithmHandler(quantum);
		}
		else {
			printf("No handler set, so we're doing some work ;)");
		}

		usleep(100 * 1000); // polling
	}


exit:

	// output pt
	doubly_linked_list_node* n = process_table.head;
	while (n) {
		process_control_block* pcb = (process_control_block*)n->value;

		printf("PROCESS\tid=%d\tST=%d\tFT=%d\n", pcb->pid, pcb->stats.start, pcb->stats.finish);

		n = n->next;
	}

	/*n = rr_seq.head;
	while (n) {
		int* pcb = (int*)n->value;

		printf("RR SEQ %d\n", *pcb);

		n = n->next;
	}*/

	// log performance
	log_perf();

	printf("[Scheduler] Exiting...\n");

	// free table & queue
	doubly_linked_list_free(&process_table);
	pri_queue_free(&process_queue);

	destroyClk(false);

	return 0;
}

/// Initializes the Gen-Sched msg queue
int initialize_message_queue() {
	process_msgq_id = msgget(MSGKEY, 0666 | IPC_CREAT);
	if (process_msgq_id == -1) {
		perror("Error in create Message Queue");
		return 0;
	}

	return 1;
}

/// Registers a process in the process table as a PCB
int register_process_control_block(process_data* data, int algorithm, /*out*/ process_control_block** pcbEntry) {
	if (!data) {
		return 0;
	}

	process_control_block* pcb = malloc(sizeof(process_control_block));

	// initially ready
	pcb->state = PROCESS_STATE_RDY;

	pcb->pid = data->id;
	pcb->priority = data->priority;

	// initially rem time = running time
	pcb->remaining_time = data->running_time;
	pcb->running_time = data->running_time;

	pcb->arrival_time = data->arrival_time;

	// not started yet
	pcb->system.proc_pid = -1;

	// initial stats
	pcb->stats.start = -1;
	pcb->stats.finish = -1;

	pcb->stats.waiting_time = 0;
	pcb->stats.last_finish = -1;

	switch (algorithm) {
	case SCHEDULING_ALGO_HPF:
		doubly_linked_list_add(&process_table, pcb);
		pri_queue_enqueue(&process_queue, pcb->priority, pcb);
		break;

	case SCHEDULING_ALGO_SRTN:
		doubly_linked_list_add(&process_table, pcb);
		pri_queue_enqueue(&process_queue, pcb->remaining_time, pcb);
		break;

	case SCHEDULING_ALGO_RR:
		doubly_linked_list_add(&process_table, pcb);

		// insert at end of queue
		pri_queue_enqueue(&process_queue, 0, pcb);
		break;

	default:
		perror("Unknown algorithm");

		free(pcb);
		return 0;
	}

	if (pcbEntry) {
		*pcbEntry = pcb;
	}

	return 1;
}

/// Forks a new process from pcb
int fork_process(process_control_block* pcb) {
	if (!pcb || pcb->state != PROCESS_STATE_RDY || pcb->system.proc_pid != -1) {
		// dont fork process
		return 0;
	}

	// fork
	pid_t child = fork();
	if (child == -1) {
		perror("Failed to fork process");
		return 0;
	}
	else if (child == 0) {
		// alloc params
		char param[10];
		sprintf(param, "%d", pcb->remaining_time);

		execl("./process.out", "process.out", param, NULL);
	}

	// assign system pid
	pcb->system.proc_pid = child;

	return 1;
}

process_control_block* process_table_find_pcb_from_system(int systemPid) {
	process_control_block* pcb = 0;

	pcb_system_pid_iterator it;
	it.system_pid = systemPid;
	it.result = &pcb;

	doubly_linked_list_iterate(&process_table, process_table_find_pcb_from_system_iterator, (void*)&it);

	return pcb;
}

void run_process(process_control_block* pcb) {
	if (!pcb || pcb->state != PROCESS_STATE_RDY || pcb->system.proc_pid == -1) {
		// dont run process
		return;
	}

	running_process = pcb;

	printf("Setting pid=%d sysPid=%d running\n", pcb->pid, pcb->system.proc_pid);

	// change state
	pcb->state = PROCESS_STATE_RESUMED;

	// set start time
	if (pcb->stats.start == -1) {
		pcb->stats.start = getClk();

		pcb->state = PROCESS_STATE_STARTED;

		// were we waiting?
		pcb->stats.waiting_time += pcb->stats.start - pcb->arrival_time;
	}
	else {
		// update waiting
		pcb->stats.waiting_time += getClk() - pcb->stats.last_finish;
	}

	log_data(pcb);

	//int* xxz = malloc(4); *xxz = pcb->pid;
	//doubly_linked_list_add(&rr_seq, xxz);

	// sleep for 200ms
	usleep(200 * 1000);

	// send cont signal
	kill(pcb->system.proc_pid, SIGCONT);
}

void pause_process(process_control_block* pcb) {
	if (!pcb || (pcb->state != PROCESS_STATE_STARTED && pcb->state != PROCESS_STATE_RESUMED) || pcb->system.proc_pid == -1) {
		// dont pause process
		return;
	}

	// keep this here for now
	if (pcb != running_process) {
		printf("[WARNING] PAUSING PROCESS OTHER THAN RUNNING\n");
	}

	// are we trying to pause a to be killed process?
	if (pcb->remaining_time > 0) {
		printf("Setting pid=%d sysPid=%d paused\n", pcb->pid, pcb->system.proc_pid);

		// change state
		pcb->state = PROCESS_STATE_RDY;
		pcb->stats.last_finish = getClk();

		log_data(pcb);

		// send pause signal
		kill(pcb->system.proc_pid, SIGTSTP);
	}

	running_process = 0;
}

// ================================Scheduling algorithms================================

/// HPF scheduler
void sched_hpf() {
	// do we have a running process?
	if (!running_process) {
		// nothing running
		// pick highest priority (lowest val)

		process_control_block* pcb;
		if (pri_queue_dequeue(&process_queue, &pcb)) {
			printf("HPF assigning new proc\n");

			run_process(pcb);
		}
	}

	// uncomment for pre-emption
	/*else {
		// we're running, but the queue may have a higher priority
		process_control_block* potentialPcb;
		if (pri_queue_peek(&process_queue, &potentialPcb) && potentialPcb->priority < running_process->priority) {
			// the other process has a higher priority

			printf("HPF Found process with higher priority\n");

			// dequeue
			pri_queue_dequeue(&process_queue, 0);

			// re-queue running
			pri_queue_enqueue(&process_queue, running_process->priority, running_process);

			// pause running proc
			pause_process(running_process);

			// run new one
			run_process(potentialPcb);
		}
	}*/
}

/// SRTN scheduler
void sched_srtn() {
	// do we have a running process?
	if (!running_process) {
		// nothing running
		// pick shortest time

		process_control_block* pcb;
		if (pri_queue_dequeue(&process_queue, &pcb)) {
			printf("SRTN assigning new proc\n");

			run_process(pcb);
		}
	}
	else {
		// we're running, but the queue may have a lower time
		process_control_block* potentialPcb;
		if (pri_queue_peek(&process_queue, &potentialPcb) && potentialPcb->remaining_time < running_process->remaining_time) {
			// the other process has a lower time

			printf("SRTN Found process with lower time\n");

			// dequeue
			pri_queue_dequeue(&process_queue, 0);

			// re-queue running
			pri_queue_enqueue(&process_queue, running_process->remaining_time, running_process);

			// pause running proc
			pause_process(running_process);

			// run new one
			run_process(potentialPcb);
		}
	}
}

/// RR scheduler
void sched_rr(int quantum) {
	// do we have a running process?
	if (!running_process) {
		// nothing running
		// pick first in queue

		process_control_block* pcb;
		if (pri_queue_dequeue(&process_queue, &pcb)) {
			printf("RR assigning new proc\n");

			// update change time
			last_rr_change_time = getClk();

			run_process(pcb);
		}
	}
	else {
		int now = getClk();
		if (now - last_rr_change_time >= quantum) {
			printf("RR quantum change delta=%d\n", now - last_rr_change_time);

			last_rr_change_time = now;

			// dequeue
			process_control_block* potentialPcb;
			if (pri_queue_dequeue(&process_queue, &potentialPcb)) {
				printf("RR Changing process\n");

				if (running_process->remaining_time > 0) {
					// re-queue running
					pri_queue_enqueue(&process_queue, 0, running_process);
				}

				//printf("Queue: [ ");
				//struct pri_queue_node* n = process_queue.head;
				//while (n)
				//{
				//	printf("%d ", ((process_control_block*)n->value)->pid);
				//	//*(int *)n->value
				//	n = n->next;
				//}

				//printf("]\n");

				if (running_process->remaining_time > 0) {
					// pause running proc
					pause_process(running_process);
				}

				// run new one
				run_process(potentialPcb);
			}
		}
	}
}

/// Called when a process terminates
void process_termination_handler(int sig) {
	pid_t pid = wait(0);

	printf("Process with pid=%d just terminated\n", pid);

	// find pcb
	process_control_block* pcb = process_table_find_pcb_from_system(pid);
	if (!pcb) {
		// how?
		perror("Cannot find pcb for termination");

		// exit for now
		exit(-1);
	}

	// set state to terminated
	pcb->state = PROCESS_STATE_TERMINATED;
	pcb->system.proc_pid = -1;

	// set finish time
	pcb->stats.finish = getClk();

	log_data(pcb);

	if (running_process == pcb) {
		running_process = 0;
	}

	terminated_processes_count++;
}

void process_running_time_handler(int sig) {
	printf("Running process updating remaining time!\n");

	if (!running_process) {
		printf("[WARNING] Received decrement signal with no running process?\n");
		return;
	}

	printf("RPID=%d rt=%d\n", running_process->pid, running_process->remaining_time);

	// decrement locally
	running_process->remaining_time--;
}

void log_data(process_control_block* pcb) {
	if (!pcb) return;

	FILE* f = fopen("scheduler.log", "a");

	char* state;
	switch (pcb->state) {
	case PROCESS_STATE_RDY:
		state = "stopped";
		break;

	case PROCESS_STATE_STARTED:
		state = "started";
		break;

	case PROCESS_STATE_RESUMED:
		state = "resumed";
		break;

	case PROCESS_STATE_TERMINATED:
		state = "finished";
		break;

	default:
		state = "";
		break;
	}

	fprintf(f, "At time %d\tprocess %d\t%s   arr %d   total %d   remain %d   wait %d",
		getClk(), pcb->pid, state, pcb->arrival_time, pcb->running_time, pcb->remaining_time, pcb->stats.waiting_time);

	if (pcb->state == PROCESS_STATE_TERMINATED) {
		fprintf(f, "   TA %d   WTA %.2f", process_control_block_turnaround_time(pcb), process_control_block_weighted_turnaround_time(pcb));
	}

	fprintf(f, "\n");
	fclose(f);
}

void log_perf() {
	int totalTime = 0;
	float totalWTA = 0.f;
	int totalWaiting = 0;

	int count = 0;

	doubly_linked_list_node* n = process_table.head;
	while (n) {
		process_control_block* pcb = (process_control_block*)n->value;

		totalTime += pcb->running_time;
		totalWTA += process_control_block_weighted_turnaround_time(pcb);
		totalWaiting += pcb->stats.waiting_time;

		count++;

		n = n->next;
	}

	float utilization = totalTime / (float)getClk();
	float avgWTA = totalWTA / (float)count;
	float avgWaiting = totalWaiting / (float)count;

	// calc std wta

	float stdWTA = 0.f;

	if (count > 0) {
		doubly_linked_list_node* n = process_table.head;
		while (n) {
			process_control_block* pcb = (process_control_block*)n->value;

			stdWTA += powf(process_control_block_weighted_turnaround_time(pcb) - avgWTA, 2);

			n = n->next;
		}

		stdWTA = sqrtf(stdWTA / count);
	}

	FILE* f = fopen("scheduler.perf", "w");
	fprintf(f, "CPU Utilization = %.2f%%\nAvg WTA = %.2f\nAvg Waiting = %.2f\nStd WTA = %.2f\n", utilization * 100.f, avgWTA, avgWaiting, stdWTA);

	fclose(f);
}