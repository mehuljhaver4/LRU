#include "utils.h"
#include "queue.h"

// A Queue (A FIFO collection of Queue Nodes)
typedef struct Queue {
	unsigned count; // Number of filled frames
	unsigned numberOfFrames; // total number of frames
	QNode *front, *rear;
} Queue;

Queue* q;

Queue* createQueue(int numberOfFrames)
{
	Queue* queue = (Queue*)malloc(task_count*sizeof(Queue));

	// The queue is empty
	queue->count = 0;
	queue->front = queue->rear = NULL;

	// Number of frames that can be stored in memory
	queue->numberOfFrames = numberOfFrames;

	return queue;
}

// A utility function to check if queue is empty
int isQueueEmpty(Queue* queue)
{
	return queue->rear == NULL;
}

