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

#include "vm/VMState.hpp"

namespace Cial {
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

    void ClassObject::setStaticField(Atom name, const Value &field) noexcept { _staticFields[name] = field; }

    [[nodiscard]] Value ClassObject::getStaticField(Atom name) const noexcept {
        const auto it = _staticFields.find(name);
        return it != _staticFields.end() ? it->second : Value{};
    }

    void ClassObject::setMethod(Atom name, const Value &method) noexcept { _methodsDef[name] = method; }

    Value ClassObject::getMethod(Atom name) const noexcept {
        const auto it = _methodsDef.find(name);
        return it != _methodsDef.end() ? it->second : Value{};
    }

    [[nodiscard]] std::vector<Value> ClassObject::getMethods() const noexcept {
        std::vector<Value> result;
        for(auto &method : _methodsDef | std::views::values) {
            result.push_back(method);
        }
        return result;
    }

    void ClassObject::setFieldDef(Atom name, const FieldMeta &fieldMeta) noexcept { _fieldsDef[name] = fieldMeta; }

    [[nodiscard]] FieldMeta ClassObject::getFieldDef(Atom name) const noexcept {
        const auto it = _fieldsDef.find(name);
        return it != _fieldsDef.end() ? it->second : FieldMeta{};
    }

    void InstanceObject::setField(Atom name, const Value &field) noexcept { _fields[name] = field; }

    [[nodiscard]] Value InstanceObject::getField(Atom name) const noexcept {
        const auto it = _fields.find(name);
        return it != _fields.end() ? it->second : Value{};
    }

    void InstanceObject::setMethod(Atom name, const Value &method) noexcept { _methods[name] = method; }

    [[nodiscard]] Value InstanceObject::getMethod(Atom name) const noexcept {
        const auto it = _methods.find(name);
        return it != _methods.end() ? it->second : Value{};
    }

    void ClassFunction::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        getFunction()->call(vmState, ret, argCount);
        vmState.curFrame()->thisValue = _thisValue;
    }

    void ClassObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        auto *instanceObj = vmState.gc.allocate<InstanceObject>(this);

        for(auto &[k, v] : getFieldDefs()) {
            instanceObj->setField(k, v.defValReg ? vmState.reg(v.defValReg.value()) : Value{});
        }

        for(auto &v : getMethods()) {
            instanceObj->setField(v.toObject()->getName(),
                                  Value{ vmState.gc.allocate<ClassFunction>(Value{ instanceObj }, v) });
        }
        vmState.reg(ret, Value{ instanceObj });
    }

    void InstanceObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        throw std::runtime_error("Not implemented InstanceObject call");
    }

} // namespace Cial
