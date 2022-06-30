#include "utils.h"
#include "BufferPool.h"
#include "LRU.h"

void *buffer_pool_ptr = NULL; 

typedef struct buffer_node {
    int value;
	struct buffer_node *prev, *next;
} buffer_node;

//defining root and ref root globally to use in other functions
buffer_node* root = NULL;
buffer_node* ref_node = NULL;

// Append buffers to the memory pool
buffer_node* append_buffer(buffer_node **ref_node, buffer_node **root, int size)
{
    buffer_node* newNode = *ref_node;
    buffer_node* lastNode = root;
    
    newNode->next = NULL;
    // newNode->value = sizeof(buffer_node);

    while(lastNode->next != NULL) {
        lastNode = lastNode->next;
    }

    lastNode->next = newNode;
    newNode->prev = lastNode;
    return newNode;
}


// create a memory pool to append buffers for later use
int buffer_pool() {
    buffer_pool_ptr = calloc(task_count ,sizeof(buffer_node));
    printf("\n Memory pool created\n");
    root = buffer_pool_ptr;
    
	if(!buffer_pool_ptr) {
        printf("\nBuffer memory allocation failed\n");
        return FAIL;
    }
    // using a ref node to update the pointer after incrementing
    ref_node = root; 

    for(int i = 0; i< task_count; i++) {
        append_buffer(&ref_node, &root, sizeof(buffer_node));
		buffer_count++;
        ref_node++;
    }
    return SUCCESS;
}

// A utility function to get a buffer from buffer pool
QNode* get_buffer(buffer_node* root, Queue* queue, Hash* hash)
{
    buffer_node* lastNode = root;
    QNode* temp = NULL;

	//if only one node present.
	if (lastNode->next == NULL) {
        temp = (QNode *) lastNode;
        temp->prev = NULL;
		buffer_count--;
        return temp;
    }

    // when multiple nodes are preset get the last buffer
    // make changes to the second last buffer
    while (lastNode->next != NULL) {
        if (lastNode->next->next == NULL) {
            
			temp = (QNode *)lastNode->next; //temp gets the last node
            temp->prev = NULL; // prev link becomes null
            lastNode->next = NULL;

			buffer_count--;
            return temp;
        }
        lastNode = lastNode->next;
    }  
}

