#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#include "threads.h"
#include "common/constants.h"
#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

//pthread_t sessions[MAX_SESSION_COUNT];

pthread_mutex_t OP_CODE_mutex = PTHREAD_MUTEX_INITIALIZER;

void executeCommands (char req_pipe[MAX_PIPE_NAME], char resp_pipe[MAX_PIPE_NAME], int session_id) {
  int fd_responses = open(resp_pipe, O_WRONLY);
  int fd_requests = open(req_pipe, O_RDONLY);
  write(fd_responses, &session_id, sizeof(session_id));

  while(1) {
    char op_code_buffer;
    unsigned int event_id;
    size_t num_rows, num_cols;
    size_t num_seats;
    size_t xs[MAX_RESERVATION_SIZE];
    size_t ys[MAX_RESERVATION_SIZE];
    int return_value;

    pthread_mutex_lock(&OP_CODE_mutex);
    read(fd_requests, &op_code_buffer, sizeof(op_code_buffer));
    printf("op_code -> %c\n", op_code_buffer);
    switch (op_code_buffer) {

      case OP_CODE_CREATE:
        
        read(fd_requests, &event_id, sizeof(event_id));
        read(fd_requests, &num_rows, sizeof(num_rows));
        read(fd_requests, &num_cols, sizeof(num_cols));
        pthread_mutex_unlock(&OP_CODE_mutex);
        return_value = ems_create(event_id, num_rows, num_cols);
        write(fd_responses, &return_value, sizeof(return_value));
        break;

      case OP_CODE_RESERVE:
        printf("vou fazer agora o reserve\n");
        read(fd_requests, &event_id, sizeof(event_id));
        printf("primeiro read -> %d\n", event_id);
        read(fd_requests, &num_seats, sizeof(num_seats));
        printf("r2 -> %ld\n", num_seats);
        read(fd_requests, xs, sizeof(size_t) * num_seats);
        printf("r3 -> %p\n", xs);
        read(fd_requests, ys, sizeof(size_t) * num_seats);
        printf("r4\n");
        pthread_mutex_unlock(&OP_CODE_mutex);
        printf("vou fazer agora o reserve\n");
        return_value = ems_reserve(event_id, num_seats, xs, ys);
        printf("vou devolver o valor de return %d\n", return_value);
        write(fd_responses, &return_value, sizeof(return_value));
        //fazer reserve;
        break;
      
      case OP_CODE_SHOW:
        read(fd_requests, &event_id, sizeof(event_id));
        pthread_mutex_unlock(&OP_CODE_mutex);
        ems_show(fd_responses, event_id);

        break;

      case OP_CODE_LIST:
        pthread_mutex_unlock(&OP_CODE_mutex);
        ems_list_events(fd_responses);
        break;

      case OP_CODE_QUIT:
        pthread_mutex_unlock(&OP_CODE_mutex);
        close(*req_pipe);
        close(*resp_pipe);
        return;
    }
    
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
  printf("criei fifo server\n");
  printf("server_pipe: %s\n", argv[1]);
  int fd_server = open(argv[1], O_RDONLY);
  printf("abri fifo do lado do server\n");
  

  

  //TODO: Intialize server, create worker threads
    while(1) {
    char req_pipe[MAX_PIPE_NAME];
    char resp_pipe[MAX_PIPE_NAME];
    char op_code;
    read(fd_server, &op_code, sizeof(op_code));
    read(fd_server, req_pipe, MAX_PIPE_NAME - 1);
    printf("pointer -> %p\n", req_pipe);
    read(fd_server, resp_pipe, MAX_PIPE_NAME - 1);

    executeCommands(req_pipe, resp_pipe, 00);
    break;
    }




    //TODO: Read from pipe
    //TODO: Write new client to the producer-consumer buffer

  //TODO: Close Server

  ems_terminate();
}