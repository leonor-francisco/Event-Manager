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
WaitCommand *waitVector;

/// @brief function that is called by the threads. It calls the parsers and the ems functions.
/// @param commandArgs 
void *chooseCommand(void *commandArgs) {
  CommandArgs *args = (CommandArgs*) commandArgs;
  unsigned int event_id, delay;
  size_t num_rows, num_columns, num_coords;
  int commandLine = 13;

  while(commandLine != EOC) {
    pthread_mutex_lock(args->mutex_get_next);
    pthread_mutex_lock(&waitVector[args->threadIndex].mutex_w);
    if (waitVector[args -> threadIndex].delay > 0) { //if a thread has a delay value to meet, its checked here

      pthread_mutex_unlock(args->mutex_get_next);

      fprintf(stdout,"Waiting...\n");

      ems_wait(waitVector[args -> threadIndex].delay); //makes the thread sleep
      waitVector[args -> threadIndex].delay = 0;
      pthread_mutex_unlock(&waitVector[args->threadIndex].mutex_w);
      continue;
    }
    pthread_mutex_unlock(&waitVector[args->threadIndex].mutex_w);
    
    if(state != NO_BARRIER) {
        pthread_mutex_unlock(args->mutex_get_next);
        break;
    }

    
    commandLine = get_next(args->fd);
    switch (commandLine){
      case CMD_CREATE:
        int create = parse_create(args -> fd, &event_id, &num_rows, &num_columns);
        pthread_mutex_unlock(args->mutex_get_next);

        if (create != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;
        }
        pthread_rwlock_wrlock(args->rd_lock);

        pthread_mutex_lock(args->mutex_c_l);
        
        if (ems_create(event_id, num_rows, num_columns)) {
          fprintf(stderr, "Failed to create event\n");
        }

        pthread_mutex_unlock(args->mutex_c_l);

        pthread_rwlock_unlock(args->rd_lock);

        break;

      case CMD_RESERVE:
        num_coords = parse_reserve(args -> fd, MAX_RESERVATION_SIZE, &event_id, args -> xs, args -> ys);
        pthread_mutex_unlock(args->mutex_get_next);

        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;
        }
        pthread_rwlock_rdlock(args->rd_lock);

        bubbleSort(args->xs,args->ys,num_coords);

        if (ems_reserve(event_id, num_coords, args -> xs, args -> ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }

        pthread_rwlock_unlock(args->rd_lock);
        
        break;


      case CMD_SHOW:
        int show = parse_show(args -> fd, &event_id);
        pthread_mutex_unlock(args->mutex_get_next);

        if (show != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;
        }

        pthread_mutex_lock(args->mutex_s_l);

        if (ems_show(event_id, args -> outFile)) {
          fprintf(stderr, "Failed to show event\n");
        }

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

      case CMD_WAIT:
        unsigned int thread_id = 0;
        int wait = parse_wait(args -> fd, &delay, &thread_id);
         
        if (wait == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          pthread_mutex_unlock(&waitVector[args->threadIndex].mutex_w);
          pthread_mutex_unlock(args->mutex_get_next);
          break;
        }
        if(thread_id == 0  && delay > 0) {
          for(int i = 0; i < args->num_threads; i++) {
            pthread_mutex_lock(&waitVector[i].mutex_w);
            waitVector[i].delay = delay;  
            pthread_mutex_unlock(&waitVector[i].mutex_w);
          }

        }
        else if(delay > 0) {
          
          pthread_mutex_lock(&waitVector[args->threadIndex].mutex_w);
          waitVector[thread_id - 1].delay = delay;
          pthread_mutex_unlock(&waitVector[args->threadIndex].mutex_w);
        }

        pthread_mutex_unlock(args->mutex_get_next);
        break;


      case CMD_INVALID: //FAZER LOCKS

        pthread_mutex_unlock(args->mutex_get_next);
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP: //FAZER LOCKS

        pthread_mutex_unlock(args->mutex_get_next);
        fprintf(stdout,
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
        return (void*) BARRIER;

      case CMD_EMPTY:
        pthread_mutex_unlock(args->mutex_get_next);
        break;

      case EOC:
        state = FINISHED;
        pthread_mutex_unlock(args->mutex_get_next);
        break;
    }
  }

  return (void*) FINISHED;

}


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  int sonCount = 0;
  int const MAX_PROC = atoi(argv[2]);
  int const MAX_THREADS = atoi(argv[3]);
  pthread_mutex_t mutex_get_next;
  pthread_mutex_t mutex_c_l;
  pthread_mutex_t mutex_s_l;
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

    waitVector =(WaitCommand*)malloc(sizeof(WaitCommand) * (long unsigned int) MAX_THREADS);

    for(int i = 0; i < MAX_THREADS; i++) {
      waitVector[i].delay = 0;
      if (pthread_mutex_init(&waitVector[i].mutex_w, NULL) != 0) { 
        fprintf(stderr,"Mutex init has failed\n"); 
        return 1; 
      }
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


    while(state != FINISHED) {
      state = NO_BARRIER;
      create_threads(MAX_THREADS, fd, outFile, threadVector, &mutex_get_next,
                    &mutex_c_l, &mutex_s_l, &rd_lock);
      int returnValue;
      int flag_barrier = 0;
      for(int i = 0; i < MAX_THREADS; i++) {
        if (pthread_join(threadVector[i].thread_id, (void**)&returnValue) != 0)
            fprintf(stderr, "Error joining thread.\n");
        if(returnValue == BARRIER) {
            state = BARRIER;
            flag_barrier = 1;
        }
        else if(returnValue == FINISHED && flag_barrier != 1)
            state = FINISHED;
      }
    }
    
    for(int i = 0; i < MAX_THREADS; i++) {
      pthread_mutex_destroy(&waitVector[i].mutex_w);
    }

    free(waitVector);

    pthread_rwlock_destroy(&rd_lock);
    pthread_mutex_destroy(&mutex_get_next);
    pthread_mutex_destroy(&mutex_c_l);
    pthread_mutex_destroy(&mutex_s_l);
    
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