/*
 * Copyright (c) 2024/5/7 下午8:44
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

#include "../types/TjsValue.hpp"
#include "DataPool.hpp"
#include "Rlc.hpp"

namespace Ciallang::VM {
    enum class Opcode : uint8_t;

    class VMChunk : DataPool<uint8_t> {

    public:
        using DataPool::count;
        using DataPool::baseAddress;

        explicit VMChunk(const std::filesystem::path& path): _rlc{ path } {
        }

        template <typename A = std::initializer_list<uint8_t>>
            requires (std::is_same_v<std::initializer_list<uint8_t>, A>
                      || std::is_same_v<std::vector<uint8_t>, A>)
        void emit(const Opcode opcode,
                  const Common::SourceLocation line,
                  const A& args = {}) {
            emit(static_cast<uint8_t>(opcode), line);
            for(uint8_t arg : args) emit(arg, line);
        }

        int16_t emitJmp(
            const Opcode& opcode,
            const Common::SourceLocation& line,
            int16_t addr = 0x7FFF
        );

        void patchJmp(size_t offset) const;

        // index from 0 start
        size_t addConstant(TjsValue&& val) {
            for(size_t i = 0; i < _valueArray.count(); i++) {
                const auto &temp = _valueArray.dataPool[i];
                if(temp == val) return i;
            }
            _valueArray.writeData(std::move(val));
            return _valueArray.count() - 1;
        }

        [[nodiscard]] const Rlc* rlc() const { return &_rlc; }

        [[nodiscard]] uint8_t bytecodes(const size_t offset) const {
            return dataPool[offset];
        }

        [[nodiscard]] const TjsValue& constants(const size_t offset) const {
            return _valueArray.dataPool[offset];
        }

        void reset() override {
            DataPool::reset();
            _valueArray.reset();
            _rlc.reset();
        }

    private:
        DataPool<TjsValue> _valueArray{};

        Rlc _rlc;

        void emit(uint8_t byte, const Common::SourceLocation line) {
            _rlc.addBytecodeLine(count(), line);
            writeData(std::forward<uint8_t>(byte));
        }
    };
}
