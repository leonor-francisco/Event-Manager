#include <stdio.h>
#include <pthread.h>

#include "threads.h"
#include "constants.h"

void create_threads(int num_threads, int fd, int outFile, CommandArgs threadVector[],
          pthread_mutex_t *mutex_get_next, pthread_mutex_t *mutex_c_l,
          pthread_mutex_t *mutex_s_l, pthread_rwlock_t *rd_lock) {

    size_t threads_xs[num_threads][MAX_RESERVATION_SIZE];
    size_t threads_ys[num_threads][MAX_RESERVATION_SIZE];
        
    for(int i = 0; i < num_threads; i++) {
      threadVector[i].fd = fd;
      threadVector[i].outFile = outFile;
      threadVector[i].num_threads = num_threads;
      threadVector[i].xs = threads_xs[i];
      threadVector[i].ys = threads_ys[i];
      threadVector[i].threadIndex = i;
      threadVector[i].mutex_get_next = mutex_get_next;
      threadVector[i].mutex_c_l = mutex_c_l;
      threadVector[i].mutex_s_l = mutex_s_l;
      threadVector[i].rd_lock = rd_lock;
      pthread_create(&threadVector[i].thread_id, NULL, chooseCommand, (void*)&threadVector[i]);

    }    
  }




