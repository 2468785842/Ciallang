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
#include <ranges>

#include "Register.hpp"
#include "gen/LocalVariable.hpp"

#include "logging/Logger.hpp"
#include "runtime/AtomTable.hpp"
#include "runtime/OctetTable.hpp"
#include "types/Value.hpp"

namespace Cial {
    class Runtime;
    namespace Bytecode {
        class Chunk;
    }
} // namespace Cial

namespace Cial {
    class [[nodiscard]] ConstIdx {
    public:
        explicit ConstIdx(const size_t index) : _index(index) {}

        [[nodiscard]] size_t index() const noexcept { return _index; }

    private:
        size_t _index;

        friend std::ostream &operator<<(std::ostream &os, const ConstIdx &reg) { return os << '*' << reg._index; }
    };

    struct FuncMeta : MarkSweepHeader {
        std::uint32_t arity;
        Bytecode::Chunk *chunk; // manager for gc
        Vec<LocalVariable> localVars; // the first vectorIndex = ScopeLevel; the second is vars

        explicit FuncMeta(FuncMeta &&funcMeta) = delete;
        FuncMeta &operator=(FuncMeta &&funcMeta) = delete;
        explicit FuncMeta(const FuncMeta &) noexcept = delete;
        FuncMeta &operator=(const FuncMeta &) noexcept = delete;

        void marked() noexcept override;

        bool operator==(const FuncMeta &) const { return false; }

    private:
        friend class Runtime;
        explicit FuncMeta(const std::uint32_t arity, Bytecode::Chunk *chunk, Vec<LocalVariable> &&localVars) :
            arity(arity), chunk(chunk), localVars(localVars) {}
    };

    struct MemberShapMeta {
        Atom name{ ATOM_INVALID };
        bool isMethod{ false };
        bool isStatic{ false };
        bool isConst{ false };
    };

    struct PropMeta : MarkSweepHeader {
        FuncMeta *setFunc;
        FuncMeta *getFunc;

        void marked() noexcept override {
            MarkSweepHeader::marked();
            if(setFunc) {
                setFunc->marked();
            }
            if(getFunc) {
                getFunc->marked();
            }
        }

    private:
        friend class Runtime;
        explicit PropMeta(FuncMeta *setFunc, FuncMeta *getFunc) : setFunc(setFunc), getFunc(getFunc) {}
    };

    struct ClassMeta : MarkSweepHeader {
        Atom className;
        std::uint32_t arity;
        Vec<MemberShapMeta> memberShapMetas;
        Vec<MarkSweepHeader *> memberMetas;

        explicit ClassMeta(ClassMeta &&classMeta) = delete;
        ClassMeta &operator=(ClassMeta &&classMeta) = delete;
        explicit ClassMeta(const ClassMeta &) noexcept = delete;
        ClassMeta &operator=(const ClassMeta &) noexcept = delete;

        [[nodiscard]] bool hasMember(const Atom name) const noexcept {
            const size_t len = memberShapMetas.size();
            for(size_t i = 0; i < len; ++i) {
                if(memberShapMetas[i].name == name) {
                    return true;
                }
            }
            return false;
        }

        template <typename T>
            requires std::is_same_v<T, PropMeta> || std::is_same_v<T, FuncMeta>
        void setMember(MemberShapMeta shapMeta, T *memberMeta) noexcept {
            shapMeta.isMethod = std::is_same_v<T, FuncMeta>;
            const size_t len = memberShapMetas.size();
            for(size_t i = 0; i < len; ++i) {
                if(auto &memberShapeMeta = memberShapMetas[i]; memberShapeMeta.name == shapMeta.name) {
                    memberShapeMeta = shapMeta;
                    memberMetas[i] = memberMeta;
                    return;
                }
            }
            memberShapMetas.push_back(shapMeta);
            memberMetas.push_back(memberMeta);
        }

        template <typename T>
            requires std::is_same_v<T, PropMeta> || std::is_same_v<T, FuncMeta>
        T *getMember(const Atom &name) noexcept {
            T *memberMeta{};
            const size_t len = memberShapMetas.size();
            for(size_t i = 0; i < len; ++i) {
                if(memberShapMetas[i].name == name) {
                    memberMeta = static_cast<T *>(memberMetas[i]);
                }
            }
            return memberMeta;
        }

        void marked() noexcept override {
            MarkSweepHeader::marked();
            for(const auto &v : memberMetas) {
                v->marked();
            }
        }

        bool operator==(const ClassMeta &) const { return false; }

    private:
        friend class Runtime;
        explicit ClassMeta(const Atom className, const std::uint32_t arity) : className(className), arity(arity) {}
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
                    return false;
            }
        }

        [[nodiscard]] Value createValue(Runtime *rt) const noexcept;

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
