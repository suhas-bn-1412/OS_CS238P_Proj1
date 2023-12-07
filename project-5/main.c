/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */
#include <signal.h>
#include "system.h"
#define MAX_THROUGHPUT 1000000000
/**
 * Needs:
 *   signal()
 */
static volatile int done;
static void
_signal_(int signum)
{
        assert( SIGINT == signum );

        done = 1;
}
double
cpu_util(const char *s)
{
        static unsigned sum_, vector_[7];
        unsigned sum, vector[7];
        const char *p;
        double util;
        uint64_t i;

        /*
          user
          nice
          system
          idle
          iowait
          irq
          softirq
        */

        if (!(p = strstr(s, " ")) ||
            (7 != sscanf(p,
                         "%u %u %u %u %u %u %u",
                         &vector[0],
                         &vector[1],
                         &vector[2],
                         &vector[3],
                         &vector[4],
                         &vector[5],
                         &vector[6]))) {
                return 0;
        }
        sum = 0.0;
        for (i=0; i<ARRAY_SIZE(vector); ++i) {
                sum += vector[i];
        }
        util = (1.0 - (vector[3] - vector_[3]) / (double)(sum - sum_)) * 100.0;
        sum_ = sum;
        for (i=0; i<ARRAY_SIZE(vector); ++i) {
                vector_[i] = vector[i];
        }
        return util;
}
void kBtoB(unsigned long long *value) {
    *value *= 1024;
}
void display_memory_stats() {
        char * mem_info = "/proc/meminfo";
        FILE *file;
        char line[1024];
        unsigned long long mem_total = 0,
                           mem_free = 0,
                           mem_buffers = 0,
                           mem_cached = 0;
        unsigned long long swap_total = 0,
                           swap_free = 0;
        unsigned long long mem_used;
        unsigned long long mem_avail;
        unsigned long long swap_used;
        unsigned long long buff_cache;
        double memory_usage_per;
        const double MiB = 1024.0 * 1024.0;

        if (!(file = fopen(mem_info, "r"))) {
            perror("Error opening the meminfo file!");
            exit(1);
        }


        while (fgets(line, sizeof(line), file)) {
                if (sscanf(line, "MemTotal: %llu", &mem_total) == 1) {
                    kBtoB(&mem_total);
                } else if (sscanf(line, "MemFree: %llu", &mem_free) == 1) {
                    kBtoB(&mem_free);
                } else if (sscanf(line, "Buffers: %llu", &mem_buffers) == 1) {
                    kBtoB(&mem_buffers);
                } else if (sscanf(line, "Cached: %llu", &mem_cached) == 1) {
                    kBtoB(&mem_cached);
                } else if (sscanf(line, "SwapTotal: %llu", &swap_total) == 1) {
                    kBtoB(&swap_total);
                } else if (sscanf(line, "SwapFree: %llu", &swap_free) == 1) {
                    kBtoB(&swap_free);
                }
        }

        mem_used = mem_total - mem_free - mem_cached - mem_buffers;
        mem_avail = mem_free + mem_buffers + mem_cached;
        swap_used = swap_total - swap_free;
        buff_cache = mem_buffers + mem_cached;
        memory_usage_per = ((double)mem_used / mem_total) * 100;
        fclose(file);
        printf("\r");
        printf("MiB Mem :%9.1f total, %9.1f free, %9.1f used, %9.1f buff/cache | ",
                mem_total / MiB, mem_free / MiB, mem_used / MiB, buff_cache / MiB);
        printf("Mem(Usage) : %.1f%% |  ", memory_usage_per);
        fflush(stdout);
        fflush(stdout);
}
double calculatePacketRate(unsigned long long packets, time_t startTime, time_t endTime) {
        double duration = difftime(endTime, startTime);
        if (duration > 0) {
            return packets / duration;
        }
        else {
            return 0;
        }
}

void calculate_network_stats(time_t startTime, time_t endTime, double maxExpectedRate) {
        char* proc_net_dev = "/proc/net/dev";
        FILE* file;
       char line[256];
        unsigned long long packets_received;
        unsigned long long packets_transmitted;

        if (!(file = fopen(proc_net_dev, "r"))) {
            perror("Error opening the /proc/net/dev");
            exit(1);
        }

        while (fgets(line, sizeof(line), file)) {
                if (strstr(line, "enp0s1:") != NULL) {
                    unsigned long long dummy;
                    sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",&packets_received, &dummy, &dummy, &dummy, &dummy, &dummy,                                                                                           &dummy, &dummy, &packets_transmitted, &dummy);
                    break;
                }
        }

        double sentPacketRate = calculatePacketRate(packets_transmitted, startTime, endTime);
        double receivedPacketRate = calculatePacketRate(packets_received, startTime, endTime);

        double sentPerformancePercentage = (sentPacketRate / maxExpectedRate) * 100;
        double receivedPerformancePercentage = (receivedPacketRate / maxExpectedRate) * 100;
//        printf("Sent Packet Rate: %.2f PPS | ",sentPacketRate);
  //      printf("Received Packet Rate: %.2f PPS | ", receivedPacketRate);
        printf("Transmit(Per): %.2f%% | ", sentPerformancePercentage);
        printf("Received(Per): %.2f%% |", receivedPerformancePercentage);
        fflush(stdout);
}
void calculate_block_device_stats() {
    const char* const PROC_DISKSTATS = "/proc/diskstats";
    FILE* file;
    char line[256];

    unsigned long long prev_blocks_read = 0;
    unsigned long long prev_blocks_written = 0;

    if (!(file = fopen(PROC_DISKSTATS, "r"))) {
        perror("Error opening /proc/diskstats");
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), file)) {
        char device_name[32];
        unsigned int read_completed, read_merged, write_completed, write_merged;
        unsigned long long sectors_read, sectors_written;
        unsigned int time_reading, time_writing, dummy1, dummy2;

        if (sscanf(line, "%u %u %31s %llu %u %u %u %llu %u %u %u", &read_completed, &read_merged, device_name, &sectors_read, &time_reading,
                   &write_completed, &write_merged, &sectors_written, &time_writing, &dummy1, &dummy2) == 11) {

            if (strstr(device_name, "sda") != NULL) {
                unsigned long long curr_blocks_read = sectors_read;
                unsigned long long curr_blocks_written = sectors_written;

                unsigned long long read_diff = curr_blocks_read - prev_blocks_read;
                unsigned long long write_diff = curr_blocks_written - prev_blocks_written;

                double time_interval = 1.0; // Monitoring for every second
                double read_rate = read_diff / time_interval;
                double write_rate = write_diff / time_interval;

                double read_performance = (read_rate / MAX_THROUGHPUT) * 100.0;
                double write_performance = (write_rate / MAX_THROUGHPUT) * 100.0;

      //          printf("Blocks Read Rate: %.2f blocks per second\n", read_rate);
        //        printf("Blocks Written Rate: %.2f blocks per second\n", write_rate);
                printf("Read(Per): %.2f%%|", read_performance);
                printf("Write(Per): %.2f%%|", write_performance);

                prev_blocks_read = curr_blocks_read;
                prev_blocks_written = curr_blocks_written;
                break;
            }
        }
    }

    fclose(file);
}

int
main(int argc, char *argv[])
{
        const char * const PROC_STAT = "/proc/stat";
        char line[1024];
        FILE *file;
        time_t startTime, endTime;
        double maxExpectedRate = 500000000;

    startTime = time(NULL);
    endTime = startTime + 60;

        UNUSED(argc);
        UNUSED(argv);

        if (SIG_ERR == signal(SIGINT, _signal_)) {
                TRACE("signal()");
                return -1;
        }
/*      while (!done) {
                if (!(file = fopen(PROC_STAT, "r"))) {
                        TRACE("fopen()");
                        return -1;
                }
                if (fgets(line, sizeof (line), file)) {
                        printf("\r%5.1f%%", cpu_util(line));
                        fflush(stdout);
                }
                us_sleep(500000);
                fclose(file);
        }*/
        while(!done)
        {
       // printf("\rMemory Stats\n");
       // printf("\r----------------------------------------------------------------------------------------\n");
        display_memory_stats();
       // printf("\r----------------------------------------------------------------------------------------\n");
       // printf("Network Stats\n");
//        printf("----------------------------------------------------------------------------------------\n");
        calculate_network_stats(startTime, endTime, maxExpectedRate);
       // printf("----------------------------------------------------------------------------------------\n");
       // printf("I/O Stats\n");
       // printf("----------------------------------------------------------------------------------------\n");
        calculate_block_device_stats();
       // printf("----------------------------------------------------------------------------------------\n");
        us_sleep(500000);
        }//
        printf("\rDone!   \n");
        return 0;
}
                                 