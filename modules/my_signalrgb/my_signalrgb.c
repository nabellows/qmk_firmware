#include <stdbool.h>
#include <stdint.h>
#include "rgb_matrix.h"
#include "signalrgb.h"

extern bool via_command_signalrgb(uint8_t *data, uint8_t length);

__attribute__((weak)) bool via_command_user(uint8_t src, uint8_t *data, uint8_t length) {
    if (rgb_matrix_is_enabled()) {
        if (data[0] == STREAM_RGB_DATA && rgb_matrix_get_mode() != RGB_MATRIX_SIGNALRGB) {
            rgb_matrix_mode_noeeprom(RGB_MATRIX_SIGNALRGB);
        }
        if (via_command_signalrgb(data, length)) {
            extern bool via_raw_hid_send(uint8_t src, uint8_t *data, uint8_t length);
            via_raw_hid_send(src, data, length);
            return true;
        }
    }
    return false;
}
