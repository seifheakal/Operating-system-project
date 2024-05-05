#pragma once

#define PROCESS_STATE_RDY 0
#define PROCESS_STATE_STARTED 1
#define PROCESS_STATE_TERMINATED 2
#define PROCESS_STATE_RESUMED 3

typedef struct process_control_block {
	int state;
	int pid;

	// linux/runtime related
	struct {
		int proc_pid;
	} system;

	struct {
		int start;
		int finish;

		// used to calc wait
		int last_finish;
		int waiting_time;
	} stats;

	int priority;
	int remaining_time;
	int running_time;
	int arrival_time;
} process_control_block;

int process_control_block_turnaround_time(process_control_block* pcb) {
	if (!pcb) return -1;

	return pcb->stats.finish - pcb->arrival_time;
}

float process_control_block_weighted_turnaround_time(process_control_block* pcb) {
	if (!pcb || pcb->running_time == 0) return -1;

	return process_control_block_turnaround_time(pcb) / (float)pcb->running_time;
}

typedef struct pcb_system_pid_iterator {
	int system_pid;
	process_control_block** result;
} pcb_system_pid_iterator;

void process_table_find_pcb_from_system_iterator(void* value, void* param) {
	if (!value || !param) return;

	process_control_block* pcb = (process_control_block*)value;
	pcb_system_pid_iterator* it = (pcb_system_pid_iterator*)param;

	if (it->system_pid == pcb->system.proc_pid) {
		*it->result = pcb;
	}
}
