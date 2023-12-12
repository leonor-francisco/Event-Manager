#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include <linux/limits.h>


typedef struct commandArgs {
  pthread_t thread_id;
  int commandLine;
  int fd;
  int outFile;
  unsigned int *event_id;
  size_t *num_rows;
  size_t *num_columns;
  size_t xs[MAX_RESERVATION_SIZE];
  size_t ys[MAX_RESERVATION_SIZE];
  unsigned int *delay;
  pthread_mutex_t mutex;
} CommandArgs;


void *chooseCommand(void *commandArgs);

void *chooseCommand(void *commandArgs) {
  CommandArgs *args = (CommandArgs*) commandArgs;
  switch (args -> commandLine ) {
          case CMD_CREATE:
          pthread_mutex_lock(&args->mutex); //LOCK
            int create = parse_create(args -> fd, args -> event_id, args -> num_rows, args -> num_columns);
          pthread_mutex_unlock(&args->mutex); //UNLOCK

            if (create != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;
            }

            if (ems_create(*args -> event_id, * args -> num_rows, * args -> num_columns)) {
              fprintf(stderr, "Failed to create event\n");
            }

            break;

          case CMD_RESERVE:
            pthread_mutex_lock(&args->mutex); //LOCK
            size_t num_coords = parse_reserve(args -> fd, MAX_RESERVATION_SIZE, args -> event_id, args -> xs, args -> ys);
            pthread_mutex_unlock(&args->mutex); //UNLOCK

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;
            }

            if (ems_reserve(*args -> event_id, &num_coords, args -> xs, args -> ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
            }

            break;

          case CMD_SHOW:
            pthread_mutex_lock(&args->mutex); //LOCK
            int show = parse_show(args -> fd, args -> event_id);
            pthread_mutex_unlock(&args->mutex); //UNLOCK

            if (show != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;
            }

            pthread_mutex_lock(&args->mutex); //LOCK
            if (ems_show(*args -> event_id, args -> outFile)) {
              fprintf(stderr, "Failed to show event\n");
            }
            pthread_mutex_unlock(&args->mutex); //UNLOCK

            break;

          case CMD_LIST_EVENTS:
            pthread_mutex_lock(&args->mutex); //LOCK

            if (ems_list_events(args -> outFile)) {
              fprintf(stderr, "Failed to list events\n");
            }
            pthread_mutex_unlock(&args->mutex); //UNLOCK

            break;

          case CMD_WAIT:
            if (parse_wait(args -> fd, args -> delay, NULL) == -1) {  // thread_id is not implemented
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;
            }
            
            if (*args -> delay > 0) {
              write(args -> outFile, "Waiting...\n", sizeof("Waiting...\n"));
              ems_wait(*args -> delay);
            }

            break;

          case CMD_INVALID:
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            break;

          case CMD_HELP:
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

          case CMD_BARRIER:  // Not implemented
          case CMD_EMPTY:
            break;

          case EOC:
            ems_terminate();
            break;
        }
  

    
  return NULL;
}


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  int sonCount = 0;
  int const MAX_PROC = atoi(argv[3]);
  int const MAX_THREADS = atoi(argv[4]);
  pthread_mutex_t mutex;
  
  if (argc > 1) {
    char *endptr;
    unsigned long int delay = strtoul(argv[1], &endptr, 10);

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
  DIR *jobsDirectory = opendir(argv[2]);
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


    
    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
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
    int outFile = open(outPath, O_WRONLY | O_CREAT | O_TRUNC);
    free(outPath);

    int commandLine;
    CommandArgs threadVector[MAX_THREADS];
    int threadState[MAX_THREADS];
    /*int threadCount;
    CommandArgs thread[MAX_THREADS];
    int joinVerify[MAX_THREADS];
    */
    for(int i = 0; i < MAX_THREADS; i++)
      threadState[i] = AVAILABLE;

    if (pthread_mutex_init(&mutex, NULL) != 0) { 
        fprintf(stderr,"Mutex init has failed\n"); 
        return 1; 
    } 
    
    commandLine = get_next(fd);
    while(commandLine != EOC) {
      for(int i = 0; i < MAX_THREADS; i++) {
        if (threadState[i] == AVAILABLE) {
          threadVector[i].commandLine = commandLine;
          threadVector[i].delay = delay;
          threadVector[i].event_id = event_id;
          threadVector[i].fd = fd;
          threadVector[i].num_columns = num_columns;
          threadVector[i].num_rows = num_rows;
          threadVector[i].outFile = outFile;
          for (int j = 0; j < MAX_RESERVATION_SIZE; j++) {
            threadVector[i].xs[j] = xs[j];
            threadVector[i].ys[j] = ys[j];
          }

          pthread_create(&threadVector[i].thread_id, NULL, chooseCommand, (void*)(threadVector + i));
          threadState[i] = UNAVAILABLE;
          commandLine = get_next(fd);
          break;
        }

      }
    }
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
        } */
    int f = 0;
    //mudar para while (tem que se esperar que todas as threads acabem)
    /*while(f <= ){
      if (joinThreads[f]) {
        if(pthread_join(&thread[f].thread_id, NULL) != 0)
          fprintf(stderr, "error joining thread.\n");
        joinThreads[f] = 0;
        f++;
      }
    }*/
    pthread_mutex_destroy(&mutex);
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
  return 0;   
}