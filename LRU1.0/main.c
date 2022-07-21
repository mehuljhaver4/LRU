#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include "utils.h"

Queue* q;
Hash* hash;

void print_buffer_pool(buffer_node *root) {
    printf("\n*****Block start: %p ****\n", buffer_pool_ptr);
    buffer_node *temp = root;
    int buf_counter = buffer_count;
    while(buf_counter){
        printf("\nBuffer %d: %p\n",buf_counter,temp);
        buf_counter--;
        temp = temp->next;
         }
    printf("NULL\n");
}

void DisplayCache(Queue* Q) {
    printf("\n\n****Final cache****\n\n ");
    
    if (isQueueEmpty(Q)){
        printf("Cache is empty\n");
    }
    else {
        QNode* temp = Q->front;
        while(temp) {
            printf("PageNumber: %d, Address: %p\n", temp->pageNumber, temp);
            temp = temp->next;
        }
    }

}

void *thread_fun(void *ThreadID){
    
    long tid = (long)ThreadID;
    unsigned int random;

    printf("\n********** Using thread: %ld *************\n", tid);
    int rand_i = rand()%4; // random (0-4) number of operations
    
    for(int i = 0; i< rand_i; i++){
        random = (rand()%(1000)) + 1; 
        // random = 1;
        printf("\n >>> Thread: %ld, Inserting page number: %d \n",tid, random);

        pthread_spin_lock(&hashtbl_lock);
        QNode* reqPage = hash->array[random];

        if (reqPage == NULL) {
            pthread_spin_unlock(&hashtbl_lock); 
            
            reqPage = allocate_node(q, hash, random);  
            
            // Add page entry to hash 
            pthread_spin_lock(&hashtbl_lock);

            if (hash->array[random] == NULL)
                hash->array[random] = reqPage; 
            else
                free_node(q,reqPage);
            
            pthread_spin_unlock(&hashtbl_lock);
        }

        else{
            pthread_spin_unlock(&hashtbl_lock);
            access_node(q,hash,reqPage); 
        }            
          
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
    q = createQueue();
    hash = createHash(HASHSPACE);

    if (pthread_spin_init(&buffer_lock, 0) != 0) {
        printf("\n spin buffer has failed\n");
        return 1;
    }

    if (pthread_spin_init(&LRU_lock, 0) != 0) {
        printf("\n spin allocate has failed\n");
        return 1;
    }

    if (pthread_spin_init(&hashtbl_lock, 0) != 0) {
        printf("\n spin hastbl has failed\n");
        return 1;
    }

   for( i = 0; i < THREADS; i++ ) {
        printf("\n main() : creating thread: %d \n",i);
        rc = pthread_create(&threads[i], 0, thread_fun, (void *) (intptr_t) i);
        if (rc) {
            printf("Error:unable to create thread, %d\n", rc);
            exit(-1);
      }
   } 

    for( i = 0; i < THREADS; i++ )
        pthread_join(threads[i], NULL);
    
    pthread_spin_destroy(&LRU_lock);
    pthread_spin_destroy(&buffer_lock);
    pthread_spin_destroy(&hashtbl_lock);

    // DisplayCache(q);

    // delete 6
    QNode* delPage = hash->array[6];
    buffer_node* free_buffer = (buffer_node *)free_node(q,delPage);
    put_buffer(&root,free_buffer);

    // // insert 5
    // QNode* new_pg = hash->array[5];
    // if (new_pg!= NULL) 
    //        access_node(q,hash,new_pg);
    // else{
    //         QNode* new_allocate2 = allocate_node(q, hash,5);  
    //         // Add page entry to hash also
    //         hash->array[5] = new_allocate2; 
    // }

    // delete 10
    QNode* delPage2 = hash->array[10];
    buffer_node* free_buffer2 = (buffer_node *)free_node(q,delPage2);
    put_buffer(&root,free_buffer2);

    // DisplayCache(q);

    return SUCCESS;
}