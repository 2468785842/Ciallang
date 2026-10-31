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
        if(_marked)
            return;
        MarkSweepHeader::marked();
        chunk->marked();
    }

    Value Constant::createValue(Runtime *rt) const noexcept {
        switch(type()) {
            case ConstantType::Object:
                return Value{ value<Object *>() };
            case ConstantType::None:
                break;
            case ConstantType::Real:
                return Value{ value<Real>() };
            case ConstantType::Atom: {
                // 字符串字面量:负载缓存后重复 Load 零分配(retain 即可)
                const auto *str = rt->atomTable.get(value<Atom>())->str;
                if(str->length() <= String::SHORT_STR_LEN)
                    return Value{ *str };
                if(!_strPayload) {
                    _strPayload = SharedPayload<String>::makeSingle(String{ str->getData(), str->length() });
                }
                SharedPayload<String>::retain(_strPayload);
                Value v{};
                v.type(ValueType::String);
                v._data.string = _strPayload;
                return v;
            }
            case ConstantType::FuncMeta: {
                return Value{ rt->create<Function>(value<FuncMeta *>()).get() };
            }
            case ConstantType::ClassMeta:
                return Value{ rt->create<ClassObject>(value<ClassMeta *>(), rt).get() };
            case ConstantType::PropMeta:
                return Value{ rt->create<Property>(nullptr, value<PropMeta *>()).get() };
            case ConstantType::Octet: {
                // 字面量 octet:variant 槽位持有 payload 所有权,装载时 retain
                auto *p = std::get<SharedPayload<Octet> *>(_value);
                SharedPayload<Octet>::retain(p);
                Value v{};
                v.type(ValueType::Octet);
                v.octetStore(p);
                return v;
            }
        }
        return Value{};
    }
} // namespace cial