#ifndef ESPUART_H
#define ESPUART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../main.h"

#define ESPUART_RX_BUF_SIZE  256

typedef void (*espuart_callback_t)(uint8_t *data, uint16_t length);

void espuart_init(UART_HandleTypeDef *huart, DMA_HandleTypeDef *hdma, espuart_callback_t callback);
void espuart_poll(void);

#ifdef __cplusplus
}
#endif

#endif // ESPUART_H
