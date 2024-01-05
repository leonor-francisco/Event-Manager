#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "threads.h"
#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

pthread_mutex_t cond_mutex;
pthread_cond_t readFromQueue_cond;

pthread_mutex_t queueFull_mutex;
pthread_cond_t queueFull_cond;

struct Producer_consumer_queue *buffer_PCQ;

int server_state;

int signal_state = 0;

static void sig_handler(int sig) {
  if(sig == SIGUSR1) {
    if(signal(SIGUSR1, sig_handler) == SIG_ERR) {
      return;
    } 
    signal_state = 1;    
  }
}

void *executeCommands (void *thread) {
  Worker_thread *args = (Worker_thread*) thread;
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &mask, NULL); 

  while(1) {
    int loopCommands = 1;
    int session_id;
    char resp_pipe[MAX_PIPE_NAME];
    char req_pipe[MAX_PIPE_NAME];
    int fd_responses;
    int fd_requests;

    if(pthread_mutex_lock(&cond_mutex) != 0) {
      fprintf(stderr, "Error locking condition mutex\n");
      exit(EXIT_FAILURE);
    }


    while(buffer_PCQ->tail == NULL) {
        pthread_cond_wait(&readFromQueue_cond, &cond_mutex);
      
    }

    strcpy(req_pipe, buffer_PCQ->head->buffer_req);
    strcpy(resp_pipe, buffer_PCQ->head->buffer_resp);
    session_id = args->session_id;

    fd_responses = open(resp_pipe, O_WRONLY);
    if(fd_responses < 0) {
      fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno)); 
      exit(EXIT_FAILURE);
    }

    fd_requests = open(req_pipe, O_RDONLY);
    if(fd_requests < 0) {
      fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    if(write(fd_responses, &session_id, sizeof(session_id)) < 0) {
      fprintf(stderr, "Failed to write to response pipe\n");
      exit(EXIT_FAILURE);
    }

    deleteAtHead(buffer_PCQ);

    pthread_cond_signal(&queueFull_cond);


    pthread_mutex_unlock(&cond_mutex);

    while(loopCommands) {
      char op_code_buffer;
      unsigned int event_id;
      size_t num_rows, num_cols;
      size_t num_seats;
      size_t xs[MAX_RESERVATION_SIZE];
      size_t ys[MAX_RESERVATION_SIZE];
      int return_value;

      //for SHOW and LIST
      char *sec_buffer;
      unsigned int offset = 0; 

      if(read(fd_requests, &op_code_buffer, sizeof(op_code_buffer)) < 0) {
        fprintf(stderr, "Failed to read from request pipe\n");
        exit(EXIT_FAILURE);
      }
      switch (op_code_buffer) {

        case OP_CODE_CREATE:
          if(read(fd_requests, &session_id, sizeof(session_id)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if(read(fd_requests, &event_id, sizeof(event_id)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if(read(fd_requests, &num_rows, sizeof(num_rows)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if(read(fd_requests, &num_cols, sizeof(num_cols)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          return_value = ems_create(event_id, num_rows, num_cols);
          if(write(fd_responses, &return_value, sizeof(return_value)) < 0) {
            fprintf(stderr, "Failed to write to response pipe\n");
            exit(EXIT_FAILURE);
          }
          break;

        case OP_CODE_RESERVE:
          if(read(fd_requests, &session_id, sizeof(session_id)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if(read(fd_requests, &event_id, sizeof(event_id)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if(read(fd_requests, &num_seats, sizeof(num_seats)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if(read(fd_requests, xs, sizeof(size_t) * num_seats) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if(read(fd_requests, ys, sizeof(size_t) * num_seats) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          return_value = ems_reserve(event_id, num_seats, xs, ys);
          if(write(fd_responses, &return_value, sizeof(return_value)) < 0) {
            fprintf(stderr, "Failed to write to response pipe\n");
            exit(EXIT_FAILURE);
          }
          break;
        
        case OP_CODE_SHOW:
          if(read(fd_requests, &session_id, sizeof(session_id)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if(read(fd_requests, &event_id, sizeof(event_id)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          return_value = ems_show(event_id, &sec_buffer);
          if(return_value) {
            if(write(fd_responses, &return_value, sizeof(return_value)) < 0) {
              fprintf(stderr, "Failed to write to response pipe\n");
              exit(EXIT_FAILURE);
            }
          }
          else {
            size_t rows;
            size_t cols;

            memcpy(&rows, sec_buffer, sizeof(size_t));
            offset += sizeof(size_t);
            memcpy(&cols, sec_buffer + offset, sizeof(size_t));
            char buffer_show[sizeof(return_value) + sizeof(size_t) * 2 + rows * cols * sizeof(unsigned int)];
            offset = 0;
            memcpy(buffer_show, &return_value, sizeof(return_value));
            offset += sizeof(return_value);
            memcpy(buffer_show + offset, sec_buffer, sizeof(size_t) * 2 + rows * cols * sizeof(unsigned int));

            if(write(fd_responses, buffer_show, sizeof(buffer_show)) < 0) {
              fprintf(stderr, "Failed to write to response pipe\n");
              exit(EXIT_FAILURE);
            }
            free(sec_buffer);               
          }
          break;

        case OP_CODE_LIST:
          if(read(fd_requests, &session_id, sizeof(session_id)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          return_value = ems_list_events(&sec_buffer);
          if(return_value) {
            if(write(fd_responses, &return_value, sizeof(return_value)) < 0) {
              fprintf(stderr, "Failed to write to response pipe\n");
              exit(EXIT_FAILURE);
            }
          }
          else {
            size_t num_events;

            memcpy(&num_events, sec_buffer, sizeof(size_t));
            char buffer_list[sizeof(return_value) + sizeof(num_events) + num_events * sizeof(unsigned int)];

            offset = 0;
            memcpy(buffer_list, &return_value, sizeof(return_value));
            offset += sizeof(return_value);

            memcpy(buffer_list + offset, sec_buffer, sizeof(num_events) + num_events * sizeof(unsigned int));

            if(write(fd_responses, buffer_list, sizeof(buffer_list)) < 0) {
              fprintf(stderr, "Failed to write to response pipe\n");
              exit(EXIT_FAILURE);
            }
            free(sec_buffer);

          }
          break;

        case OP_CODE_QUIT:
          if(read(fd_requests, &session_id, sizeof(session_id)) < 0) {
            fprintf(stderr, "Failed to read from request pipe\n");
            exit(EXIT_FAILURE);
          }
          close(*req_pipe);
          close(*resp_pipe);
          loopCommands = 0;
          if(server_state == SHUT_DOWN)
            return NULL;

      }   
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
    exit(EXIT_FAILURE);
  }

  if(pthread_mutex_init(&cond_mutex, NULL) != 0) {
    fprintf(stderr, "Failed to initialize mutex\n");
    exit(EXIT_FAILURE);
  }

  if(pthread_cond_init(&readFromQueue_cond, NULL) != 0) {
    fprintf(stderr, "Failed to initialize condition\n");
    exit(EXIT_FAILURE);
  }

  if(pthread_mutex_init(&queueFull_mutex, NULL) != 0) {
    fprintf(stderr, "Failed to initialize mutex\n");
    exit(EXIT_FAILURE);
  }

  if(pthread_cond_init(&queueFull_cond, NULL) != 0) {
    fprintf(stderr, "Failed to initialize condition\n");
    exit(EXIT_FAILURE);
  }

  buffer_PCQ = initializePCQ();
  Worker_thread threads[MAX_SESSION_COUNT];
  
  mkfifo(argv[1], 0777);
  int fd_server = open(argv[1], O_RDWR);

  if(fd_server < 0) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno)); 
    exit(EXIT_FAILURE);
  }

  create_threads(threads);
  server_state = NORMAL;

  if (signal(SIGUSR1, sig_handler) == SIG_ERR) {
    return 1;
  }
  while(1) {
    char req_pipe[MAX_PIPE_NAME];
    char resp_pipe[MAX_PIPE_NAME];
    char op_code;
    char buffer[MAX_PIPE_NAME * 2 + 1];

    if(signal_state) {
      sig_show();
      signal_state = 0;
    }
    if(pthread_mutex_lock(&queueFull_mutex) != 0) {
      fprintf(stderr, "Error locking condition mutex\n");
      exit(EXIT_FAILURE);
    }

    while(buffer_PCQ->event_counter >= MAX_QUEUE_SIZE) {
      pthread_cond_wait(&queueFull_cond, &queueFull_mutex);
    }

    pthread_mutex_unlock(&queueFull_mutex);

    ssize_t bytes_read = read(fd_server, buffer, sizeof(buffer)); 
    if (bytes_read == -1) {
      if (errno == EINTR) {
        continue;
      }
      else {
        server_state = SHUT_DOWN;
        break;
      }
    }

    op_code = buffer[0];
    memcpy(req_pipe, buffer + 1, sizeof(req_pipe));
    memcpy(resp_pipe, buffer + MAX_PIPE_NAME + 1, sizeof(resp_pipe));



    insertAtTail(buffer_PCQ, resp_pipe, req_pipe);
    pthread_cond_signal(&readFromQueue_cond);

  }

  deleteQueue(buffer_PCQ);
  free(buffer_PCQ);

  for(int i = 0; i < MAX_SESSION_COUNT; i++) {
    if(pthread_join(threads[i].thread, NULL) != 0) {
      fprintf(stderr, "Failed to join thread\n");
      exit(EXIT_FAILURE);
    }
  }
  if(pthread_mutex_destroy(&cond_mutex) < 0) {
    fprintf(stderr, "Failed to destroy mutex\n");
    exit(EXIT_FAILURE);
  }
  if(pthread_cond_destroy(&readFromQueue_cond) < 0) {
    fprintf(stderr, "Failed to destroy mutex\n");
    exit(EXIT_FAILURE);
  }
  if(pthread_mutex_destroy(&queueFull_mutex) < 0) {
    fprintf(stderr, "Failed to destroy mutex\n");
    exit(EXIT_FAILURE);
  }
  if(pthread_cond_destroy(&queueFull_cond) < 0) {
    fprintf(stderr, "Failed to destroy mutex\n");
    exit(EXIT_FAILURE);
  }
  if(close(fd_server) < 0) {
    fprintf(stderr, "Failed to close file descriptor\n");
    exit(EXIT_FAILURE);
  }
  if(unlink(argv[1]) < 0) {
    fprintf(stderr, "Failed to unlink pipe\n");
    exit(EXIT_FAILURE);
  }
  ems_terminate();
}