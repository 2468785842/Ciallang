/*
 * Copyright (c) 2024/6/13 下午8:15
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#pragma once

#include <fmt/ostream.h>

#include "logging/Logger.hpp"
#include "parser/AtomTable.hpp"
#include "parser/OctetTable.hpp"

#include "Runtime.hpp"

namespace Cial::Bytecode {
    class Chunk;
}

namespace Cial {
    class [[nodiscard]] ConstIdx {
    public:
        explicit ConstIdx(const size_t index) : _index(index) {}

        [[nodiscard]] size_t index() const noexcept { return _index; }

    private:
        size_t _index;

        friend std::ostream &operator<<(std::ostream &os, const ConstIdx &reg) { return os << '*' << reg._index; }
    };

    struct FuncMeta {
        Atom name;
        std::uint32_t arity;
        Bytecode::Chunk *chunk;
        explicit FuncMeta(const Atom name, const std::uint32_t argCount, Bytecode::Chunk *chunk) :
            name(name), arity(argCount), chunk(chunk) {}
        ~FuncMeta() noexcept;
        bool operator==(const FuncMeta &funcMeta) const { return false; }
    };

    struct ClassMeta {
        std::uint32_t argCount;
        bool operator==(const ClassMeta &classMeta) const { return false; }
    };

    // Enum, Type, Value

#define CONSTANT_TYPE_ENUM_F(O, PointerF, ValueF)                                                                      \
    O(Integer, Integer, integer, ValueF)                                                                               \
    O(Real, Real, real, ValueF)                                                                                        \
    O(Atom, Atom, atom, ValueF)                                                                                        \
    O(OctetIdx, OctetIdx, octetIdx, ValueF)                                                                            \
    O(FuncMeta, FuncMeta *, funcMeta, PointerF)                                                                        \
    O(ClassMeta, ClassMeta *, classMeta, PointerF)

#define CONSTANT_TYPE_ENUM(O) CONSTANT_TYPE_ENUM_F(O, , )

    enum class ConstantType : std::uint8_t {
#define ENUM(E, T, V, F) E,
        None,
        CONSTANT_TYPE_ENUM(ENUM)
#undef ENUM
    };

    struct Constant {
        constexpr explicit Constant(const ConstantType type, const std::uint64_t value) : _type(type), _value{} {
            static_assert(sizeof(_value) == sizeof(value));
            std::memcpy(&_value, &value, sizeof(value));
        }

        constexpr explicit Constant() : _type(ConstantType::None), _value{} {}
#define DEF_CONSTRUCT(E, T, V, F)                                                                                      \
    constexpr explicit Constant(T value) : _type(ConstantType::E), _value{ .V = value } {}
        CONSTANT_TYPE_ENUM(DEF_CONSTRUCT)
#undef DEF_CONSTRUCT

        Constant(const Constant &constant) noexcept = delete;

        Constant &operator=(const Constant &constant) noexcept = delete;

        Constant(Constant &&constant) noexcept : _type(constant._type), _value(constant._value) {
            constant._type = ConstantType::None;
        }

        Constant &operator=(Constant &&constant) noexcept {
            if(this != &constant) {
                this->~Constant();
                new(this) Constant{ std::move(constant) };
            }
            return *this;
        }

        ~Constant() {
            switch(_type) {
#define RELEASE_POINTER(E, F)                                                                                          \
    case ConstantType::E:                                                                                              \
        delete _value.F;                                                                                               \
        break;
#define RELEASE_VALUE(E, F)
#define CHECK_RELEASE_VALUE(E, T, V, F) F(E, V)
                CONSTANT_TYPE_ENUM_F(CHECK_RELEASE_VALUE, RELEASE_POINTER, RELEASE_VALUE)
#undef CHECK_RELEASE_VALUE
#undef RELEASE_POINTER
#undef RELEASE_VALUE
                default:
                    break;
            }
        }

        [[nodiscard]] constexpr ConstantType type() const noexcept { return _type; }

        template <typename T>
        [[nodiscard]] constexpr T value() const noexcept {
            if constexpr(!std::is_same_v<T, T>) {
                /* hook */
            }
#define CHECK_RET_VALUE(E, Type, V, F)                                                                                 \
    else if constexpr(std::is_same_v<T, Type>) {                                                                       \
        CLL_ASSERT(_type == ConstantType::E, "value type is not " #V);                                                 \
        return _value.V;                                                                                               \
    }
            CONSTANT_TYPE_ENUM(CHECK_RET_VALUE)
#undef CHECK_RET_VALUE
            else {
                static_assert(!std::is_same_v<T, T> && "value type is not support");
            }
            throw std::logic_error("unreachable");
        }

        bool operator==(const Constant &value) const {
            if(_type != value._type)
                return false;
            switch(_type) {
#define CHECK_EQ(E, T, V, F)                                                                                           \
    case ConstantType::E:                                                                                              \
        return F _value.V == F value._value.V;
                CONSTANT_TYPE_ENUM_F(CHECK_EQ, *, )
#undef CHECK_EQ
                default:
                    return true;
            }
        }

        [[nodiscard]] Value createValue(const Runtime &rt) const noexcept;

    private:
        ConstantType _type;
        union {
#define DEF_VALUE(E, T, V, F) T V;
            CONSTANT_TYPE_ENUM(DEF_VALUE)
#undef DEF_VALUE
        } _value; // Integer, Real or Atom(String Index)
    };

} // namespace Cial

template <>
struct fmt::formatter<Cial::ConstIdx> : ostream_formatter {};
