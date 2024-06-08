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
#include "vm/VMChunk.hpp"

namespace Ciallang {
    class TjsFunction final : public TjsObject {
        using DefParameters = std::vector<VM::VMChunk>;
        using DefParametersPtr = std::shared_ptr<DefParameters>;
        using VMChunkPtr = std::shared_ptr<VM::VMChunk>;

    public:
        TjsFunction() = delete;

        explicit TjsFunction(
            VM::VMChunk&& chunk,
            const std::string& name
        ): _chunk(std::make_shared<VM::VMChunk>(std::move(chunk))), _name(name) {
        }

        explicit TjsFunction(
            DefParameters&& defParameters,
            VM::VMChunk&& chunk,
            const std::string& name
        ): _defParameters(std::make_shared<DefParameters>(std::move(defParameters))),
           _chunk(std::make_shared<VM::VMChunk>(std::move(chunk))), _name(name) {
        }

        explicit TjsFunction(
            const DefParametersPtr& defParameters,
            const VMChunkPtr& chunk,
            const std::string& name
        ): _defParameters(defParameters), _chunk(chunk), _name(name) {
        }

        [[nodiscard]] std::string_view name() const noexcept override { return _name; }

        [[nodiscard]] const VM::VMChunk& chunk() const { return *_chunk; }

        [[nodiscard]] std::unique_ptr<TjsObject> clone() const noexcept override {
            return std::make_unique<TjsFunction>(
                _defParameters, _chunk, _name
            );
        }

        ~TjsFunction() noexcept override = default;

    private:
        const DefParametersPtr _defParameters{ nullptr };
        const VMChunkPtr _chunk{ nullptr };
        const std::string _name{};
    };
}
