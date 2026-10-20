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

namespace cial::vm {

    // FIFO Model
    class FastRegisterPool {
    public:
        explicit FastRegisterPool() : _sp(0) { allocateBlock(); }

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
            maybeShrink(_sp);
        }

        void maybeShrink(const u64 sp) {
            _sp = sp;
            const size_t totalUsed = _sp;
            size_t totalCapacity = (_blocks.size() - 1) * _blockSize;

            // 至少保留 minBlocks 个块
            constexpr size_t minBlocks = 2;

            while(_blocks.size() > minBlocks && totalUsed <= totalCapacity - _blockSize) {
                _blocks.pop_back();
                totalCapacity -= _blockSize;
            }
        }

        [[nodiscard]] Value *ptrAt(const u64 globalIndex) const {
            static_assert((_blockSize & (_blockSize - 1)) == 0, "blockSize must be a power of two");
            // 编译期常量块大小：除法/取模优化为移位与掩码
            const size_t blockIndex = globalIndex >> _blockShift;
            const size_t offset = globalIndex & (_blockSize - 1);
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
            }

            ~Block() { delete[] data; }
        };

        static constexpr size_t _blockSize = 1 << 12;
        static constexpr size_t _blockShift = 12;

        Vec<Block> _blocks;
        size_t _sp{ 0 }; // 全局栈指针（相对于首块）

        void allocateBlock() { _blocks.emplace_back(_blockSize); }

        void ensureCapacity(const size_t need) {
            size_t totalCapacity = _blocks.size() * _blockSize;

            while(need > totalCapacity) {
                allocateBlock();
                totalCapacity += _blockSize;
            }
        }
    };
} // namespace cial::vm