#pragma once

#include <memory>
#include "types/Types.hpp"

namespace cial::vm {
    class Chunk;
    enum class JitMode { Off, Eager, Trace };
    struct JitStats {
        u64 compiled{};
        u64 executions{};
        u64 typeExits{};
        u64 sideExits{};
        u64 recordedTraces{};
    };

    // 可选整数后端。所有 guard 在独立数值窗口中完成，失败不提交寄存器或 PC。
    class IntegerJit {
    public:
        explicit IntegerJit(JitMode mode, u32 threshold = 32);
        ~IntegerJit();
        IntegerJit(const IntegerJit &) = delete;
        IntegerJit &operator=(const IntegerJit &) = delete;
        static bool available() noexcept;
        bool tryRun(Chunk &chunk, Value *registers, u64 &pc, bool &zf);
        void observe(Chunk &chunk, u64 before, u64 after);
        void breakTrace() noexcept;
        [[nodiscard]] JitStats stats() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
} // namespace cial::vm
