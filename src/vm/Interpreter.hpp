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
#include "collections/ConservativeVector.hpp"
#include "types/TjsFunction.hpp"

#include "types/TjsValue.hpp"

#include "vm/Register.hpp"

namespace Ciallang::Bytecode {
    // 每个 CallFrame 都有自己的一组 registers vector 太影响性能, 改为共享容器加偏移量.
    struct CallFrame {
        const Chunk *chunk;
        uint32_t registersOffset;
        std::optional<Register> ret{};
        size_t pc{};

        explicit CallFrame(const Chunk *chunk_, const uint32_t registersOffset_, const std::optional<Register> ret_) :
            chunk(chunk_), registersOffset(registersOffset_), ret(ret_) {}

        CallFrame(CallFrame &&callFrame) noexcept :
            chunk(callFrame.chunk), registersOffset(callFrame.registersOffset), ret(callFrame.ret), pc(callFrame.pc) {}

        CallFrame &operator=(CallFrame &&callFrame) noexcept {
            if(this == &callFrame)
                return *this;

            chunk = callFrame.chunk;
            ret = callFrame.ret;
            registersOffset = callFrame.registersOffset;
            pc = callFrame.pc;

            return *this;
        }

        CallFrame(const CallFrame &) = delete;
        CallFrame &operator=(const CallFrame &) = delete;
    };

    class Interpreter {
    public:
        explicit Interpreter() = default;

        void run(const Chunk *mainChunk);

        void reg(const Register reg, TjsValue value) {
            const auto index = reg.index() + _currentFrame->registersOffset;
            allocRegisters(index);

            _registers[index] = std::move(value);
            ++_logicRegistersSize;
        }

        [[nodiscard]] TjsValue reg(Register reg);
        [[nodiscard]] const TjsValue &reg(Register reg) const;

        [[nodiscard]] const TjsValue &global(const std::string &identifier) const { return _globals.at(identifier); }

        void global(const std::string &identifier, TjsValue &&value) { _globals[identifier] = std::move(value); }

        template <typename T>
            requires std::is_base_of_v<TjsObject, T>
        void global(T *obj) {
            global(obj->name(), TjsValue{ obj });
        }

        void setZF(const bool zf) { _zf = zf; }

        [[nodiscard]] bool getZF() const { return _zf; }

        void setPC(const Label label) { _callStack.back().pc = label.address(); }

        [[nodiscard]] size_t getPC() const { return _currentFrame->pc; }

        void pushCallFrame(CallFrame &&frame) {
            _callStack.pushBack(std::move(frame));
            _currentFrame = &_callStack.back();
        }

        CallFrame popCallFrame() {
            CallFrame frame = std::move(*_currentFrame);
            _callStack.popBack();
            _currentFrame = &_callStack.back();
            _logicRegistersSize = frame.registersOffset;
            return std::move(frame);
        }

        [[nodiscard]] const std::vector<Op::Instruction *> &instructions() const noexcept {
            return _currentFrame->chunk->instructions();
        }

        [[nodiscard]] const Chunk *current() const noexcept { return _currentFrame->chunk; }

        CallFrame createCallFrame(const Chunk *chunk, const std::optional<Register> ret = {}) const {
            return CallFrame{ chunk, _logicRegistersSize, ret };
        }

        [[nodiscard]] std::string dumpRegisters() const {
            std::stringstream ss{};
            for(size_t i = 0; i < _logicRegistersSize; i++) {
                ss << fmt::format("(%{}): {}\n", i, _registers[i]);
            }
            return ss.str();
        }

        [[nodiscard]] std::string dumpInstruction(const Chunk &chunk) const {
            std::stringstream ss{};
            size_t pc{};
            std::vector<TjsFunction *> functions{};
            while(pc < chunk.instructions().size()) {
                const auto instruction = chunk.instructions()[pc];
                ss << fmt::format("{: <6}: {}\n", Label{ pc }, instruction->dump(*this, false));

                if(const auto loadIns = dynamic_cast<Op::Load *>(instruction); loadIns && loadIns->value().isObject()) {
                    if(auto fun = dynamic_cast<TjsFunction *>(loadIns->value().toObject())) {
                        functions.push_back(fun);
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

        void allocRegisters(const size_t index) {
            if(index >= _registers.size()) {
                // maybe increase or decrease
                _registers.resize(index + 1);
                // CHECK_LE(_registers.size(), std::numeric_limits<uint32_t>::max());
            }
        }

    private:
        CallFrame *_currentFrame{ nullptr };
        // 考虑使用map?
        Collections::ConservativeVector<CallFrame> _callStack{};
        Collections::ConservativeVector<TjsValue> _registers{};
        uint32_t _logicRegistersSize{};
        std::unordered_map<std::string, TjsValue> _globals{};

        bool _zf{ false };

    public:
        void applyArgument(const CallFrame &frame, const Register reg, TjsValue value) {

            const auto index = reg.index() + frame.registersOffset;
            allocRegisters(index);

            _registers[index] = std::move(value);
            ++_logicRegistersSize;
        }
    };
} // namespace Ciallang::Bytecode
