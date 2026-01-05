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

#include <cassert>

#include "types/Value.hpp"

namespace cial::Bytecode {

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

        [[nodiscard]] Value *ptrAt(const size_t globalIndex) const {
            const size_t blockIndex = globalIndex / _blockSize; // 第几个块
            const size_t offset = globalIndex % _blockSize; // 块内偏移
            return _blocks[blockIndex].data + offset;
        }

        [[nodiscard]] size_t used() const noexcept { return _sp; }

    private:
        struct Block {
            Value *data;

            explicit Block(const size_t blockSize) : data(new Value[blockSize]) {}

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

        Vec<Block> _blocks;
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
} // namespace cial::Bytecode