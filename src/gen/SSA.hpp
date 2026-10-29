//
// Created by LiDong on 2026/10/29.
//
// SSA（静态单赋值）按需构造。输入既有 TAC + CFG，输出支配树/支配边界/
// pruned phi 放置/重命名后的到达定义与 def-use 链。只读分析，不改写指令流，
// 不接入 parseAst 默认管线（--dump-ssa 等入口按需调用，见 README 任务约定）。
//
// 受限范围（明确拒绝优于错误 SSA，见 Agent Mail 线程 #25/#27）：
//  - CallOmit/CallExpand/CollapseArgs 隐式读取帧实参（%0..%argc-1）且无静态
//    分析 API，extractAccess 直接抛错；含这三类指令的 chunk 不构造 SSA。
//  - 寄存器级 SSA：全局槽/属性内存、_zf 标志不在建模范围。
#pragma once

#include <limits>

#include "ControlFlowGraph.hpp"
#include "Instruction.hpp"
#include "vm/Chunk.hpp"

namespace cial::inter {

    // SSA 定义点引用。
    // Kind::Inst  — TAC 指令的定义（index = TAC pc，slot = 该指令的第几个定义；
    //               GMethodC 同时定义 dst 与 receiver，两槽身份不同）
    // Kind::Phi   — 块入口汇合（index = SsaForm::phis() 下标）
    // Kind::Entry — 函数入口值（index = 寄存器号）：无任何到达定义时入口实参
    // Kind::Catch — 异常着陆块引入的未知值（index = 着陆块号）：与入口参数、
    //               其他 catch 的未知值身份互不相同
    // Kind::None  — 无定义：不可达指令，或非 catch 语境下某前驱路径上无定义
    struct SsaRef {
        enum class Kind : u8 { None, Inst, Phi, Entry, Catch };
        Kind kind{Kind::None};
        u32 index{};
        u16 slot{};

        [[nodiscard]] bool valid() const noexcept { return kind != Kind::None; }
        bool operator==(const SsaRef &rhs) const noexcept = default;
    };

    // phi 节点：位于块入口，inputs 与 predecessorsOf(block) 一一对应
    struct SsaPhi {
        size_t block{};
        Register reg{};
        Vec<SsaRef> inputs; // 无定义路径为 None；catch 路径为 Catch 引用
    };

    // 一个定义点的全部使用位置（def-use 链）
    struct SsaUse {
        enum class Kind : u8 { Operand, PhiInput };
        Kind kind{Kind::Operand};
        u32 a{}; // Operand: TAC pc;  PhiInput: phi 下标
        u32 b{}; // Operand: 该指令使用点序号;  PhiInput: inputs 下标
        bool operator==(const SsaUse &rhs) const noexcept = default;
    };

    // 单条 TAC 指令的 SSA 信息（与 chunk.getInstVec() 平行；抽象构造时为空）
    struct InstSsa {
        Vec<Register> defs;      // 有序定义槽位（空 = 无定义）
        Vec<SsaRef> operandDefs; // 与使用点平行，按出现顺序；不可达指令为空
        size_t block{std::numeric_limits<size_t>::max()}; // 所属块；不可达指令为 max
    };

    // 抽象 CFG 的每块事实（变量为任意 u16 编号；chunk 便捷入口内部生成）
    struct SsaBlockFacts {
        Vec<u16> defined;    // 块内被定义的变量（去重）
        Vec<u16> upwardUses; // 块内任何定义之前被读取的变量（去重）
        // 块内指令级事实（按序；纯块级分析可留空）
        struct Inst {
            u32 pc{};       // 全局指令编号（回填 InstSsa 时的下标）
            Vec<u16> defs;  // 有序定义（空 = 无定义）
            Vec<u16> uses;  // 有序使用（读-改-写的读取先于定义）
        };
        Vec<Inst> insts;
    };

    class SsaForm {
    public:
        [[nodiscard]] size_t blockCount() const noexcept { return _successors.size(); }
        [[nodiscard]] size_t entryBlock() const noexcept { return _entry; }
        [[nodiscard]] const Vec<Vec<size_t>> &successors() const noexcept { return _successors; }
        [[nodiscard]] const Vec<size_t> &successorsOf(const size_t block) const { return _successors.at(block); }
        [[nodiscard]] const Vec<size_t> &predecessorsOf(const size_t block) const { return _predecessors.at(block); }

        // 直接支配者；入口块返回自身，不可达块返回 InvalidBlock
        static constexpr size_t InvalidBlock = std::numeric_limits<size_t>::max();
        [[nodiscard]] size_t idomOf(const size_t block) const { return _idom.at(block); }
        [[nodiscard]] const Vec<Vec<size_t>> &dominatorTree() const noexcept { return _domTree; }
        [[nodiscard]] const Vec<size_t> &dominanceFrontierOf(const size_t block) const { return _domFrontiers.at(block); }

        [[nodiscard]] const Vec<SsaPhi> &phis() const noexcept { return _phis; }
        [[nodiscard]] const Vec<u32> &phiIndicesAt(const size_t block) const { return _phiIndices.at(block); }
        [[nodiscard]] const Vec<InstSsa> &insts() const noexcept { return _insts; }
        [[nodiscard]] const InstSsa &instSsa(const size_t pc) const { return _insts.at(pc); }
        [[nodiscard]] bool isExceptionEntry(const size_t block) const { return _exceptionEntry.at(block); }
        [[nodiscard]] const Vec<SsaUse> &usesOfDef(const SsaRef &def) const;

        [[nodiscard]] size_t variableCount() const noexcept { return _variableCount; }

    private:
        friend class SsaBuilder;

        Vec<Vec<size_t>> _successors;
        Vec<Vec<size_t>> _predecessors;
        size_t _entry{};
        size_t _variableCount{};
        Vec<size_t> _idom;
        Vec<Vec<size_t>> _domTree;
        Vec<Vec<size_t>> _domFrontiers;
        Vec<SsaPhi> _phis;
        Vec<Vec<u32>> _phiIndices; // 每块 → _phis 下标
        Vec<bool> _exceptionEntry;
        Vec<InstSsa> _insts;
        Map<u64, Vec<SsaUse>> _usesOfDef;
    };

    class SsaBuilder {
    public:
        // 单条指令的精确寄存器访问：defs 为有序定义槽位（GMethodC 两个），
        // def 为首个定义（单定义指令的主定义，兼容便捷查询），uses 按出现
        // 顺序（读-改-写的读取在 defs 之前生效）。
        // 未登记 def/use 的 opcode、以及隐式读帧实参的三个调用指令抛错——不猜测。
        struct Access {
            Opt<Register> def;      // 首个定义；无定义为空
            Vec<Register> defs;     // 全部定义槽位（有序）
            Vec<Register> uses;
        };
        static Access extractAccess(const Instruction &instruction);

        // 抽象 CFG 核心。successors/ facts 按块下标平行；entryBlock 必须合法。
        // preserveAllLiveOut：对应块的出口视为所有变量活跃（try 保守）。
        // exceptionEntries：SSA 边界块（异常着陆点）——到达定义在此重置并引入
        // Catch 未知值，块内不放置 phi。不可达块不参与支配/phi，仅保留 defs 静态信息。
        static SsaForm build(Vec<Vec<size_t>> successors, size_t entryBlock,
                             Vec<SsaBlockFacts> facts, size_t variableCount,
                             const Vec<bool> &preserveAllLiveOut = {},
                             const Vec<bool> &exceptionEntries = {});

        // Chunk 便捷入口：精确 def/use 提取 + try 区间保守标记 + build()。
        // 只读，不修改 chunk。
        static SsaForm buildChunkSsa(vm::Chunk &chunk, const Vec<BasicBlock> &blocks);

        // 文本导出（调试用；不依赖 VMState）
        static std::string dumpSsaText(vm::Chunk &chunk, const SsaForm &form);
    };

} // namespace cial::inter
