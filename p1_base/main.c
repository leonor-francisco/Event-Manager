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


void chooseCommand(int a, int fd, int outFile, unsigned int *event_id, size_t *num_rows, size_t *num_columns,
                    size_t xs[MAX_RESERVATION_SIZE], size_t ys[MAX_RESERVATION_SIZE],
                    size_t num_coords, unsigned int delay) {
  switch (a) {
          case CMD_CREATE:
            if (parse_create(fd, event_id, num_rows, num_columns) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              return;
            }

            if (ems_create(event_id, num_rows, num_columns)) {
              fprintf(stderr, "Failed to create event\n");
            }

            break;

          case CMD_RESERVE:
            num_coords = parse_reserve(fd, MAX_RESERVATION_SIZE, event_id, xs, ys);

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              return;
            }

            if (ems_reserve(event_id, num_coords, xs, ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
            }

            break;

          case CMD_SHOW:
            if (parse_show(fd, event_id) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              return;
            }
      
            if (ems_show(event_id, outFile)) {
              fprintf(stderr, "Failed to show event\n");
            }

            break;

          case CMD_LIST_EVENTS:
            if (ems_list_events(outFile)) {
              fprintf(stderr, "Failed to list events\n");
            }

            break;

          case CMD_WAIT:
            if (parse_wait(fd, delay, NULL) == -1) {  // thread_id is not implemented
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              return -1;
            }

            if (delay > 0) {
              write(outFile, "Waiting...\n", sizeof("Waiting...\n"));
              ems_wait(delay);
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
            return 0;
        }
  
}






int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  int sonCount = 0;
  int const MAX_PROC = atoi(argv[3]);
  int const MAX_THREADS = atoi(argv[4]);

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

    /*int thread_number = 1;
    char thread_buffer;*/
    while((a = get_next(fd)) != EOC) {

      /*sprintf(thread_buffer, "thread%d", thread_number);
      pthread_t pthread1;
      pthread_create(&pthread1, NULL, );*/

      chooseCommand(a, fd, outFile, &event_id, &num_rows, &num_columns, xs, ys, 
                    num_coords, delay);
        //thread_number++;
    }

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