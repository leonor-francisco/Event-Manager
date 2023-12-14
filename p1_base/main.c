#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

#include "threads.h"
#include "constants.h"
#include "operations.h"
#include "parser.h"
#include <linux/limits.h>
#include <bits/pthreadtypes.h>


int state = NO_BARRIER;


void *chooseCommand(void *commandArgs) {
  CommandArgs *args = (CommandArgs*) commandArgs;
  unsigned int event_id, delay;
  size_t num_rows, num_columns, num_coords;
  int commandLine = 1;

  while(commandLine != EOC) {
    
    pthread_mutex_lock(args->mutex_get_next);
    if(state != NO_BARRIER)
        break;
    commandLine = get_next(args->fd);
    switch (commandLine){
      case CMD_CREATE:
        int create = parse_create(args -> fd, &event_id, &num_rows, &num_columns);
        pthread_mutex_unlock(args->mutex_get_next); //UNLOCK

        if (create != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;
        }

        //pthread_rwlock_rdlock(args->rwlock);

        pthread_mutex_lock(args->mutex_c_l);
        
        if (ems_create(event_id, num_rows, num_columns)) {
          fprintf(stderr, "Failed to create event\n");
        }

        pthread_mutex_unlock(args->mutex_c_l);
        //pthread_rwlock_unlock(args->rwlock);

        break;

      case CMD_RESERVE:
        num_coords = parse_reserve(args -> fd, MAX_RESERVATION_SIZE, &event_id, args -> xs, args -> ys);
        pthread_mutex_unlock(args->mutex_get_next); //UNLOCK

        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;
        }
        
        sortVectors(args -> xs, args -> ys, event_id);

        if (ems_reserve(event_id, num_coords, args -> xs, args -> ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }
        
        break;


      case CMD_SHOW:
        int show = parse_show(args -> fd, &event_id);
        pthread_mutex_unlock(args->mutex_get_next);

        if (show != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;
        }

        pthread_mutex_lock(args->mutex_s_l);

        pthread_rwlock_wrlock(args->rd_lock);

        if (ems_show(event_id, args -> outFile)) {
          fprintf(stderr, "Failed to show event\n");
        }
        
        pthread_rwlock_unlock(args->rd_lock);

        pthread_mutex_unlock(args->mutex_s_l);


        break;

      case CMD_LIST_EVENTS:
        pthread_mutex_unlock(args->mutex_get_next);
        
        pthread_mutex_lock(args->mutex_s_l);
        pthread_mutex_lock(args->mutex_c_l);

        if (ems_list_events(args -> outFile)) {
          fprintf(stderr, "Failed to list events\n");
        }
        
        pthread_mutex_unlock(args->mutex_c_l);
        pthread_mutex_unlock(args->mutex_s_l);

        break;

      case CMD_WAIT: //FAZER LOCKS

        if (parse_wait(args -> fd, &delay, NULL) == -1) {  // thread_id is not implemented
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;
        }
        
        if (delay > 0) {
          write(args -> outFile, "Waiting...\n", sizeof("Waiting...\n"));
          ems_wait(delay);
        }

        break;

      case CMD_INVALID: //FAZER LOCKS

        pthread_mutex_unlock(args->mutex_get_next);
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP: //FAZER LOCKS

        pthread_mutex_unlock(args->mutex_get_next);
        printf(
            "Available commands:\n"
              "CREATE <event_id> <num_rows> <num_columns>\n"
              "RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
              "SHOW <event_id>\n"
              "LIST\n"
              "WAIT <delay_ms> [thread_id]\n"
              "BARRIER\n"                   
              "HELP\n");

        break;

      case CMD_BARRIER:
      
        state = BARRIER;
        pthread_mutex_unlock(args->mutex_get_next);
        break;

      case CMD_EMPTY:
        break;

      case EOC:
        break;
    }
    
  }

  *args->threadStateIndex = JOINABLE;
  if(commandLine == EOC)
    state = FINISHED;
  
  return NULL;

}





int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  int sonCount = 0;
  int const MAX_PROC = atoi(argv[2]);
  int const MAX_THREADS = atoi(argv[3]);
  //pthread_mutex_t mutex;
  pthread_mutex_t mutex_get_next;
  pthread_mutex_t mutex_c_l;
  pthread_mutex_t mutex_s_l;
  pthread_rwlock_t rwlock;
  pthread_rwlock_t rd_lock;
  
  if (argc > 1) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }
  DIR *jobsDirectory = opendir(argv[1]);
  if (jobsDirectory == NULL) {
    fprintf(stderr, "Error opening directory '%s'\n", argv[2]);
    return 1;  
  }

  pid_t pid;
  int status;
  int process_id;
  

  struct dirent *entry;
  while ((entry = readdir(jobsDirectory)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 &&
        strstr(entry->d_name, ".jobs") != NULL) {
        if(sonCount > MAX_PROC){
          process_id = wait(&status);
          sonCount--;
          fprintf(stdout, "ID -> %d\nChild process exited with status: %d\n", process_id, WEXITSTATUS(status));
        }
        pid = fork();
        if(pid < 0){
          fprintf(stderr, "Fork failed\n");
          exit(EXIT_FAILURE);
        }
        else if(pid > 0){
          sonCount++;
          continue;
        }
    }
    else
      continue;


    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

    char *fileName = entry -> d_name;
    char *path = (char *)malloc(strlen("../jobs/")+strlen(fileName)); 
    strcpy(path, "jobs/");
    strcat(path, fileName);
    int fd = open(path, O_RDONLY);
    free(path);
    
    fileName[strlen(fileName) - strlen(".jobs")] = '\0';
    char *outPath = (char *)malloc(strlen("../jobs/")+strlen(".out") +strlen(fileName)); 
    strcpy(outPath, "jobs/");
    strcat(outPath, fileName);
    strcat(outPath, ".out");
    int outFile = open(outPath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    free(outPath);

    CommandArgs threadVector[MAX_THREADS];
    int threadState[MAX_THREADS];
    /*int threadCount;
    CommandArgs thread[MAX_THREADS];
    int joinVerify[MAX_THREADS];
    */

    if (pthread_rwlock_init(&rwlock, NULL) != 0) { 
        fprintf(stderr,"Mutex init has failed\n"); 
        return 1; 
    }

    if (pthread_mutex_init(&mutex_get_next, NULL) != 0) { 
        fprintf(stderr,"Mutex init has failed\n"); 
        return 1; 
    }

    if (pthread_mutex_init(&mutex_c_l, NULL) != 0) {
        fprintf(stderr,"Mutex init has failed\n"); 
        return 1;
    }

    if (pthread_mutex_init(&mutex_s_l, NULL) != 0) {
        fprintf(stderr,"Mutex init has failed\n"); 
        return 1;
    }

    if (pthread_rwlock_init(&rd_lock, NULL) != 0) {
        fprintf(stderr,"Rwlock init has failed\n"); 
        return 1; 
    }

    create_threads(MAX_THREADS, fd, outFile, xs, ys, threadState, threadVector, &mutex_get_next,
                    &mutex_c_l, &mutex_s_l, &rwlock, &rd_lock);
    /*for(int i = 0; i < MAX_THREADS; i++)
      threadState[i] = AVAILABLE;

    for(int i = 0; i < MAX_THREADS; i++) {
      threadVector[i].fd = fd;
      threadVector[i].outFile = outFile;
      threadVector[i].xs = xs;
      threadVector[i].ys = ys;
      threadVector[i].threadStateIndex = &threadState[i];
      threadVector[i].mutex_get_next = &mutex_get_next;
      threadVector[i].rwlock = &rwlock;
      threadVector[i].rd_lock = &rd_lock;
      pthread_create(&threadVector[i].thread_id, NULL, chooseCommand, (void*)&threadVector[i]);
    }    */


    while(1) {
      if(state == BARRIER) { 
        join_threads(MAX_THREADS, threadState, threadVector);
          //dar join e colocar o state em NO_BARRIER; voltar a criar
        state = NO_BARRIER;
        create_threads(MAX_THREADS, fd, outFile, xs, ys, threadState, threadVector, &mutex_get_next,
                    &mutex_c_l, &mutex_s_l, &rwlock, &rd_lock);
      }

      else if(state == FINISHED) { //dar join e dar break no ciclo
        join_threads(MAX_THREADS, threadState, threadVector);
        break;
      }



      //se o state for NO_BARRIER, continua só
        
    }

    /*int activeThreads = MAX_THREADS;

    while(activeThreads > 0) {
      for(int i = 0; i < MAX_THREADS; i++){
        if(threadState[i] == JOINABLE) {
          printf("\tDeu join a %d\n", i);
          if (pthread_join(threadVector[i].thread_id, NULL) != 0)
            fprintf(stderr, "Error joining thread.\n");
          threadState[i] = FINISHED;
          activeThreads--;
        }
      }
    }*/


    /*for(int i = 0; i < ; i++) {
      if (threadState[i] == AVAILABLE || threadState[i] == JOINABLE) {
        printf("criou thread %d\n", i);
        threadVector[i].fd = fd;
        threadVector[i].outFile = outFile;
        for (int j = 0; j < MAX_RESERVATION_SIZE; j++) {
          threadVector[i].xs[j] = xs[j];
          threadVector[i].ys[j] = ys[j];
        }
        threadVector[i].threadStateIndex = &threadState[i];
        pthread_create(&threadVector[i].thread_id, NULL, chooseCommand, (void*)&threadVector[i]);
      }
    }

    while(retorno thread != alguma coisa){
    */




   /* for (int i = 0; i < MAX_THREADS; i++) {
      if(threadState[i] == UNAVAILABLE)
        i = 1;
      else if(threadState[i] == JOINABLE) {
        printf("ola %d\n", i);
        threadState[i] = AVAILABLE;
        if(pthread_join(threadVector[i].thread_id, NULL) != 0)
          fprintf(stderr, "Error joining thread.\n");
      }
    }*/
    /*
    while((a = get_next(fd)) != EOC) {
      
      int index = thread[threadCount].threadIndex;

      thread[index].a = a;
      thread[index].delay = delay;
      thread[index].event_id = event_id;
      thread[index].fd = fd;
      thread[index].num_columns = num_columns;
      thread[index].num_rows = num_rows;
      thread[index].outFile = outFile;
      
      for(int j = 0; j < MAX_RESERVATION_SIZE; j++) {
        thread[index].xs[j] = xs[j];
        thread[index].ys[j] = ys[j];
      }
      
      pthread_create(&thread[index].thread_id, NULL, chooseCommand, (void*)(thread + i));
      threadCount++;
      joinVerify[index] = 1;
      

      while(threadCount == MAX_THREADS) {
        for(int f = 0; f < MAX_THREADS; f++) {
          if (joinVerify[f]) {
            if(pthread_join(&thread[f].thread_id, NULL) != 0)
              fprintf(stderr, "error joining thread.\n");
            thread[f].threadIndex = f;
            threadCount--;
            joinVerify[f] = 0;
            break;
          }
        }
      }*/

        /*
        for(int f = 0; f < MAX_THREADS; f++) {
          if (joinThreads[f]) {
            if(pthread_join(&thread[f].thread_id, NULL) != 0)
              fprintf(stderr, "error joining thread.\n");
            thread[f].threadIndex;
            i--;
            joinThreads[f] = 0;
          }       
        } 
    int f = 0;
    //mudar para while (tem que se esperar que todas as threads acabem)
    while(f <= ){
      if (joinThreads[f]) {
        if(pthread_join(&thread[f].thread_id, NULL) != 0)
          fprintf(stderr, "error joining thread.\n");
        joinThreads[f] = 0;
        f++;
      }
    }*/

    

    pthread_rwlock_destroy(&rd_lock);
    pthread_mutex_destroy(&mutex_get_next);
    pthread_rwlock_destroy(&rwlock);
    
    close(fd);
    close(outFile);
    exit(EXIT_SUCCESS);
  }
  while(sonCount != 0){
    process_id = wait(&status);
    fprintf(stdout, "ID -> %d\nChild process exited with status: %d\n", process_id, WEXITSTATUS(status));
    sonCount --;
  }
  closedir(jobsDirectory);
  ems_terminate();
  return 0;   
}