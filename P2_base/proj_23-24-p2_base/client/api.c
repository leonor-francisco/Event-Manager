#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "common/constants.h"
#include "api.h"
#include "parser.h"
#include "common/io.h"

int session_id;

int fd_response;
int fd_request;

char req_pipe[MAX_PIPE_NAME];
char resp_pipe[MAX_PIPE_NAME];
char server_pipe[MAX_PIPE_NAME];


int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {  
  char buffer[MAX_PIPE_NAME * 2 + 1];
  unsigned int buffer_offset = 0;

  //define request_pipe and response_pipe name
  memcpy(req_pipe, req_pipe_path, sizeof(char) * strlen(req_pipe_path) + 1);
  memcpy(resp_pipe, resp_pipe_path, sizeof(char) * strlen(resp_pipe_path) + 1);

  //define server_pipe name
  strcpy(server_pipe, server_pipe_path);


  //open register pipe for client and write both request and response pipes
  int fd_server = open(server_pipe, O_WRONLY);
  if(fd_server < 0) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno)); 
    return 1;
  }

  char op_code = OP_CODE_SETUP;

  memset(req_pipe + strlen(req_pipe), '\0', sizeof(req_pipe) - strlen(req_pipe));
  memset(resp_pipe + strlen(resp_pipe), '\0', sizeof(resp_pipe) - strlen(resp_pipe));

  mkfifo(req_pipe, 0777);
  mkfifo(resp_pipe, 0777);

  memcpy(buffer, &op_code, sizeof(op_code));
  buffer_offset += sizeof(op_code);
  memcpy(buffer + buffer_offset, req_pipe, sizeof(req_pipe));
  buffer_offset += sizeof(req_pipe);
  memcpy(buffer + buffer_offset, resp_pipe, sizeof(resp_pipe));
  
  if(write(fd_server, buffer, sizeof(buffer)) < 0) {
    fprintf(stderr, "Failed to write to server pipe\n");
    return 1;
  }

  
  if(close(fd_server) < 0) {
    fprintf(stderr, "Failed to close file descriptor\n");
    return 1;
  }

  //open response pipe
  fd_response = open(resp_pipe, O_RDONLY);
  if(fd_response < 0) {
      fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno)); 
      return 1;
    }
  fd_request = open(req_pipe, O_WRONLY);
  if(fd_request < 0) {
      fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno)); 
      return 1;
    }

  //read from response pipe the session_id associated with the client
  if(read(fd_response, &session_id, sizeof(session_id)) < 0) {
    fprintf(stderr, "Failed to read from response pipe\n");
    return 1;
  }
  return 0;
}

int ems_quit(void) { 
  char op_code = OP_CODE_QUIT;
  unsigned int buffer_size = sizeof(op_code) + sizeof(session_id);
  char buffer[buffer_size];
  unsigned int buffer_offset = 0;
  memcpy(buffer, &op_code, sizeof(op_code));
  buffer_offset += sizeof(op_code);
  memcpy(buffer + buffer_offset, &session_id, sizeof(session_id));

  write(fd_request, buffer, sizeof(buffer));

  if(close(fd_response) < 0) {
    fprintf(stderr, "Failed to close file descriptor\n");
    return 1;
  }
  if(close(fd_request) < 0) {
    fprintf(stderr, "Failed to close file descriptor\n");
    return 1;
  }
  if(unlink(req_pipe) < 0) {
    fprintf(stderr, "Failed to unlink pipe\n");
    return 1;
  }
  if(unlink(resp_pipe) < 0) {
    fprintf(stderr, "Failed to unlink pipe\n");
    return 1;
  }
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char op_code = OP_CODE_CREATE;
  unsigned int buffer_size = sizeof(op_code) + sizeof(session_id) + sizeof(event_id) + sizeof(num_rows) + sizeof(num_cols);
  unsigned int buffer_offset = 0;
  char buffer[buffer_size];

  memcpy(buffer, &op_code, sizeof(op_code));
  buffer_offset += sizeof(op_code);
  memcpy(buffer + buffer_offset, &session_id, sizeof(session_id));
  buffer_offset += sizeof(session_id);
  memcpy(buffer + buffer_offset, &event_id, sizeof(event_id));
  buffer_offset += sizeof(event_id);
  memcpy(buffer + buffer_offset, &num_rows, sizeof(num_rows));
  buffer_offset += sizeof(num_rows);
  memcpy(buffer + buffer_offset, &num_rows, sizeof(num_rows));
  buffer_offset += sizeof(num_cols);
  memcpy(buffer + buffer_offset, &num_cols, sizeof(num_cols));

  if(write(fd_request, buffer, sizeof(buffer)) < 0) {
    fprintf(stderr, "Failed to write to request pipe\n");
    return 1;
  }
  int response;
  if(read(fd_response, &response, sizeof(response)) < 0) {
    fprintf(stderr, "Failed to read from response pipe\n");
    return 1;
  }
  if(response) {
    return 1;
  }
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  char op_code = OP_CODE_RESERVE;
  char buffer[sizeof(op_code) + sizeof(session_id) + sizeof(event_id) + 
                          sizeof(num_seats) + sizeof(size_t) * num_seats * 2];
  unsigned int buffer_offset = 0;

  memcpy(buffer, &op_code, sizeof(op_code));
  buffer_offset += sizeof(op_code);
  memcpy(buffer + buffer_offset, &session_id, sizeof(session_id));
  buffer_offset += sizeof(session_id);
  memcpy(buffer + buffer_offset, &event_id, sizeof(event_id));
  buffer_offset += sizeof(event_id);
  memcpy(buffer + buffer_offset, &num_seats, sizeof(num_seats));
  buffer_offset += sizeof(num_seats);
  memcpy(buffer + buffer_offset, xs, sizeof(size_t) * num_seats);
  buffer_offset += (sizeof(size_t) * num_seats);
  memcpy(buffer + buffer_offset, ys, sizeof(size_t) * num_seats);

  if(write(fd_request, buffer, sizeof(buffer)) < 0) {
    fprintf(stderr, "Failed to write to request pipe\n");
    return 1;
  }
  int response;
  if(read(fd_response, &response, sizeof(response)) < 0) {
    fprintf(stderr, "Failed to read from response pipe\n");
    return 1;
  }
  if(response) {
    return 1;
  }
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  char op_code = OP_CODE_SHOW;
  unsigned int buffer_size = sizeof(op_code) + sizeof(session_id) + sizeof(event_id);
  char buffer[buffer_size];
  unsigned int buffer_offset = 0;

  memcpy(buffer, &op_code, sizeof(op_code));
  buffer_offset += sizeof(op_code);
  memcpy(buffer + buffer_offset, &session_id, sizeof(session_id));
  buffer_offset += sizeof(session_id);
  memcpy(buffer + buffer_offset, &event_id, sizeof(event_id));

  if(write(fd_request, buffer, sizeof(buffer)) < 0) {
    fprintf(stderr, "Failed to write to request pipe\n");
    return 1;
  }

  int response;
  if(read(fd_response, &response, sizeof(response)) < 0) {
    fprintf(stderr, "Failed to read from response pipe\n");
    return 1;
  }
  if(response) {return 1;}

  else {
    size_t num_rows;
    size_t num_cols;
    if(read(fd_response, &num_rows, sizeof(size_t)) < 0) {
      fprintf(stderr, "Failed to read from response pipe\n");
      return 1;
    }
    if(read(fd_response, &num_cols, sizeof(size_t)) < 0) {
      fprintf(stderr, "Failed to read from response pipe\n");
      return 1;
    }
    unsigned int *seats = malloc(num_rows * num_cols * sizeof(unsigned int));
    if(read(fd_response, seats, num_cols * num_rows * sizeof(unsigned int)) < 0) {
      fprintf(stderr, "Failed to read from response pipe\n");
      return 1;
    }
    for(size_t i = 1; i <= num_rows; i++) {
      for(size_t j = 1; j <= num_cols; j++) {
        char buffer_write[16];
        sprintf(buffer_write, "%u", seats[seat_index_client(num_rows, i, j)]);
        if (print_str(out_fd, buffer_write)) {
        perror("Error writing to file descriptor");
        return 1;
        }

        if(j < num_cols){
          if (print_str(out_fd, " ")) {
          perror("Error writing to file descriptor");
          return 1;
          }
        }
      }
        if (print_str(out_fd, "\n")) {
        perror("Error writing to file descriptor");
        return 1;
        }
    }
    free(seats);
  }
  return 0;
}

int ems_list_events(int out_fd) {
  char op_code = OP_CODE_LIST;
  unsigned int buffer_size = sizeof(op_code) + sizeof(session_id);
  char buffer[buffer_size];
  unsigned int buffer_offset = 0;
  memcpy(buffer, &op_code, sizeof(op_code));
  buffer_offset += sizeof(op_code);
  memcpy(buffer + buffer_offset, &session_id, sizeof(session_id));

  if(write(fd_request, buffer, sizeof(buffer)) < 0) {
    fprintf(stderr, "Failed to write to request pipe\n");
    return 1;
  }
  int response;
  if(read(fd_response, &response, sizeof(response)) < 0) {
    fprintf(stderr, "Failed to read from response pipe\n");
    return 1;
  }
  if(response) {
    return 1;
  }

  else {
    size_t num_events;
    if(read(fd_response, &num_events, sizeof(num_events)) < 0) {
      fprintf(stderr, "Failed to read from response pipe\n");
      return 1;
    }
    unsigned int *ids = malloc( (unsigned int) num_events * sizeof(unsigned int));
    if(num_events == 0) {
      if (print_str(out_fd, "No events\n")) {
        perror("Error writing to file descriptor");
        return 1;
      }
    }
    else {
      if(read(fd_response, ids, sizeof(unsigned int) * (unsigned int) num_events) < 0) {
        fprintf(stderr, "Failed to read from response pipe\n");
        return 1;
      }
      for(size_t i = 0; i < num_events; i++) {
        char buffer_list[16];
        sprintf(buffer_list, "%u", ids[i]);
        if (print_str(out_fd, "Event: ")) {
          perror("Error writing to file descriptor");
          return 1;
        }
        if (print_str(out_fd, buffer_list)) {
          perror("Error writing to file descriptor");
          return 1;
        }
        if (print_str(out_fd, "\n")) {
        perror("Error writing to file descriptor");
        return 1;
        }
      }
    }
    free(ids);
  }
  return 0;
}