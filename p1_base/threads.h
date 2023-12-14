#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>

typedef struct commandArgs {
  pthread_t thread_id;
  int *threadStateIndex;
  
  int fd;
  int outFile;
  size_t *xs;
  size_t *ys;
  //pthread_mutex_t mutex;
  pthread_mutex_t *mutex_get_next;
  pthread_rwlock_t *rd_lock;
  pthread_rwlock_t *rwlock;


} CommandArgs;


void join_threads(int num_threads, int threadState[], CommandArgs threadVector[]);

void create_threads(int num_threads, int fd, int outFile, size_t xs[], size_t ys[],
  int threadState[], CommandArgs threadVector[], pthread_mutex_t *mutex_get_next, pthread_rwlock_t *rwlock, pthread_rwlock_t *rd_lock);

void *chooseCommand(void *commandArgs);


#endif 