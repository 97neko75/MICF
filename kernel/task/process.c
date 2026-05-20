#include "process.h"
#include <common.h>
#include "../mm/pmm.h"

#define STACK_SIZE 4096
#define MAX_PROCS 16

int proc_count = 0;
int current_pid = -1;
static int next_pid = 1;

static struct {
    uint32_t pid;
    uint32_t esp;
    uint32_t ebp;
    void (*entry)(void);
} procs[MAX_PROCS];

void process_init(void) {
    procs[0].pid = 0;
    procs[0].entry = NULL;
    __asm__ volatile (
        "mov %%esp, %0\n"
        "mov %%ebp, %1\n"
        : "=r"(procs[0].esp), "=r"(procs[0].ebp)
    );
    proc_count = 1;
    current_pid = 0;
}

int process_create(void (*entry)(void)) {
    if (proc_count >= MAX_PROCS) return -1;
    void* stack = pmm_alloc_page();
    if (!stack) return -1;
    procs[proc_count].pid = next_pid++;
    procs[proc_count].esp = (uint32_t)stack + STACK_SIZE;
    procs[proc_count].ebp = procs[proc_count].esp;
    procs[proc_count].entry = entry;
    proc_count++;
    return procs[proc_count-1].pid;
}

static void switch_to_process(int new_pid) {
    __asm__ volatile (
        "mov %%esp, %0\n"
        "mov %%ebp, %1\n"
        : "=r"(procs[current_pid].esp), "=r"(procs[current_pid].ebp)
    );
    current_pid = new_pid;
    __asm__ volatile (
        "mov %0, %%esp\n"
        "mov %1, %%ebp\n"
        : : "r"(procs[current_pid].esp), "r"(procs[current_pid].ebp)
    );
    __asm__ volatile ("jmp *%0" : : "r"(procs[current_pid].entry));
}

void process_schedule(void) {
    if (proc_count <= 1) return;
    int next = (current_pid + 1) % proc_count;
    if (next == 0) next = 1;
    if (next >= proc_count) next = 1;
    if (next == current_pid) return;
    switch_to_process(next);
}

void process_yield(void) {
    process_schedule();
}