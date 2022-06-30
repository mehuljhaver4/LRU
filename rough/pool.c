#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <sys/sysinfo.h>
#include <sys/queue.h>


#define SUCCESS 0
#define FAIL -1
int task_count = 4;

void *buffer_pool_ptr = NULL; 

typedef struct buffer_node {
    int value;
	struct buffer_node *prev, *next;
} buffer_node;

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
    printf("\n Mem pool created\n");
    root = buffer_pool_ptr;
    if(!buffer_pool_ptr) {
        printf("\nBuffer memory allocation failed\n");
        return FAIL;
    }
    // using a ref node to update the pointer after incrementing
    ref_node = root; 

    for(int i = 0; i< task_count; i++) {
        append_buffer(&ref_node, &root, sizeof(buffer_node));
        ref_node++;
    }
    return SUCCESS;
}


void printList(buffer_node *root) {
    printf("\n****root address: %p ******\n",root);
    printf("\n*****ref_node address final: %p ******\n",ref_node);
    printf("\n*****Block start: %p ****\n", buffer_pool_ptr);
    buffer_node *temp = root;
    while(temp != NULL) {
         printf("\nTemp: %p, Temp->value: %d\n ",temp,temp->value);
         temp = temp->next;
         }
    printf("NULL\n");
}


int main() {
    buffer_pool();
    printList(root);
    return FAIL;
}



