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

#include "VMState.hpp"
#include "runtime/Runtime.hpp"

#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/ExprNode.hpp"

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
    struct HandleConvert<bool> : DefaultHandleConvert {
        static bool fromValue(const Value &v) { return v.toBool(); }
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
            return { &_vmState->rt, _vmState->global(name.getData()) };
        }

        template <typename T>
        [[nodiscard]] Handle<T> eval(const String &str, bool filepath = false) const {
            Runtime &rt = _vmState->rt;
            if(str.isEmpty())
                return Handle<T>{ &rt, Value{} };

            Common::Result r{};
            Common::SourceFile sourceFile;
            if(filepath) {
                sourceFile = Common::SourceFile{ str.toStdStr() };
                sourceFile.load(r);
            } else {
                sourceFile.load(r, str.toStdStr());
            }
            assert(!r.isFailed());

            Syntax::Parser parser{ rt, sourceFile };

            Syntax::AstNode *node = parser.parse(r);
            assert(!r.isFailed());

            Inter::IRGenerator codeGen{ rt, sourceFile };

            bool isSub = _vmState->context.stackTop != 0;

            if(isSub) {
                for(auto localVar : _vmState->prev()->funcMeta->localVars) {
                    if(localVar.startPC > _vmState->curFrame()->pc && _vmState->curFrame()->pc <= localVar.endPC) {
                        localVar.startPC = 0;
                        localVar.endPC = 0;
                        codeGen.addLocalVar(LocalVariable{ localVar });
                    }
                }
            }

            OptReg ignoreReg{};
            Opt<Bytecode::Chunk> chunk = codeGen.parseAst(r, node, ignoreReg);


            assert(chunk);
            assert(!r.isFailed());
            assert(chunk->getRegCount() != 0);
            assert(!chunk->getInstVec().empty());

            auto *evalChunk = _vmState->rt.create<Bytecode::Chunk>(std::move(*chunk));

            Bytecode::Register retReg{ 0 };
            Bytecode::Chunk tmpChunk{};
            tmpChunk.setRegCount(1); // accept ret val

            _vmState->allocCallFrame(&tmpChunk);
            _vmState->allocCallFrame(evalChunk, retReg);
            std::uint32_t stackTop = _vmState->context.stackTop;

            if(isSub) {
                _vmState->runFlat();
            } else {
                _vmState->run();
            }
            Value ret = _vmState->reg(retReg);

            // when evalChunk include `ret` inst, `ret` will call freeCallFrame, so we need check
            // keep Stack balancing
            if(stackTop - 1 != _vmState->context.stackTop) {
                _vmState->freeCallFrame();
            }
            _vmState->freeCallFrame();

            return Handle<T>{ &rt, ret };
        }

        template <typename T>
        [[nodiscard]] Handle<T> evalExpr(const String &expr) const {
            Runtime &rt = _vmState->rt;
            if(expr.isEmpty())
                return Handle<T>{ &rt, Value{} };

            Common::SourceFile sourceFile{};
            Common::Result r{};

            sourceFile.load(r, expr.toStdStr());
            assert(!r.isFailed());

            Syntax::Parser parser{ rt, sourceFile };

            Syntax::ExprNode *node = parser.parseExpression(r);
            assert(!r.isFailed());

            Inter::IRGenerator codeGen{ rt, sourceFile };

            bool isSub = _vmState->context.stackTop != 0;

            if(isSub) {
                for(auto localVar : _vmState->prev()->funcMeta->localVars) {
                    if(localVar.startPC > _vmState->curFrame()->pc && _vmState->curFrame()->pc <= localVar.endPC) {
                        localVar.startPC = 0;
                        localVar.endPC = 0;
                        codeGen.addLocalVar(LocalVariable{ localVar });
                    }
                }
            }

            OptReg retReg{};
            Opt<Bytecode::Chunk> chunk = codeGen.parseAst(r, node, retReg);

            assert(chunk);
            assert(!r.isFailed());
            assert(retReg);
            assert(chunk->getRegCount() != 0);
            assert(!chunk->getInstVec().empty());

            _vmState->allocCallFrame(&*chunk);

            if(isSub) {
                _vmState->runFlat();
            } else {
                _vmState->run();
            }
            Value ret = _vmState->reg(*retReg);
            _vmState->freeCallFrame();

            return Handle<T>{ &rt, ret };
        }

        void eval(const String &str, const bool filepath = false) const { auto h = eval<Value>(str, filepath); }
        void evalExpr(const String &expr) const { auto h = evalExpr<Value>(expr); }

        [[nodiscard]] String dumpCurCallFrameInst() const { return _vmState->curFrame()->chunk->dumpInstruction(); }

    private:
        Bytecode::VMState *_vmState;
    };

} // namespace Cial