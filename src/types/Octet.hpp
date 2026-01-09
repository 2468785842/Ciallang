// Copyright (c) 2024/5/23 下午9:43
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

#pragma once

#include "gc/gc.hpp"

namespace cial {
    class Octet final {
    public:
        explicit Octet(const std::uint8_t *src, std::uint32_t size);

        explicit Octet(const std::uint8_t *src1, std::uint32_t size1, const std::uint8_t *src2, std::uint32_t size2);

        explicit Octet(const Octet &oct1, const Octet &oct2);

        Octet(Octet &&oct) noexcept : _buf(oct._buf), _size(oct._size) { oct._buf = nullptr; }

        Octet &operator=(Octet &&oct) noexcept {
            if(this != &oct) {
                this->~Octet();
                new(this) Octet(std::move(oct));
            }
            return *this;
        }

        Octet(const Octet &oct) noexcept : Octet(oct._buf, oct._size) {}

        Octet &operator=(const Octet &oct) noexcept {
            if(this != &oct) {
                this->~Octet();
                new(this) Octet{ oct };
            }
            return *this;
        }

        ~Octet() noexcept { delete[] _buf; }

        [[nodiscard]] std::uint32_t getSize() const { return _size; }

        [[nodiscard]] const std::uint8_t *getData() const { return _buf; }

        [[nodiscard]] int getPersistSize() const { return static_cast<int>(sizeof(decltype(_size)) + _size); }

        void persist(std::uint8_t *dst);

    private:
        std::uint8_t *_buf;
        std::uint32_t _size;
    };


} // namespace cial
