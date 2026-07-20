#include <stdbool.h>
#include <stdint.h>
bool via_command_user(uint8_t src, uint8_t *data, uint8_t length) {
    extern bool via_command_signalrgb(uint8_t *data, uint8_t length);
    return via_command_signalrgb(data, length);
}
