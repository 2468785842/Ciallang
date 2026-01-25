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

#include "Register.hpp"
#include "gen/LocalVariable.hpp"

#include "runtime/AtomTable.hpp"
#include "types/Value.hpp"

namespace cial {
    class Runtime;
    namespace Bytecode {
        class Chunk;
    }
} // namespace cial

namespace cial {
    class [[nodiscard]] ConstIdx {
    public:
        explicit ConstIdx(const size_t index) : _index(index) {}

        [[nodiscard]] size_t index() const noexcept { return _index; }

    private:
        size_t _index;

        friend std::ostream &operator<<(std::ostream &os, const ConstIdx &reg) { return os << '*' << reg._index; }
    };

    struct FuncMeta : MarkSweepHeader {
        Atom name = ATOM_INVALID;
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
        explicit FuncMeta(const std::uint32_t arity, Bytecode::Chunk *chunk, Vec<LocalVariable> localVars) :
            arity(arity), chunk(chunk), localVars(std::move(localVars)) {}
    };

    struct MemberShapeMeta {
        Atom name{ ATOM_INVALID };
        bool isVar{ false };
        bool isProp{ false };
        bool isMethod{ false };
        bool isStatic{ false };
        // bool isConst{ false };
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

    struct ClassFieldMeta {
        union {
            PropMeta *propMeta{};
            FuncMeta *funcMeta;
        };

        explicit ClassFieldMeta() = default;

        explicit ClassFieldMeta(PropMeta *propMeta) : propMeta(propMeta) {}
        explicit ClassFieldMeta(FuncMeta *funcMeta) : funcMeta(funcMeta) {}
    };

    struct ClassMeta : MarkSweepHeader {
        Atom className;
        std::uint32_t arity;
        FuncMeta *initDefaultVal{};
        FuncMeta *constructor{};
        Vec<Atom> extends; // 因为动态语言无法在生成bytecode时决定class所以,只有new
                           // object是通过名字去查找继承父类,和TJS2一致的行为
        Vec<MemberShapeMeta> memberShapeMetas;
        Vec<ClassFieldMeta> memberMetas;

        explicit ClassMeta(ClassMeta &&classMeta) = delete;
        ClassMeta &operator=(ClassMeta &&classMeta) = delete;
        explicit ClassMeta(const ClassMeta &) noexcept = delete;
        ClassMeta &operator=(const ClassMeta &) noexcept = delete;

        [[nodiscard]] std::int64_t hasMember(const Atom name) const noexcept {
            const size_t len = memberShapeMetas.size();
            for(std::int64_t i = 0; i < len; ++i) {
                if(memberShapeMetas[i].name == name) {
                    return i;
                }
            }
            return -1;
        }

        /**
         * support shadow
         * @param shapeMeta
         * @param memberMeta
         */
        void setMember(const MemberShapeMeta shapeMeta, const ClassFieldMeta memberMeta) noexcept {
            const size_t len = memberShapeMetas.size();
            for(size_t i = 0; i < len; ++i) {
                if(auto &memberShapeMeta = memberShapeMetas[i]; memberShapeMeta.name == shapeMeta.name) {
                    memberShapeMeta = shapeMeta;
                    memberMetas[i] = memberMeta;
                    return;
                }
            }
            memberShapeMetas.push_back(shapeMeta);
            memberMetas.push_back(memberMeta);
        }

        [[nodiscard]] MemberShapeMeta getMemberShape(const size_t idx) const noexcept { return memberShapeMetas[idx]; }

        [[nodiscard]] ClassFieldMeta getMember(const size_t idx) const noexcept { return memberMetas[idx]; }

        void setConstructor(FuncMeta *funcMeta) { this->constructor = funcMeta; }

        void marked() noexcept override {
            MarkSweepHeader::marked();
            const size_t len = memberShapeMetas.size();
            for(size_t i = 0; i < len; ++i) {
                if(memberShapeMetas[i].isMethod) {
                    memberMetas[i].funcMeta->marked();
                } else if(memberShapeMetas[i].isProp) {
                    memberMetas[i].propMeta->marked();
                }
            }
        }

        bool operator==(const ClassMeta &) const { return false; }

    private:
        friend class Runtime;
        explicit ClassMeta(const Atom className, const std::uint32_t arity) : className(className), arity(arity) {}
    };

    enum class ConstantType : std::uint8_t {
        None,
        Real,
        Atom,
        FuncMeta,
        ClassMeta,
        PropMeta,
    };

    using ConstantValue = std::variant<std::monostate, Real, Atom, FuncMeta *, ClassMeta *, PropMeta *>;

    struct Constant {

        explicit Constant() = default;

        template <typename T>
        explicit Constant(T value) : _value{ value } {}

        Constant(const Constant &constant) noexcept = default;

        Constant &operator=(const Constant &constant) noexcept = default;

        Constant(Constant &&constant) noexcept = default;

        Constant &operator=(Constant &&constant) noexcept = default;

        [[nodiscard]] ConstantType type() const noexcept { return static_cast<ConstantType>(_value.index()); }

        template <typename T>
        [[nodiscard]] T value() const noexcept {
            assert(std::holds_alternative<T>(_value));
            return std::get<T>(_value);
        }

        bool operator==(const Constant &rhs) const { return _value == rhs._value; }

        [[nodiscard]] Value createValue(Runtime *rt) const noexcept;

    private:
        ConstantValue _value;
    };

} // namespace cial

template <>
struct fmt::formatter<cial::ConstIdx> : ostream_formatter {};
