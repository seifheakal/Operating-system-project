#include "headers.h"

void clear_resources(int);

int read_processes(pri_queue* processes, int* count);
void get_scheduler_data(int* chosenAlgorithm, int* quantum);

int fork_clk(/* out */ pid_t* clkPid);
int fork_scheduler(int algorithm, int quantum, int procCount, /* out */ pid_t* schedPid);

int initialize_message_queue();
int process_loop(pri_queue* processes, key_t clk_child);

key_t process_msgq_id;

int main(int argc, char* argv[]) {
	// set interrupt handler
	signal(SIGINT, clear_resources);

	// process queue (priority = arrivalTime) incase processes.txt isnt sorted by AT
	pri_queue processesQueue;
	pri_queue_init(&processesQueue);

	int procsCount;
	int readProcResult;
	if (!(readProcResult = read_processes(&processesQueue, &procsCount))) {
		printf("Cannot read processes result=%d\n", readProcResult);
		goto exit;
	}

	// ask user for scheduler data
	int schedAlgo = -1;
	int quantum = -1;
	get_scheduler_data(&schedAlgo, &quantum);

	// fork scheduler
	pid_t schedulerPid;
	if (!fork_scheduler(schedAlgo, quantum, procsCount, &schedulerPid)) {
		// error msg is printed inside
		goto exit;
	}

	pid_t clkPid;
	if (!fork_clk(&clkPid)) {
		goto exit;
	}

	initClk();

	if (!process_loop(&processesQueue, clkPid)) {
		perror("Error in process loop");
		goto exit;
	}

	wait(NULL);

exit:
	pri_queue_free(&processesQueue);
	
	// invoke our own handler for now?
	raise(SIGINT);

	return 0;
}

void clear_resources(int signum) {
	printf("[ProcGen] Cleaning up...\n");

	msgctl(process_msgq_id, IPC_RMID, (struct msqid_ds*)0);
	destroyClk(true);
	exit(0);
}

void get_scheduler_data(int* algorithm, int* quantum) {
	do {
		printf("Choose a scheduling algorithm\n%d - HPF (Non-preemptive Highest Priority First)\n%d - SRTN (Shortest Remaining time Next)\n%d - RR (Round Robin)\nAlgorithm: ",
			SCHEDULING_ALGO_HPF, SCHEDULING_ALGO_SRTN, SCHEDULING_ALGO_RR);
		scanf("%d", algorithm);
	} while (*algorithm < 0 || *algorithm > 2);


	if (*algorithm == SCHEDULING_ALGO_RR) {
		do {
			printf("RR quantum: ");
			scanf("%d", quantum);
		} while (*quantum < 1);
	}
}

int read_processes(pri_queue* processes, int* count) {
	if (count) {
		*count = 0;
	}

	if (!processes)
		return 0;

	FILE* f = fopen("processes.txt", "r");
	if (!f)
	{
		// invalid file?
		return 0;
	}

	char* line = 0;
	size_t lineLen = 0;
	while (getline(&line, &lineLen, f) != EOF)
	{
		// we have a line :P
		// ignore empty lines or lines that start with #
		if (lineLen == 0 ||
			strlen(line) == 0 ||
			line[0] == '#')
			continue;

		// allocate process
		struct process_data* p = malloc(sizeof(process_data));
		memset(p, 0, sizeof(process_data));

		// read proc data
		sscanf(line, "%d%d%d%d", &p->id, &p->arrival_time, &p->running_time, &p->priority);

		// insert in queue
		pri_queue_enqueue(processes, p->arrival_time, p);
		printf("Process with id %d, arrivaltime %d, remainingtime %d, priority %d\n", p->id, p->arrival_time, p->running_time, p->priority);

		if (count) {
			(*count)++;
		}
	}

	// close file
	fclose(f);

	// free line
	if (line)
	{
		free(line);
	}

	return 1;
}

int fork_clk(/* out */ pid_t* clkPid) {
	pid_t child = fork();
	if (child == -1) {
		perror("Failed to fork clk");
		return 0;
	}
	else if (child == 0) {
		execl("./clk.out", "clk.out", NULL);
	}

	*clkPid = child;
	return 1;
}

int fork_scheduler(int algorithm, int quantum, int procCount, /* out */ pid_t* schedPid) {
	pid_t child = fork();
	if (child == -1) {
		perror("Failed to fork scheduler");
		return 0;
	}
	else if (child == 0) {
		// alloc params
		char params[3][10];
		sprintf(params[0], "%d", algorithm);
		sprintf(params[1], "%d", quantum);
		sprintf(params[2], "%d", procCount);

		execl("./scheduler.out", "scheduler.out", params[0], params[1], params[2], NULL);
	}

	// parent

	*schedPid = child;
	return 1;
}

int initialize_message_queue() {
	process_msgq_id = msgget(MSGKEY, 0666 | IPC_CREAT);
	if (process_msgq_id == -1) {
		perror("Error in create Message Queue");
		return 0;
	}

	return 1;
}

int process_loop(pri_queue* processes, key_t clk_child) {
	// Message Queue Generation to send the process data to the scheduler
	if (!initialize_message_queue()) {
		// error msg already printed
		return 0;
	}

	process_message_buffer msgBuffer;
	msgBuffer.type = 1;

	process_data* proc = 0;
	while (pri_queue_dequeue(processes, (void**)&proc))
	{
		// keep waiting
		while (proc->arrival_time > getClk())
		{
			//printf("waiting for %d\n", proc->arrival_time - getClk());

			// sleep for 200ms
			usleep(200 * 1000);
		}

		printf("[ProcGen] %d - sending process with id %d and running time %d and arrivaltime  %d and priority %d\n", getClk(), proc->id, proc->running_time, proc->arrival_time, proc->priority);

		// send via msgq
		msgBuffer.data = *proc;

		if (msgsnd(process_msgq_id, &msgBuffer, sizeof(msgBuffer.data), !IPC_NOWAIT) == -1) {
			perror("Error in send");
			return 0;
		}
	}

	return 1;
}