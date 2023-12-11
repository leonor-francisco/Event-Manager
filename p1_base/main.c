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
  int threadIndex;

  int a;
  int fd;
  int outFile;
  unsigned int *event_id;
  size_t *num_rows;
  size_t *num_columns;
  size_t xs[MAX_RESERVATION_SIZE];
  size_t ys[MAX_RESERVATION_SIZE];
  unsigned int *delay;
} CommandArgs;


void *chooseCommand(void *commandArgs);

pthread_mutex_t mutex;





void *chooseCommand(void *commandArgs) {
  CommandArgs *args = (CommandArgs*) commandArgs;
  switch (args -> a ) {
          case CMD_CREATE:
            if (parse_create(args -> fd, args -> event_id, args -> num_rows, args -> num_columns) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;
            }
            pthread_mutex_lock(&mutex); //LOCK
            if (ems_create(*args -> event_id, * args -> num_rows, * args -> num_columns)) {
              fprintf(stderr, "Failed to create event\n");
            }
            pthread_mutex_unlock(&mutex); //UNLOCK

            break;

          case CMD_RESERVE:
            size_t num_coords = parse_reserve(args -> fd, MAX_RESERVATION_SIZE, args -> event_id, args -> xs, args -> ys);

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;
            }
            pthread_mutex_lock(&mutex); //LOCK
            if (ems_reserve(*args -> event_id, &num_coords, args -> xs, args -> ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
            }
            pthread_mutex_unlock(&mutex); //UNLOCK

            break;

          case CMD_SHOW:
            if (parse_show(args -> fd, args -> event_id) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;
            }
            pthread_mutex_lock(&mutex); //LOCK
            if (ems_show(*args -> event_id, args -> outFile)) {
              fprintf(stderr, "Failed to show event\n");
            }
            pthread_mutex_unlock(&mutex); //UNLOCK

            break;

          case CMD_LIST_EVENTS:
            pthread_mutex_lock(&mutex); //LOCK
            if (ems_list_events(args -> outFile)) {
              fprintf(stderr, "Failed to list events\n");
            }
            pthread_mutex_unlock(&mutex); //UNLOCK

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

  int joinThreads[MAX_THREADS];
  
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

    int a;
    int i = 0;
    CommandArgs thread[MAX_THREADS];

    pthread_mutex_init(&mutex, NULL);

    while((a = get_next(fd)) != EOC) {
      
      thread[i].a = a;
      thread[i].delay = delay;
      thread[i].event_id = event_id;
      thread[i].fd = fd;
      thread[i].num_columns = num_columns;
      thread[i].num_rows = num_rows;
      thread[i].outFile = outFile;
      
      for(int j = 0; j < MAX_RESERVATION_SIZE; j++) {
        thread[i].xs[j] = xs[j];
        thread[i].ys[j] = ys[j];
      }
      
      pthread_create(&thread[i].thread_id, NULL, chooseCommand, (void*)(thread + i));

      if(i > MAX_THREADS) {
        for(int f = 0; f < MAX_THREADS; f++) {
          if (joinThreads[f]) {
            if(pthread_join(&thread[f].thread_id, NULL) != 0)
              fprintf(stderr, "error joining thread.\n");
            thread[f].threadIndex;
            i--;
            joinThreads[f] = 0;
          }       
        } 
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
    }

    
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