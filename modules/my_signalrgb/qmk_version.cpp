#define restrict
#include <string_view>

extern "C" {

#include "version.h"
#include "qmk_version.h"

constexpr uint8_t get_byte(int n) {
    std::string_view ver = QMK_VERSION;
    int begin = 0;
    while (n--) {
        begin = ver.find('.', begin) + 1;
    }
    int end = ver.find_first_of(".-", begin);
    auto subs = ver.substr(begin, end-begin);
    uint8_t res = 0;
    for (char c : subs) {
        res *= 10;
        res += c - '0';
    }
    return res;
}

const QmkVersion kQmkVersion = {
    .major = get_byte(0),
    .minor = get_byte(1),
    .patch = get_byte(2),
};

}
