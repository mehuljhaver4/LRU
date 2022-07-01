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
        printf("\nTemp: %p, Temp->value: %d\n ",temp,temp->value);
        buf_counter--;
        temp = temp->next;
         }
    printf("NULL\n");
}

void DisplayCache(Queue* Q, Hash* H) {
    printf("\n\n****Final cache****\n\n ");
    QNode* temp = Q->front;
    while(temp) {
        printf("PageNumber: %d, Address: %p\n", temp->pageNumber, temp);
        temp = temp->next;
    }
}

void *thread_fun(void *ThreadID){
    
    long tid = (long)ThreadID;
    unsigned int random;

    printf("\n********** Using thread: %ld *************\n", tid);
    int rand_i = rand()%4; // random (0-4) number of operations
    

    // inserting 1,2,3,4,5,6,7,8,9
    for(int i = 1; i< 10; i++){
        // random = (rand()%(10)) + 1; 
        printf("\n >>> Thread: %ld, Inserting page number: %d \n",tid, i);

        pthread_spin_lock(&hashtbl_lock);
        QNode* reqPage = hash->array[i];

        if (reqPage == NULL) {
            QNode* new_allocate = allocate_node(q, hash, i);  
            hash->array[i] = new_allocate; // Add page entry to hash also
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

    for( i = 0; i < THREADS; i++ )
        pthread_join(threads[i], NULL);
    
    pthread_spin_destroy(&LRU_lock);
    pthread_spin_destroy(&buffer_lock);
    pthread_spin_destroy(&hashtbl_lock);

    DisplayCache(q,hash);

    //deleting 1,5,9
    
    // QNode* delPage1 = hash->array[1];
    // buffer_node* free_buffer1 = free_node(q,delPage1);
    // printf("Freed buffer %p\n",free_buffer1);
    // put_buffer(&root,free_buffer1);

    QNode* delPage5 = hash->array[6];
    buffer_node* free_buffer5 = free_node(q,delPage5);
    printf("Freed buffer %p\n",free_buffer5);
    put_buffer(&root,free_buffer5);

    QNode* delPage9 = hash->array[10];
    buffer_node* free_buffer9 = free_node(q,delPage9);
    printf("Freed buffer %p\n",free_buffer9);
    put_buffer(&root,free_buffer9);


    DisplayCache(q,hash);

    return SUCCESS;
}



