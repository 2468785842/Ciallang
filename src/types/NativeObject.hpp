#pragma once

#include <memory>
#include "Object.hpp"

namespace cial {
    // 描述符只存原生元数据；脚本引用必须保存在 payload 并由 trace 枚举。
    struct NativeTypeBinding {
        NativeTypeKey key;
        Atom name;
        std::function<void(void *)> trace;
    };

    class NativeObject final : public Object {
    public:
        NativeObject(std::shared_ptr<NativeTypeBinding> type, std::shared_ptr<void> payload) :
            Object(type->name), _type(std::move(type)), _payload(std::move(payload)) {
            if(!_payload)
                throw std::runtime_error("null native payload");
        }
        [[nodiscard]] NativeTypeKey nativeTypeKey() const noexcept override { return _type->key; }
        [[nodiscard]] void *nativePayload(NativeTypeKey key) const noexcept override {
            return key == _type->key ? _payload.get() : nullptr;
        }
        bool instanceOf(Atom name) override { return name == _type->name; }
        void call(vm::VMState &, u32, size_t) override { throw std::runtime_error("native instance is not callable"); }
        [[nodiscard]] bool hasProp(Atom) const override { return false; }
        Value getProp(Atom) override { throw std::runtime_error("native member not found"); }
        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            if(_type->trace)
                _type->trace(_payload.get());
        }

    private:
        std::shared_ptr<NativeTypeBinding> _type;
        std::shared_ptr<void> _payload;
    };
} // namespace cial
