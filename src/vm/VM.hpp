/*
 * Copyright (c) 2025/12/29 上午8:08
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
#include "types/Value.hpp"

namespace Cial {
    class Runtime;
    namespace Bytecode {
        class VMState;
    }
} // namespace Cial

namespace Cial {
    class VM {
    public:
        class Handle {
            Runtime *rt;

        public:
            Value value;
            Handle(Runtime *rt, const Value &o) noexcept;

            ~Handle() noexcept;
        };

        explicit VM(Bytecode::VMState *vmState);

        [[nodiscard]] Handle getGlobal(const String &name) const;

        [[nodiscard]] Handle evalExpr(const String &expr) const;

    private:
        Bytecode::VMState *_vmState;
    };
} // namespace Cial