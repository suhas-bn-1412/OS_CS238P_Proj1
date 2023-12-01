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

struct block {
        uint64_t blk;
        void *buf;
};

struct logfs {
        struct device* device;

        uint8_t done;
        pthread_t thread_id;

        void *cur_blk;
        void *cur_off;
        uint64_t off;
        uint64_t blk_sz;
        uint64_t available;

        struct wcache_utils {
                uint8_t head;
                uint8_t tail;
                uint8_t active_blks;
                pthread_mutex_t mutex;
                pthread_cond_t space;
                pthread_cond_t item;
        } wc_utils;

        struct block wcache[WCACHE_BLOCKS];
        struct block rcache[RCACHE_BLOCKS];
};

uint64_t block_start(struct logfs *logfs, uint64_t off) {
        return (off - (off % logfs->blk_sz));
}

void write_to_device(struct logfs* logfs) {
        void *tmp_buf = malloc(2*logfs->blk_sz);
        void *buf = memory_align(tmp_buf, logfs->blk_sz);
        struct block *wcache_unit;

        if (0 != pthread_mutex_lock(&logfs->wc_utils.mutex)) {
                TRACE("error while acquiring the lock");
                exit(1);
        }
        while ((!logfs->done) &&
               (logfs->wc_utils.active_blks <= 0)) {
                if (0 != pthread_cond_wait(&logfs->wc_utils.item,
                                           &logfs->wc_utils.mutex)) {
                        TRACE("error while waiting for consumer condition variable");
                        exit(1);
                }
        }

        if (logfs->done) {
                free(tmp_buf);
                if (0 != pthread_mutex_unlock(&logfs->wc_utils.mutex)) {
                        TRACE("error while releasing the lock");
                        exit(1);
                }
                return;
        }

        assert (0 != logfs->wc_utils.active_blks);

        wcache_unit = &logfs->wcache[logfs->wc_utils.tail];
        memcpy(buf, wcache_unit->buf, logfs->blk_sz);

        logfs->wc_utils.tail = (logfs->wc_utils.tail + 1) % WCACHE_BLOCKS;
        logfs->wc_utils.active_blks--;
        pthread_cond_signal(&logfs->wc_utils.space);

        if (0 != pthread_mutex_unlock(&logfs->wc_utils.mutex)) {
                TRACE("error while releasing the lock");
                exit(1);
        }

        if (device_write(logfs->device,
                         buf,
                         wcache_unit->blk,
                         logfs->blk_sz)) {
                TRACE("device write failed");
                exit(1);
        }
        free(tmp_buf);
}

void write_to_queue(struct logfs* logfs, void *buf, uint64_t blk) {
        struct block *wcache_unit;
        if (0 != pthread_mutex_lock(&logfs->wc_utils.mutex)) {
                TRACE("error while releasing the lock");
                exit(1);
        }
        while (logfs->wc_utils.active_blks >= WCACHE_BLOCKS) {
                if (0 != pthread_cond_wait(&logfs->wc_utils.space,
                                           &logfs->wc_utils.mutex)) {
                        TRACE("error while waiting for producer condition variable");
                        exit(1);
                }
        }

        wcache_unit = &logfs->wcache[logfs->wc_utils.head];
        wcache_unit->blk = blk;
        memcpy(wcache_unit->buf, buf, logfs->blk_sz);

        logfs->wc_utils.head = (logfs->wc_utils.head + 1) % WCACHE_BLOCKS;
        logfs->wc_utils.active_blks++;
        pthread_cond_signal(&logfs->wc_utils.item);

        if (0 != pthread_mutex_unlock(&logfs->wc_utils.mutex)) {
                TRACE("error while releasing the lock");
                exit(1);
        }
}

void* thread(void* arg) {
        struct logfs *logfs = (struct logfs*)arg;
        while (!logfs->done) {
                write_to_device(logfs);
        }
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

        logfs->off = 0;
        logfs->done = 0;
        logfs->blk_sz = device_block(logfs->device);
        logfs->wc_utils.head = 0;
        logfs->wc_utils.tail = 0;
        logfs->wc_utils.active_blks = 0;

        logfs->cur_blk = malloc(logfs->blk_sz);
        memset(logfs->cur_blk, 0, logfs->blk_sz);
        logfs->available = logfs->blk_sz;

        logfs->cur_off = logfs->cur_blk;

        /* initialise the read and write cache buffers */
        for (i=0; i<RCACHE_BLOCKS; i++) {
                logfs->rcache[i].blk = 3; /* random number which isn't block aligned */
                logfs->rcache[i].buf = malloc(logfs->blk_sz);
        }

        for (i=0; i<WCACHE_BLOCKS; i++) {
                logfs->wcache[i].blk = 3; /* random number which isn't block aligned */
                logfs->wcache[i].buf = malloc(logfs->blk_sz);
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
        if (0 != pthread_cond_init(&logfs->wc_utils.item, NULL)) {
                TRACE("unable to initialise consumer condition variable");
                exit(1);
        }

        if (0 != pthread_cond_init(&logfs->wc_utils.space, NULL)) {
                TRACE("unable to initialise producer condition variable");
                exit(1);
        }

        if (0 != pthread_mutex_init(&logfs->wc_utils.mutex, NULL)) {
                TRACE("unable to initialise mutex variable");
                exit(1);
        }

        if (0 != pthread_create(&logfs->thread_id, NULL, &thread, (void*)logfs)) {
                TRACE("unable to spawn thread");
                exit(1);
        }

        return logfs;
}

void logfs_close(struct logfs* logfs) {
        int i;
        
        logfs->done = 1;

        /* signal and destroy the condition variables and mutex */
        pthread_cond_signal(&logfs->wc_utils.item);
        pthread_cond_signal(&logfs->wc_utils.space);
        pthread_mutex_destroy(&logfs->wc_utils.mutex);
        pthread_cond_destroy(&logfs->wc_utils.space);
        pthread_cond_destroy(&logfs->wc_utils.item);

        /* wait for the thread to join */
        if (0 != pthread_join(logfs->thread_id, NULL)) {
                TRACE("error while joining thread");
        }

        /* close the device handle */
        device_close(logfs->device);

        /* free the buffers */
        free(logfs->cur_blk);

        for (i=0; i<RCACHE_BLOCKS; i++) {
                free(logfs->rcache[i].buf);
        }

        for (i=0; i<WCACHE_BLOCKS; i++) {
                free(logfs->wcache[i].buf);
        }

        memset(logfs, 0, sizeof(struct logfs));
        free(logfs);
}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len) {
        void *buf_;
        int rc_idx;
        uint64_t blk;
        uint64_t left_to_write;

        buf_ = (void*)buf;
        left_to_write = len;

        /**
         * if the curresnt block we are updating was
         * already flushed to device and had been read
         * into the read cache, then set the block in
         * read cache as invalid
         */
        blk = block_start(logfs, logfs->off);
        rc_idx = (blk / logfs->blk_sz) % RCACHE_BLOCKS;
        if (blk == logfs->rcache[rc_idx].blk) {
                logfs->rcache[rc_idx].blk = 3;
        }

        while(left_to_write >= logfs->available) {
                memcpy(logfs->cur_off, buf_, logfs->available);
                buf_ = (void*)((char*)buf_ + logfs->available);
                logfs->off += logfs->available;
                left_to_write -= logfs->available;
                logfs->available = 0;

                write_to_queue(logfs, logfs->cur_blk,
                               block_start(logfs, logfs->off - 1));

                memset(logfs->cur_blk, 0, logfs->blk_sz);
                logfs->available = logfs->blk_sz;
                logfs->cur_off = logfs->cur_blk;
        }

        if (0 < left_to_write) {
                memcpy(logfs->cur_off, buf_, left_to_write);
                logfs->off += left_to_write;
                logfs->cur_off = (void*)((char*)logfs->cur_off + left_to_write);
                logfs->available -= left_to_write;
        }

        return 0;
}

int check_in_wcache(struct logfs *logfs, const uint64_t blk_start, void *buf) {
        int i, ret;
        pthread_mutex_lock(&logfs->wc_utils.mutex);

        ret = 0;
        for (i=0; i<WCACHE_BLOCKS; i++) {
                if (blk_start == logfs->wcache[i].blk) {
                        memcpy(buf, logfs->wcache[i].buf, logfs->blk_sz);
                        ret = 1;
                        break;
                }
        }

        pthread_mutex_unlock(&logfs->wc_utils.mutex);
        return ret;
}

int read_mem(struct logfs *logfs, const uint64_t blk_start, void *buf, uint64_t off, size_t len) {
        /**
         * check if the read cache has the block
         * else read the block to read cache
         * then from read cache, read it to buf
         */

        int rc_idx, wc_idx;
        uint64_t blk_off;
        void *start;
        struct block *rcache_unit;
        void *tmp_buf = malloc(2*logfs->blk_sz);
        void *buf_ = memory_align(tmp_buf, logfs->blk_sz);

        rc_idx = (blk_start / logfs->blk_sz) % RCACHE_BLOCKS;
        rcache_unit = &logfs->rcache[rc_idx];

        if (blk_start != rcache_unit->blk) {
                /* read the block from device */
                rcache_unit->blk = blk_start;
                if (blk_start == block_start(logfs, logfs->off)) {
                        /* current block */
                        memcpy(buf_, logfs->cur_blk, logfs->blk_sz);
                }
                else if (check_in_wcache(logfs, blk_start, buf_)) {
                        assert( 1 );
                }
                else {
                        if (device_read(logfs->device, buf_,
                                        blk_start, logfs->blk_sz)) {
                                return -1;
                        }
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
