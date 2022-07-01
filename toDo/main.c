#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
// #include "utils.h"
#include "hash.h"
#include "bufferPool.h"
#include "LRU.h"

Queue* q;
Hash* hash;

void print_buffer_pool(buffer_node *root) {
    printf("\n*****Block start: %p ****\n", buffer_pool_ptr);
    buffer_node *temp = root;
    while(temp!= NULL) {
         printf("\nTemp: %p, Temp->value: %d\n ",temp,temp->value);
         temp = temp->next;
         }
    printf("NULL\n");
}

void PrintResult(Queue* Q, Hash* H){
	printf("PageNumber: %d, Address: %p\n", Q->front->pageNumber, Q->front);
    while(Q->front->next!= NULL){
        printf("PageNumber: %d, Address: %p\n", Q->front->next->pageNumber, Q->front->next);
        Q->front->next = Q->front->next->next;
        }  
    }

void *thread_fun(void *ThreadID){
    
    long tid = (long)ThreadID;
    unsigned int random;

    printf("\n********** Using thread: %ld *************\n", tid);
    int rand_i = rand()%4; // random (0-4) number of operations
    
    for(int i = 0; i< rand_i; i++){
        random = (rand()%(9)) + 1; 
        printf("\n >>> Thread: %ld, Inserting page number: %d \n",tid, random);

        pthread_spin_lock(&hashtbl_lock);
        QNode* reqPage = hash->array[random];

        if (reqPage == NULL) {
            QNode* new_allocate = allocate_node(q, hash, random);  
            // Add page entry to hash also
            hash->array[random] = new_allocate; 
        }

        else            
            access_node(q,hash,reqPage);
    
        pthread_spin_unlock(&hashtbl_lock);   
        sleep(pow(5.0, -3));  //sleeping for 5 milliseconds
    }
    printf("\n x--------- Thread: %ld job done-------------x \n", tid);    
    return NULL;
}


int main() {
    buffer_pool();
    print_buffer_pool(root);

    pthread_t threads[THREADS];
    int rc;
    int i;
    q = createQueue(CACHESPACE);
    hash = createHash(HASHSPACE);

    if (pthread_spin_init(&buffer_lock, NULL) != 0) {
        printf("\n spin buffer has failed\n");
        return 1;
    }

    if (pthread_spin_init(&LRU_lock, NULL) != 0) {
        printf("\n spin allocate has failed\n");
        return 1;
    }

    if (pthread_spin_init(&hashtbl_lock, NULL) != 0) {
        printf("\n spin hastbl has failed\n");
        return 1;
    }

   for( i = 0; i < THREADS; i++ ) {
        printf("\n main() : creating thread: %d \n",i);
        rc = pthread_create(&threads[i], NULL, thread_fun, (void *)i);
        if (rc) {
            printf("Error:unable to create thread, %d\n", rc);
            exit(-1);
      }
   } 

    for( i = 0; i < THREADS; i++ ){
        pthread_join(threads[i], NULL);
    }

    pthread_spin_destroy(&LRU_lock);
    pthread_spin_destroy(&buffer_lock);
    pthread_spin_destroy(&hashtbl_lock);

    printf("Before free() \n");
    print_buffer_pool(root);

    QNode* delPage = hash->array[5];
    buffer_node* free_buffer = free_node(q,delPage);
    printf("Freed buffer %p\n",free_buffer);
    put_buffer(&root,free_buffer);

    printf("\n\n****Final cache****\n\n ");
    PrintResult(q,hash);
    print_buffer_pool(root);

    return SUCCESS;
}



