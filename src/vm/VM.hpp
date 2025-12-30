/*
 * Copyright (c) 2025/12/29 上午8:08
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

#include "Runtime.hpp"
#include "VMState.hpp"

#include "ast/ExprNode.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"

#include "types/Value.hpp"

namespace Cial::Bytecode {
    class VMState;
} // namespace Cial::Bytecode

namespace Cial {

    template <typename T>
    struct HandleConvert;

    struct DefaultHandleConvert {
        static void handleValue(Runtime *, const Value &) noexcept {}
        static void releaseValue(Runtime *, const Value &) noexcept {}
    };

    template <>
    struct HandleConvert<Value> : DefaultHandleConvert {
        static Value fromValue(const Value &v) { return v; }
    };

    template <>
    struct HandleConvert<Integer> : DefaultHandleConvert {
        static Integer fromValue(const Value &v) { return v.toInteger(); }
    };

    template <>
    struct HandleConvert<Real> : DefaultHandleConvert {
        static Real fromValue(const Value &v) { return v.toReal(); }
    };

    template <>
    struct HandleConvert<String *> : DefaultHandleConvert {
        static String *fromValue(const Value &v) { return v.toString(); }
    };

    template <>
    struct HandleConvert<Octet *> : DefaultHandleConvert {
        static Octet *fromValue(const Value &v) { return v.toOctet(); }
    };

    template <>
    struct HandleConvert<Object *> {

        static Object *fromValue(const Value &v) { return v.toObject(); }

        static void handleValue(Runtime *rt, const Value &v) noexcept {
            if(v.isObject()) {
                rt->addHandleVal(v.toObject());
            }
        }

        static void releaseValue(Runtime *rt, const Value &v) noexcept {
            if(v.isObject()) {
                rt->removeHandleVal(v.toObject());
            }
        }
    };

    template <typename T>
    struct HandleConvert<T *> : HandleConvert<Object *> {

        static T *fromValue(const Value &v) { return dynamic_cast<T *>(v.toObject()); }
    };

    class VM {
    public:
        template <typename T>
        class Handle {
            Runtime *_rt;
            Value _value;

        public:
            Handle(Runtime *rt, const Value &value) noexcept : _rt(rt), _value(value) {
                HandleConvert<T>::handleValue(rt, value);
            }

            ~Handle() noexcept { HandleConvert<T>::releaseValue(_rt, _value); }

            T operator*() const { return HandleConvert<T>::fromValue(_value); }

            T *operator->() {
                if constexpr(std::is_same_v<T, Value>) {
                    return &_value;
                } else if constexpr(std::is_pointer_v<T>) {
                    return HandleConvert<T>::fromValue(_value);
                } else {
                    static_assert(
                        std::is_same_v<T, Value>,
                        "operator-> is only supported for Handle<Value> or Handle<T*> to access raw Value methods.");
                    return nullptr;
                }
            }
        };

        explicit VM(Bytecode::VMState *vmState) : _vmState(vmState) {}

        template <typename T>
        Handle<T> getGlobal(const String &name) const {
            return { &_vmState->rt(), _vmState->global(name.getData()) };
        }

        template <typename T>
        Handle<T> evalExpr(const String &expr) const {
            Runtime &rt = _vmState->rt();
            if(expr.isEmpty())
                return Handle<T>{ &rt, Value{} };

            Common::SourceFile sourceFile{};
            Common::Result r{};

            sourceFile.load(r, expr.getData());
            assert(!r.isFailed());

            Syntax::Parser parser{ rt, sourceFile };

            Syntax::ExprNode *node = parser.parseExpression(r);
            assert(!r.isFailed());

            Inter::IRGenerator codeGen{ sourceFile };

            codeGen.beginScope();

            OptReg retReg{};
            auto chunk = codeGen.parseAst(r, node, retReg);

            assert(!r.isFailed());
            assert(retReg);
            assert(chunk->getRegCount() != 0);
            assert(!chunk->getInstVec().empty());

            _vmState->allocCallFrame(chunk.get());
            _vmState->run();
            Value ret = _vmState->reg(*retReg);
            _vmState->freeCallFrame();

            return Handle<T>{ &rt, ret };
        }

    private:
        Bytecode::VMState *_vmState;
    };

} // namespace Cial