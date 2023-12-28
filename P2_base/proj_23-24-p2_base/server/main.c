#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

#include "threads.h"
#include "constants.h"
#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

//pthread_t sessions[MAX_SESSION_COUNT];



void executeCommands (char req_pipe[], char resp_pipe[], int session_id) {
  int fd_responses = open(resp_pipe, O_WRONLY);
  write(fd_responses, &session_id, sizeof(session_id));
  int fd_requests = open(req_pipe, O_RDONLY);

  char buffer;
  while(1) {
    if(read(fd_requests, buffer, sizeof(buffer)) == 0)
      //close thread
      ;
    switch (buffer)
    {
    case 'C':
      unsigned int event_id;
      size_t num_rows;
      size_t num_cols;
      read(fd_requests, event_id, sizeof(event_id));
      read(fd_requests, num_rows, sizeof(num_rows));
      read(fd_requests, num_cols, sizeof(num_cols));
      int return_value = ems_create(event_id, num_rows, num_cols);
      write(fd_responses, return_value, sizeof(return_value));
      break;

    case 'R':
      unsigned int event_id;
      size_t num_seats;
      size_t* xs;
      size_t* ys;
      read(fd_requests, &event_id, sizeof(event_id));
      read(fd_requests, &num_seats, sizeof(num_seats));
      read(fd_requests, xs, sizeof(*xs));
      read(fd_requests, ys, sizeof(*ys));
      int return_value = ems_reserve(event_id, num_seats, xs, ys);
      write(fd_responses, return_value, sizeof(return_value));
      //fazer reserve;
      break;
    
    case 'S':
      int out_fd;
      unsigned int event_id;
      read(fd_requests, &event_id, sizeof(event_id));
      read(fd_requests, &out_fd, sizeof(out_fd));
      size_t num_rows;
      size_t num_cols;
      
      int return_value = ems_show(out_fd, event_id, &num_rows, &num_cols);

      unsigned int seats = (unsigned int) num_rows * num_cols;
      write(fd_responses, return_value, sizeof(return_value));
      write(fd_responses, num_rows, sizeof(num_rows));
      write(fd_responses, num_cols, sizeof(num_cols));
      write(fd_responses, seats)

      //fazer show;
      break;

    case 'L':
      //fazer list;
      break;
  }



  
}














int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  mkfifo(argv[1], 0777);
  int fd_server = open(argv[1], O_RDONLY);
  

  

  //TODO: Intialize server, create worker threads

  while (1) {
    char req_pipe[MAX_PIPE_NAME];
    char resp_pipe[MAX_PIPE_NAME];
    read(fd_server, req_pipe, MAX_PIPE_NAME - 1);
    read(fd_server, resp_pipe, MAX_PIPE_NAME - 1);

    executeCommands(req_pipe, resp_pipe, 00);




    //TODO: Read from pipe
    //TODO: Write new client to the producer-consumer buffer
  }

  //TODO: Close Server

  ems_terminate();
}