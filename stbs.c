#include "stbs.h"
#include "uart_comm.h"  // Incluir as funções UART
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>


#define BUTTON0_NODE DT_ALIAS(sw0)
#define BUTTON1_NODE DT_ALIAS(sw1)
#define BUTTON2_NODE DT_ALIAS(sw2)
#define BUTTON3_NODE DT_ALIAS(sw3)

// Verificar se o node existe no DeviceTree
#if !DT_NODE_HAS_STATUS(BUTTON0_NODE, okay) || \
    !DT_NODE_HAS_STATUS(BUTTON1_NODE, okay) || \
    !DT_NODE_HAS_STATUS(BUTTON2_NODE, okay) || \
    !DT_NODE_HAS_STATUS(BUTTON3_NODE, okay)
#error "Unsupported board: button devicetree aliases are not defined"
#endif

// GPIO dev and pin configuration
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET_OR(BUTTON0_NODE, gpios, {0});
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET_OR(BUTTON1_NODE, gpios, {0});
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET_OR(BUTTON2_NODE, gpios, {0});
static const struct gpio_dt_spec button3 = GPIO_DT_SPEC_GET_OR(BUTTON3_NODE, gpios, {0});


LOG_MODULE_REGISTER(button_control);

// UART device definition
const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

// RTDB structure definition
typedef struct {
    uint8_t inputs[4];  // Valores dos botões
    uint8_t outputs[4]; // Valores dos LEDs
} RTDB;

RTDB rtdb;

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

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready\n");
        return -1;
    }

    // Initialize RTDB
    memset(&rtdb, 0, sizeof(RTDB));

    return 0;  // Success
}
void buttons_init() {
    if (!device_is_ready(button0.port) ||
        !device_is_ready(button1.port) ||
        !device_is_ready(button2.port) ||
        !device_is_ready(button3.port)) {
        LOG_ERR("GPIO device is not ready\n");
        return;
    }

    gpio_pin_configure_dt(&button0, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&button1, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&button2, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&button3, GPIO_INPUT | GPIO_PULL_UP);

    LOG_INF("Buttons have been initialized successfully\n");
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
    // Supondo que temos uma função fictícia para definir o estado dos LEDs
    for (int i = 0; i < 4; i++) {
        set_led_state(i, rtdb.outputs[i]);  // Substituir por uma função real para definir o LED
    }
}

// UART send function
void send_uart_message(const char *message) {
    for (size_t i = 0; i < strlen(message); i++) {
        uart_poll_out(uart_dev, message[i]);
    }
    uart_poll_out(uart_dev, '\n');
}

// UART receive function
void receive_uart_message(char *buffer, size_t max_len) {
    size_t i = 0;
    int c;
    while (i < max_len - 1) {
        c = uart_poll_in(uart_dev, &buffer[i]);
        if (c == 0) {  // Character successfully read
            if (buffer[i] == '\n') {
                break;
            }
            i++;
        }
    }
    buffer[i] = '\0';
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

uint8_t read_button_state(int button_index) {
    int state = 0;
    switch (button_index) {
        case 0:
            state = gpio_pin_get_dt(&button0);
            break;
        case 1:
            state = gpio_pin_get_dt(&button1);
            break;
        case 2:
            state = gpio_pin_get_dt(&button2);
            break;
        case 3:
            state = gpio_pin_get_dt(&button3);
            break;
        default:
            LOG_ERR("Invalid button index: %d\n", button_index);
            return 0;
    }

    return (state == 0) ? 1 : 0;  // 1 para pressionado, 0 para não pressionado (assumindo pull-up)
}

void set_led_state(int led_index, uint8_t state) {
    // Função fictícia para definir o estado de um LED
    // Implementar controlo real aqui
}
void update_inputs() {
    // Ler o estado dos botões e atualizar o RTDB
    for (int i = 0; i < 4; i++) {
        rtdb.inputs[i] = read_button_state(i);
    }
}
