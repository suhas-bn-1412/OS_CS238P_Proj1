/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include "scm.h"

#define VM_START_ADDR 0x700000000000

/**
 * defining size info id as INT_MAX-1 
 * when we see this id at the start of the file, then
 * it means that the size was stored from previous
 * execution of the process, and this much data is
 * currently stored in the file
 */
#define SIZE_INFO_ID INT_MAX-1
#define SIZE_INFO_BYTES 2*sizeof(int)
#define ADDR_INFO_BYTES 2*sizeof(size_t)

/**
 * Needs:
 *   fstat()
 *   S_ISREG()
 *   open()
 *   close()
 *   sbrk()
 *   mmap()
 *   munmap()
 *   msync()
 */

struct map {
        void *addr;
        size_t size;
};

struct scm {
        int fd;
        size_t size;
        size_t used;
        void *base_addr;
        void *mapped_addr;
        int* size_info;
};

/* research the above Needed API and design accordingly */

struct scm *scm_open(const char* pathname, int truncate) {
        struct stat statbuf;
        struct scm* scm;

        assert(safe_strlen(pathname));

        scm = (struct scm*)malloc(sizeof(struct scm));
        scm->base_addr = NULL;
        scm->mapped_addr = NULL;

        scm->fd = open(pathname, O_RDWR);

        if (-1 == scm->fd) {
                TRACE("cannot open file");
                free(scm);
                scm = NULL;
                return NULL;
        }

        fstat(scm->fd, &statbuf);
        scm->size = statbuf.st_size;
        scm->used = 0;

        /**
         * scm has fd, size of the file in bytes 
         * now push the program break by 'size' (size is in bytes)
         * the whole memory will serve us as virtual memory that
         * should be mapped to the file
         */

        scm->mapped_addr = mmap((void*)VM_START_ADDR,
                                scm->size,
                                PROT_EXEC | PROT_READ | PROT_WRITE,
                                MAP_FIXED | MAP_SHARED,
                                scm->fd,
                                0);

        if (MAP_FAILED == scm->mapped_addr) {
                TRACE("map failed");
                free(scm);
                scm = NULL;
                return NULL;
        }

        scm->base_addr = (void*)((char*)scm->mapped_addr + SIZE_INFO_BYTES);

        scm->size_info = (int*)scm->mapped_addr;
        if ((SIZE_INFO_ID != scm->size_info[0]) || truncate) {
                /* first invocation, or truncate is true
                 * in this case, the size should be set to 0
                 * and then complete region should be used
                 * by the process
                 */
                scm->size_info[0] = SIZE_INFO_ID;
                scm->size_info[1] = 0;
                scm->used = 0;
        }
        scm->used = scm->size_info[1];
        printf("file has %ld bytes of data\n", scm->used);

        close(scm->fd);

        return scm;
}

void *scm_malloc(struct scm *scm, size_t n) {
        size_t *addr_info;
        void *addr;
        void *base;
        bool found;

        found = false;
        addr = base = scm_mbase(scm);
        while (!found && (addr < (void*)((char*)base + scm->used))) {
                addr_info = (size_t*)addr - 2;
                if ((addr_info[0] == 0) && (addr_info[1] >= n)) {
                        found = true;
                        break;
                }
                addr = (void*)((char*)addr + ((size_t*)addr-2)[1] + ADDR_INFO_BYTES);
        }

        if (found) {
                addr_info[0] = 1;
                return addr;
        }
        
        if ((scm->used + n + ADDR_INFO_BYTES) > scm->size) {
                EXIT("not enough memory");
                return NULL;
        }
        
        addr_info = (size_t*)((char*)scm->base_addr + scm->used);
        addr_info[0] = 1; /* memory is in use */
        addr_info[1] = n;
        scm->used += n + ADDR_INFO_BYTES;
        scm->size_info[1] = scm->used;
        addr = (void*)((char*)addr_info + ADDR_INFO_BYTES);
        return addr;
}

void scm_close(struct scm *scm) {
        scm->size_info[0] = SIZE_INFO_ID;
        scm->size_info[1] = scm->used;
        msync(scm->mapped_addr, scm->size, MS_SYNC);
        munmap(scm->mapped_addr, scm->size);
        free(scm);
}

char *scm_strdup(struct scm *scm, const char *s) {
        char *temp;
        size_t *addr_info;

        addr_info = (size_t *)((char *)scm->base_addr + scm->used);
        addr_info[0] = 1;
        addr_info[1] = strlen(s) + 1;

        temp =  (char*)addr_info + ADDR_INFO_BYTES;

        strcpy(temp, s);
        scm->used += ADDR_INFO_BYTES + addr_info[1];
        scm->size_info[1] = scm->used;
        return temp;
}

bool scm_allocated_addr(struct scm* scm, void *p) {
        void* addr;
        void* base;
        bool found;

        addr = base = scm_mbase(scm);
        found = false;

        while (!found && (addr < (void*)((char*)base + scm->used))) {
                if (addr == p) {
                        found = true;
                        break;
                }

                addr = (void*)((char*)addr + ((size_t*)addr-2)[1] + ADDR_INFO_BYTES);
        }
        return found;
}

/**
 * given address p, we should free the memory
 * at address p. subsequent malloc calls can
 * make use of this memory
 *
 * given address p, if it was allocated by previous
 * call to malloc, then (p - 2*sizeof(size_t))th address would
 * have the flag which represents whether the memory is active
 * or in use.. (p - sizeof(size_t))th address would have the
 * value of the size of the memory pointed by p
 */
void scm_free(struct scm *scm, void *p) {
        size_t* addr_info;

        if (!scm_allocated_addr(scm, p)) {
                return;
        }

        addr_info = (size_t*)p - 2;
        addr_info[0] = 0; /* not in use */
        return;
}

size_t scm_utilized(const struct scm *scm) {
        return scm->used;
}

size_t scm_capacity(const struct scm *scm) {
        return scm->size;
}

void *scm_mbase(struct scm *scm) {
        if (scm->used) {
                return (void*)((size_t*)scm->base_addr + 2);
        }
        return scm->base_addr;
}
