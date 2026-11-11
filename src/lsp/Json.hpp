// Copyright (c) 2026/11/11
//
// LSP 进程内 JSON-RPC 所需的最小 JSON 值模型：解析、序列化与便捷取值。
// 仅覆盖 LSP 消息所需子集（对象/数组/字符串/数字/布尔/null），字符串保持
// UTF-8 原样（转义按 RFC 8259 解码，含代理对）；数字以 double 承载，
// 整数值序列化时不带小数点。不依赖第三方库。
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace cial::lsp {
    class Json {
    public:
        enum class Type { Null, Bool, Number, String, Array, Object };

        Json() = default;
        Json(std::nullptr_t) {}
        Json(bool value) : _type(Type::Bool), _bool(value) {}
        Json(int value) : _type(Type::Number), _number(value) {}
        Json(size_t value) : _type(Type::Number), _number(static_cast<double>(value)) {}
        Json(double value) : _type(Type::Number), _number(value) {}
        Json(std::string value) : _type(Type::String), _string(std::move(value)) {}
        Json(std::string_view value) : _type(Type::String), _string(value) {}
        Json(const char *value) : _type(Type::String), _string(value) {}

        [[nodiscard]] Type type() const noexcept { return _type; }
        [[nodiscard]] bool isNull() const noexcept { return _type == Type::Null; }
        [[nodiscard]] bool isBool() const noexcept { return _type == Type::Bool; }
        [[nodiscard]] bool isNumber() const noexcept { return _type == Type::Number; }
        [[nodiscard]] bool isString() const noexcept { return _type == Type::String; }
        [[nodiscard]] bool isArray() const noexcept { return _type == Type::Array; }
        [[nodiscard]] bool isObject() const noexcept { return _type == Type::Object; }

        [[nodiscard]] bool asBool(bool fallback = false) const { return _type == Type::Bool ? _bool : fallback; }
        [[nodiscard]] double asNumber(double fallback = 0) const { return _type == Type::Number ? _number : fallback; }
        // LSP 的 id/version 等整数字段；非整数或越界返回 fallback
        [[nodiscard]] int asInt(int fallback = -1) const {
            if(_type != Type::Number || _number < -2147483648.0 || _number > 2147483647.0)
                return fallback;
            return static_cast<int>(_number);
        }
        [[nodiscard]] const std::string &asString() const noexcept { return _string; }
        [[nodiscard]] const std::vector<Json> &asArray() const noexcept { return _array; }
        [[nodiscard]] const std::map<std::string, Json> &asObject() const noexcept { return _object; }

        [[nodiscard]] const Json *find(std::string_view key) const {
            if(_type != Type::Object)
                return nullptr;
            const auto it = _object.find(std::string{ key });
            return it == _object.end() ? nullptr : &it->second;
        }
        // 成员便捷读取：缺失或类型不符返回 fallback
        [[nodiscard]] std::string stringAt(std::string_view key, std::string fallback = {}) const {
            const auto *value = find(key);
            return value != nullptr && value->isString() ? value->_string : std::move(fallback);
        }
        [[nodiscard]] int intAt(std::string_view key, int fallback = -1) const {
            const auto *value = find(key);
            return value != nullptr ? value->asInt(fallback) : fallback;
        }
        [[nodiscard]] bool boolAt(std::string_view key, bool fallback = false) const {
            const auto *value = find(key);
            return value != nullptr ? value->asBool(fallback) : fallback;
        }

        void set(std::string key, Json value) { _object[std::move(key)] = std::move(value); }
        void push(Json value) { _array.push_back(std::move(value)); }

        static Json array() { return Json{ Type::Array }; }
        static Json object() { return Json{ Type::Object }; }

        // 解析非法输入返回 Null 并可选带错误信息
        static Json parse(std::string_view text, std::string *error = nullptr);
        std::string dump() const;

    private:
        explicit Json(Type type) : _type(type) {}

        Type _type{ Type::Null };
        bool _bool{};
        double _number{};
        std::string _string;
        std::vector<Json> _array;
        std::map<std::string, Json> _object;
    };
} // namespace cial::lsp
