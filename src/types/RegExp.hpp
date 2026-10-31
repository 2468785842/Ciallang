// Copyright (c) 2026/10/24
//
// RegExp 内建对象（TJS2 RegExp）：/pattern/flags 字面量的运行时载体。
// 引擎用 std::regex（ECMAScript 方言；TJS2 原版为 oniguruma，复杂模式差异记 TODO）。
// 匹配状态（start/index/lastIndex/input/lastMatch/lastParen/leftContext/rightContext/
// matches）对齐 tjsRegExp.cpp 的 tTJSNI_RegExp；位置为 UTF-8 码点偏移
// （TJS2 为 UTF-16 码元，本项目字符串统一码点语义，与 str.length 一致）。
#pragma once

#include <algorithm>
#include <regex>

#include "Array.hpp"
#include "Object.hpp"
#include "Value.hpp"
#include "runtime/Runtime.hpp"

namespace cial {

    class Runtime;

    // UTF-8 字节偏移 ↔ 码点偏移换算（匹配位置码点语义的底座）
    namespace regexp_cp {
        // 统计 [0, bytePos) 字节区间内的码点数（前导字节计数）
        inline size_t cpCountTo(const std::string &s, const size_t bytePos) {
            size_t n = 0;
            for(size_t i = 0; i < bytePos; ++i)
                if((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
                    ++n;
            return n;
        }

        // 第 cp 个码点的字节偏移；cp 超出码点数时钳位到串长
        inline size_t cpToByte(const std::string &s, size_t cp) {
            size_t i = 0;
            while(i < s.size() && cp > 0) {
                if((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
                    --cp;
                ++i;
            }
            return i;
        }
    } // namespace regexp_cp

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
        explicit RegExp(Runtime *rt, const String &patternStr, const String &flagsStr = String{}) : _rt(rt) {
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
                                std::regex::ECMAScript | (flagIgnoreCase ? std::regex::icase : std::regex::ECMAScript));
            } catch(const std::regex_error &e) {
                throw std::runtime_error("invalid regex /" + pattern + "/: " + e.what());
            }
        }

        // TJS2 tTJSNC_RegExp::Match：start（码点）达到或超过目标码点长度 → 不匹配。
        // start 为码点偏移，先换算为字节偏移再交给 std::regex
        [[nodiscard]] bool searchFrom(const std::string &target, std::smatch &m) const {
            const size_t byteStart = regexp_cp::cpToByte(target, static_cast<size_t>(start));
            if(byteStart >= target.size())
                return false;
            return std::regex_search(target.begin() + static_cast<i64>(byteStart), target.end(), m, re);
        }

        // TJS2 tTJSNC_RegExp::Exec：搜索 + 更新全部状态；result 非空时按
        // GetResultArray 语义填充（[0]=整体匹配、[i]=第 i 组；未匹配=空数组）。
        // index/lastIndex 存码点偏移；TJS2 失败时 LeftContext = 前 start 个字符
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
                leftContext =
                    String(target.c_str(), static_cast<u32>(regexp_cp::cpToByte(target, static_cast<size_t>(start))));
                // TJS2 失败时 RightContext 不更新（保留上次值）
                return false;
            }

            const size_t s = regexp_cp::cpToByte(target, static_cast<size_t>(start));
            index = Integer{ static_cast<i64>(regexp_cp::cpCountTo(target, s + static_cast<size_t>(m.position(0)))) };
            size_t lastEnd = 0;
            for(size_t i = 0; i < m.size(); ++i) {
                if(m[i].matched)
                    lastEnd = std::max(lastEnd, static_cast<size_t>(m.position(i) + m.length(i)));
            }
            lastIndex = Integer{ static_cast<i64>(regexp_cp::cpCountTo(target, s + lastEnd)) };

            if(result) {
                const std::string whole = m[0].str();
                result->elements.emplace_back(String(whole.c_str(), static_cast<u32>(whole.size())));
            }
            for(size_t i = 1; i < m.size(); ++i) {
                if(!result)
                    continue;
                const std::string g = m[i].str(); // 未匹配组为空串（TJS2 num_regs 全量填充语义）
                result->elements.emplace_back(String(g.c_str(), static_cast<u32>(g.size())));
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
            rightContext = String(target.c_str() + s + lastEnd, static_cast<u32>(target.size() - (s + lastEnd)));
            if(flagGlobal)
                start = lastIndex; // g：下次搜索从本次匹配尾起（码点）
            return true;
        }

        static constexpr TypeId nativeTypeId = TypeId::RegExp;
        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::RegExp; }

        // instanceof "RegExp"（TJS2 tTJSNC_RegExp::IsInstanceOf 同语义）
        bool instanceOf(const Atom a) override { return a == Runtime::ATOM_REGEXP; }

        // 类对象构造入口：RegExp(pattern, flags?) / new RegExp(...)（TJS2 无 new 亦可）。
        // 全局 "RegExp" 挂一个空模式占位实例，call 时按实参构造新对象
        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;

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
