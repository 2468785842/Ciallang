#include "IntegerJit.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include "Chunk.hpp"
#include "types/Value.hpp"

#if defined(CIAL_ENABLE_JIT)
#include <asmjit/x86.h>
#endif

namespace cial::vm {
#if defined(CIAL_ENABLE_JIT)
    namespace {
        struct Step {
            VmOpCode op{};
            u16 dst{}, lhs{}, rhs{};
            u8 comparison{};
            i64 immediate{};
            u64 pc{}, end{}, next{}, target{};
        };
        bool decode(const Chunk &chunk, const u64 start, Step &s, const bool branches) {
            const auto &code = chunk.code();
            if(start >= code.size())
                return false;
            const auto *ip = code.data();
            u64 pc = start;
            s.pc = start;
            s.op = static_cast<VmOpCode>(ip[pc++]);
            const auto need = [&](const size_t n) { return n <= code.size() - pc; };
            switch(s.op) {
                case VmOpCode::LoadImm:
                    if(!need(3))
                        return false;
                    s.dst = read<u16>(ip, pc);
                    s.immediate = decodeSleb128(ip, pc, std::min<size_t>(10, code.size() - pc));
                    break;
                case VmOpCode::GLocal:
                case VmOpCode::DLocal:
                    if(!need(4))
                        return false;
                    s.lhs = read<u16>(ip, pc);
                    s.dst = read<u16>(ip, pc);
                    break;
                case VmOpCode::Add:
                case VmOpCode::Sub:
                case VmOpCode::Mul:
                case VmOpCode::EQ:
                case VmOpCode::NEQ:
                case VmOpCode::AbsEQ:
                case VmOpCode::AbsNEQ:
                case VmOpCode::LT:
                case VmOpCode::LE:
                case VmOpCode::GT:
                case VmOpCode::GE:
                    if(!need(6))
                        return false;
                    s.lhs = read<u16>(ip, pc);
                    s.rhs = read<u16>(ip, pc);
                    s.dst = read<u16>(ip, pc);
                    break;
                case VmOpCode::CompareTest:
                    if(!need(7))
                        return false;
                    s.comparison = ip[pc++];
                    if(s.comparison > 7)
                        return false;
                    s.lhs = read<u16>(ip, pc);
                    s.rhs = read<u16>(ip, pc);
                    s.dst = read<u16>(ip, pc);
                    break;
                case VmOpCode::Test:
                    if(!need(2))
                        return false;
                    s.lhs = read<u16>(ip, pc);
                    break;
                case VmOpCode::Jmp:
                case VmOpCode::JmpE:
                case VmOpCode::JmpNE:
                    if(!branches || !need(4))
                        return false;
                    s.target = read<u32>(ip, pc);
                    break;
                default:
                    return false;
            }
            if(s.dst >= chunk.getRegCount() || s.lhs >= chunk.getRegCount() || s.rhs >= chunk.getRegCount())
                return false;
            s.end = pc;
            s.next = pc;
            return true;
        }
        bool isJump(const VmOpCode op) { return op == VmOpCode::Jmp || op == VmOpCode::JmpE || op == VmOpCode::JmpNE; }
        struct EmitErrors final : asmjit::ErrorHandler {
            bool failed{};
            void handleError(asmjit::Error, const char *, asmjit::BaseEmitter *) override { failed = true; }
        };
    } // namespace

    struct IntegerJit::Impl {
        using Key = std::pair<u64, u64>;
        using Native = int (*)(i64 *);
        struct Entry {
            Native native{};
            Vec<u16> inputs, outputs;
            u64 next{};
        };
        struct Recording {
            Key key;
            u64 expected{};
            Vec<Step> steps;
        };
        JitMode mode;
        u32 threshold;
        JitStats counters;
        asmjit::JitRuntime runtime;
        std::map<Key, Entry> cache;
        std::map<Key, u32> visits;
        Opt<Recording> recording;
        Vec<i64> scratch;

        Impl(const JitMode m, const u32 t) : mode(m), threshold(t) {}
        ~Impl() {
            for(const auto &[key, entry] : cache) {
                (void)key;
                if(entry.native)
                    runtime.release(entry.native);
            }
        }
        void compile(const Key key, const Vec<Step> &steps, const u32 regCount) {
            if(cache.size() >= 128 || cache.contains(key))
                return;
            auto &entry = cache[key];
            if(steps.size() < 2)
                return;
            EmitErrors errors;
            asmjit::CodeHolder code;
            if(code.init(runtime.environment(), runtime.cpuFeatures()))
                return;
            code.setErrorHandler(&errors);
            asmjit::x86::Compiler cc{ &code };
            auto *function = cc.addFunc(asmjit::FuncSignature::build<int, i64 *>());
            if(!function || errors.failed)
                return;
            auto base = cc.newIntPtr();
            function->setArg(0, base);
            const auto fail = cc.newLabel();
            Vec<bool> defined(regCount, false), required(regCount, false);
            const auto input = [&](const u16 r) {
                if(!defined[r] && !required[r]) {
                    required[r] = true;
                    entry.inputs.push_back(r);
                }
            };
            const auto output = [&](const u16 r) {
                if(!defined[r]) {
                    defined[r] = true;
                    entry.outputs.push_back(r);
                }
            };
            const auto mem = [&](const u32 r) { return asmjit::x86::qword_ptr(base, static_cast<int32_t>(r * 8)); };
            const auto load = [&](const u16 r) {
                input(r);
                auto value = cc.newInt64();
                cc.mov(value, mem(r));
                return value;
            };
            for(const auto &s : steps) {
                if(s.op == VmOpCode::LoadImm) {
                    auto value = cc.newInt64();
                    cc.mov(value, asmjit::imm(s.immediate));
                    cc.mov(mem(s.dst), value);
                    output(s.dst);
                } else if(s.op == VmOpCode::GLocal || s.op == VmOpCode::DLocal) {
                    const auto value = load(s.lhs);
                    cc.mov(mem(s.dst), value);
                    output(s.dst);
                } else if(s.op == VmOpCode::Test) {
                    const auto value = load(s.lhs);
                    auto flag = cc.newInt64();
                    cc.mov(flag, 0);
                    cc.test(value, value);
                    cc.setne(flag.r8());
                    cc.mov(mem(regCount), flag);
                } else if(isJump(s.op)) {
                    if(s.op != VmOpCode::Jmp && s.target != s.end) {
                        cc.cmp(mem(regCount), 0);
                        const bool taken = s.next == s.target;
                        const bool expectedFlag = s.op == VmOpCode::JmpE ? taken : !taken;
                        if(expectedFlag)
                            cc.je(fail);
                        else
                            cc.jne(fail);
                    }
                } else {
                    auto lhs = load(s.lhs);
                    const auto rhs = load(s.rhs);
                    if(s.op == VmOpCode::Add || s.op == VmOpCode::Sub || s.op == VmOpCode::Mul) {
                        if(s.op == VmOpCode::Add)
                            cc.add(lhs, rhs);
                        else if(s.op == VmOpCode::Sub)
                            cc.sub(lhs, rhs);
                        else
                            cc.imul(lhs, rhs);
                        cc.jo(fail); // 失败时真实寄存器与 PC 完全未提交。
                    } else {
                        u8 kind = s.comparison;
                        switch(s.op) {
                            case VmOpCode::EQ:
                                kind = 0;
                                break;
                            case VmOpCode::NEQ:
                                kind = 1;
                                break;
                            case VmOpCode::AbsEQ:
                                kind = 2;
                                break;
                            case VmOpCode::AbsNEQ:
                                kind = 3;
                                break;
                            case VmOpCode::LT:
                                kind = 4;
                                break;
                            case VmOpCode::LE:
                                kind = 5;
                                break;
                            case VmOpCode::GT:
                                kind = 6;
                                break;
                            case VmOpCode::GE:
                                kind = 7;
                                break;
                            default:
                                break;
                        }
                        auto flag = cc.newInt64();
                        cc.mov(flag, 0);
                        cc.cmp(lhs, rhs);
                        switch(kind) {
                            case 0:
                            case 2:
                                cc.sete(flag.r8());
                                break;
                            case 1:
                            case 3:
                                cc.setne(flag.r8());
                                break;
                            case 4:
                                cc.setl(flag.r8());
                                break;
                            case 5:
                                cc.setle(flag.r8());
                                break;
                            case 6:
                                cc.setg(flag.r8());
                                break;
                            case 7:
                                cc.setge(flag.r8());
                                break;
                        }
                        lhs = flag;
                        if(s.op == VmOpCode::CompareTest)
                            cc.mov(mem(regCount), flag);
                    }
                    cc.mov(mem(s.dst), lhs);
                    output(s.dst);
                }
            }
            auto result = cc.newInt32();
            cc.mov(result, 1);
            cc.ret(result);
            cc.bind(fail);
            cc.mov(result, 0);
            cc.ret(result);
            cc.endFunc();
            if(cc.finalize() || errors.failed)
                return;
            if(runtime.add(&entry.native, &code)) {
                entry.native = nullptr;
                return;
            }
            entry.next = steps.back().next;
            ++counters.compiled;
        }
    };
#else
    struct IntegerJit::Impl {
        JitStats counters;
    };
#endif

    bool IntegerJit::available() noexcept {
#if defined(CIAL_ENABLE_JIT)
        return true;
#else
        return false;
#endif
    }
    IntegerJit::IntegerJit(const JitMode mode, const u32 threshold) {
        if(!available())
            throw std::runtime_error("JIT backend not enabled; configure with CIAL_ENABLE_JIT=ON on x64");
        if(threshold == 0)
            throw std::runtime_error("JIT threshold must be positive");
#if defined(CIAL_ENABLE_JIT)
        _impl = std::make_unique<Impl>(mode, threshold);
#else
        (void)mode;
#endif
    }
    IntegerJit::~IntegerJit() = default;
    JitStats IntegerJit::stats() const noexcept { return _impl ? _impl->counters : JitStats{}; }
    void IntegerJit::breakTrace() noexcept {
#if defined(CIAL_ENABLE_JIT)
        _impl->recording.reset();
#endif
    }
    bool IntegerJit::tryRun(Chunk &chunk, Value *registers, u64 &pc, bool &zf) {
#if defined(CIAL_ENABLE_JIT)
        auto &p = *_impl;
        if(!chunk.getThrowHandlers().empty() || p.mode == JitMode::Off)
            return false;
        const Impl::Key key{ chunk.executionTraceId(), pc };
        if(p.recording)
            return false;
        if(!p.cache.contains(key)) {
            if(p.cache.size() >= 128)
                return false;
            if(p.mode == JitMode::Eager) {
                Vec<Step> steps;
                u64 next = pc;
                for(size_t i = 0; i < 32; ++i) {
                    Step step;
                    if(!decode(chunk, next, step, false))
                        break;
                    steps.push_back(step);
                    next = step.end;
                }
                p.compile(key, steps, chunk.getRegCount());
            } else {
                Step step;
                if(!decode(chunk, pc, step, true))
                    return false;
                if(p.visits.size() >= 512 && !p.visits.contains(key))
                    return false;
                if(++p.visits[key] >= p.threshold)
                    p.recording = Impl::Recording{ key, pc, {} };
                return false;
            }
        }
        const auto found = p.cache.find(key);
        if(found == p.cache.end() || !found->second.native)
            return false;
        const auto &entry = found->second;
        for(const auto r : entry.inputs) {
            if(!registers[r].isIntegerFast()) {
                ++p.counters.typeExits;
                return false;
            }
        }
        p.scratch.resize(chunk.getRegCount() + 1);
        for(const auto r : entry.inputs)
            p.scratch[r] = registers[r].asIntegerUnsafe();
        p.scratch[chunk.getRegCount()] = zf ? 1 : 0;
        if(!entry.native(p.scratch.data())) {
            ++p.counters.sideExits;
            return false;
        }
        for(const auto r : entry.outputs) {
            registers[r] = Value{ p.scratch[r] };
        }
        zf = p.scratch[chunk.getRegCount()] != 0;
        pc = entry.next;
        ++p.counters.executions;
        return true;
#else
        (void)chunk;
        (void)registers;
        (void)pc;
        (void)zf;
        return false;
#endif
    }
    void IntegerJit::observe(Chunk &chunk, const u64 before, const u64 after) {
#if defined(CIAL_ENABLE_JIT)
        auto &p = *_impl;
        if(!p.recording)
            return;
        auto &recording = *p.recording;
        Step step;
        if(recording.key.first != chunk.executionTraceId() || recording.expected != before ||
           !decode(chunk, before, step, true)) {
            p.recording.reset();
            return;
        }
        step.next = after;
        recording.steps.push_back(step);
        recording.expected = after;
        if(recording.steps.size() == 32 || after == recording.key.second) {
            p.compile(recording.key, recording.steps, chunk.getRegCount());
            ++p.counters.recordedTraces;
            p.recording.reset();
        }
#else
        (void)chunk;
        (void)before;
        (void)after;
#endif
    }
} // namespace cial::vm
