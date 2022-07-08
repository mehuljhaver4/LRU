#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pthread_t tid[2];
int counter;
int ret;
pthread_spinlock_t lock;

void* trythis(void* arg)
{
  // int lock1 = pthread_spin_trylock(&lock);
  // // int lock2 = pthread_spin_trylock();
  // printf("Lock: %d \n",lock1);
  // if (lock1 != 0) printf("Error \n");
  // else {
  //   unsigned long i = 0;
  //   counter += 1;
  //   printf("\n Job %d has started\n", counter);

  //   for (i = 0; i < (0xFFFFFFFF); i++)
  //     ;
  // }

    ret = pthread_spin_trylock(&lock);
    printf("ret: %d",ret);
    unsigned long i = 0;
    counter += 1;
    printf("\n Job %d has started\n", counter);

    for (i = 0; i < (0xFFFFFFFF); i++)
      ;
  printf("\n Job %d has finished\n", counter);
  pthread_spin_unlock(&lock);

  return NULL;
}

int main(void)
{
  int i = 0;
  int error;

  if (pthread_spin_init(&lock, NULL) != 0) {
    printf("\n spin init has failed\n");
    return 1;
  }

  while (i < 2) {
    error = pthread_create(&(tid[i]),
              NULL,
              &trythis, NULL);
    if (error != 0)
      printf("\nThread can't be created :[%s]",
        strerror(error));
    i++;
  }

  pthread_join(tid[0], NULL);
  pthread_join(tid[1], NULL);
  pthread_spin_destroy(&lock);

  return 0;
}
