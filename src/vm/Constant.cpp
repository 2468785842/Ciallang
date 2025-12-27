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

#include "Chunk.hpp"
#include "types/Class.hpp"
#include "types/Function.hpp"
#include "types/Octet.hpp"
#include "types/String.hpp"

namespace Cial {
    FuncMeta::~FuncMeta() noexcept { delete chunk; }

    Value Constant::createValue(const Runtime &rt) const noexcept {
        switch(_type) {
            case ConstantType::Integer:
                return Value{ value<Integer>() };
            case ConstantType::Real:
                return Value{ _value.real };
            case ConstantType::Atom: {
                const auto *aEntry = rt.atomTable.get(value<Atom>());
                return Value{ new String(aEntry->str, aEntry->length) };
            }
            case ConstantType::OctetIdx: {
                const auto *oEntry = rt.octetTable.get(value<OctetIdx>());
                return Value{ new Octet(oEntry->data, oEntry->size) };
            }
            case ConstantType::None:
                break;
            case ConstantType::FuncMeta: {
                return Value{ new Function(value<FuncMeta *>()) };
            }
            case ConstantType::ClassMeta:
                break;
        }
        return Value{};
    }
} // namespace Cial