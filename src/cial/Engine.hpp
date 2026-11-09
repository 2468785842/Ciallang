#pragma once

#include "cial/Capability.hpp"
#include "stdlib/ScriptsLib.hpp"
#include "stdlib/StandardLibrary.hpp"
#include "vm/VM.hpp"

namespace cial {
    // 源码接口版本由生成的 cial/Version.hpp 提供（CIAL_SDK_API_VERSION /
    // capabilities::sdkApiVersion()）；此别名过渡保留，0.2 移除。
    // 不承诺跨编译器/运行库的二进制 ABI。
    inline constexpr unsigned SDK_API_VERSION = capabilities::sdkApiVersion();

    class Engine final {
    public:
        Engine() {
            stdlib::registerStandardLibrary(_context);
            stdlib::registerScriptsLib(_context, &_state);
        }
        Engine(const Engine &) = delete;
        Engine &operator=(const Engine &) = delete;

        [[nodiscard]] Context &context() noexcept { return _context; }
        [[nodiscard]] Runtime &runtime() noexcept { return _runtime; }
        [[nodiscard]] VM &vm() noexcept { return _vm; }

        void setStorageReader(std::shared_ptr<TextStorageReader> reader) {
            stdlib::registerScriptsLib(_context, &_state, {}, std::move(reader));
        }

        // sourceName 是宿主提供的逻辑身份，可为归档名或虚拟 URI。
        // Result 由调用方渲染；运行期异常继续抛出。返回句柄不能超过 Engine 生命周期。
        [[nodiscard]] VM::Handle<Value> execute(const std::string &text, Common::Result &result,
                                                const std::string &sourceName = "<host>", bool expression = false,
                                                const Value &executionContext = {}) {
            Common::SourceFile source;
            source.setSourceName(sourceName);
            source.load(result, text);
            return _vm.execute(source, result, expression, executionContext);
        }

    private:
        Runtime _runtime;
        Context _context{ _runtime };
        vm::VMState _state{ _context };
        VM _vm{ &_state };
    };
} // namespace cial
