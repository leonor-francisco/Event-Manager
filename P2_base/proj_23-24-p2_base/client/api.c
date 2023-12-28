#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "constants.h"
#include "api.h"

int session_id = -1;


int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
    int len;
    mkfifo(*req_pipe_path, 0777);
    mkfifo(*resp_pipe_path, 0777);
    char req_pipe[MAX_PIPE_NAME];
    char resp_pipe[MAX_PIPE_NAME];

    memcpy(req_pipe, *req_pipe_path, sizeof(char) * strlen(*req_pipe_path));
    len = strlen(req_pipe);
    if(len < MAX_PIPE_NAME) {
      for(int i = len; i < MAX_PIPE_NAME; i++)
        req_pipe[i] = '\0';
    }
    memcpy(resp_pipe, *req_pipe_path, sizeof(char) * strlen(*resp_pipe_path));
    len = strlen(resp_pipe);
    if(len < MAX_PIPE_NAME) {
      for(int i = len; i < MAX_PIPE_NAME; i++)
        resp_pipe[i] = '\0';
    }

    int fd_server = open(*server_pipe_path, O_WRONLY);
    write(fd_server, req_pipe, sizeof(char) * MAX_PIPE_NAME);
    write(fd_server, resp_pipe, sizeof(char) * MAX_PIPE_NAME);

    close(fd_server);
    
    int fd_resp = open(*resp_pipe_path, O_RDONLY);
    read(fd_resp, session_id, sizeof(int));

    close(fd_resp);



  //...TODO -> talvez falte algo




  //TODO: create pipes and connect to the server
  return 1;
}

int ems_quit(void) { 
  //TODO: close pipes
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  



  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
