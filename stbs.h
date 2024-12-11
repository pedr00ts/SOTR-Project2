#ifndef DEF_STBS
#define DEF_STBS

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <string.h>

typedef struct {
    uint32_t period_ms;
    uint8_t priority;
    uint8_t period_ticks;   // number of microcycle ticks between activations
    uint8_t activations;
    char* task_id;
    k_tid_t tid;
} Task;

typedef struct {
    uint32_t tick_ms;       // duration of a microcycle tick in ms
    uint32_t ticks;         // number of microcycle ticks since table generation
    uint8_t max_tasks;
    Task* task_list;
    uint8_t cycle_ticks;    // number of microcycle ticks in a macrocycle
    bool running;
} STBS;

// Initializes the STBS system
int STBS_Init(STBS* scheduler, uint32_t tick_ms, uint8_t max_tasks);

// Starts the STBS scheduler
// returns 0 on success
int STBS_Start(STBS* scheduler);

// Terminates the STBS scheduler
int STBS_Stop(STBS* scheduler);

// Create task struct. Task thread must be defined
int Create_Task(Task* t, uint32_t period_ms, uint8_t priority, char* task_id, k_tid_t tid);

// Adds a task to the scheduler
// If necessary, stops the scheduler to adjust vars
int STBS_AddTask(STBS* scheduler, Task* t);

// Recalculates temporal values; useful when tasks are added/removed
uint32_t STBS_CalculateTicks(STBS* scheduler);

// Removes a task from the scheduler
int STBS_RemoveTask(STBS* scheduler, char* task_id);

// Task waits for its next activation
void STBS_WaitPeriod(STBS* scheduler);

// Prints contents of the STBS
void STBS_print(STBS* scheduler);

// Prints information of a certain task in scheduler with provided id
void STBS_printTaskByID(STBS* scheduler, char* task_id);

// Prints information of given task
void STBS_printTask(Task* t);

// returns greater common divisor between integers a and b
uint32_t GCD(uint32_t a, uint32_t b);

// returns least common multiple between integers a and b
uint32_t LCM(uint32_t a, uint32_t b);

#endif