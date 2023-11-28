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

struct read_buf {
        uint64_t blk;
        void *buf;
};

struct logfs {
        struct device* device;
        struct write_buf {
                void *head;
                void *tail;
                void *buf;
                uint8_t active_blks;
                pthread_cond_t producer_cond;
                pthread_cond_t consumer_cond;
                pthread_mutex_t producer_mutex;
                pthread_mutex_t consumer_mutex;
                pthread_mutex_t active_blks_mutex;
        } write_buf;

        struct read_buf rcache[RCACHE_BLOCKS];

        uint64_t device_offset;
        pthread_t thread_id;

        void *cur_blk;
        void *cur_off;
        uint64_t blk_sz;
        uint64_t available;
};

uint8_t current_active_blks(struct logfs *logfs) {
        uint8_t tmp;
        if (0 != pthread_mutex_lock(&logfs->write_buf.active_blks_mutex)) {
                TRACE("error while locking");
                exit(1);
        }

        tmp = logfs->write_buf.active_blks;

        if (0 != pthread_mutex_unlock(&logfs->write_buf.active_blks_mutex)) {
                TRACE("error while unlocking");
                exit(1);
        }
        return tmp;
}

void increment_active_blk(struct logfs *logfs, int val) {
        if (0 != pthread_mutex_lock(&logfs->write_buf.active_blks_mutex)) {
                TRACE("error while locking");
                exit(1);
        }

        logfs->write_buf.active_blks+=val;

        if (0 != pthread_mutex_unlock(&logfs->write_buf.active_blks_mutex)) {
                TRACE("error while unlocking");
                exit(1);
        }
}

void increment_head(struct logfs *logfs) {
        logfs->write_buf.head = (void*)((char*)logfs->write_buf.head + logfs->blk_sz);

        if ((void*)((char*)logfs->write_buf.buf + WCACHE_BLOCKS*logfs->blk_sz) <=
            logfs->write_buf.head) {
                logfs->write_buf.head = logfs->write_buf.buf;
        }

        increment_active_blk(logfs, 1);
        pthread_cond_signal(&logfs->write_buf.consumer_cond);
}

void increment_tail(struct logfs *logfs) {
        logfs->write_buf.tail = (void*)((char*)logfs->write_buf.tail + logfs->blk_sz);

        if ((void*)((char*)logfs->write_buf.buf + WCACHE_BLOCKS*logfs->blk_sz) <=
            logfs->write_buf.tail) {
                logfs->write_buf.tail = logfs->write_buf.buf;
        }

        increment_active_blk(logfs, -1);
        pthread_cond_signal(&logfs->write_buf.producer_cond);
}

void write_to_device(struct logfs* logfs) {
        void *tmp_buf = malloc(2*logfs->blk_sz);
        void *buf = memory_align(tmp_buf, logfs->blk_sz);

        while (current_active_blks(logfs) == 0) {
                if (0 != pthread_mutex_lock(&logfs->write_buf.consumer_mutex)) {
                        TRACE("error while acquiring the lock");
                        exit(1);
                }

                if (0 != pthread_cond_wait(&logfs->write_buf.consumer_cond,
                                           &logfs->write_buf.consumer_mutex)) {
                        TRACE("error while waiting for consumer condition variable");
                        exit(1);
                }

                if (0 != pthread_mutex_unlock(&logfs->write_buf.consumer_mutex)) {
                        TRACE("error while releasing the lock");
                        exit(1);
                }
        }

        assert( logfs->write_buf.tail!=logfs->write_buf.head );

        memcpy(buf, logfs->write_buf.tail, logfs->blk_sz);
        if (device_write(logfs->device,
                         buf,
                         logfs->device_offset,
                         logfs->blk_sz)) {
                TRACE("device write failed");
                exit(1);
        }

        free(tmp_buf);
        logfs->device_offset += logfs->blk_sz;

        increment_tail(logfs);

        write_to_device(logfs);
}

void write_to_queue(struct logfs* logfs, void *buf) {
        TRACE("write to queue");
        while (current_active_blks(logfs) >= WCACHE_BLOCKS) {
                if (0 != pthread_mutex_lock(&logfs->write_buf.producer_mutex)) {
                        TRACE("error while releasing the lock");
                        exit(1);
                }

                if (0 != pthread_cond_wait(&logfs->write_buf.producer_cond,
                                           &logfs->write_buf.producer_mutex)) {
                        TRACE("error while waiting for producer condition variable");
                        exit(1);
                }

                if (0 != pthread_mutex_unlock(&logfs->write_buf.producer_mutex)) {
                        TRACE("error while releasing the lock");
                        exit(1);
                }
        }

        memcpy(logfs->write_buf.head, buf, logfs->blk_sz);
        increment_head(logfs);
}

void flush_write_buffer(struct logfs *logfs) {
        memset(logfs->cur_off, 0, logfs->available);
        write_to_queue(logfs, logfs->cur_blk);

        /**
         * wait till all blocks are written
         * to the device
         */

        while (current_active_blks(logfs) > 0) {
                if (0 != pthread_mutex_lock(&logfs->write_buf.producer_mutex)) {
                        TRACE("error while releasing the lock");
                        exit(1);
                }

                if (0 != pthread_cond_wait(&logfs->write_buf.producer_cond,
                                           &logfs->write_buf.producer_mutex)) {
                        TRACE("error while waiting for producer condition variable");
                        exit(1);
                }

                if (0 != pthread_mutex_unlock(&logfs->write_buf.producer_mutex)) {
                        TRACE("error while releasing the lock");
                        exit(1);
                }
        }
        return;
}

void* thread(void* arg) {
        write_to_device((struct logfs*)arg);
        pthread_exit(NULL);
}

struct logfs *logfs_open(const char *pathname) {
        struct logfs* logfs;
        int i;

        logfs = (struct logfs*) malloc(sizeof(struct logfs));

        logfs->device = device_open(pathname);
        if (NULL == logfs->device) {
                TRACE("unable to open device");
                free(logfs);
                return NULL;
        }

        logfs->device_offset = 0;
        logfs->blk_sz = device_block(logfs->device);
        logfs->write_buf.buf = malloc(WCACHE_BLOCKS * logfs->blk_sz);
        logfs->write_buf.head = logfs->write_buf.buf;
        logfs->write_buf.tail = logfs->write_buf.buf;
        logfs->write_buf.active_blks = 0;

        logfs->cur_blk = malloc(logfs->blk_sz);
        logfs->cur_off = logfs->cur_blk;

        /* initialise the read cache buffers */
        for (i=0; i<RCACHE_BLOCKS; i++) {
                logfs->rcache[i].blk = 3; /* random number which is not block aligned */
                logfs->rcache[i].buf = malloc(logfs->blk_sz);
        }

        /**
         * initialise condition variable for signaling
         * data addition into the write queue
         *
         * then spawn a thread waiting on this condition
         * the thread will read the data upon getting a
         * signal from the producer and writes that data
         * into the device using device api's
         */
        if (0 != pthread_cond_init(&logfs->write_buf.consumer_cond, NULL)) {
                TRACE("unable to initialise consumer condition variable");
                exit(1);
        }

        if (0 != pthread_cond_init(&logfs->write_buf.producer_cond, NULL)) {
                TRACE("unable to initialise producer condition variable");
                exit(1);
        }

        if (0 != pthread_mutex_init(&logfs->write_buf.consumer_mutex, NULL)) {
                TRACE("unable to initialise mutex variable");
                exit(1);
        }

        if (0 != pthread_mutex_init(&logfs->write_buf.producer_mutex, NULL)) {
                TRACE("unable to initialise mutex variable");
                exit(1);
        }

        if (0 != pthread_mutex_init(&logfs->write_buf.active_blks_mutex, NULL)) {
                TRACE("unable to initialise mutex variable");
                exit(1);
        }

        pthread_create(&logfs->thread_id, NULL, &thread, (void*)logfs);

        return logfs;
}

void logfs_close(struct logfs* logfs) {
        /*destroy the condition variables and mutex */
        pthread_cond_destroy(&logfs->write_buf.producer_cond);
        pthread_cond_destroy(&logfs->write_buf.consumer_cond);
        pthread_mutex_destroy(&logfs->write_buf.consumer_mutex);
        pthread_mutex_destroy(&logfs->write_buf.producer_mutex);

        /* wait for the thread to join */
        /* cancel thread, if there is no way thread can exit gracefully */
        pthread_cancel(logfs->thread_id);

        /* close the device handle */
        device_close(logfs->device);

        /* free the buffers */
        free(logfs->write_buf.buf);
        free(logfs->cur_blk);

        free(logfs);
}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len) {
        uint64_t left_to_write;
        void *buf_;

        buf_ = (void*)buf;
        left_to_write = len;

        char info[100];
        sprintf(info, "append - %ld", len);
        TRACE(info);

        while(left_to_write > logfs->available) {
                memcpy(logfs->cur_off, buf_, logfs->available);
                write_to_queue(logfs, logfs->cur_blk);

                left_to_write -= logfs->available;
                buf_ = (void*)((char*)buf_ + logfs->available);

                memset(logfs->cur_blk, 0, logfs->blk_sz);
                logfs->available = logfs->blk_sz;
                logfs->cur_off = logfs->cur_blk;
        }

        if (0 != left_to_write) {
                memcpy(logfs->cur_off, buf_, left_to_write);
                logfs->cur_off = (void*)((char*)logfs->cur_off + left_to_write);
                logfs->available -= left_to_write;
        }

        return 0;
}

uint64_t block_start(struct logfs *logfs, uint64_t off) {
        return (off - (off % logfs->blk_sz));
}

int read_mem(struct logfs *logfs, const uint64_t blk_start, void *buf, uint64_t off, size_t len) {
        /**
         * check if the read cache has the block
         * else read the block to read cache
         * then from read cache, read it to buf
         */

        uint8_t rc_idx;
        uint64_t blk_off;
        void *start;
        struct read_buf *rcache_unit;
        void *tmp_buf = malloc(2*logfs->blk_sz);
        void *buf_ = memory_align(tmp_buf, logfs->blk_sz);

        rc_idx = (blk_start / logfs->blk_sz) % RCACHE_BLOCKS;
        rcache_unit = &logfs->rcache[rc_idx];

        if (blk_start != rcache_unit->blk) {
                /* read the block from device */
                TRACE("reading cache");
                rcache_unit->blk = blk_start;
                if (device_read(logfs->device, buf_, blk_start, logfs->blk_sz)) {
                        return -1;
                }
                memcpy(rcache_unit->buf, buf_, logfs->blk_sz);
        }
        free(tmp_buf);
        blk_off = off - blk_start;
        start = (void*)((char*)rcache_unit->buf + blk_off);
        memcpy(buf, start, len);
        return 0;
}

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len) {
        void *buf_;
        uint64_t off_;
        uint64_t read_len;
        uint64_t blk_start;
        uint64_t left_to_read;
        
        char info[100];
        sprintf(info, "active blocks to flush - %d", logfs->write_buf.active_blks);
        TRACE(info);

        flush_write_buffer(logfs);

        sprintf(info, "active blocks after flush - %d", logfs->write_buf.active_blks);
        TRACE(info);

        buf_ = buf;
        off_ = off;
        left_to_read = len;

        blk_start = block_start(logfs, off);
        read_len = MIN(len, blk_start + logfs->blk_sz - off);

        if (read_mem(logfs, blk_start, buf_, off_, read_len)) {
                return -1;
        }
        blk_start += logfs->blk_sz;
        buf_ = (void*)((char*)buf_ + read_len);
        off_ += read_len;
        left_to_read -= read_len;

        while(left_to_read > logfs->blk_sz) {
                read_len = logfs->blk_sz;
                if (read_mem(logfs, blk_start, buf_, off_, read_len)) {
                        return -1;
                }
                blk_start += logfs->blk_sz;
                buf_ = (void*)((char*)buf_ + read_len);
                off_ += read_len;
                left_to_read -= read_len;
        }

        if (0 != left_to_read) {
                if (read_mem(logfs, blk_start, buf_, off_, left_to_read)) {
                        return -1;
                }
        }
        return 0;
}
