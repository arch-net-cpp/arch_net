#include "define.h"

namespace arch_net { namespace coin {

std::unordered_map<std::string, bool> supported_http_methods({
    {"GET", true},
    {"PUT", true},
    {"POST", true},
    {"PATCH", true},
    {"DELETE", true},
    {"HEAD", true},
    {"options", true},
    {"ALL", true},
});

}}
