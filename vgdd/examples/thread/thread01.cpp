#include <pthread.h>

static void *Worker(void *)
{
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    return 0;
}

int main()
{
    pthread_t thread;
    return pthread_create(&thread, 0, Worker, 0);
}
