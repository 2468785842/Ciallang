// RegExp 使用 TJS2 的 Oniguruma Perl 方言，位置按 UTF-16 码元计数。
#pragma once

#include <memory>
#include <string_view>

#include "Array.hpp"
#include "Object.hpp"
#include "Value.hpp"
#include "common/UTF16.hpp"
#include "runtime/Runtime.hpp"

namespace cial {
    struct RegexMatch {
        struct Group {
            bool matched{};
            size_t begin{}, end{};
            std::u16string_view text;
            [[nodiscard]] std::string str() const { return Common::fromUtf16(text); }
        };
        Vec<Group> groups;
        [[nodiscard]] size_t size() const { return groups.size(); }
        [[nodiscard]] const Group &operator[](size_t index) const { return groups.at(index); }
        [[nodiscard]] size_t position(size_t index) const { return groups.at(index).begin; }
        [[nodiscard]] size_t length(size_t index) const { return groups.at(index).end - groups.at(index).begin; }
    };

    class RegExp final : public Object {
    public:
        struct CompiledPattern;
        using Pattern = std::shared_ptr<const CompiledPattern>;
        std::string pattern;
        bool flagGlobal{}, flagIgnoreCase{};
        Integer start{}, index{}, lastIndex{};
        String input, lastMatch, lastParen, leftContext, rightContext;
        Array *matches{};

        explicit RegExp(Runtime *runtime, const String &expression, const String &flags = String{});
        void compilePattern(const std::string &expression, const std::string &flags);
        [[nodiscard]] Pattern patternSnapshot() const { return _compiled; }
        [[nodiscard]] bool search(std::u16string_view target, size_t offset, RegexMatch &match,
                                  const Pattern &snapshot = {}) const;
        [[nodiscard]] bool searchFrom(std::u16string_view target, RegexMatch &match) const;
        bool execState(const String &target, Array *result);

        static constexpr TypeId nativeTypeId = TypeId::RegExp;
        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::RegExp; }
        bool instanceOf(const Atom name) override { return name == Runtime::ATOM_REGEXP; }
        void call(vm::VMState &state, u32 result, size_t argumentCount) override;
        void setProp(Atom name, const Value &value) override;
        Value getProp(Atom name) override;
        [[nodiscard]] bool hasProp(Atom name) const override;
        [[nodiscard]] Runtime *runtime() const noexcept { return _rt; }
        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            if(matches)
                matches->marked();
        }

    private:
        Runtime *_rt{};
        Pattern _compiled;
    };
} // namespace cial
