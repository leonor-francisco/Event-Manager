#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>

typedef struct waitCommand{
  unsigned int delay;
  pthread_mutex_t mutex_w;
} WaitCommand;

typedef struct commandArgs {
  pthread_t thread_id;
  int num_threads;
  int threadIndex;
  int fd;
  int outFile;
  size_t *xs;
  size_t *ys;
  //pthread_mutex_t mutex;
  pthread_mutex_t *mutex_get_next;
  pthread_mutex_t *mutex_c_l;  //mutex para o comando CREATE e LIST
  pthread_mutex_t *mutex_s_l;  //mutex para o comando SHOW e LIST
  pthread_rwlock_t *rd_lock;

} CommandArgs;


//void join_threads(int num_threads, int threadState[], CommandArgs threadVector[]);

void create_threads(int num_threads, int fd, int outFile, CommandArgs threadVector[],
          pthread_mutex_t *mutex_get_next, pthread_mutex_t *mutex_c_l,
          pthread_mutex_t *mutex_s_l, pthread_rwlock_t *rd_lock);


void *chooseCommand(void *commandArgs);


#endif 