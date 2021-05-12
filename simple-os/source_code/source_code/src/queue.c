#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
	/* TODO: put a new process to queue [q] */
	if (q->size == MAX_QUEUE_SIZE) return;
	q->proc[q->size] = proc;
	q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	if (empty(q)) return NULL;

	//find highest priority proc
	uint32_t priority_max = q->proc[0]->priority;
	for (int i = 1; i < q->size; i++)
	{
		if (q->proc[i]->priority > priority_max)
		{
			priority_max = q->proc[i]->priority;
		}
	}

	//return highest priority proc
	for (int i = 0; i < q->size; i++)
	{
		if (q->proc[i]->priority == priority_max)
		{
			/*
			//solution 1
			//copy
			struct pcb_t *return_proc = (struct pcb_t *)malloc(sizeof(struct pcb_t));
			return_proc->pid = q->proc[i]->pid;
			return_proc->priority = q->proc[i]->priority;

			return_proc->code = (struct code_seg_t *)malloc(sizeof(struct code_seg_t));
			return_proc->code->size = q->proc[i]->code->size;
			return_proc->code->text->arg_0 = q->proc[i]->code->text->arg_0;
			return_proc->code->text->arg_1 = q->proc[i]->code->text->arg_1;
			return_proc->code->text->arg_2 = q->proc[i]->code->text->arg_2;
			return_proc->code->text->opcode = q->proc[i]->code->text->opcode;

			for (int j = 0; j < 10; j++)
				return_proc->regs[j] = q->proc[i]->regs[j];

			return_proc->pc = q->proc[i]->pc;

			return_proc->seg_table = (struct seg_table_t *)malloc(sizeof(struct seg_table_t));
			return_proc->seg_table->size = q->proc[i]->seg_table->size;
			for (int j = 0; j < (1 << SEGMENT_LEN); j++) 
				return_proc->seg_table->table[j] = q->proc[i]->seg_table->table[j];

			return_proc->bp = q->proc[i]->bp;
			
			//delete
			free(q->proc[i]);
			for (int j = i; j < q->size - 1; j++)
				q->proc[j] = q->proc[j + 1];
			q->proc[q->size - 1] = NULL;
			q->size--;

			return return_proc;
			*/

			//solution 2
			struct pcb_t *return_proc = q->proc[i];
			q->proc[i] = NULL;
			for (int j = i; j < q->size - 1; j++)
				q->proc[j] = q->proc[j + 1];
			q->proc[q->size - 1] = NULL;
			q->size--;

			return return_proc;
		}
	}

	return NULL;
}

