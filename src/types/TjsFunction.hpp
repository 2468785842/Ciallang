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

#include "pch.h"

#include "TjsObject.hpp"
#include "vm/Chunk.hpp"

namespace Ciallang {
    class TjsFunction final : public TjsObject {
    public:
        using VMChunkPtr = std::shared_ptr<Bytecode::Chunk>;

        TjsFunction() = delete;

        explicit TjsFunction(
            Bytecode::Chunk&& chunk,
            const std::string& name
        ): _chunk(std::make_shared<Bytecode::Chunk>(std::move(chunk))), _name(name) {
        }

        explicit TjsFunction(
            Bytecode::Chunk&& chunk,
            const std::string& name,
            const size_t arity
        ): _chunk(std::make_shared<Bytecode::Chunk>(std::move(chunk))),
           _name(name), _arity(arity) {
        }

        // clone
        explicit TjsFunction(
            VMChunkPtr& chunk,
            std::string& name,
            const size_t arity
        ): _chunk(chunk), _name(name), _arity(arity) {
        }

        [[nodiscard]] std::string_view name() const noexcept override {
            return _name;
        }

        [[nodiscard]] Bytecode::Chunk* chunk() const {
            return _chunk.get();
        }

        [[nodiscard]] size_t arity() const noexcept override {
            return _arity;
        }

        [[nodiscard]] bool isNative() const noexcept override {
            return false;
        }

        ~TjsFunction() noexcept override = default;

    private:
        const VMChunkPtr _chunk{ nullptr };
        const std::string _name{};
        const size_t _arity{};
    };
}
