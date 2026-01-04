/*
 * Copyright (c) 2025/12/30 上午8:08
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

#include <ranges>
#include <unordered_map>

#include "AtomTable.hpp"
#include "Runtime.hpp"

#include "vm/CallFrame.hpp"
#include "vm/Chunk.hpp"
#include "vm/FastRegisterPool.hpp"

#include "gc/GC.hpp"

#include "types/Object.hpp"
#include "types/Value.hpp"

#include "stdlib/NativeRegister.hpp"

namespace Cial {

    class Context {
    public:
        Runtime &rt;
        Map<Atom, Value> gObj{};
        NativeRegister nativeRegister{ rt };

        Bytecode::FastRegisterPool regPool{};

        // Stack Max Depth Is 1024
        static constexpr auto maxCallDepth = 1024;
        CallFrame callStack[maxCallDepth];

        size_t stackTop{ 0 }; // callFrame count

        explicit Context(Runtime &rt) noexcept : rt(rt) {}

        template <typename T, typename Callable>
        void registerMethod(const String &name, Callable &&fn, const bool isStatic) {
            return nativeRegister.registerMethod<T>(name, std::forward<Callable>(fn), isStatic);
        }

        template <typename T>
        [[nodiscard]] NativeFunction *findMethod(const String &name, const bool isStatic) const {
            return nativeRegister.findMethod<T>(name, isStatic);
        }

        void registryGlobalFunc(const String &name, NativeFunction *func) noexcept {
            const Atom a = rt.atomTable.intern(name);
            gObj[a] = Value{ func };
        }

        void collectMark() {
            rt.collectMark();

            for(const auto &[staticMethods, instanceMethods] : nativeRegister.tables | std::views::values) {
                for(const auto &method : staticMethods | std::views::values) {
                    method->marked();
                }
                for(const auto &method : instanceMethods | std::views::values) {
                    method->marked();
                }
            }

            for(const auto &val : gObj | std::views::values) {
                if(val.isObject()) {
                    if(auto *msHeader = val.toObject()) {
                        msHeader->marked();
                        if(msHeader->_name != ATOM_INVALID) {
                            rt.atomTable.get(msHeader->_name)->marked();
                        }
                    }
                }
            }

            // scan stack
            const std::uint32_t used = regPool.used();
            for(std::uint32_t i = 0; i < used; ++i) {
                if(const Value *val = regPool.ptrAt(i)) {
                    if(auto *msHeader = val->toObject()) {
                        msHeader->marked();
                        if(msHeader->_name != ATOM_INVALID) {
                            rt.atomTable.get(msHeader->_name)->marked();
                        }
                    }
                }
            }

            // call frame
            for(std::uint32_t i = 0; i < stackTop; ++i) {
                const CallFrame &callFrame = callStack[i];
                if(callFrame.funcMeta)
                    callFrame.funcMeta->marked();
                else
                    callFrame.chunk->marked();
                if(callFrame.thisObj.isObject())
                    callFrame.thisObj.toObject()->marked();
            }
        }
    };
} // namespace Cial