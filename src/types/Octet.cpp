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

namespace cial {
    Octet::Octet(const u8 *src, const u32 size) {
        _size = size;
        _buf = new u8[_size];
        std::memcpy(_buf, src, _size);
    }

    Octet::Octet(const u8 *src1, const u32 size1, const u8 *src2, const u32 size2) {
        _size = size1 + size2;
        _buf = new u8[_size];
        std::memcpy(_buf, src1, size1);
        std::memcpy(_buf + size1, src2, size2);
    }

    Octet::Octet(const Octet &oct1, const Octet &oct2) {
        _size = oct1._size + oct2._size;
        _buf = new u8[_size];
        std::memcpy(_buf, oct1._buf, oct1._size);
        std::memcpy(_buf + oct1._size, oct2._buf, oct2._size);
    }

    void Octet::persist(u8 *dst) {
        *reinterpret_cast<decltype(_size) *>(dst) = _size;
        if(_buf) {
            std::memcpy(dst + sizeof(decltype(_size)), _buf, _size);
        }
    }

} // namespace cial
