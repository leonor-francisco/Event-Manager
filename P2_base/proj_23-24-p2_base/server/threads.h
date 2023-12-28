#ifndef THREADS_H
#define THREADS_H

typedef struct thread {
    char *req_pipe;
    char *resp_pipe;
    int session_id;
}
Worker_thread;

#endif