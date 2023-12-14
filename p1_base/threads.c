#include <stdio.h>
#include <pthread.h>

#include "threads.h"
#include "constants.h"



void join_threads(int num_threads, int threadState[], CommandArgs threadVector[]) {
    int activeThreads = num_threads;
    while(activeThreads > 0) {
      for(int i = 0; i < num_threads; i++){
        if(threadState[i] == JOINABLE) {
          if (pthread_join(threadVector[i].thread_id, NULL) != 0)
            fprintf(stderr, "Error joining thread.\n");
          threadState[i] = FINISHED;
          activeThreads--;
        }
      }
    }
}

void create_threads(int num_threads, int fd, int outFile, size_t xs[], size_t ys[],
  int threadState[], CommandArgs threadVector[], pthread_mutex_t *mutex_get_next, pthread_mutex_t *mutex_c_l,
   pthread_mutex_t *mutex_s_l, pthread_rwlock_t *rwlock, pthread_rwlock_t *rd_lock); {

    for(int i = 0; i < num_threads; i++)
      threadState[i] = AVAILABLE;

    for(int i = 0; i < num_threads; i++) {
      threadVector[i].fd = fd;
      threadVector[i].outFile = outFile;
      threadVector[i].xs = xs;
      threadVector[i].ys = ys;
      threadVector[i].threadStateIndex = &threadState[i];
      threadVector[i].mutex_get_next = mutex_get_next;
      threadVector[i].mutex_c_l = mutex_c_l;
      threadVector[i].mutex_s_l = mutex_s_l;
      threadVector[i].rwlock = rwlock;
      threadVector[i].rd_lock = rd_lock;
      pthread_create(&threadVector[i].thread_id, NULL, chooseCommand, (void*)&threadVector[i]);

    }    
  }




