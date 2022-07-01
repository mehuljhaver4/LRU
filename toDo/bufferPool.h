#ifndef BUFFPOOL
#define BUFFPOOL
#include "utils.h"

// typedef struct buffer_node {
//     int value;
// 	struct buffer_node *prev, *next;
// } buffer_node;

// //defining root and ref root globally to use in other functions
// buffer_node* root = NULL;
// buffer_node* ref_node = NULL;

void *buffer_pool_ptr = NULL; 

// Append buffers to the memory pool
buffer_node* append_buffer(buffer_node **ref_node, buffer_node **root, int size);

// create a memory pool to append buffers for later use
int buffer_pool();

// A utility function to add the buffer back to the buffer pool
// after deleting a pageNumber from cache.
buffer_node* put_buffer(buffer_node **root, QNode* free_buffer);

// A utility function to get a buffer from buffer pool
QNode* get_buffer(buffer_node* root, Queue* queue, Hash* hash);

#endif