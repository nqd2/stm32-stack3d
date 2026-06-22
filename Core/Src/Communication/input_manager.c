#include "Communication/input_manager.h"

InputAction_t InputManager_MapCharacter(char raw_char)
{
    switch (raw_char)
    {
        case 'w':
        case 'W':
            return INPUT_ACTION_MOVE_UP;
        case 's':
        case 'S':
            return INPUT_ACTION_MOVE_DOWN;
        case 'a':
        case 'A':
            return INPUT_ACTION_MOVE_LEFT;
        case 'd':
        case 'D':
            return INPUT_ACTION_MOVE_RIGHT;
        case '1':
        case ' ':
            return INPUT_ACTION_SELECT;
        default:
            return INPUT_ACTION_NONE;
    }
}
