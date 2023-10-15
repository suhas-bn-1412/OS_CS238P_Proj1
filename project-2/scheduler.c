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

void scheduler_init(void) {
        sch_obj = malloc(sizeof(scheduler));
        sch_obj->head = NULL;
}

int scheduler_create(scheduler_fnc_t fnc, void* arg) {
        /**
         * create a task using the given function and arg
         * add the task to the job queue in the scheduler
         * optional - taking lock on job queue
         */

        if (NULL == sch_obj) {
                return -1;
        }

        job* j = malloc(sizeof(job));
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
}

void scheduelr_execute(void) {
}

void scheduler_yield(void) {
}
