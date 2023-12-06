#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include <linux/limits.h>

/*DIR *verifyDirectory(const char *path) {
    DIR *dir;
    dir = opendir(path);

    if (dir == NULL) {
        perror("Error opening directory");
        return NULL;
    }
    
    return dir;
}*/

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc > 1) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

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
    fprintf(stderr, "Error opening directory '%s'\n", argv[1]);
    return 1;  
  }


  struct dirent *entry;

  
  while ((entry = readdir(jobsDirectory)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    
    printf("2. %s\n", entry->d_name);
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
    

    
    while((a = get_next(fd)) != EOC) {
      switch (a) {
          case CMD_CREATE: {
            if (parse_create(fd, &event_id, &num_rows, &num_columns) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }
          }

            if (ems_create(event_id, num_rows, num_columns)) {
              fprintf(stderr, "Failed to create event\n");
            }

            break;

          case CMD_RESERVE:
            num_coords = parse_reserve(fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_reserve(event_id, num_coords, xs, ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
            }

            break;

          case CMD_SHOW:
            if (parse_show(fd, &event_id) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }
      
            if (ems_show(event_id, outFile)) {
              fprintf(stderr, "Failed to show event\n");
            }

            break;

          case CMD_LIST_EVENTS:
            if (ems_list_events()) {
              fprintf(stderr, "Failed to list events\n");
            }

            break;

          case CMD_WAIT:
            if (parse_wait(fd, &delay, NULL) == -1) {  // thread_id is not implemented
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
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
            write(outFile,
                "Available commands:\n"
                "  CREATE <event_id> <num_rows> <num_columns>\n"
                "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
                "  SHOW <event_id>\n"
                "  LIST\n"
                "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
                "  BARRIER\n"                      // Not implemented
                "  HELP\n", sizeof(
                  "Available commands:\n"
                "  CREATE <event_id> <num_rows> <num_columns>\n"
                "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
                "  SHOW <event_id>\n"
                "  LIST\n"
                "  WAIT <delay_ms> [thread_id]\n"  
                "  BARRIER\n"                      
                "  HELP\n"
                ));

            break;

          case CMD_BARRIER:  // Not implemented
          case CMD_EMPTY:
            break;

          case EOC:
            ems_terminate();
            return 0;
        }
    }

    close(fd);
    close(outFile);
  }
  closedir(jobsDirectory);   
}