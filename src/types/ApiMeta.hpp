// Copyright (c) 2026/11/10
//
#pragma once

#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "NativeFunDetail.hpp"
#include "common/ApiParam.hpp"

namespace cial {
    // 注册期 API 元数据（doc/api-metadata.md）：描述宿主/标准库注册的符号及注册点
    // 可确定的签名。类型推导不出的字段保持空串，导出时输出 null，不编造类型；
    // 数据为纯 C++ 字符串所有权，不涉及 GC。
    enum class ApiKind { Method, Property, Static, Global, Namespace, Class, Constructor };

    struct ApiMeta {
        ApiKind kind{ ApiKind::Method };
        std::string name;
        // 全局符号的点分归属路径（"" = 顶层）；类成员的归属由 catalog 的
        // typeMembers/typeNames 表达，不在此重复。
        std::string owner;
        std::vector<std::string> paramTypes; // 空串元素 = 类型未知
        bool variadic{}; // Value* 尾参形态或注册点显式声明
        std::string ret; // 空串 = 未知
        std::string doc;
        // nullopt = 旧签名推导；显式空数组 = 确定无参数（可覆盖 Value* 回调）。
        std::optional<std::vector<ApiParam>> params;
        bool accessSpecified{};
        bool hasGetter{};
        bool hasSetter{};
        std::vector<std::string> extends;
    };

    namespace api {
        inline void normalize(ApiMeta &meta) {
            if(!meta.params)
                return;
            meta.paramTypes.clear();
            meta.variadic = false;
            for(const auto &param : *meta.params) {
                meta.paramTypes.push_back(param.type);
                meta.variadic |= param.collapse;
            }
        }

        // 只补充描述，不允许注解改写实际登记的名字、归属或成员类别。
        inline ApiMeta annotate(ApiMeta inferred, const ApiMeta &description) {
            if(!description.ret.empty())
                inferred.ret = description.ret;
            if(!description.doc.empty())
                inferred.doc = description.doc;
            if(description.params) {
                inferred.params = description.params;
                normalize(inferred);
            }
            if(description.accessSpecified) {
                inferred.accessSpecified = true;
                inferred.hasGetter = description.hasGetter;
                inferred.hasSetter = description.hasSetter;
            }
            if(!description.extends.empty())
                inferred.extends = description.extends;
            return inferred;
        }

        inline ApiMeta signature(std::vector<ApiParam> params, std::string ret = {}, std::string doc = {}) {
            ApiMeta meta;
            meta.params = std::move(params);
            meta.ret = std::move(ret);
            meta.doc = std::move(doc);
            normalize(meta);
            return meta;
        }
        // 注册点可确定类型的名称推导；宿主 payload 类第一版无名字映射（空串）。
        // 参数名与默认值由注册点显式提供；doc 只保存说明，不从注释猜测签名。
        template <typename T>
        consteval const char *typeName() {
            using U = std::remove_cv_t<std::remove_reference_t<T>>;
            if constexpr(std::is_void_v<U>)
                return "void";
            else if constexpr(std::is_same_v<U, Value>)
                return "any";
            else if constexpr(std::is_same_v<U, bool>)
                return "Boolean";
            else if constexpr(std::is_integral_v<U>)
                return "Integer";
            else if constexpr(std::is_floating_point_v<U> || std::is_same_v<U, Real>)
                return "Real";
            else if constexpr(std::is_same_v<U, String> || std::is_same_v<U, std::string>)
                return "String";
            else if constexpr(std::is_same_v<U, Octet>)
                return "Octet";
            else if constexpr(std::is_pointer_v<U>) {
                using P = std::remove_cv_t<std::remove_pointer_t<U>>;
                if constexpr(std::is_same_v<P, String>)
                    return "String";
                else if constexpr(std::is_same_v<P, Octet>)
                    return "Octet";
                else if constexpr(std::is_same_v<P, Object>)
                    return "Object";
                else
                    return "";
            } else {
                return "";
            }
        }

        template <typename Tuple, size_t... I>
        std::vector<std::string> tupleTypeNames(std::index_sequence<I...>) {
            return { typeName<std::tuple_element_t<I, Tuple>>()... };
        }

        // 依 NativeFunTraits 推导签名；不可推导的 callable（泛型 lambda、类型擦除的
        // NativeFunction 等）回退为仅名字与归属的条目。Value* 首参（(VM*, Value,
        // size_t, Value*) 形态）是 stdlib 的变长方法惯例，登记为 variadic。
        template <typename F>
        ApiMeta makeMeta(ApiKind kind, std::string name, std::string owner = {}, std::string doc = {}) {
            ApiMeta meta{ .kind = kind, .name = std::move(name), .owner = std::move(owner), .doc = std::move(doc) };
            using Decay = std::decay_t<F>;
            if constexpr(requires { typename native::NativeFunTraits<Decay>::ArgsTuple; }) {
                using Traits = native::NativeFunTraits<Decay>;
                meta.ret = typeName<typename Traits::ReturnType>();
                if constexpr(Traits::arity > 0) {
                    if constexpr(native::is_value_ptr_v<typename Traits::template Arg<0>, Value>) {
                        meta.variadic = true;
                    } else {
                        meta.paramTypes =
                            tupleTypeNames<typename Traits::ArgsTuple>(std::make_index_sequence<Traits::arity>{});
                    }
                }
            }
            return meta;
        }

        // callable 值版：F 由实参推导后转发 makeMeta（注册点直接引用函数/lambda 时使用）。
        template <typename F>
        ApiMeta makeMetaOf(ApiKind kind, F &&, std::string name, std::string owner = {}, std::string doc = {}) {
            return makeMeta<std::decay_t<F>>(kind, std::move(name), std::move(owner), std::move(doc));
        }
    } // namespace api
} // namespace cial
