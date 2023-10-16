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
#include <setjmp.h>
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

        struct job* j;
        struct job* last;

        if (NULL == sch_obj) {
                return -1;
        }

        j = (struct job*)malloc(sizeof(struct job));
        j->fnc = fnc;
        j->arg = arg;
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
}

void scheduler_yield(void) {
}
