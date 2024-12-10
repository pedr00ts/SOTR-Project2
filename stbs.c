#include "stbs.h"
#include "uart_comm.h"  // Incluir as funções UART
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "leds.h" // Incluir o cabeçalho para LEDs


// Forward declaration for the thread entry function
void stbs_thread_entry(void *scheduler_ptr, void *unused1, void *unused2);

// Initializes the STBS system
int STBS_Init(STBS *scheduler, uint32_t tick_ms, uint8_t max_tasks) {
    leds_init();  // Inicializar os LEDs
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

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready\n");
        return -1;
    }

    // Initialize RTDB
    memset(&rtdb, 0, sizeof(RTDB));

    return 0;  // Success
}

// Starts the STBS scheduler
int STBS_Start(STBS *scheduler) {
    update_outputs();  // Atualizar os LEDs inicialmente
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
            LOG_INF("Task %s added.\n", t->task_id);
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
            if (cycle_period == 0) {
                cycle_period = t.period_ms;
                scheduler->tick_ms = t.period_ms;
            } else {
                cycle_period = LCM(cycle_period, t.period_ms);
                scheduler->tick_ms = GCD(scheduler->tick_ms, t.period_ms);
            }
        }
    }
    scheduler->ticks = 0;
    scheduler->cycle_ticks = cycle_period / scheduler->tick_ms;

    // Update activation ticks for each task
    for (int i = 0; i < scheduler->max_tasks; i++) {
        Task* t = &scheduler->task_list[i];
        if (t->task_id != NULL) {
            t->period_ticks = t->period_ms / scheduler->tick_ms;
        }
    }

    return scheduler->cycle_ticks;
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
            LOG_INF("Task %s removed.\n", task_id);
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
        // Atualizar entradas digitais (botões)
        update_inputs();

        // wake up threads

        for (int i = 0; i < scheduler->max_tasks; i++) {
            Task *current_task = &scheduler->task_list[i];
            if (current_task->task_id != NULL) {
                // Check if the task should be activated in this cycle
                if (scheduler->ticks % current_task->period_ticks == 0) {
                    LOG_INF("Activating task: %s (Activation %d)\n",
                            current_task->task_id, current_task->activations + 1);
                    current_task->activations++;

                    // Construir o frame UART para informar sobre a ativação da tarefa
                    char frame[50];
                    build_uart_frame(frame, 'M', 'A', current_task->task_id);
                    send_uart_message(frame);
                }
            }
        }

        // Atualizar saídas digitais (LEDs)
        update_outputs();

        // Wait until next tick period
        STBS_WaitPeriod(scheduler);
    }
}


// Função para atualizar o estado das entradas digitais (botões)
void update_inputs() {
    // Supondo que temos uma função fictícia para ler os botões
    for (int i = 0; i < 4; i++) {
        rtdb.inputs[i] = read_button_state(i);  // Substituir por uma função real de leitura
    }
}

// Função para atualizar o estado das saídas digitais (LEDs)
void update_outputs() {
    // Atualizar os LEDs com base nos valores do RTDB
    for (int i = 0; i < 4; i++) {
        set_led_state(i, rtdb.outputs[i]);  // Substituir por uma função real para definir o LED
    }
}

// Utility functions
uint32_t GCD(uint32_t a, uint32_t b) {
    uint32_t aux;
    while (b != 0) {
        aux = b;
        b = a % b;
        a = aux;
    }
    return a;
}

uint32_t LCM(uint32_t a, uint32_t b) {
    return a * b / GCD(a, b);
}
