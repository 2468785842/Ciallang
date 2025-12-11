// Copyright (c) 2024/5/21 下午8:44
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

#include "Octet.hpp"

namespace Ciallang {
    Octet::Octet(const std::uint8_t *src, std::uint32_t size) {
        _size = size;
        _buf = new std::uint8_t[_size];
        std::memcpy(_buf, src, _size);
    }

    Octet::Octet(const std::uint8_t *src1, std::uint32_t size1, const std::uint8_t *src2, std::uint32_t size2) {
        _size = size1 + size2;
        _buf = new std::uint8_t[_size];
        std::memcpy(_buf, src1, size1);
        std::memcpy(_buf + size1, src2, size2);
    }

    Octet::Octet(const Octet &oct1, const Octet &oct2) {
        _size = oct1._size + oct2._size;
        _buf = new std::uint8_t[_size];
        std::memcpy(_buf, oct1._buf, oct1._size);
        std::memcpy(_buf + oct1._size, oct2._buf, oct2._size);
    }

    void Octet::persist(std::uint8_t *dst) {
        *reinterpret_cast<decltype(_size) *>(dst) = _size;
        if(_buf) {
            std::memcpy(dst + sizeof(decltype(_size)), _buf, _size);
        }
    }
} // namespace Ciallang
