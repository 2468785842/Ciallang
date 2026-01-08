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
#include "types/Octet.hpp"
#include "types/String.hpp"

#include "Chunk.hpp"
#include "runtime/Runtime.hpp"

namespace cial {

    void FuncMeta::marked() noexcept {
        MarkSweepHeader::marked();
        chunk->marked();
    }

    Value Constant::createValue(Runtime *rt) const noexcept {
        switch(_type) {
            case ConstantType::None:
                break;
            case ConstantType::Integer:
                return Value{ value<Integer>() };
            case ConstantType::Real:
                return Value{ _value.real };
            case ConstantType::Atom: {
                const auto *str = rt->atomTable.get(value<Atom>())->str;
                return Value{ new String(str->getData(), str->length()) };
            }
            case ConstantType::FuncMeta: {
                return Value{ rt->create<Function>(value<FuncMeta *>()) };
            }
            case ConstantType::ClassMeta:
                return Value{ rt->create<ClassObject>(value<ClassMeta *>()) };
        }
        return Value{};
    }
} // namespace cial