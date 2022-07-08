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

// void DisplayCache(Queue* Q) {
//     printf("\n\n****Final cache****\n\n ");
    
//     if (isQueueEmpty(Q)){
//         printf("Cache is empty\n");
//     }
//     else {
//         QNode* temp = Q->front;
//         while(temp) {
//             printf("Key: %d, Address: %p\n", temp->key, temp);
//             temp = temp->next;
//         }
//     }
// }

// void *thread_fun(void *ThreadID){
    
//     long tid = (long)ThreadID;
//     unsigned int random;

//     printf("\n********** Using thread: %ld *************\n", tid);
//     int rand_i = rand()%4; // random (0-4) number of operations
    
//     for(int i = 0; i< rand_i; i++){
//         random = (rand()%(10)) + 1; 
//         printf("\n >>> Thread: %ld, Inserting page number: %d \n",tid, random);

//         pthread_spin_lock(&hashtbl_lock);
//         QNode* reqPage = hash->array[random];

//         if (reqPage == NULL) {
//             QNode* new_allocate = allocate_node(q, hash, random);  
//             // Add page entry to hash also
//             hash->array[random] = new_allocate; 
//         }

//         else            
//             access_node(q,hash,reqPage);
    
//         pthread_spin_unlock(&hashtbl_lock);   
//         sleep(pow(5.0, -3));  //sleeping for 5 milliseconds
//     }
//     printf("\n x--------- Thread: %ld job done-------------x \n", tid);    
//     return NULL;
// }

// void *thread_fun(void *ThreadID){
    
//     long tid = (long)ThreadID;
//     unsigned int random;

//     printf("\n********** Using thread: %ld *************\n", tid);
//     // int rand_i = rand()%2; // random (0-4) number of operations
    
//     for(int i = 0; i< 6; i++){
//         // random = (rand()%(10)) + 1; 
//         printf("\n >>> Thread: %ld, Inserting page number: %d \n",tid, i);

//         // pthread_spin_lock(&hashtbl_lock);
//         QNode* reqPage = hash->array[i];

//         if (reqPage == NULL) {
//             QNode* new_allocate = allocate_node(q, hash, i);  
//             // Add page entry to hash also
//             hash->array[i] = new_allocate; 
//             printf("new_allocate : %p key: %d\n",new_allocate, new_allocate->key);
//         }

//         else {

//         }         
//             // access_node(q,hash,reqPage);
    
//         // pthread_spin_unlock(&hashtbl_lock);   
//         sleep(pow(5.0, -3));  //sleeping for 5 milliseconds
//     }
//     printf("\n x--------- Thread: %ld job done-------------x \n", tid);    
//     return NULL;
// }

int main() {
    buffer_pool();
    print_buffer_pool(root);

    // pthread_t threads[THREADS];
    int rc;
    int i;
    q = createQueue();
    hash = createHash(HASHSPACE);

    for (i = 0; i<10; i++) {
        QNode* temp =  allocate_node(q,hash,i);
        hash->array[i] = temp;
        printf("Temp : %p key: %d\n",temp, temp->key);
    }

    //check the hash
    for (i=0; i<10; i++)
        printf("hash->array[%d]: %p \n",i, hash->array[i]);

    //access 8
    QNode* new_pg = hash->array[8];
    printf("new_pg : %p key: %d\n",new_pg, new_pg->key);
    access_node(q,hash,new_pg,8);

 print_buffer_pool(root);
     //check the hash
    for (i=0; i<10; i++)
        printf("hash->array[%d]: %p \n",i, hash->array[i]);

    // if (pthread_spin_init(&buffer_lock, 0) != 0) {
    //     printf("\n spin buffer has failed\n");
    //     return 1;
    // }

//     if (pthread_spin_init(&QNode->node_lock , 0) != 0) {
//         printf("\n spin allocate has failed\n");
//         return 1;
//     }

//     if (pthread_spin_init(&hashtbl_lock, 0) != 0) {
//         printf("\n spin hastbl has failed\n");
//         return 1;
//     }

//    for( i = 0; i < THREADS; i++ ) {
//         printf("\n main() : creating thread: %d \n",i);
//         rc = pthread_create(&threads[i], 0, thread_fun, (void *) (intptr_t) i);
//         if (rc) {
//             printf("Error:unable to create thread, %d\n", rc);
//             exit(-1);
//       }
//    } 

    // for( i = 0; i < THREADS; i++ )
    //     pthread_join(threads[i], NULL);
    
//     pthread_spin_destroy(&QNode->node_lock);
    // pthread_spin_destroy(&buffer_lock);
//     pthread_spin_destroy(&hashtbl_lock);

    return SUCCESS;
}