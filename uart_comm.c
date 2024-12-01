#include "uart_comm.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uart_comm);

extern const struct device *uart_dev; // Referência à UART inicializada no stbs.c

// Função para construir um frame UART
void build_uart_frame(char *buffer, char device_id, char command, char *payload) {
    uint16_t checksum = 0;
    int i = 0;

    // Símbolo de sincronização
    buffer[i++] = UART_SYNC_SYMBOL;

    // ID do dispositivo
    buffer[i++] = device_id;
    checksum += device_id;

    // Comando
    buffer[i++] = command;
    checksum += command;

    // Payload
    if (payload != NULL) {
        while (*payload) {
            buffer[i++] = *payload;
            checksum += *payload++;
        }
    }

    // Adiciona os 3 dígitos do checksum
    char checksum_str[4];
    snprintf(checksum_str, sizeof(checksum_str), "%03d", checksum % 1000);
    for (int j = 0; j < 3; j++) {
        buffer[i++] = checksum_str[j];
    }

    // Símbolo de fim do frame
    buffer[i++] = UART_END_SYMBOL;

    // Terminar a string
    buffer[i] = '\0';
}

// Função para interpretar um frame UART
bool interpret_uart_frame(const char *frame, char *device_id, char *command, char *payload) {
    // Verificar se o frame começa e termina com os símbolos corretos
    if (frame[0] != UART_SYNC_SYMBOL || frame[strlen(frame) - 1] != UART_END_SYMBOL) {
        LOG_ERR("Frame inválido: Falta símbolo de sincronização ou fim de frame.\n");
        return false;
    }

    // Extrair informações do frame
    *device_id = frame[1];
    *command = frame[2];

    // Extrair payload
    int i = 3;
    int payload_len = 0;
    while (frame[i] != '\0' && frame[i] != UART_END_SYMBOL && payload_len < MAX_PAYLOAD_SIZE) {
        payload[payload_len++] = frame[i++];
    }
    payload[payload_len] = '\0';

    // Verificar e calcular checksum
    uint16_t checksum = 0;
    for (i = 1; frame[i] != '\0' && frame[i] != UART_END_SYMBOL && i < (strlen(frame) - 4); i++) {
        checksum += frame[i];
    }

    // Comparar checksum
    char received_checksum[4];
    strncpy(received_checksum, &frame[strlen(frame) - 4], 3);
    received_checksum[3] = '\0';
    if (atoi(received_checksum) != (checksum % 1000)) {
        LOG_ERR("Checksum inválido.\n");
        return false;
    }

    return true;
}

// Função para enviar uma mensagem via UART
void send_uart_message(const char *message) {
    for (size_t i = 0; i < strlen(message); i++) {
        uart_poll_out(uart_dev, message[i]);
    }
    uart_poll_out(uart_dev, '\n');
}

// Função para receber uma mensagem via UART
void receive_uart_message(char *buffer, size_t max_len) {
    size_t i = 0;
    int c;
    while (i < max_len - 1) {
        c = uart_poll_in(uart_dev, &buffer[i]);
        if (c == 0) {  // Caráter lido com sucesso
            if (buffer[i] == '\n') {
                break;
            }
            i++;
        }
    }
    buffer[i] = '\0';
}
