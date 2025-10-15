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
#include "gen/BytecodeGenerator.hpp"
#include "logging/Logger.hpp"
#include "types/TjsFunction.hpp"

#include "types/TjsValue.hpp"

#include "vm/Register.hpp"

namespace Ciallang::Bytecode {

    // FIFO Model
    class FastRegisterPool {
    public:
        explicit FastRegisterPool(const size_t blockSize = 1 << 12) : _blockSize(blockSize), _sp(0) { allocateBlock(); }

        // 分配连续的寄存器帧
        size_t allocFrame(const size_t n) {
            ensureCapacity(_sp + n);
            const size_t baseSP = _sp;
            _sp += n;
            return baseSP;
        }

        // 释放最近分配的帧
        void freeFrame(const size_t n) {
            assert(_sp >= n);
            _sp -= n;
            maybeShrink();
        }

        [[nodiscard]] TjsValue *ptrAt(const size_t globalIndex) const {
            const size_t blockIndex = globalIndex / _blockSize; // 第几个块
            const size_t offset = globalIndex % _blockSize; // 块内偏移
            return _blocks[blockIndex].data + offset;
        }

        [[nodiscard]] size_t used() const noexcept { return _sp; }

    private:
        struct Block {
            TjsValue *data;

            explicit Block(const size_t blockSize) : data(new TjsValue[blockSize]) {}

            Block(const Block &) = delete;
            Block &operator=(const Block &) = delete;
            Block(Block &&rhs) noexcept : data(rhs.data) { rhs.data = nullptr; }
            Block &operator=(Block &&rhs) noexcept {
                if(this != &rhs) {
                    this->~Block();
                    new(this) Block(std::move(rhs));
                }
                return *this;
            };

            ~Block() { delete[] data; }
        };

        std::vector<Block> _blocks;
        size_t _blockSize;
        size_t _sp; // 全局栈指针（相对于首块）

        void allocateBlock() { _blocks.emplace_back(_blockSize); }

        void ensureCapacity(const size_t need) {
            size_t totalCapacity = _blocks.size() * _blockSize;

            while(need > totalCapacity) {
                allocateBlock();
                totalCapacity += _blockSize;
            }
        }

        void maybeShrink() {
            const size_t totalUsed = _sp;
            size_t totalCapacity = (_blocks.size() - 1) * _blockSize;

            // 至少保留 minBlocks 个块
            constexpr size_t minBlocks = 2;

            while(_blocks.size() > minBlocks && totalUsed <= totalCapacity - _blockSize) {
                _blocks.pop_back();
                totalCapacity -= _blockSize;
            }
        }
    };

    struct CallFrame {
        const Chunk *chunk{ nullptr };
        size_t baseRegSP{ 0 };
        std::optional<Register> ret{};
        size_t pc{};

        explicit CallFrame() = default;

        explicit CallFrame(const Chunk *chunk_, const std::optional<Register> ret_, FastRegisterPool &pool) :
            chunk(chunk_), baseRegSP(pool.allocFrame(chunk_->getRegCount())), ret(ret_), _pool(&pool) {}

        CallFrame(CallFrame &&callFrame) noexcept :
            chunk(callFrame.chunk), baseRegSP(callFrame.baseRegSP), ret(callFrame.ret), pc(callFrame.pc),
            _pool(callFrame._pool) {
            callFrame._pool = nullptr;
        }

        CallFrame &operator=(CallFrame &&callFrame) noexcept {
            if(this == &callFrame)
                return *this;

            chunk = callFrame.chunk;
            ret = callFrame.ret;
            baseRegSP = callFrame.baseRegSP;
            pc = callFrame.pc;
            _pool = callFrame._pool;

            callFrame._pool = nullptr;

            return *this;
        }

        CallFrame(const CallFrame &) = delete;
        CallFrame &operator=(const CallFrame &) = delete;

        ~CallFrame() {
            if(_pool) {
                _pool->freeFrame(chunk->getRegCount());
            }
        }

        [[nodiscard]] TjsValue &getReg(const size_t index) { return *_pool->ptrAt(baseRegSP + index); }

        [[nodiscard]] const TjsValue &getReg(const size_t index) const { return *_pool->ptrAt(baseRegSP + index); }

    private:
        FastRegisterPool *_pool{ nullptr };
    };

    class VMState {
    public:
        explicit VMState(Inter::SymbolTable &symbolTable) : _symbolTable(symbolTable) {}

        void run(const Chunk *mainChunk);

        void reg(const Register &reg, const TjsValue &value) const;

        void reg(const Register &reg, TjsValue &&value) const;

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
        }

        [[nodiscard]] const std::vector<Op::Instruction> &instructions() const noexcept {
            return _currentFrame->chunk->instructions();
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
            std::vector<TjsFunction *> functions{};
            while(pc < chunk.instructions().size()) {
                const auto &instruction = chunk.instructions()[pc];

                ss << fmt::format("{: <6}: {}\n", Label{ pc }, Op::Instruction::dump(instruction, *this, false));

                if(instruction.opcode == Op::OpCode::Load) {
                    if(auto value = Op::Load::value(instruction); value.isObject()) {
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

        void push(const Register &r) { *_regPool.ptrAt(_regPool.allocFrame(1)) = reg(r); }

        void pop(const size_t count) { _regPool.freeFrame(count); }
        [[nodiscard]] size_t getRegPoolTop() const { return _regPool.used(); }

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
