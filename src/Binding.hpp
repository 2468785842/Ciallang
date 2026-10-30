#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include "runtime/Context.hpp"
#include "types/Class.hpp"
#include "types/NativeObject.hpp"
#include "vm/VM.hpp"

namespace cial::binding {
    struct Tracer {
        void operator()(const Value &value) const noexcept {
            if(value.isObject())
                value.asObject().unwrap()->marked();
        }
    };

    template <typename T>
    T &thisAs(const Value &self) {
        using U = std::remove_cv_t<T>;
        if(!self.isObject())
            throw std::runtime_error("native receiver must be an object");
        auto *object = self.asObject().unwrap();
        if constexpr(std::is_base_of_v<Object, U>) {
            if(object->nativeTypeKey() != TypeTraits<U>::key())
                throw std::runtime_error("native receiver type mismatch");
            return *static_cast<U *>(object);
        } else {
            auto *payload = object->nativePayload(nativeTypeKeyFor<U>());
            if(!payload)
                throw std::runtime_error("native receiver type mismatch");
            return *static_cast<U *>(payload);
        }
    }

    template <typename T>
    decltype(auto) fromValue(const Value &value) {
        using U = std::remove_cvref_t<T>;
        if constexpr(std::same_as<U, Value>)
            return Value{ value };
        else if constexpr(std::same_as<U, String>)
            return String{ *value.asString().unwrap() };
        else if constexpr(std::same_as<U, std::string>)
            return value.asString().unwrap()->toStdStr();
        else if constexpr(std::same_as<U, bool>)
            return value.asBool();
        else if constexpr(std::integral<U>) {
            const auto integer = value.asInteger().unwrap();
            if(!std::in_range<U>(integer))
                throw std::runtime_error("native integer argument out of range");
            return static_cast<U>(integer);
        } else if constexpr(std::floating_point<U>) {
            return static_cast<U>(value.isInteger() ? static_cast<double>(value.asInteger().unwrap())
                                                    : value.asReal().unwrap().value());
        } else if constexpr(std::same_as<U, Real>)
            return value.asReal().unwrap();
        else {
            static_assert(std::is_lvalue_reference_v<T>, "native objects must be passed by reference");
            return thisAs<U>(value);
        }
    }

    template <typename T>
    Value wrap(Context &ctx, std::shared_ptr<T> payload) {
        const auto type = ctx.nativeBinding(nativeTypeKeyFor<T>());
        if(!type)
            throw std::runtime_error("native type has not been registered");
        return Value{ ctx.rt().create<NativeObject>(type, std::move(payload)).get() };
    }

    template <typename T>
    Value toValue(Context &ctx, T &&input) {
        using U = std::remove_cvref_t<T>;
        if constexpr(std::same_as<U, Value>)
            return std::forward<T>(input);
        else if constexpr(std::same_as<U, String> || std::same_as<U, Real>)
            return Value{ input };
        else if constexpr(std::same_as<U, std::string>)
            return Value{ String{ input } };
        else if constexpr(std::same_as<U, bool>)
            return Value{ Integer{ input ? 1 : 0 } };
        else if constexpr(std::integral<U>) {
            if(!std::in_range<Integer>(input))
                throw std::runtime_error("native integer result out of range");
            return Value{ static_cast<Integer>(input) };
        } else if constexpr(std::floating_point<U>)
            return Value{ Real{ static_cast<double>(input) } };
        else
            return wrap(ctx, std::forward<T>(input));
    }

    inline void checkArity(const size_t count, const size_t expected) {
        if(count != expected)
            throw std::runtime_error("native argument count mismatch");
    }

    template <typename F, typename Tuple, size_t... I>
    Value invoke(Context &ctx, F &&fn, Value *args, std::index_sequence<I...>) {
        if constexpr(std::is_void_v<
                         std::invoke_result_t<F, decltype(fromValue<std::tuple_element_t<I, Tuple>>(args[I]))...>>) {
            std::invoke(std::forward<F>(fn), fromValue<std::tuple_element_t<I, Tuple>>(args[I])...);
            return {};
        } else {
            return toValue(ctx,
                           std::invoke(std::forward<F>(fn), fromValue<std::tuple_element_t<I, Tuple>>(args[I])...));
        }
    }

    template <typename F>
    RootGuard<NativeFunction> makeFunction(Context &ctx, F &&callback) {
        return ctx.rt().create<NativeFunction>(std::forward<F>(callback));
    }

    template <typename F>
    RootGuard<NativeFunction> makeTypedFunction(Context &ctx, F fn) {
        using Traits = native::NativeFunTraits<F>;
        return makeFunction(ctx, [fn](const VM *vm, const Value &, const size_t argc, Value *args) {
            checkArity(argc, Traits::arity);
            return invoke<const F &, typename Traits::ArgsTuple>(vm->context(), fn, args,
                                                                 std::make_index_sequence<Traits::arity>{});
        });
    }

    // 命名空间和构造入口拥有自己的静态成员，GC 显式遍历。
    class Namespace : public Object {
    public:
        Map<Atom, Value> members;
        NativeFunction *constructor{};
        void call(vm::VMState &state, const u32 ret, const size_t argc) override {
            if(!constructor)
                throw std::runtime_error("native namespace has no constructor");
            constructor->call(state, ret, argc);
        }
        [[nodiscard]] bool hasProp(const Atom name) const override { return members.contains(name); }
        Value getProp(const Atom name) override {
            const auto it = members.find(name);
            if(it == members.end())
                throw std::runtime_error("native static member not found");
            return it->second;
        }
        void setProp(const Atom name, const Value &value) override { members[name] = value; }
        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            if(constructor)
                constructor->marked();
            for(const auto &value : members | std::views::values)
                Tracer{}(value);
        }
    };

    template <typename T>
    class ClassBuilder {
        static_assert(!std::is_base_of_v<Object, T>, "Object subclasses use Context::nativeType<T>()");
        Context &_ctx;
        RootGuard<Namespace> _class;

    public:
        ClassBuilder(Context &ctx, Namespace *classObject, const Atom name) :
            _ctx(ctx), _class(&ctx.rt(), classObject) {
            if(!ctx.nativeBinding(nativeTypeKeyFor<T>()))
                ctx.registerNativeBinding(std::make_shared<NativeTypeBinding>(
                    NativeTypeBinding{ .key = nativeTypeKeyFor<T>(), .name = name, .trace = {} }));
        }
        template <typename... Args>
        ClassBuilder &constructor() {
            auto fn = makeFunction(_ctx, [](const VM *vm, const Value &, const size_t argc, Value *args) {
                checkArity(argc, sizeof...(Args));
                return [&]<size_t... I>(std::index_sequence<I...>) {
                    return wrap(vm->context(), std::make_shared<T>(fromValue<Args>(args[I])...));
                }(std::index_sequence_for<Args...>{});
            });
            _class->constructor = fn.get();
            return *this;
        }
        template <typename F>
        ClassBuilder &method(const String &name, F fn) {
            using Traits = native::NativeFunTraits<F>;
            _ctx.registerMethod(
                nativeTypeKeyFor<T>(), name, [fn](const VM *vm, const Value &self, const size_t argc, Value *args) {
                    checkArity(argc, Traits::arity);
                    auto &object = thisAs<T>(self);
                    auto bound = [&](auto &&...values) -> decltype(auto) { return std::invoke(fn, object, values...); };
                    return invoke<decltype(bound) &, typename Traits::ArgsTuple>(
                        vm->context(), bound, args, std::make_index_sequence<Traits::arity>{});
                });
            return *this;
        }
        template <typename G, typename S = std::nullptr_t>
        ClassBuilder &property(const String &name, G getter, S setter = nullptr) {
            auto get = makeFunction(_ctx, [getter](const VM *vm, const Value &self, size_t, Value *) {
                return toValue(vm->context(), std::invoke(getter, thisAs<T>(self)));
            });
            RootGuard<NativeFunction> set{ &_ctx.rt(), nullptr };
            if constexpr(!std::same_as<S, std::nullptr_t>) {
                using Arg = native::NativeFunTraits<S>::template Arg<0>;
                set = makeFunction(_ctx, [setter](const VM *, const Value &self, const size_t argc, const Value *args) {
                    checkArity(argc, 1);
                    std::invoke(setter, thisAs<T>(self), fromValue<Arg>(args[0]));
                    return Value{};
                });
            }
            _ctx.registerAccessor(nativeTypeKeyFor<T>(), name, get.get(), set.get());
            return *this;
        }
        template <typename F>
        ClassBuilder &trace(F fn) {
            static_assert(std::is_nothrow_invocable_v<F, T &, Tracer>, "GC trace must be noexcept");
            _ctx.nativeBinding(nativeTypeKeyFor<T>())->trace = [fn](void *payload) {
                fn(*static_cast<T *>(payload), Tracer{});
            };
            return *this;
        }
        template <typename F>
        ClassBuilder &staticFunction(const String &name, F fn) {
            const auto function = makeTypedFunction(_ctx, std::move(fn));
            _class->members[_ctx.rt().atomTable.intern(name)] = Value{ function.get() };
            return *this;
        }
    };

    class Module {
        Context &_ctx;
        RootGuard<Namespace> _namespace;
        void put(const String &name, const Value &value) const {
            if(_namespace.get())
                _namespace->members[_ctx.rt().atomTable.intern(name)] = value;
            else
                _ctx.registerGlobal(name, value);
        }

    public:
        explicit Module(Context &ctx, Namespace *space = nullptr) : _ctx(ctx), _namespace(&ctx.rt(), space) {}
        [[nodiscard]] Module module(const String &name) const {
            const auto atom = _ctx.rt().atomTable.intern(name);
            Object *parent = _namespace.get() ? static_cast<Object *>(_namespace.get()) : _ctx.global();
            if(parent->hasProp(atom)) {
                const auto value = parent->getProp(atom);
                auto *space = value.isObject() ? dynamic_cast<Namespace *>(value.asObject().unwrap()) : nullptr;
                if(!space)
                    throw std::runtime_error("native module name already belongs to another value");
                return Module{ _ctx, space };
            }
            const auto space = _ctx.rt().create<Namespace>();
            put(name, Value{ space.get() });
            return Module{ _ctx, space.get() };
        }
        template <typename T>
        ClassBuilder<T> class_(const String &name) {
            const auto type = _ctx.rt().create<Namespace>();
            put(name, Value{ type.get() });
            return ClassBuilder<T>{ _ctx, type.get(), _ctx.rt().atomTable.intern(name) };
        }
        template <typename F>
        Module &function(const String &name, F fn) {
            const auto function = makeTypedFunction(_ctx, std::move(fn));
            put(name, Value{ function.get() });
            return *this;
        }
    };
} // namespace cial::binding

inline cial::binding::Module cial::Context::module(const String &name) { return binding::Module{ *this }.module(name); }
