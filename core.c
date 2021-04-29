/*
 * callbench.c
 *
 * This program benchmarks the clock_gettime kernel syscall on Unix systems by
 * reading the CLOCK_MONOTONIC value. This is usually the fastest value with a
 * vDSO counterpart, so we can ensure minimal CPU time spent in the kernel and
 * leave just the time taken to perform the context switch.
 *
 * Licensed under the MIT License (MIT)
 *
 * Copyright (c) 2018-2020 Danny Lin <danny@kdrag0n.dev>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <getopt.h>

#include <graalvm/llvm/polyglot.h>

#define TEST_READ_PATH "/dev/zero"
#define TEST_READ_LEN 65536

#define NS_PER_SEC 1000000000
#define MS_PER_USEC 1000

#if defined(__linux__)
#define CLOCK_GETTIME_SYSCALL_NR __NR_clock_gettime
#elif defined(__APPLE__)
// macOS does not allow direct syscalls that bypass libSystem.dylib
#define NO_DIRECT_SYSCALL
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#define CLOCK_GETTIME_SYSCALL_NR SYS_clock_gettime
#elif defined(__NetBSD__)
#define CLOCK_GETTIME_SYSCALL_NR SYS___clock_gettime50
#else
#error Unsupported platform: missing clock_gettime syscall number!
#endif

typedef void (*bench_impl)(void);

struct CallbenchResults {
    unsigned int time_syscall_ns;
    unsigned int time_libc_ns;
    unsigned int file_mmap_ns;
    unsigned int file_read_ns;
};
POLYGLOT_DECLARE_STRUCT(CallbenchResults);

static char test_read_buf[TEST_READ_LEN];

static long ts_to_ns(struct timespec ts) {
    return ts.tv_nsec + (ts.tv_sec * NS_PER_SEC);
}

#ifndef NO_DIRECT_SYSCALL
static void time_syscall_mb(void) {
    struct timespec ts;
    syscall(CLOCK_GETTIME_SYSCALL_NR, CLOCK_MONOTONIC, &ts);
}
#endif

static void time_libc_mb(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
}

static void mmap_mb(void) {
    int fd = open(TEST_READ_PATH, O_RDONLY);
    int len = TEST_READ_LEN;

    void *data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    memcpy(test_read_buf, data, len);

    munmap(data, len);
    close(fd);
}

static void file_mb(void) {
    int fd = open(TEST_READ_PATH, O_RDONLY);
    long len = TEST_READ_LEN;

    read(fd, test_read_buf, len);

    close(fd);
}

static long run_bench_ns(bench_impl inner_call, int calls, int loops, int rounds) {
    long best_ns1 = LONG_MAX;

    for (int round = 0; round < rounds; round++) {
        long best_ns2 = LONG_MAX;

        for (int loop = 0; loop < loops; loop++) {
            struct timespec before;
            clock_gettime(CLOCK_MONOTONIC, &before);

            for (int call = 0; call < calls; call++) {
                inner_call();
            }

            struct timespec after;
            clock_gettime(CLOCK_MONOTONIC, &after);

            long elapsed_ns = ts_to_ns(after) - ts_to_ns(before);
            if (elapsed_ns < best_ns2) {
                best_ns2 = elapsed_ns;
            }
        }

        best_ns2 /= calls; // per call in the loop

        if (best_ns2 < best_ns1) {
            best_ns1 = best_ns2;
        }

        usleep(125 * MS_PER_USEC);
    }

    return best_ns1;
}

static int default_arg(int arg, int def) {
    return arg == -1 ? def : arg;
}

void benchTime(struct CallbenchResults *results, int calls, int loops, int rounds) {
    calls = default_arg(calls, 100000);
    loops = default_arg(loops, 32);
    rounds = default_arg(rounds, 5);

#ifndef NO_DIRECT_SYSCALL
    long best_ns_syscall = run_bench_ns(time_syscall_mb, calls, loops, rounds);
#endif
    long best_ns_libc = run_bench_ns(time_libc_mb, calls, loops, rounds);

#ifdef NO_DIRECT_SYSCALL
    results->time_syscall_ns = -1;
#else
    results->time_syscall_ns = best_ns_syscall;
#endif
    results->time_libc_ns = best_ns_libc;
}

void benchFile(struct CallbenchResults *results, int calls, int loops, int rounds) {
    calls = default_arg(calls, 100);
    loops = default_arg(loops, 128);
    rounds = default_arg(rounds, 5);

    long best_ns_mmap = run_bench_ns(mmap_mb, calls, loops, rounds);
    long best_ns_read = run_bench_ns(file_mb, calls, loops, rounds);

    results->file_mmap_ns = best_ns_mmap;
    results->file_read_ns = best_ns_read;
}

struct CallbenchResults *allocResults() {
    struct CallbenchResults *results = malloc(sizeof(*results));
    return polyglot_from_CallbenchResults(results);
}

void freeResults(struct CallbenchResults *results) {
    free(results);
}
