#include "stbs.h"
#include <stdlib.h>  // for malloc, free
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

K_THREAD_STACK_DEFINE(stbs_stack_area, 1024);
struct k_thread stbs_thread_data;
k_tid_t stbs_thread_id;

// Forward declaration for the thread entry function
void stbs_thread_entry(void *scheduler_ptr, void *unused1, void *unused2);

// Initializes the STBS system
int STBS_Init(STBS *scheduler, uint32_t tick_ms, uint8_t max_tasks) {
    scheduler->tick_ms = tick_ms;
    scheduler->ticks = 0;
    scheduler->max_tasks = max_tasks;
    scheduler->task_list = (Task *)malloc(sizeof(Task) * max_tasks);
    scheduler->cycle_ticks = 0;
    scheduler->running = false;

    if (scheduler->task_list == NULL) {
        LOG_ERR("Failed to allocate memory for tasks\n");
        return -1;  // Indicate failure
    }

    // Initialize all task slots to NULL
    for (int i = 0; i < max_tasks; i++) {
        scheduler->task_list[i].task_id = NULL;
    }

    return 0;  // Success
}

// Starts the STBS scheduler
int STBS_Start(STBS *scheduler) {
    if (!scheduler->running) {
        // Initialize cycle_ticks based on current tasks' periods
        scheduler->cycle_ticks = 0;
        STBS_CalculateTicks(scheduler);
        scheduler->running = true;

        // Create Zephyr thread to execute scheduler tasks
        stbs_thread_id = k_thread_create(&stbs_thread_data, stbs_stack_area,
                                         K_THREAD_STACK_SIZEOF(stbs_stack_area),
                                         stbs_thread_entry,
                                         scheduler, NULL, NULL,
                                         0, 0, K_NO_WAIT);  // priority 0, no options, start immediately
        
        LOG_INF("Scheduler has started\n");
        return 0;
    }
    return -1;  // Scheduler was already running
}

// Stops the STBS scheduler
int STBS_Stop(STBS *scheduler) {
    if (scheduler->running) {
        scheduler->running = false;
        k_thread_abort(stbs_thread_id);  // Abort the scheduler thread
        LOG_INF("Scheduler has stopped\n");
        return 0;
    }
    return -1;  // Scheduler was not running
}

// Create a task
int Create_Task(Task *t, uint32_t period_ms, uint8_t priority, char *task_id) {
    t->period_ms = period_ms;
    t->priority = priority;
    t->period_ticks = 0;
    t->activations = 0;
    t->task_id = task_id;
    return 0;  // Success
}

// Adds a task to the scheduler
int STBS_AddTask(STBS *scheduler, Task *t) {
    // Stop scheduler if running
    if (scheduler->running)
        STBS_Stop(scheduler);

    // Add task
    for (int i = 0; i < scheduler->max_tasks; i++) {
        if (scheduler->task_list[i].task_id == NULL) {
            scheduler->task_list[i] = *t;
            LOG_INF("Task %s added.\n",t->task_id);
            return 0;  // Successfully added
        }
    }

    LOG_ERR("No available slots for adding a new task.\n");
    return -1;  // Failure
}

uint32_t STBS_CalculateTicks(STBS* scheduler) {
    
    uint32_t cycle_period = 0;

    // Determine microcycle duration
    for (int i = 0; i < scheduler->max_tasks; i++) {
        Task t = scheduler->task_list[i];
        if (t.task_id != NULL) {
            if (i == 0) {
                cycle_period = t.period_ms;
                scheduler->tick_ms = t.period_ms;
            }
            else {
                cycle_period = LCM(cycle_period, t.period_ms)
                scheduler->tick_ms = GCD(scheduler->tick_ms, t.period_ms)
            }
        }
    }
    scheduler->ticks = 0;
    scheduler->cycle_ticks = cycle_period / scheduler->tick_ms;

    // Update activation ticks for each task
    for (int i = 0; i < scheduler->max_tasks; i++) {
        Task t = scheduler->task_list[i];
        if (t.task_id != NULL) {
            t.period_ticks = t.period_ms / scheduler->tick_ms;
        }
    }
}

// Removes a task from the scheduler
int STBS_RemoveTask(STBS *scheduler, char *task_id) {
    
    // Stop scheduler if running
    if (scheduler->running)
        STBS_Stop(scheduler);

    for (int i = 0; i < scheduler->max_tasks; i++) {
        if (scheduler->task_list[i].task_id != NULL &&
            strcmp(scheduler->task_list[i].task_id, task_id) == 0) {
            scheduler->task_list[i].task_id = NULL;  // Mark slot as unused
            LOG_INF("Task %s removed.\n",task_id);
            return 0;  // Successfully removed
        }
    }
    LOG_ERR("Task not found.\n");
    return -1;  // Failure
}

// Task waits for its next activation
void STBS_WaitPeriod(STBS *scheduler) {
    if (scheduler->running) {
        k_msleep(scheduler->tick_ms);
        scheduler->ticks++;
    }
}

// Scheduler thread entry function
void stbs_thread_entry(void *scheduler_ptr, void *unused1, void *unused2) {
    STBS *scheduler = (STBS *)scheduler_ptr;

    while (scheduler->running) {
        for (int i = 0; i < scheduler->max_tasks; i++) {
            Task *current_task = &scheduler->task_list[i];
            if (current_task->task_id != NULL) {
                // Check if the task should be activated in this cycle
                if (scheduler->ticks % current_task->period_ticks == 0) {
                    LOG_INF("Activating task: %s (Activation %d)\n",
                        current_task->task_id, current_task->activations + 1);
                    current_task->activations++;
                }
            }
        }
        // Wait until next tick period
        STBS_WaitPeriod(scheduler);
    }
}

// Prints contents of the STBS scheduler
void STBS_print(STBS *scheduler) {
    LOG_INF("Scheduler State:\n");
    LOG_INF("Tick Interval (ms): %d\n", scheduler->tick_ms);
    LOG_INF("Number of Tasks: %d\n", scheduler->max_tasks);
    LOG_INF("Running: %s\n", scheduler->running ? "Yes" : "No");
    for (int i = 0; i < scheduler->max_tasks; i++) {
        if (scheduler->task_list[i].task_id != NULL) {
            LOG_INF("Task %s: Period %d ms, Priority %d, Activations %d\n",
                   scheduler->task_list[i].task_id,
                   scheduler->task_list[i].period_ms,
                   scheduler->task_list[i].priority,
                   scheduler->task_list[i].activations);
        }
    }
}

// Prints information of a specific task
void STBS_printTask(STBS *scheduler, char *task_id) {
    for (int i = 0; i < scheduler->max_tasks; i++) {
        if (scheduler->task_list[i].task_id != NULL &&
            strcmp(scheduler->task_list[i].task_id, task_id) == 0) {
            LOG_INF("Task %s: Period %d ms, Priority %d, Activations %d\n",
                   scheduler->task_list[i].task_id,
                   scheduler->task_list[i].period_ms,
                   scheduler->task_list[i].priority,
                   scheduler->task_list[i].activations);
            return;
        }
    }
    LOG_ERR("Task %s not found.\n", task_id);
}
