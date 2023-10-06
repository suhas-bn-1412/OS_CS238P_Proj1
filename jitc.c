/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * jitc.c
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dlfcn.h>
#include "system.h"
#include "jitc.h"

/**
 * Needs:
 *   fork()
 *   execv()
 *   waitpid()
 *   WIFEXITED()
 *   WEXITSTATUS()
 *   dlopen()
 *   dlclose()
 *   dlsym()
 */

/* research the above Needed API and design accordingly */

/**
 * definition of struct jitc
 * needs to hold handle which we get when
 * dlopen is called on the .so file
 */
struct jitc {
        void* handle;
};

/**
 * implementation of jitc_compile
 *
 * take in c file as input
 * fork and create a process which compiles the c file
 * process runs the below command to generate the .so file
 * gcc -shared -o <.so file name> -fPIC <.c file name>
 * include gcc flags if you want like -Wall
 */

int jitc_compile(const char* input, const char* output) {
        pid_t pid = fork();
        if (pid == 0) {
                char* argsForGcc[7];
                argsForGcc[0] = "/usr/bin/gcc";
                argsForGcc[1] = "-shared";
                argsForGcc[2] = "-o";
                argsForGcc[3] = (char*)output;
                argsForGcc[4] = "-fPIC";
                argsForGcc[5] = (char*)input;
                argsForGcc[6] = NULL;

                execv(argsForGcc[0], argsForGcc);

                /* shouldn't reach here if execv is successful */
                TRACE("execv failed");
                exit(1);
        } else {
                if (pid == -1) {
                        /* error in creating child process */
                        TRACE("error creating child process");
                } else {
                        int wstatus = 0;

                        /* wait for child process to exit */
                        if (-1 == waitpid(pid, &wstatus, 0)) {
                                TRACE("waitpid failed");
                                return -1;
                        }

                        if (!WIFEXITED(wstatus)) {
                                TRACE("child process did not exit gracefully");
                                return -1;
                        }
                }
        }
        return 0;
}

struct jitc* jitc_open(const char* pathname) {
        struct jitc* jitc_ = malloc(sizeof(struct jitc));

        jitc_->handle = dlopen(pathname, RTLD_NOW | RTLD_LOCAL);

        if (jitc_->handle == NULL) {
                TRACE("Handle is NULL");
        }

        return jitc_;
}

void jitc_close(struct jitc* jitc) {
        if (jitc->handle != NULL) {
                dlclose(jitc->handle);
        }
        free(jitc);
}

long jitc_lookup(struct jitc* jitc, const char* symbol) {
        void* addr = dlsym(jitc->handle, symbol);

        if (NULL == addr) {
                TRACE("couldn't find the address for the symbol");
                return 0;
        }

        return (long)addr;
}
