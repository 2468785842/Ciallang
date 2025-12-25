/*
 * Copyright (c) 2024/5/8 上午8:08
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

#include <fmt/format.h>

#include "Chunk.hpp"
#include "FastRegisterPool.hpp"
#include "Runtime.hpp"
#include "gen/IRGenerator.hpp"
#include "types/Function.hpp"

#include "types/Value.hpp"

#include "vm/Register.hpp"

namespace Cial::Bytecode {
    // enum class ContextType {
    //     TopLevel,
    //     Function,
    //     ExprFunction,
    //     Property,
    //     PropertySetter,
    //     PropertyGetter,
    //     Class,
    //     SuperClassGetter,
    // };

    struct CallFrame {
        const Chunk *chunk{};
        size_t baseRegSP{};
        std::optional<Register> ret{};
        size_t pc{};
        Value thisValue{};

        explicit CallFrame() = default;

        explicit CallFrame(const Chunk *chunk_, const std::optional<Register> ret_, FastRegisterPool &pool) :
            chunk(chunk_), baseRegSP(pool.allocFrame(chunk_->getRegCount())), ret(ret_), _pool(&pool) {}

        CallFrame(CallFrame &&callFrame) noexcept :
            chunk(callFrame.chunk), baseRegSP(callFrame.baseRegSP), ret(callFrame.ret), pc(callFrame.pc),
            _pool(callFrame._pool) {
            callFrame._pool = nullptr;
        }

        CallFrame &operator=(CallFrame &&callFrame) noexcept {
            if(this != &callFrame) {
                new(this) CallFrame(std::move(callFrame));
            }

            return *this;
        }

        CallFrame(const CallFrame &) = delete;
        CallFrame &operator=(const CallFrame &) = delete;

        ~CallFrame() {
            if(_pool) {
                _pool->freeFrame(chunk->getRegCount());
            }
        }

        [[nodiscard]] Value &getReg(const size_t index) { return *_pool->ptrAt(baseRegSP + index); }

        [[nodiscard]] const Value &getReg(const size_t index) const { return *_pool->ptrAt(baseRegSP + index); }

    private:
        FastRegisterPool *_pool{ nullptr };
    };

    class VMState {
    public:
        MarkSweep gc;

        explicit VMState(Inter::SymbolTable &symbolTable, Runtime &rt) :
            gc{ 1024, rt }, _rt(rt), _symbolTable(symbolTable) {}

        void run();

        void reg(const Register &reg, const Value &value) const;

        [[nodiscard]] Value reg(Register reg) const;

        [[nodiscard]] Value global(const size_t symbolIndex) const {
            return _rt.gObj.contains(getSymbol(symbolIndex)) ? _rt.gObj[getSymbol(symbolIndex)] : Value{};
        }

        void global(const size_t symbolIndex, const Value &value) const { _rt.gObj[getSymbol(symbolIndex)] = value; }

        [[nodiscard]] Value global(const std::string &name) const {
            return _rt.gObj.contains(name) ? _rt.gObj[name] : Value{};
        }

        void global(const std::string &name, const Value &value) const { _rt.gObj[name] = value; }

        void setZF(const bool zf) { _zf = zf; }

        [[nodiscard]] bool getZF() const { return _zf; }

        void setPC(const Label &label) { _callStack[_stackTop - 1].pc = label.address(); }

        [[nodiscard]] size_t getPC() const { return _currentFrame->pc; }

        void allocCallFrame(const Chunk *chunk, const std::optional<Register> &ret = {}) {
            if(_stackTop >= MAX_CALL_DEPTH)
                throw std::runtime_error("Call stack overflow");
            _currentFrame = new(&_callStack[_stackTop++]) CallFrame{ chunk, ret, _regPool };
        }

        void freeCallFrame() {
            if(_stackTop == 0)
                throw std::runtime_error("Call stack underflow");
            _currentFrame = _stackTop > 0 ? &_callStack[--_stackTop - 1] : nullptr;
            _callStack[_stackTop].~CallFrame();
            new(&_callStack[_stackTop]) CallFrame{};
        }

        [[nodiscard]] const std::vector<Op::Instruction *> &instructions() const noexcept {
            return _currentFrame->chunk->getInstVec();
        }

        [[nodiscard]] CallFrame *current() noexcept { return _currentFrame; }
        [[nodiscard]] const CallFrame *current() const noexcept { return _currentFrame; }

        [[nodiscard]] CallFrame *prev() noexcept { return &_callStack[_stackTop - 2]; }
        [[nodiscard]] const CallFrame *prev() const noexcept { return &_callStack[_stackTop - 2]; }

        [[nodiscard]] std::string dumpRegisters() const {
            std::stringstream ss{};
            for(size_t i = 0; i < _stackTop; i++) {
                const auto &call = _callStack[i];
                for(size_t j = 0; j < call.chunk->getRegCount(); j++) {
                    ss << fmt::format("(%{}): {}\n", j, call.getReg(j));
                }
            }
            return ss.str();
        }

        [[nodiscard]] std::string dumpInstruction(const Chunk &chunk) const {
            std::stringstream ss{};
            size_t pc{};
            std::vector<Function *> functions{};
            while(pc < chunk.getInstVec().size()) {
                const auto &instruction = chunk.getInstVec()[pc];

                ss << fmt::format("{: <6}: {}\n", Label{ pc }, Op::Instruction::dump(*instruction, *this, false));

                if(instruction->opcode == Op::OpCode::Load) {
                    if(auto value = current()->chunk->getConstant(Op::Load::value(*instruction)); value.isObject()) {
                        if(auto fun = dynamic_cast<Function *>(value.toObject())) {
                            functions.push_back(fun);
                        }
                    }
                }

                ++pc;
            }

            for(const auto &fun : functions) {
                ss << fmt::format("{:=^30}\n", fmt::format(" function {} ", fun->name()))
                   << dumpInstruction(*fun->chunk());
            }

            return ss.str();
        }

        [[nodiscard]] const char *getSymbol(const size_t index) const { return _symbolTable.getSymbol(index); }

        void pushVoid(const size_t n) { _regPool.allocFrame(n); }

        void push(const Value &v) { *_regPool.ptrAt(_regPool.allocFrame(1)) = v; }

        void pop(const size_t count) { _regPool.freeFrame(count); }
        [[nodiscard]] size_t getRegPoolTop() const { return _regPool.used(); }

    private:
        Runtime &_rt;
        Inter::SymbolTable &_symbolTable;
        CallFrame *_currentFrame{ nullptr };
        // Stack Max Depth Is 1024
        static constexpr auto MAX_CALL_DEPTH = 1024;
        FastRegisterPool _regPool{};
        CallFrame _callStack[MAX_CALL_DEPTH];
        size_t _stackTop{ 0 };
        bool _zf{ false };
    };
} // namespace Cial::Bytecode
