#ifndef __INPUT_MANAGER_H
#define __INPUT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_ACTION_NONE,
    INPUT_ACTION_MOVE_UP,
    INPUT_ACTION_MOVE_DOWN,
    INPUT_ACTION_MOVE_LEFT,
    INPUT_ACTION_MOVE_RIGHT,
    INPUT_ACTION_SELECT
} InputAction_t;

InputAction_t InputManager_MapCharacter(char raw_char);

#ifdef __cplusplus
}
#endif

#endif /* __INPUT_MANAGER_H */
