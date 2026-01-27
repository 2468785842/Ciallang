//
// Created by LiDong on 2026/1/23.
//

#include "Chunk.hpp"

#include "VMState.hpp"

namespace cial::vm {

    [[nodiscard]] String Chunk::dumpConstant(const Runtime *rt, const ConstIdx idx) const {
        std::stringstream ss{ "" };
        switch(const auto &constant = _constants[idx.index()]; constant.type()) {
            case ConstantType::None:
                ss << "Void";
                break;
            case ConstantType::Real:
                ss << constant.value<Real>().value();
                break;
            case ConstantType::Atom:
                ss << '"' << *rt->atomTable.get(constant.value<Atom>())->str << '"';
                break;
            case ConstantType::FuncMeta:
                ss << "Ptr<FuncMeta>";
                break;
            case ConstantType::ClassMeta:
                ss << "Ptr<ClassMeta>";
                break;
            case ConstantType::PropMeta:
                ss << "Ptr<PropMeta>";
                break;
        }
        return String{ ss.str() };
    }

    [[nodiscard]] String Chunk::dumpConstants(const Runtime *rt) const {
        std::stringstream ss{ "" };
        u16 i{};
        while(i < _constants.size()) {
            ConstIdx idx{ i };
            ss << fmt::format("{: <6} = {}", idx, dumpConstant(rt, idx));
            if(i != _instructions.size() - 1) {
                ss << '\n';
            }
            ++i;
        }
        return String{ ss.str() };
    }

    [[nodiscard]] String Chunk::dumpInstructions() const {
        std::stringstream ss{ "" };
        size_t pc{};
        while(pc < _instructions.size()) {
            const auto &instruction = _instructions[pc];
            ss << fmt::format("{: <6}: {}", inter::Label{ pc }, instruction->dump(nullptr));
            if(pc != _instructions.size() - 1) {
                ss << '\n';
            }
            ++pc;
        }
        return String{ ss.str() };
    }

    [[nodiscard]] String Chunk::dumpInstructions(const VMState *vmState) const {
        std::stringstream ss{ "" };
        size_t pc{};
        while(pc < vmState->getPC()) {
            const bool isLastInst = pc == vmState->getPC() - 1;
            const auto &instruction = _instructions[pc];
            ss << fmt::format("{: <6}: {}", inter::Label{ pc }, instruction->dump(isLastInst ? vmState : nullptr));
            if(pc != _instructions.size() - 1) {
                ss << '\n';
            }
            ++pc;
        }
        return String{ ss.str() };
    }

} // namespace cial::vm