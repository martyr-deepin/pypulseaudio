#include <stdio.h>
#include <pthread.h>

#include "hello_loop.h"

static int m_loop = 1;

/* Python do not support unblocking multithread */
static void *m_hello_loop_cb(void *argv) 
{
    while (m_loop) {
        printf("DEBUG tick ... \n");
        sleep(1);
    }
}

int hello_loop_init() 
{
    pthread_t thread;

    printf("DEBUG hello_loop_init\n");
    pthread_create(&thread, NULL, m_hello_loop_cb, NULL);

    return 0;
}

void hello_loop_cleanup() 
{
    printf("DEBUG hello_loop_cleanup\n");
    m_loop = 0;
}
