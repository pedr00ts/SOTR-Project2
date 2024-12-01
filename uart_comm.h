#ifndef UART_COMM_H
#define UART_COMM_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

#define UART_SYNC_SYMBOL '!'
#define UART_END_SYMBOL '#'
#define MAX_PAYLOAD_SIZE 10

void build_uart_frame(char *buffer, char device_id, char command, char *payload);
bool interpret_uart_frame(const char *frame, char *device_id, char *command, char *payload);
void send_uart_message(const char *message);
void receive_uart_message(char *buffer, size_t max_len);

#endif // UART_COMM_H
