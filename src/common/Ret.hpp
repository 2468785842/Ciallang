//
// Created by LiDong on 2026/1/7.
//
#pragma once

#include <cassert>
#include "types/String.hpp"

namespace cial {
    enum class ErrCode {
        None,
        TypeError,
        RangeError,
        DivideByZero,
        InvalidCast,
    };

    struct Err : std::exception {
        ErrCode code{ ErrCode::None };
        String msg{};

        Err() = default;

        Err(const ErrCode code, String msg) noexcept : code(code), msg(std::move(msg)) {}

        Err(Err &&err) noexcept : code(err.code), msg(std::move(err.msg)) {}

        Err(const Err &err) noexcept : code(err.code), msg(err.msg) {}

        Err &operator=(Err &&err) noexcept {
            if(this != &err) {
                this->~Err();
                new(this) Err(std::move(err));
            }
            return *this;
        }

        Err &operator=(const Err &err) noexcept {
            if(this != &err) {
                this->~Err();
                new(this) Err(err);
            }
            return *this;
        }

        ~Err() noexcept override = default;

        [[nodiscard]] const char *what() const noexcept override { return msg.getData(); }
    };

    template <typename T>
    class Ret {
    public:
        static Ret ok(T value) noexcept {
            Ret r;
            r._value = std::move(value);
            r._ok = true;
            return r;
        }

        static Ret err(const ErrCode code, String msg) noexcept {
            Ret r;
            r._error = Err{ code, std::move(msg) };
            r._ok = false;
            return r;
        }

        static Ret err(Err error) noexcept {
            Ret r;
            r._error = std::move(error);
            r._ok = false;
            return r;
        }

        [[nodiscard]] bool isOk() const noexcept { return _ok; }
        [[nodiscard]] bool isFailed() const noexcept { return !_ok; }

        const T &value() const {
            assert(_ok);
            return *_value;
        }

        [[nodiscard]] const Err &getErr() const {
            assert(!_ok);
            return _error;
        }

        T unwrap() {
            if(isFailed())
                throw getErr();
            return value();
        }

    private:
        bool _ok = false;
        Opt<T> _value;
        Err _error{};
    };

    template <>
    class Ret<void> {
    public:
        static Ret ok() noexcept {
            Ret r;
            r._ok = true;
            return r;
        }

        static Ret err(const ErrCode code, String msg) noexcept {
            Ret r;
            r._error = Err{ code, std::move(msg) };
            r._ok = false;
            return r;
        }

        static Ret err(Err error) noexcept {
            Ret r;
            r._ok = false;
            r._error = std::move(error);
            return r;
        }

        [[nodiscard]] bool isOk() const noexcept { return _ok; }
        [[nodiscard]] bool isFailed() const noexcept { return !_ok; }

        [[nodiscard]] Err getErr() const {
            assert(!_ok);
            return _error;
        }

    private:
        bool _ok = false;
        Err _error;
    };

} // namespace cial
