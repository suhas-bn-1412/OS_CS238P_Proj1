/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * logfs.c
 */

#include <pthread.h>
#include "device.h"
#include "logfs.h"

#define WCACHE_BLOCKS 32
#define RCACHE_BLOCKS 256

/**
 * Needs:
 *   pthread_create()
 *   pthread_join()
 *   pthread_mutex_init()
 *   pthread_mutex_destroy()
 *   pthread_mutex_lock()
 *   pthread_mutex_unlock()
 *   pthread_cond_init()
 *   pthread_cond_destroy()
 *   pthread_cond_wait()
 *   pthread_cond_signal()
 */

/* research the above Needed API and design accordingly */

struct logfs {
        struct device* device;
        struct write_buf {
                void *head;
                void *tail;
                void *buf;
        } write_buf;

        void *cur_blk;
        void *cur_off;
};

void write_to_device(struct logfs *logfs) {
}

void write_to_queue(struct logfs *logfs, void *buf) {
}

struct logfs *logfs_open(const char *pathname) {
        struct logfs* logfs;

        logfs = (struct logfs*) malloc(sizeof(struct logfs));
        
        logfs->device = device_open(pathname);
        if (NULL == logfs->device) {
                TRACE("unable to open device");
                free(logfs);
                return NULL;
        }

        logfs->write_buf.buf = malloc(WCACHE_BLOCKS * logfs->device->block);
        logfs->write_buf.head = logfs->write_buf.buf;
        logfs->write_buf.tail = logfs->write_buf.buf;

        logfs->cur_blk = malloc(logfs->device->block);
        logfs->cur_off = logfs->cur_blk;

        /**
         * initialise condition variable for signaling
         * data addition into the write queue
         *
         * then spawn a thread waiting on this condition
         * the thread will read the data upon getting a
         * signal from the producer and writes that data
         * into the device using device api's
         */

        return logfs;
}

void logfs_close(struct logfs* logfs) {
        /*destroy the condition variable */

        /* wait for the thread to join */

        /* close the device handle */
        device_close(logfs->device);

        /* free the buffers */
        free(logfs->write_buf.buf);
        free(logfs->cur_blk);

        free(logfs);
}
