#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h> // sleep()
#include "buffer.h"

// global buffer
// buffer_item and BUFFER_SIZE defined in buffer.h
buffer_item buffer[BUFFER_SIZE];
// global index used in insert_item and remove_item
int idx = 0;
int insert_item(buffer_item);
int remove_item(buffer_item*);

/* global mutex and semaphores */
pthread_mutex_t mutex;
sem_t empty, full;

/* consumer thread entry */
void* consumer(void* param) {
    // rand_r seed
    unsigned seed = time(NULL);

    do {
        // will sleep for some time between 1 and 10 seconds
        sleep(rand_r(&seed) % 10 + 1);

        /* wait for full to be greater than 0 and acquire lock */
        sem_wait(&full);
        pthread_mutex_lock(&mutex);

        /* critical section */
        buffer_item item;
        int state = remove_item(&item);
        if (state < 0) printf("Error removing item from buffer...\n");

        /* release lock and increment empty counter */
        pthread_mutex_unlock(&mutex);
        sem_post(&empty);

    } while(1);
}

/* producer thread entry */
void* producer(void* param) {
    // rand_r seed
    unsigned seed = time(NULL);

    do {
        // will sleep for some time between 1 and 10 seconds
        sleep(rand_r(&seed) % 10 + 1);

        /* wait for empty to be greater than 0 and acquire lock */
        sem_wait(&empty);
        pthread_mutex_lock(&mutex);

        /* critical section */
        buffer_item rand = (buffer_item)rand_r(&seed);
        int state = insert_item(rand);
        if (state < 0) printf("Error inserting item into buffer...\n");

        /* release lock and increment full counter */
        pthread_mutex_unlock(&mutex);
        sem_post(&full);

    } while (1);
}

int main(int argc, char* argv[]) {

    /* usage statment if argument count not satisfied */
    if (argc != 4) {
        printf("USAGE: ./lab2 [SLEEP TIME] [NUMBER OF PRODUCER THREADS] [NUMBER OF CONSUMER THREADS]\n");
        exit(0);
    }

    /* get args from cmd line input */
    int sleepTime  = atoi(argv[1]);
    int nConsumers = atoi(argv[2]);
    int nProducers = atoi(argv[3]);

    /* input validation */
    if (nConsumers <= 0 || nProducers <= 0 || sleepTime <= 0) {
        printf("Please only use integers larger than 0. Exiting...\n");
        exit(0);
    }

    /* declare thread id arrays and attributes */
    pthread_t consumers[nConsumers];
    pthread_t producers[nProducers];
    pthread_attr_t attr;

    /* initialize attributes, mutex, and semaphores */
    pthread_attr_init(&attr);
    pthread_mutex_init(&mutex, NULL);
    sem_init(&empty, 0, BUFFER_SIZE);
    sem_init(&full, 0, 0);
    
    /* create producer threads */
    for (int i = 0; i < nProducers; ++i) {
        int tid = pthread_create(producers+i, &attr, producer, NULL);

        if (tid < 0) {
            // if tid is less than 0 there was an error
            perror("producer thread creation error: ");
            exit(0);
        }

        // sleep 1 second to avoid duplicate seed values
        sleep(1);
    }

    /* create consumer threads */
    for (int i = 0; i < nConsumers; ++i) {
        int tid = pthread_create(consumers+i, &attr, consumer, NULL);

        if (tid < 0) {
            // if tid is less than 0 there was an error
            perror("consumer thread creation error: ");
            exit(0);
        }

        // sleep 1 second to avoid duplicate seed values
        sleep(1);
    }

    // sleep main thread for specified amount of time
    sleep(sleepTime);

    /* cancel child threads */
    for (int i = 0; i < nProducers; ++i) pthread_cancel(producers[i]);
    for (int i = 0; i < nConsumers; ++i) pthread_cancel(consumers[i]);

    /* destroy mutex, semaphores, and attributes */
    sem_destroy(&empty);
    sem_destroy(&full);
    pthread_mutex_destroy(&mutex);
    pthread_attr_destroy(&attr);

    // return from main, exiting program
    return 0;
}

int insert_item(buffer_item item) {
    if (idx == BUFFER_SIZE) {
        // if no room to insert item (i.e. buffer is full) return error state
        return -1;
    }

    // insert produced item into buffer
    buffer[idx] = item;
    printf("Producer produced: %d\n", (int)item);

    // update index
    ++idx;
    return 0;
}

int remove_item(buffer_item *item) {
    if (idx == 0) {
        // if no items to remove (i.e. buffer is empty) return error state
        return -1;
    }

    // decrement index to get position of consumed item
    --idx;

    // consume item
    *item = buffer[idx];
    printf("Consumer consumed: %d\n", (int)(*item));
    return 0;
}
