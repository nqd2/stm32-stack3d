#ifndef ESPI2C_H
#define ESPI2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../main.h"

#define ESPI2C_MAX_BUFFER_SIZE  256

typedef void (*espi2c_callback_t)(uint8_t *data, uint16_t length);

void espi2c_init(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart, espi2c_callback_t callback);
void espi2c_poll(void);

#ifdef __cplusplus
}
#endif

#endif // ESPI2C_H
