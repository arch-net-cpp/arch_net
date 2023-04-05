#pragma once

namespace arch_net {

class TNonCopyable {
protected:
    TNonCopyable() = default;

    ~TNonCopyable() = default;

    TNonCopyable(const TNonCopyable &) = delete;

    TNonCopyable &operator=(const TNonCopyable &) = delete;
};
}