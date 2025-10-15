/*
 * Copyright (c) 2024/6/8 上午10:23
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

#include "TjsObject.hpp"
#include "vm/Chunk.hpp"

namespace Ciallang {
    class TjsFunction final : public TjsObject {
    public:
        TjsFunction() = delete;

        explicit TjsFunction(Bytecode::Chunk *chunk, const std::string &name);

        explicit TjsFunction(Bytecode::Chunk *chunk, std::string name, size_t arity);

        [[nodiscard]] const char *name() const noexcept override { return _name.c_str(); }

        [[nodiscard]] const Bytecode::Chunk *chunk() const { return _chunk.get(); }

        [[nodiscard]] size_t arity() const noexcept override { return _arity; }

        [[nodiscard]] bool isNative() const noexcept override { return false; }

        ~TjsFunction() noexcept override = default;

    private:
        const std::unique_ptr<Bytecode::Chunk> _chunk;
        const std::string _name;
        const size_t _arity;
    };
} // namespace Ciallang
