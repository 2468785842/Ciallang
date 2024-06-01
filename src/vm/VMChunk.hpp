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

#include <cstdint>

#include "DataPool.hpp"
#include "Instruction.hpp"

namespace Ciallang::VM {
    // pd (direct / property direct) 直接,属性直接
    // pi (indirect / property indirect) 间接,属性间接

    //     enum class Opcodes : uint8_t {
    //         NOP,
    //         CONST,
    //         CP,
    //         CL,
    //         CCL,
    //         TT,
    //         TF,
    //         CEQ,
    //         CDEQ,
    //         CLT,
    //         CGT,
    //         SETF,
    //         SETNF,
    //         LNOT,
    //         NF,
    //         JF,
    //         JNF,
    //         JMP,
    // #define TJS_NORMAL_AND_PROPERTY_ACCESSER(x) x, x##PD, x##PI, x##P
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(INC),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(DEC),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(LOR),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(LAND),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(BOR),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(BXOR),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(BAND),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(SAR),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(SAL),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(SR),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(ADD),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(SUB),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(MOD),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(DIV),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(IDIV),
    //         TJS_NORMAL_AND_PROPERTY_ACCESSER(MUL),
    // #undef TJS_NORMAL_AND_PROPERTY_ACCESSER
    //
    //         BNOT,
    //         TYPEOF,
    //         TYPEOFD,
    //         TYPEOFI,
    //         EVAL,
    //         EEXP,
    //         CHKINS,
    //         ASC,
    //         CHR,
    //         NUM,
    //         CHS,
    //         INV,
    //         CHKINV,
    //         INT,
    //         REAL,
    //         STR,
    //         OCTET,
    //         CALL,
    //         CALLD,
    //         CALLI,
    //         NEW,
    //         GPD,
    //         SPD, SPDE, SPDEH,
    //         GPI,
    //         SPI,
    //         SPIE,
    //         GPDS, SPDS, GPIS, SPIS,
    //         SETP, GETP,
    //         DELD,
    //         DELI,
    //         SRV,
    //         RET,
    //         ENTRY,
    //         EXTRY,
    //         THROW,
    //         CHGTHIS,
    //         GLOBAL,
    //         ADDCI,
    //         REGMEMBER,
    //         DEBUGGER,
    //     };

    enum class OperandTypes {
        Register,
        Constants
    };

    // inline static std::unordered_map<Opcodes, std::string> S_OpcodeToName{
    //         { Opcodes::NOP, "NOP" },
    //         { Opcodes::CONST, "CONST" },
    //         { Opcodes::GPD, "GPD" },
    //         { Opcodes::CALL, "CALL" },
    //         { Opcodes::SRV, "SRV", },
    //         { Opcodes::RET, "RET" }
    // };


    class VMChunk : DataPool<uint8_t> {
    public:
        explicit VMChunk(const std::filesystem::path& path): _rlc{ path } {
        };

        void emit(const size_t line, const Opcodes opcode,
                  const std::initializer_list<uint8_t> args = {}) {
            emit(static_cast<uint8_t>(opcode), line);
            for(uint8_t arg : args) {
                emit(arg, line);
            }
        }

        // index from 0 start
        size_t load(TjsValue&& val) {
            _valueArray.writeData(std::move(val));
            return count - 1;
        }

        [[nodiscard]] uint8_t* bytecodes() const { return _bytecodes; }

        [[nodiscard]] TjsValue* constants() const { return _valueArray.dataPool; }

        void disassemble() const;

    private:
        uint8_t*& _bytecodes = dataPool;

        DataPool<TjsValue> _valueArray{};

        Rlc _rlc;

        void emit(uint8_t byte, const size_t line) {
            _rlc.addBytecodeLine(count, line);
            writeData(std::forward<uint8_t>(byte));
        }
    };
}
