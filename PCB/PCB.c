#include "headers.h"

/*state : 0 : idle
     1:running
     2:Terminating
     3:resumed
     4:finished
    */

typedef struct pcb
{
    int id;
    int start_time;
    int end_time;
    int remaining_time;
    int arrival_time;
    int waiting_time;
    int running_time;
    int last_running_time;
    int total_time;
    int state;
    int priority;
    int TA;
    float WTA;
} pcb;

typedef struct process_table
{
    struct pcb **processTable;
    int size;
    int max_capacity;
} process_table;

void process_table_init(process_table *pt, int max_capacity)
{
    pt->processTable = (pcb **)malloc(sizeof(pcb *) * max_capacity);
    pt->size = 0;
    pt->max_capacity = max_capacity;
}

int process_table_add(process_table *pt, process_data *p)
{
    if (!pt || !p || pt->size >= pt->max_capacity)
        return 0;
    pcb *new_pcb = (pcb *)malloc(sizeof(pcb));
    memset(new_pcb, 0, sizeof(pcb));
    new_pcb->id = p->id;
    new_pcb->running_time = p->running_time;
    new_pcb->arrival_time = p->arrival_time;
    new_pcb->remaining_time = p->running_time;
    new_pcb->priority = p->priority;
    pt->processTable[p->id - 1] = new_pcb;
    pt->size++;
    return 1;
}
pcb *process_table_find(process_table *pt, process_data *p)
{
    if (!p || !pt || p->id < 0 || p->id > pt->size)
        return NULL;
    return pt->processTable[p->id - 1];
}
int process_table_remove(process_table *pt, process_data *p)
{
    if (!p || !pt || p->id < 0 || p->id > pt->size || pt->size == 0 || !process_table_find(pt, p))
        return 0;
    pt->processTable[p->id - 1] = NULL;
    free(pt->processTable[p->id - 1]);
    pt->size--;
    return 1;
}
void process_table_free(process_table *pt)
{
    while (pt->size != 0)
    {
        free(pt->processTable[pt->size - 1]);
        pt->size--;
    }
    free(pt->processTable);
}

void process_table_print(process_table *pt)
{
    for (int i = 0; i < pt->size; i++)
    {
        printf("Process %d : id %d , arrival time %d , running time %d , priority %d\n", i + 1, pt->processTable[i]->id, pt->processTable[i]->arrival_time, pt->processTable[i]->running_time, pt->processTable[i]->priority);
    }
}
int process_table_get_size(process_table *pt)
{
    return pt->size;
}