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

#include "pch.h"

#include "Chunk.hpp"
#include "gen/BytecodeGenerator.hpp"
#include "logging/Logger.hpp"
#include "types/TjsFunction.hpp"

#include "types/TjsValue.hpp"

#include "vm/Register.hpp"

namespace Ciallang::Bytecode {

    class FastRegisterPool {
    public:
        explicit FastRegisterPool(const size_t initialCap = 1 << 16) {
            _storage.reserve(initialCap);
            _storage.resize(0);
            _sp = 0;
        }

        // 预分配整个栈容量（避免重分配）
        void reserve(const size_t total) {
            if(total > _storage.capacity()) {
                _storage.reserve(total);
            }
        }

        // 分配一个连续的寄存器块，返回指针基址（TjsValue*）
        // 使用方式：TjsValue* base = stack.allocFrame(n); base[i] ...
        TjsValue *allocFrame(const size_t n) {
            const size_t base = _sp;
            ensureCapacity(_sp + n);
            _sp += n;
            // 返回内部数组的裸指针：极快的访问
            return _storage.data() + base;
        }

        // 释放最近分配的帧（必须和 allocFrame 按栈顺序配对）
        void freeFrame(const size_t n) {
            assert(_sp >= n);
            _sp -= n;
        }

        // peek base pointer to stack top - useful for debugging
        TjsValue *topPtr() noexcept { return _storage.data() + _sp; }

        [[nodiscard]] size_t used() const noexcept { return _sp; }
        [[nodiscard]] size_t capacity() const noexcept { return _storage.capacity(); }

    private:
        std::vector<TjsValue> _storage;
        size_t _sp; // stack pointer (next free slot index)

        void ensureCapacity(const size_t need) {
            if(need <= _storage.size()) {
                // 已有构造对象覆盖（最常见）
                return;
            }

            if(need <= _storage.capacity()) {
                // 有容量但 size() 小：只增加 size（构造需要的对象）
                _storage.resize(need);
                return;
            }

            // 容量不够：按指数增长（2x），避免频繁扩容
            size_t curCap = _storage.capacity();
            if(curCap == 0)
                curCap = 1;
            size_t newCap = curCap;
            // 倍增直到满足 need（防止溢出）
            while(newCap < need) {
                newCap = newCap >= size_t{ 1 } << 62 ? need : newCap * 2;
                if(newCap < curCap) {
                    newCap = need;
                    break;
                } // 防溢出保底
            }

            // 一次性 reserve 到 newCap，然后 resize 到 need（构造对象）
            _storage.reserve(newCap);
            _storage.resize(need);
        }
    };

    struct CallFrame {
        const Chunk *chunk{ nullptr };
        TjsValue *regs{ nullptr };
        std::optional<Register> ret{};
        size_t pc{};

        explicit CallFrame() = default;

        explicit CallFrame(const Chunk *chunk_, const std::optional<Register> ret_, FastRegisterPool &pool) :
            chunk(chunk_), regs(pool.allocFrame(chunk_->getRegisterCount())), ret(ret_), _pool(&pool) {}

        CallFrame(CallFrame &&callFrame) noexcept :
            chunk(callFrame.chunk), regs(callFrame.regs), ret(callFrame.ret), pc(callFrame.pc), _pool(callFrame._pool) {
            callFrame._pool = nullptr;
        }

        CallFrame &operator=(CallFrame &&callFrame) noexcept {
            if(this == &callFrame)
                return *this;

            chunk = callFrame.chunk;
            ret = callFrame.ret;
            regs = callFrame.regs;
            pc = callFrame.pc;
            _pool = callFrame._pool;

            callFrame._pool = nullptr;

            return *this;
        }

        CallFrame(const CallFrame &) = delete;
        CallFrame &operator=(const CallFrame &) = delete;

        ~CallFrame() {
            if(_pool) {
                _pool->freeFrame(chunk->getRegisterCount());
            }
        }

        [[nodiscard]] TjsValue &getReg(const size_t index) {
            return regs[index];
        }

        [[nodiscard]] const TjsValue &getReg(const size_t index) const {
            return regs[index];
        }

    private:
        FastRegisterPool *_pool{ nullptr };
    };

    class Interpreter {
    public:
        CallFrame createCallFrame(const Chunk *chunk, const std::optional<Register> &ret = {}) {
            return CallFrame{ chunk, ret, _regPool };
        }

        explicit Interpreter(Inter::SymbolTable &symbolTable) : _symbolTable(symbolTable) {}

        void run(const Chunk *mainChunk);

        void reg(const Register &reg, const TjsValue& value) const;

        void reg(const Register &reg, TjsValue&& value) const;

        [[nodiscard]] TjsValue reg(Register reg);
        [[nodiscard]] const TjsValue &reg(Register reg) const;

        [[nodiscard]] const TjsValue &global(const size_t symbolIndex) const { return _globals[symbolIndex]; }

        void global(const size_t symbolIndex, TjsValue &&value) {
            if(_globals.size() < symbolIndex + 1) {
                _globals.resize(symbolIndex * 2 + 1);
            }
            _globals[symbolIndex] = std::move(value);
        }

        void global(const std::string &identifier, TjsValue &&value) {
            const auto index = _symbolTable.getSymbolIndex(identifier);
            if(!index) {
                return;
            }
            global(*index, std::move(value));
        }

        template <typename T>
            requires std::is_base_of_v<TjsObject, T>
        void global(T *obj) {
            global(obj->name(), TjsValue{ obj });
        }

        void setZF(const bool zf) { _zf = zf; }

        [[nodiscard]] bool getZF() const { return _zf; }

        void setPC(const Label &label) { _callStack[_stackTop - 1].pc = label.address(); }

        [[nodiscard]] size_t getPC() const { return _currentFrame->pc; }

        void pushCallFrame(CallFrame &&frame) {
            if(_stackTop >= MAX_CALL_DEPTH)
                throw std::runtime_error("Call stack overflow");
            _callStack[_stackTop++] = std::move(frame);
            _currentFrame = &_callStack[_stackTop - 1];
        }

        CallFrame popCallFrame() {
            if(_stackTop == 0)
                throw std::runtime_error("Call stack underflow");
            _currentFrame = _stackTop > 0 ? &_callStack[--_stackTop - 1] : nullptr;
            return std::move(_callStack[_stackTop]);
        }

        [[nodiscard]] const std::vector<Op::Instruction *> &instructions() const noexcept {
            return _currentFrame->chunk->instructions();
        }

        [[nodiscard]] const Chunk *current() const noexcept { return _currentFrame->chunk; }

        [[nodiscard]] std::string dumpRegisters() const {
            std::stringstream ss{};
            for(size_t i = 0; i < _stackTop; i++) {
                const auto &call = _callStack[i];
                for(size_t j = 0; j < call.chunk->getRegisterCount(); j++) {
                    ss << fmt::format("(%{}): {}\n", j, call.getReg(j));
                }
            }
            return ss.str();
        }

        [[nodiscard]] std::string dumpInstruction(const Chunk &chunk) const {
            std::stringstream ss{};
            size_t pc{};
            std::vector<TjsFunction *> functions{};
            while(pc < chunk.instructions().size()) {
                const auto instruction = chunk.instructions()[pc];

                ss << fmt::format("{: <6}: {}\n", Label{ pc }, Op::Instruction::dump(instruction->getOpcode(), *instruction, *this, false));

                if(instruction->getOpcode() == Op::OpCode::Load) {
                    if(auto value = Op::Load::value(*instruction); value.isObject()) {
                        if(auto fun = dynamic_cast<TjsFunction *>(value.toObject())) {
                            functions.push_back(fun);
                        }
                    }
                }

                ++pc;
            }
            for(const auto fun : functions) {
                ss << fmt::format("{:=^30}\n", fmt::format(" function {} ", fun->name()))
                   << dumpInstruction(*fun->chunk());
            }
            return ss.str();
        }

        [[nodiscard]] const char *getSymbol(const size_t index) const { return _symbolTable.getSymbol(index); }

    private:
        Inter::SymbolTable &_symbolTable;
        CallFrame *_currentFrame{ nullptr };
        // 栈的最大深度为1024
        static constexpr auto MAX_CALL_DEPTH = 1024;
        FastRegisterPool _regPool{};
        CallFrame _callStack[MAX_CALL_DEPTH];
        size_t _stackTop{ 0 };
        std::vector<TjsValue> _globals{};
        bool _zf{ false };
    };
} // namespace Ciallang::Bytecode
