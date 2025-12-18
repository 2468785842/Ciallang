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

#include "Class.hpp"

#include <ranges>

namespace Ciallang {
    ClassObject::~ClassObject() noexcept {
        if(_base) {
            _base->decRef();
        }
    }

    void ClassObject::setBase(ClassObject *base) noexcept {
        if(_base) {
            _base->decRef();
        }
        _base = base;
        if(_base) {
            _base->incRef();
        }
    }

    void ClassObject::setStaticField(const std::string &name, const Value &field) noexcept {
        _staticFields[name] = field;
    }

    [[nodiscard]] Value ClassObject::getStaticField(const std::string &name) const noexcept {
        const auto it = _staticFields.find(name);
        return it != _staticFields.end() ? it->second : Value{};
    }

    void ClassObject::setMethod(const std::string &name, const Value &method) noexcept { _methodsDef[name] = method; }

    Value ClassObject::getMethod(const std::string &name) const noexcept {
        const auto it = _methodsDef.find(name);
        return it != _methodsDef.end() ? it->second : Value{};
    }

    [[nodiscard]] std::vector<Value> ClassObject::getAllMethod() const noexcept {
        std::vector<Value> result;
        for(auto &method : _methodsDef | std::views::values) {
            result.push_back(method);
        }
        return result;
    }

    void ClassObject::setFieldDef(const std::string &name, const FieldMeta &fieldMeta) noexcept {
        _fieldsDef[name] = fieldMeta;
    }

    [[nodiscard]] FieldMeta ClassObject::getFieldDef(const std::string &name) const noexcept {
        const auto it = _fieldsDef.find(name);
        return it != _fieldsDef.end() ? it->second : FieldMeta{};
    }

    void InstanceObject::setField(const std::string &name, const Value &field) noexcept { _fields[name] = field; }

    [[nodiscard]] Value InstanceObject::getField(const std::string &name) const noexcept {
        if(const auto it = _fields.find(name); it != _fields.end())
            return it->second;
        return _class->getMethod(name);
    }

} // namespace Ciallang
