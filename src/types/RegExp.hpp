// Copyright (c) 2026/10/24
//
// RegExp 内建对象（TJS2 RegExp）：/pattern/flags 字面量的运行时载体。
// 引擎用 std::regex（ECMAScript 方言；TJS2 原版为 oniguruma，复杂模式差异记 TODO）。
// 匹配状态（start/index/lastIndex/input/lastMatch/lastParen/leftContext/rightContext/
// matches）对齐 tjsRegExp.cpp 的 tTJSNI_RegExp；位置为字节偏移（UTF-8 码点语义记 TODO）。
#pragma once

#include <algorithm>
#include <regex>

#include "Array.hpp"
#include "Object.hpp"
#include "Value.hpp"

namespace cial {

    class Runtime;

    class RegExp final : public Object {
    public:
        std::string pattern{};
        std::regex re{};

        // TJS2 flags：g=全局（exec 推进 start / replace 全替换）、i=忽略大小写、l=忽略
        bool flagGlobal{ false };
        bool flagIgnoreCase{ false };

        // 匹配状态（exec/test 更新；match/replace/split 只读不更新）
        Integer start{ 0 }; // 可写（TJS2 start 属性）：下次搜索起点
        Integer index{ 0 };
        Integer lastIndex{ 0 };
        String input{};
        String lastMatch{};
        String lastParen{};
        String leftContext{};
        String rightContext{};
        Array *matches{}; // 最近一次 exec/test 的结果数组（GC 标记）

        explicit RegExp() = delete;

        // lexer 产物编码 "pattern\nflags"（见 Lexer::regexConstVal）；flags 缺省为空
        explicit RegExp(Runtime *rt, const String &patternStr,
                        const String &flagsStr = String{}) : _rt(rt) {
            compilePattern(std::string(patternStr.getData(), patternStr.length()),
                           std::string(flagsStr.getData(), flagsStr.length()));
        }

        // 重编译（TJS2 compile 方法：替换模式与 flags，不重置匹配状态）
        void compilePattern(const std::string &pat, const std::string &flags) {
            flagGlobal = false;
            flagIgnoreCase = false;
            for(const char f : flags) {
                if(f == 'g')
                    flagGlobal = true;
                else if(f == 'i')
                    flagIgnoreCase = true;
                // 'l' 及未知字母忽略（TJS2 同）
            }
            pattern = pat.empty() ? "(?:)" : pat; // TJS2：空模式按空正则处理
            try {
                re = std::regex(pattern,
                                std::regex::ECMAScript |
                                    (flagIgnoreCase ? std::regex::icase : std::regex::ECMAScript));
            } catch(const std::regex_error &e) {
                throw std::runtime_error("invalid regex /" + pattern + "/: " + e.what());
            }
        }

        // TJS2 tTJSNC_RegExp::Match：start 达到或超过目标长度 → 不匹配
        [[nodiscard]] bool searchFrom(const std::string &target, std::smatch &m) const {
            const auto s = static_cast<size_t>(start);
            if(s >= target.size())
                return false;
            return std::regex_search(target.begin() + static_cast<i64>(s), target.end(), m, re);
        }

        // TJS2 tTJSNC_RegExp::Exec：搜索 + 更新全部状态；result 非空时按
        // GetResultArray 语义填充（[0]=整体匹配、[i]=第 i 组；未匹配=空数组）
        bool execState(const String &targetStr, Array *result) {
            const std::string target(targetStr.getData(), targetStr.length());
            std::smatch m;
            const bool matched = searchFrom(target, m);
            input = targetStr;
            if(!matched) {
                index = start;
                lastIndex = start;
                lastMatch = ""_str;
                lastParen = ""_str;
                leftContext = String(target.c_str(), static_cast<u32>(static_cast<size_t>(start)));
                // TJS2 失败时 RightContext 不更新（保留上次值）
                return false;
            }

            const size_t s = static_cast<size_t>(start);
            index = Integer{ static_cast<i64>(s + static_cast<size_t>(m.position(0))) };
            size_t lastEnd = 0;
            for(size_t i = 0; i < m.size(); ++i) {
                if(m[i].matched)
                    lastEnd = std::max(lastEnd, static_cast<size_t>(m.position(i) + m.length(i)));
            }
            lastIndex = Integer{ static_cast<i64>(s + lastEnd) };

            if(result) {
                const std::string whole = m[0].str();
                result->elements.push_back(Value{ String(whole.c_str(), static_cast<u32>(whole.size())) });
            }
            for(size_t i = 1; i < m.size(); ++i) {
                if(!result)
                    continue;
                const std::string g = m[i].str(); // 未匹配组为空串（TJS2 num_regs 全量填充语义）
                result->elements.push_back(Value{ String(g.c_str(), static_cast<u32>(g.size())) });
            }

            const std::string wholeMatch = m[0].str();
            lastMatch = String(wholeMatch.c_str(), static_cast<u32>(wholeMatch.size()));
            if(m.size() > 1 && m[m.size() - 1].matched) {
                const std::string lp = m[m.size() - 1].str();
                lastParen = String(lp.c_str(), static_cast<u32>(lp.size()));
            } else {
                lastParen = ""_str;
            }
            leftContext = String(target.c_str(), static_cast<u32>(s + static_cast<size_t>(m.position(0))));
            rightContext = String(target.c_str() + static_cast<size_t>(lastIndex),
                                   static_cast<u32>(target.size() - (s + lastEnd)));
            if(flagGlobal)
                start = lastIndex; // g：下次搜索从本次匹配尾起
            return true;
        }

        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::RegExp; }

        void call(vm::VMState &, u32, size_t) override {
            throw std::runtime_error("Not implemented RegExp call");
        }

        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            if(matches)
                matches->marked();
        }

        void setProp(Atom a, const Value &v) override;

        Value getProp(Atom a) override;

        [[nodiscard]] bool hasProp(Atom a) const override;

        [[nodiscard]] Runtime *runtime() const noexcept { return _rt; }

    private:
        Runtime *_rt{};
    };

} // namespace cial
