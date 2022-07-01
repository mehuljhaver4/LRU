#ifndef LRU_
#define LRU_
#include "utils.h"

// typedef struct QNode {
// 	struct QNode *prev, *next;
// 	unsigned pageNumber; 
// } QNode;

// // A Queue (A FIFO collection of Queue Nodes)
// typedef struct Queue {
// 	unsigned count; // Number of filled frames
// 	unsigned numberOfFrames; // total number of frames
// 	QNode *front, *rear;
// } Queue;

Queue* createQueue(int numberOfFrames);

// A utility function to check if queue is empty
int isQueueEmpty(Queue* queue);

// A utility function to delete a frame from queue
QNode* deQueue(Queue* queue);


QNode* newQNode(Queue* queue, Hash* hash, unsigned pageNumber);
QNode* allocate_node(Queue* queue, Hash* hash, unsigned pageNumber);

void access_node(Queue* queue, Hash* hash, QNode* reqPage);

// A utility function to delete a pageNumber from cache
QNode* free_node(Queue* queue, QNode* reqPage);
#endif