/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scheduler.c
 */

#undef _FORTIFY_SOURCE

#include <unistd.h>
#include <signal.h>
#include "system.h"
#include "scheduler.h"

/**
 * Needs:
 *   setjmp()
 *   longjmp()
 */

/* research the above Needed API and design accordingly */

struct scheduler* sch_obj = NULL;

void scheduler_init(void) {
        sch_obj = (struct scheduler*)malloc(sizeof(struct scheduler));
        sch_obj->head = NULL;
}

int scheduler_create(scheduler_fnc_t fnc, void* arg) {
        /**
         * create a task using the given function and arg
         * add the task to the job queue in the scheduler
         * optional - taking lock on job queue
         */

        size_t page_size_v;
        struct job* j;
        struct job* last;

        if (NULL == sch_obj) {
                return -1;
        }

        page_size_v = page_size();
        j = (struct job*)malloc(sizeof(struct job));
        j->start_addr = malloc(4*page_size_v);
        j->stack_addr = memory_align(j->start_addr, page_size_v);
        j->stack_addr = (void *)((size_t)j->stack_addr + 3*page_size_v);
        j->fnc = fnc;
        j->arg = arg;
        j->status = 0;
        j->next = NULL;

        if (NULL == sch_obj->head) {
                sch_obj->head = j;
                return 0;
        }

        last = sch_obj->head;

        while (NULL != last->next) {
                last = last->next;
        }

        last->next = j;
        return 0;
}

void scheduler_execute(void) {
        /**
         * current is pointing to currently executing task
         * always go to next task and start executing
         *
         * if task is already started then call longjmp
         * else start execution now
         */

        uint64_t rsp;

        setjmp(sch_obj->env);

        sch_obj->prev = sch_obj->curr;

        if (sch_obj->curr == NULL || sch_obj->curr->next == NULL) {
                sch_obj->curr = sch_obj->head;
                sch_obj->prev = NULL;
        } else {
                sch_obj->curr = sch_obj->curr->next;
        }

        if (NULL == sch_obj->curr) {
                free(sch_obj);
                return;
        }

        if (sch_obj->curr->status == 0) {
                /**
                 * set the stack pointer to this threads stack pointer
                 */
                rsp = (uint64_t)sch_obj->curr->stack_addr;
                __asm__ volatile ("mov %[rs], %%rsp \n" : [rs] "+r" (rsp) ::);
                
                sch_obj->curr->status = 1;
                sch_obj->curr->fnc(sch_obj->curr->arg);
                
                if (NULL == sch_obj->prev) {
                        /* possibly we're deleting the head node */
                        sch_obj->head = sch_obj->head->next;
                } else {
                        sch_obj->prev->next = sch_obj->curr->next;
                }
                
                free(sch_obj->curr->start_addr);
                free(sch_obj->curr);
                sch_obj->curr = sch_obj->prev;
                longjmp(sch_obj->env, 1);
        } else {
                longjmp(sch_obj->curr->env, 1);
        }
}

void interrupt_handler(int signal) {
        assert(SIGALRM==signal);
        alarm(1);
        scheduler_yield();
}

void scheduler_yield(void) {
        /**
         * using current job's jmp_buf do setjmp
         * in the same return, call scheduler execute to start
         * executing the next task
         * in fake return, return to caller function so that
         * it can continue its execution
         */

        int ret;

        signal(SIGALRM, interrupt_handler);

        if ((ret = setjmp(sch_obj->curr->env)) == 0) {
                longjmp(sch_obj->env, 1);
        }
        return;
}
