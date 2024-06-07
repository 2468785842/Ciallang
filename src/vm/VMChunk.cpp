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
#include "VMChunk.hpp"

#include "Instruction.hpp"

namespace Ciallang::VM {
    /**
     * relative jmp
     * @return address offset
     */
    int16_t VMChunk::emitJmp(
        const Opcode& opcode,
        const Common::SourceLocation& line,
        const int16_t addr) {
        emit(opcode, line, {
                static_cast<uint8_t>(addr >> 8 & 0xFF),
                static_cast<uint8_t>(addr & 0xFF)
        });
        return static_cast<int16_t>(count() - 2);
    }

    void VMChunk::patchJmp(const size_t offset) const {
        uint16_t jump = count() - offset - 2;

        if(jump > UINT16_MAX) {
            DLOG(FATAL) << "Too much code to jump over.";
        }

        dataPool[offset] = static_cast<uint8_t>(jump >> 8 & 0xFF);
        dataPool[offset + 1] = static_cast<uint8_t>(jump & 0xFF);
    }

}
