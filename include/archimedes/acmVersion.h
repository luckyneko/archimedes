#pragma once

#include <cstdint>

namespace acm
{
    struct Version
    {
        uint8_t major{ 0 };
        uint8_t minor{ 0 };
        uint8_t patch{ 0 };
        uint8_t unused{ 0 };
    };
    static const acm::Version VERSION = {0, 1, 0, 0};
}
