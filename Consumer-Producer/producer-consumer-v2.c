#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/random.h>
#include <semaphore.h>
#include <pthread.h>
#include "buffer.h"

_Thread_local struct _th_args {
    unsigned int seed;
    unsigned int t_sleep;
    unsigned int count;
} th_args;

sem_t empty, full;
pthread_mutex_t mutex;

int insert_item(const buffer_item *item, unsigned int *count) {
    sem_wait(&empty);

    pthread_mutex_lock(&mutex);
    buffer[*count % BUFFER_SIZE] = *item;
    (*count)++;
    pthread_mutex_unlock(&mutex);

    sem_post(&full);
    return 0;
}

int remove_item(unsigned int *count) {
    buffer_item item = -1;

    sem_wait(&full);

    pthread_mutex_lock(&mutex);
    if (*count > 0) {
        item = buffer[*count % BUFFER_SIZE];
        (*count)--;
    }
    pthread_mutex_unlock(&mutex);

    sem_post(&empty);
    return item;
}


void th_prod_entry(void *args) {

    buffer_item item = 0;

    struct _th_args *th_args_ = (struct _th_args *)args;

    while (1) {

        /* generate random number */
        item = (buffer_item)rand_r((unsigned int*)&(th_args_->seed));

        /* call insert_item and attempt to insert random number */
        if (insert_item(&item, &th_args_->count) < 0) perror("Error inserting item");
        else printf("producer produced %d\n", item);
    }
}

void th_cons_entry(void *args) {

    struct _th_args *th_args_ = (struct _th_args *)args;

    buffer_item item = -1;

    while (1) {

        item = remove_item(&th_args_->count);

        /* call remove_item and retrieve item from buffer */
        if (item < 0) perror("Error removing item");
        else printf("consumer consumed %d\n", item);
    }
}


int main (int argc, char **argv) {

    /*
     *
     * 1. get command line args: agv[1], argv[2], argv[3]
     * 2. initialize buffer, mutex, semaphores, and other global vars
     * 3. create producer thread(s)
     * 4. create consumer thread(s)
     * 5. sleep
     * 6. release resources, e.g. destroy mutex and semaphores
     * 7. exit
     *
     */

    /* 1.
     *  1.1 Check to see if enough arguments were supplied
     *  1.2 Parse arguments into integers
     */

    /* 1.1 */
    if (argc != 4) {
        perror("args count should be exactly 4\nnote: this includes the program name\n\n");
        exit(1);
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    /* 1.2 */
    unsigned int n_producers = 0,
                 n_consumers = 0;

    th_args.t_sleep = (unsigned int)strtol(argv[1], NULL, 10);
    n_producers = (unsigned int)strtol(argv[2], NULL, 10);
    n_consumers = (unsigned int)strtol(argv[3], NULL, 10);

    /* 2.
     *  2.1 Generate random numbers
     *  * MacOS: getentropy(void* buffer, size_t buffer_length)
     *  * POSIX: getrandom(void* buffer, size_t buffer_length, unsigned int flags)
     *  2.2 Initialize buffer, mutex, and semaphores
     *  2.3 Initialize thread attribute object
     */

    /* 2.1 */
    if (getentropy((void*)(&th_args.seed), sizeof(unsigned int)) < 0) {
        perror("error returned from getentropy()");
        exit(1);
    }

    /* 2.2 */
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, BUFFER_SIZE);
    pthread_mutex_init(&mutex, NULL);

    /* 2.3 */
    pthread_attr_t th_attr;
    pthread_attr_init(&th_attr);

    /* 3. Create threads */
    pthread_t th_id[n_consumers + n_producers];

    th_args.count = 0;
    unsigned int i = 0;
    for (i = 0 ; i < n_producers; i++) {
        if (pthread_create(&th_id[i], &th_attr, (void*)&th_prod_entry, (void *)&th_args) < 0) {
            perror("Error creating thread");
            exit(-1);
        }
    }

    for ( ; i < (n_consumers + n_consumers); i++) {
        if (pthread_create(&th_id[i], &th_attr, (void*)&th_cons_entry, (void *)&th_args) < 0) {
            perror("Error creating thread");
            exit(-1);
        }
    }

    /* 4. Wait */
    sleep(th_args.t_sleep);

    /* lock mutex to stop producer/consumer action */
    pthread_mutex_lock(&mutex);

    /* destroy semaphores and mutex */
    sem_destroy(&full);
    sem_destroy(&empty);
    pthread_mutex_destroy(&mutex);

    /* exit */
    return 0;
}
