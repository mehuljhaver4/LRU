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

    // inserting 1,2,3,4,5,6,7,8,9
    for(int i = 1; i< 10; i++){
        printf("\n >>> Thread: %ld, Inserting page number: %d \n",tid, i);

        pthread_spin_lock(&hashtbl_lock);
        QNode* reqPage = hash->array[i];

        if (reqPage == NULL) {
            pthread_spin_unlock(&hashtbl_lock);  
            reqPage = allocate_node(q, hash, i);  
            
            pthread_spin_lock(&hashtbl_lock);  
            hash->array[i] = reqPage; // Add page entry to hash also
            pthread_spin_unlock(&hashtbl_lock);  
        }

        else {
            pthread_spin_unlock(&hashtbl_lock);
            access_node(q,hash,reqPage);  
        }
            

        // pthread_spin_unlock(&hashtbl_lock);   
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

    DisplayCache(q);

    //deleting 6, 8
    QNode* delPage6 = hash->array[6];
    hash->array[6] = NULL;
    buffer_node* free_buffer6 = (buffer_node *)free_node(q,delPage6);
    put_buffer(&root,free_buffer6);

    QNode* delPage8 = hash->array[8];
    buffer_node* free_buffer8 = (buffer_node *)free_node(q,delPage8);
    put_buffer(&root,free_buffer8);

    // // insert 6
    // QNode* new_pg = hash->array[6];
    // if (new_pg!= NULL) 
    //        access_node(q,hash,new_pg);
    // else{
    //         QNode* new_allocate2 = allocate_node(q, hash,6);  
    //         // Add page entry to hash also
    //         hash->array[6] = new_allocate2; 
    // }

    DisplayCache(q);
    // print_buffer_pool(root);

    return SUCCESS;
}