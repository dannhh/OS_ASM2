#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) 
{
	// TODO: put a new process to queue [q] 	
	if (q->size == 0) 
	{
		q->proc[0] = proc;
		++(q->size); 
	}
	else if (q->size < MAX_QUEUE_SIZE)
	{
		int ind = q->size;
	
		while (ind > 0 && (q->proc[ind-1]->priority) < (proc->priority))
		{
			--ind;
		}
		
		for (int i = q->size; i > ind; --i)
		{
		 	q->proc[i] = q->proc[i-1];
		}
		q->proc[ind] = proc;
		++(q->size);
	}
}

struct pcb_t * dequeue(struct queue_t * q) 
{
	struct pcb_t * highest_proc;
	if (q->size == 0)
	{
		return NULL;
	}
	else if (q->size == 1)
	{
		q->size = 0;
		highest_proc = q->proc[0];
		q->proc[0] = NULL; //essential??? how about free???
	}
	else
	{
		highest_proc = q->proc[0];
		for (int i = 0; i < ((q->size) - 1); ++i)
		{
			q->proc[i] = q->proc[i+1];
		}
		q->proc[(q->size) - 1] = NULL;
		--(q->size);
	}
	return highest_proc;
}

