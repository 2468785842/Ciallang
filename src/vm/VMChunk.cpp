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
    // void VMChunk::disassemble() const {
    //     #define RLC_NAME_LEN (_rlc.name().length() < 35 ? 35 - _rlc.name().length() : 35)
    //     #define RLC_NAME (RLC_NAME_LEN == 35 ? "-" : _rlc.name())
    //
    //     fmt::println("{0:-^{1}}", ' ' + RLC_NAME + ' ', RLC_NAME_LEN);
    //
    //     for(size_t offset = 0; offset < count;) {
    //         const auto* inst = Instruction::instance(
    //             static_cast<Opcode>(bytecodes(offset))
    //         );
    //         fmt::println("{}", inst->disassemble(dataPool, constants(), &_rlc, offset));
    //         offset += inst->length();
    //     }
    // }

    /**
     * abs jmp
     * @return address offset
     */
    size_t VMChunk::emitJmp(
        const Opcode& opcode,
        const Common::SourceLocation& line,
        const std::initializer_list<uint8_t>& addr) {

        auto addrs = encodeIEX(addr);
        emit(opcode, line, addr);
        return count - 2;
    }

    void VMChunk::patchJmp(const size_t offset) const {
        size_t jump = count - offset - 2;

        if(jump > UINT16_MAX) {
            DLOG(FATAL) << "Too much code to jump over.";
        }

        dataPool[offset] = static_cast<uint8_t>(jump >> 8 & 0xff);
        dataPool[offset + 1] = static_cast<uint8_t>(jump & 0xff);
    }
}
