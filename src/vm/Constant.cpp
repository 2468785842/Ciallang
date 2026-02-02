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

#include "Constant.hpp"

#include "types/Class.hpp"
#include "types/Function.hpp"
#include "types/String.hpp"

#include "Chunk.hpp"
#include "runtime/Runtime.hpp"
#include "types/Property.hpp"

namespace cial {

    void FuncMeta::marked() noexcept {
        MarkSweepHeader::marked();
        chunk->marked();
    }

    [[nodiscard]] String Constant::dumpConstant(const Runtime *rt) const {
        std::stringstream ss{ "" };
        switch(type()) {
            case ConstantType::None:
                ss << "Void";
                break;
            case ConstantType::Real:
                ss << value<Real>().value();
                break;
            case ConstantType::Atom:
                ss << '"' << *rt->atomTable.get(value<Atom>())->str << '"';
                break;
            case ConstantType::FuncMeta:
                ss << "Ptr<FuncMeta>";
                break;
            case ConstantType::ClassMeta:
                ss << "Ptr<ClassMeta>";
                break;
            case ConstantType::PropMeta:
                ss << "Ptr<PropMeta>";
                break;
        }
        return String{ ss.str() };
    }

    Value Constant::createValue(Runtime *rt) const noexcept {
        switch(type()) {
            case ConstantType::None:
                break;
            case ConstantType::Real:
                return Value{ value<Real>() };
            case ConstantType::Atom: {
                const auto *str = rt->atomTable.get(value<Atom>())->str;
                return Value{ String{ str->getData(), str->length() } };
            }
            case ConstantType::FuncMeta: {
                return Value{ rt->create<Function>(value<FuncMeta *>()).get() };
            }
            case ConstantType::ClassMeta:
                return Value{ rt->create<ClassObject>(value<ClassMeta *>(), rt).get() };
            case ConstantType::PropMeta:
                return Value{ rt->create<Property>(nullptr, value<PropMeta *>()).get() };
        }
        return Value{};
    }

    [[nodiscard]] String dumpConstants(const Vec<Constant> &constants, const Runtime *rt) {
        std::stringstream ss{ "" };
        u16 i{};
        const size_t len = constants.size();
        while(i < len) {
            ConstIdx idx{ i };
            ss << fmt::format("{: <6} = {}", idx, constants[i].dumpConstant(rt));
            if(i != len - 1) {
                ss << '\n';
            }
            ++i;
        }
        return String{ ss.str() };
    }
} // namespace cial