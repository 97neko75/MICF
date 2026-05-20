#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

typedef struct process {
    uint32_t pid;
    uint32_t esp;
    uint32_t ebp;
    void (*entry)(void);
    struct process* next;
} process_t;

void process_init(void);
int process_create(void (*entry)(void));
void process_yield(void);
void process_schedule(void);
process_t* get_current_process(void);

// 全局计数变量，供 threads 命令使用
extern int proc_count;
extern int current_pid;

#endif