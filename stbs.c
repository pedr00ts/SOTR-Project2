#include "stbs.h"

int STBS_Init(STBS* scheduler, uint32_t tick_ms, uint8_t max_tasks) {
    scheduler->tick_ms = tick_ms;
    scheduler->ticks = 0;
    scheduler->max_tasks = max_tasks;
    scheduler->tasks = new Task[max_tasks];
    scheduler->running = 0;
    return 0;
}

int STBS_Start(STBS* scheduler) {
    //TODO: start stbs thread
    scheduler->running = 1;
}

int STBS_Stop(STBS* scheduler) {
    //TODO: stop stbs thread
    scheduler->running = 0;
}

int Create_Task(Task* t, uint32_t period_ms, uint8_t priority, char* task_id) {
    t->period_ms = period_ms;
    t->priority = priority;
    t->period_ticks = 0;
    t->activations = 0;
}

int STBS_AddTask(STBS* scheduler, Task* t) {
    //
    if ((scheduler->cycle_ticks * scheduler->tick_ms) % t->period_ms != 0)
}