#ifndef STBS
#define STBS

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

typedef struct {
    uint32_t tick_ms;
    uint32_t ticks;
    uint8_t max_tasks;
    Task* task_list;
    uint8_t cycle_ticks;
    bool running;
} STBS;

typedef struct {
    uint32_t period_ms;
    uint8_t priority;
    uint8_t period_ticks;
    uint8_t activations;
    char* task_id;
} Task;

// Initializes the STBS system
int STBS_Init(STBS* scheduler, uint32_t tick_ms, uint8_t max_tasks);

// Starts the STBS scheduler
// returns 0 on success
int STBS_Start(STBS* scheduler) {
    //TODO: define cycle_ticks; start stbs thread
    scheduler->running = 1;
}

// Terminates the STBS scheduler
int STBS_Stop(STBS* scheduler) {
    //TODO: stop stbs thread
    scheduler->running = 0;
}

int Create_Task(Task* t, uint32_t period_ms, uint8_t priority, char* task_id);

// Adds a task to the scheduler
// If necessary, stops the scheduler to adjust vars
int STBS_AddTask(STBS* scheduler, Task* t);

// Removes a task from the scheduler
int STBS_RemoveTask(STBS* scheduler, char* task_id);

// Task waits for its next activation
void STBS_WaitPeriod(STBS* scheduler);

// Prints contents of the STBS
void STBS_print(STBS* scheduler);

// Prints information of a certain task
void STBS_printTask(STBS* scheduler, char* task_id);

#endif