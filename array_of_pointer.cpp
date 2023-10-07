#include <cstdio>
#include <cstring>
#include <cassert>
#include <sched.h>
#include <cstdint>
#include <random>
#include <atomic>
#include <thread>

#define ARRAY_SIZE 1048576
#define STOP_POS 524288
#define TEST_POS 786432

void pin_one_cpu(int cpu) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu, &cpu_set);
    assert(sched_setaffinity(0, sizeof(cpu_set), &cpu_set) == 0);
}

long *array[ARRAY_SIZE];
long array2[ARRAY_SIZE];

char flush_cache() {
    long size = 1024*1024*1024;
    char *arr = new char[size];
    memset(arr, 0xff, size);
    char res = 0;
    for (int i=0;i<size;i++) {
        res ^= arr[i];
    }
    delete[] arr;
    return res;
}

void init_array() {
    std::mt19937 rng;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        array2[i] = rng() & 0x7fffffffffffffffl;
        array[i] = &array2[rng() % ARRAY_SIZE];
    }
}

volatile uint64_t global_clock[1];

void clock_thread(int pin_cpu) {
    pin_one_cpu(pin_cpu);
    while (true) {
        global_clock[0] ++;
    }
}

uint64_t test() {
    flush_cache();
    atomic_thread_fence(std::memory_order_seq_cst);
    long p = 0;
    // Test pos
    int i = 0;
    while (i < STOP_POS) {
        p ^= *array[i];
        i ++;
    }
    atomic_thread_fence(std::memory_order_seq_cst);
    uint64_t count1 = global_clock[p & 0x8000000000000000l];
    uint64_t value = *array[(i+p&0xff) | (count1 & 0x8000000000000000l)];
    p ^= value;
    uint64_t count2 = global_clock[p & 0x8000000000000000l];
    uint64_t time_test = count2 - count1;
    flush_cache();
    atomic_thread_fence(std::memory_order_seq_cst);
    // Test baseline
    p ^= p & 0x8000000000000000l;
    p ^= (long)array[(i+p&0xff) | (count1 & 0x8000000000000000l)] & 0x8000000000000000l;
    atomic_thread_fence(std::memory_order_seq_cst);
    uint64_t count3 = global_clock[p & 0x8000000000000000l];
    uint64_t value2 = *array[(i+p&0xff) | (count1 & 0x8000000000000000l)];
    p ^= value2;
    uint64_t count4 = global_clock[p & 0x8000000000000000l];
    uint64_t time_base = count4 - count3;
    printf("test:%ld, base:%d\n", time_test, time_base);
    return value ^ value2;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: ./array_of_pointer [PIN_CPU] [COUNTER_CPU=7]\n\nExample: ./array_of_pointer 4 7\n");
        return 1;
    }
    int pin_cpu = atoi(argv[1]);
    int counter_cpu = 7;
    if (argc >= 3) counter_cpu = atoi(argv[2]);
    std::thread clock_counter_thread(clock_thread, counter_cpu);
    clock_counter_thread.detach();
    pin_one_cpu(pin_cpu);

    init_array();
    for (int i=0;i<8;i++) {
        test();
    }
    return 0;
}
