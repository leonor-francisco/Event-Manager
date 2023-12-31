/*depois remover*/ 
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

int session_id;

int fd_response;
int fd_request;

char req_pipe[MAX_PIPE_NAME];
char resp_pipe[MAX_PIPE_NAME];
char server_pipe[MAX_PIPE_NAME];


int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  size_t len;
  mkfifo(req_pipe_path, 0777);
  mkfifo(resp_pipe_path, 0777);
  printf("criei os meus fifos\n");

  //define request_pipe name
  memcpy(req_pipe, req_pipe_path, sizeof(char) * strlen(req_pipe_path));
  len = strlen(req_pipe);
  if(len < MAX_PIPE_NAME) {
    for(size_t i = len; i < MAX_PIPE_NAME; i++)
      req_pipe[i] = '\0';
  }

  //define response_pipe name
  memcpy(resp_pipe, resp_pipe_path, sizeof(char) * strlen(resp_pipe_path));
  len = strlen(resp_pipe);
  if(len < MAX_PIPE_NAME) {
    for(size_t i = len; i < MAX_PIPE_NAME; i++)
      resp_pipe[i] = '\0';
  }
  
  //define server_pipe name
  memcpy(server_pipe, server_pipe_path, sizeof(char) * strlen(server_pipe_path));
  len = strlen(server_pipe);
  if(len < MAX_PIPE_NAME) {
    for(size_t i = len; i < MAX_PIPE_NAME; i++)
      server_pipe[i] = '\0';
  }

  printf("vou abrir o server_pipe\n");
  //open register pipe for client and write both request and response pipes
  printf("server_pipe: %s\n", server_pipe);
  int fd_server = open(server_pipe, O_WRONLY);
  printf("abri o pipe do server\n");
  char op_code = OP_CODE_SETUP;
  write(fd_server, &op_code, sizeof(op_code));
  write(fd_server, req_pipe, MAX_PIPE_NAME - 1);
  write(fd_server, resp_pipe, MAX_PIPE_NAME - 1);
  close(fd_server);
  
  //open response and request pipes
  printf("response_pipe -> %s\nrequest_pipe -> %s\n", resp_pipe, req_pipe);
  fd_response = open(resp_pipe, O_RDONLY);
  fd_request = open(req_pipe, O_WRONLY);

  //read from response pipe the session_id associated with the client
  read(fd_response, &session_id, sizeof(session_id));



  //...TODO -> talvez falte algo


  //TODO: create pipes and connect to the server
  return 0;
}

int ems_quit(void) { 
  char op_code = OP_CODE_QUIT;
  write(fd_request, &op_code, sizeof(op_code));
  close(fd_response);
  close(fd_request);
  unlink(req_pipe);
  unlink(resp_pipe);
  //TODO: close pipes
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char op_code = OP_CODE_CREATE;
  write(fd_request, &op_code, sizeof(op_code));
  write(fd_request, &event_id, sizeof(event_id));
  write(fd_request, &num_rows, sizeof(num_rows));
  write(fd_request, &num_cols, sizeof(num_cols));
  int response;
  read(fd_response, &response, sizeof(response));
  if(response) {return 1;}
  return 0;
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  char op_code = OP_CODE_RESERVE;
  size_t buffer[256];

  write(fd_request, &op_code, sizeof(op_code));
  write(fd_request, &event_id, sizeof(event_id));
  write(fd_request, &num_seats, sizeof(num_seats));

  memcpy(buffer, xs, sizeof(size_t) * num_seats);
  write(fd_request, buffer, sizeof(size_t) * num_seats);
  memcpy(buffer, ys, sizeof(size_t) * num_seats);
  write(fd_request, buffer, sizeof(size_t) * num_seats); 
  int response;
  read(fd_response, &response, sizeof(response));
  printf("faz read bem\n");
  if(response) {return 1;}
  return 0;


  // send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
}

int ems_show(int out_fd, unsigned int event_id) {
  char op_code = OP_CODE_SHOW;
  write(fd_request, &op_code, sizeof(op_code));
  write(fd_request, &event_id, sizeof(event_id));
  int response;
  read(fd_response, &response, sizeof(response));
  if(response) {return 1;}

  else {
    size_t num_rows;
    size_t num_cols;
    char buffer[16];
    read(fd_response, &num_rows, sizeof(num_rows));
    read(fd_response, &num_cols, sizeof(num_cols));
    unsigned int *seats = malloc(num_rows * num_cols * sizeof(unsigned int));
    read(fd_response, seats, num_rows * num_cols * sizeof(unsigned int));
    for(size_t i = 1; i <= num_rows; i++) {
      for(size_t j = 1; j <= num_cols; j++) {
        sprintf(buffer, "%u", seats[seat_index_client(num_rows, i, j)]);
        write(out_fd, buffer, strlen(buffer));
        memset(buffer, 0, 16);

        if(j < num_cols){
          write(out_fd, " ", 1);
        }
      }
      write(out_fd, "\n", 1);
    }
    free(seats);
  }
  return 0;

  
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
}

int ems_list_events(int out_fd) {
  char op_code = OP_CODE_LIST;
  write(fd_request, &op_code, sizeof(op_code));
  int response;
  read(fd_response, &response, sizeof(response));
  if(response) {
    return 1;
  }

  else {
    int num_events;
    read(fd_response, &num_events, sizeof(num_events));
    unsigned int *ids = malloc( (unsigned int) num_events * sizeof(unsigned int));
    if(num_events == 0) {
      write(out_fd, "No events\n", strlen("No events\n"));
    }
    else {
      read(fd_response, ids, sizeof(unsigned int) * (unsigned int) num_events);
      for(int i = 0; i < num_events; i++) {
        char buffer[16];
        sprintf(buffer, "%u", ids[i]);
        write(out_fd, "Event: ", strlen("Event: "));
        write(out_fd, buffer, strlen(buffer));
        write(out_fd, "\n", 1);
      }
    }
    free(ids);
  }

  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 0;
}