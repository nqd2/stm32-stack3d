#ifndef APP_STATE_MANAGER_H
#define APP_STATE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../main.h"

#include "Communication/input_manager.h"

void AppStateManager_Init(void);
void AppStateManager_Update(float delta_seconds);
void AppStateManager_Render(void);
void AppStateManager_HandleAction(InputAction_t action);

extern int16_t cursor_x;
extern int16_t cursor_y;
extern uint8_t show_cursor;

#ifdef __cplusplus
}
#endif

#endif // APP_STATE_MANAGER_H
