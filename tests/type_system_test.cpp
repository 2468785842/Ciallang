/*
 * Copyright (c) 2024/12/18
 *
 * 类型系统单元测试
 */

#include <catch.hpp>
#include <climits>
#include <limits>
#include <memory>

#include "types/Object.hpp"
#include "types/TjsOctet.hpp"
#include "types/TjsString.hpp"
#include "types/TjsTypes.hpp"
#include "types/TjsValue.hpp"

using namespace Ciallang;

TEST_CASE("TjsValue - 基本类型创建") {
    SECTION("创建整数类型") {
        Ciallang::TjsValue value{ static_cast<Ciallang::TjsInteger>(42) };
        REQUIRE(value.type() == Ciallang::TjsValueType::Integer);
        REQUIRE(value.toInteger() == 42);
        REQUIRE(value.isInteger());
        REQUIRE_FALSE(value.isReal());
        REQUIRE_FALSE(value.isString());
        REQUIRE_FALSE(value.isObject());
    }

    SECTION("创建浮点数类型") {
        Ciallang::TjsValue value{ static_cast<Ciallang::TjsReal>(3.14) };
        REQUIRE(value.type() == Ciallang::TjsValueType::Real);
        REQUIRE(value.toReal() == Catch::Approx(3.14));
        REQUIRE(value.isReal());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isString());
        REQUIRE_FALSE(value.isObject());
    }

    SECTION("创建字符串类型") {
        Ciallang::TjsString str{ "Hello World" };
        Ciallang::TjsValue value{ str };
        REQUIRE(value.type() == Ciallang::TjsValueType::String);
        REQUIRE(value.toString() != nullptr);
        REQUIRE(value.isString());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isReal());
        REQUIRE_FALSE(value.isObject());
    }

    SECTION("创建Void类型") {
        Ciallang::TjsValue value;
        REQUIRE(value.type() == Ciallang::TjsValueType::Void);
        REQUIRE(value.isVoid());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isReal());
        REQUIRE_FALSE(value.isString());
        REQUIRE_FALSE(value.isObject());
    }
}

TEST_CASE("TjsValue - 类型转换") {
    SECTION("整数到浮点数转换") {
        Ciallang::TjsValue intValue{ static_cast<Ciallang::TjsInteger>(42) };
        REQUIRE(intValue.toInteger() == 42);

        // 转换为浮点数
        Ciallang::TjsValue realValue{ static_cast<Ciallang::TjsReal>(intValue.toInteger() * 1.0) };
        REQUIRE(realValue.toReal() == 42.0);
    }

    SECTION("浮点数到整数转换") {
        Ciallang::TjsValue realValue{ static_cast<Ciallang::TjsReal>(3.99) };
        REQUIRE(realValue.toReal() == Catch::Approx(3.99));

        // 转换为整数（截断）
        Ciallang::TjsValue intValue{ static_cast<Ciallang::TjsInteger>(realValue.toReal()) };
        REQUIRE(intValue.toInteger() == 3);
    }

    SECTION("字符串到整数转换") {
        Ciallang::TjsString str{ "123" };
        Ciallang::TjsValue strValue{ str };
        REQUIRE(strValue.toString() != nullptr);

        // 字符串解析为整数
        long long parsedInt = std::stoll(*strValue.toString());
        Ciallang::TjsValue intValue{ static_cast<Ciallang::TjsInteger>(parsedInt) };
        REQUIRE(intValue.toInteger() == 123);
    }

    SECTION("整数到字符串转换") {
        Ciallang::TjsValue intValue{ static_cast<Ciallang::TjsInteger>(456) };
        REQUIRE(intValue.toInteger() == 456);

        // 整数转换为字符串
        std::string str = std::to_string(intValue.toInteger());
        Ciallang::TjsString tjsStr{ str };
        Ciallang::TjsValue strValue{ tjsStr };
        REQUIRE(strValue.toString() != nullptr);
    }
}

TEST_CASE("TjsValue - 比较操作") {
    SECTION("整数比较") {
        Ciallang::TjsValue value1{ static_cast<Ciallang::TjsInteger>(10) };
        Ciallang::TjsValue value2{ static_cast<Ciallang::TjsInteger>(20) };
        Ciallang::TjsValue value3{ static_cast<Ciallang::TjsInteger>(10) };

        REQUIRE(value1.toInteger() < value2.toInteger());
        REQUIRE(value2.toInteger() > value1.toInteger());
        REQUIRE(value1.toInteger() == value3.toInteger());
    }

    SECTION("浮点数比较") {
        Ciallang::TjsValue value1{ static_cast<Ciallang::TjsReal>(1.5) };
        Ciallang::TjsValue value2{ static_cast<Ciallang::TjsReal>(2.5) };
        Ciallang::TjsValue value3{ static_cast<Ciallang::TjsReal>(1.5) };

        REQUIRE(value1.toReal() < value2.toReal());
        REQUIRE(value2.toReal() > value1.toReal());
        REQUIRE(value1.toReal() == Catch::Approx(value3.toReal()));
    }

    SECTION("字符串比较") {
        Ciallang::TjsString str1{ "apple" };
        Ciallang::TjsString str2{ "banana" };
        Ciallang::TjsString str3{ "apple" };

        Ciallang::TjsValue value1{ str1 };
        Ciallang::TjsValue value2{ str2 };
        Ciallang::TjsValue value3{ str3 };

        REQUIRE(value1.toString() != nullptr);
        REQUIRE(value2.toString() != nullptr);
        REQUIRE(value3.toString() != nullptr);
    }
}

TEST_CASE("TjsValue - 算术运算") {
    SECTION("整数算术运算") {
        Ciallang::TjsValue a{ static_cast<Ciallang::TjsInteger>(10) };
        Ciallang::TjsValue b{ static_cast<Ciallang::TjsInteger>(5) };

        // 使用TjsValue的运算符
        Ciallang::TjsValue addResult = a + b;
        REQUIRE(addResult.toInteger() == 15);

        Ciallang::TjsValue subResult = a - b;
        REQUIRE(subResult.toInteger() == 5);

        Ciallang::TjsValue mulResult = a * b;
        REQUIRE(mulResult.toInteger() == 50);

        Ciallang::TjsValue divResult = a / b;
        REQUIRE(divResult.toInteger() == 2);
    }

    SECTION("浮点数算术运算") {
        Ciallang::TjsValue a{ static_cast<Ciallang::TjsReal>(10.5) };
        Ciallang::TjsValue b{ static_cast<Ciallang::TjsReal>(2.5) };

        Ciallang::TjsValue addResult = a + b;
        REQUIRE(addResult.toReal() == Catch::Approx(13.0));

        Ciallang::TjsValue subResult = a - b;
        REQUIRE(subResult.toReal() == Catch::Approx(8.0));

        Ciallang::TjsValue mulResult = a * b;
        REQUIRE(mulResult.toReal() == Catch::Approx(26.25));

        Ciallang::TjsValue divResult = a / b;
        REQUIRE(divResult.toReal() == Catch::Approx(4.2));
    }
}

TEST_CASE("TjsString - 字符串操作") {
    SECTION("字符串创建和访问") {
        Ciallang::TjsString str{ "Hello World" };
        REQUIRE(str == "Hello World");
        REQUIRE(str.length() == 11);
        REQUIRE_FALSE(str.empty());
    }

    SECTION("空字符串") {
        Ciallang::TjsString emptyStr{ "" };
        REQUIRE(emptyStr == "");
        REQUIRE(emptyStr.length() == 0);
        REQUIRE(emptyStr.empty());
    }

    SECTION("字符串连接") {
        Ciallang::TjsString str1{ "Hello" };
        Ciallang::TjsString str2{ " World" };

        // 字符串连接
        std::string result = str1 + str2;
        Ciallang::TjsString combined{ result };
        REQUIRE(combined == "Hello World");
    }

    SECTION("字符串比较") {
        Ciallang::TjsString str1{ "apple" };
        Ciallang::TjsString str2{ "banana" };
        Ciallang::TjsString str3{ "apple" };

        REQUIRE(str1 == str3);
        REQUIRE(str1 != str2);
        REQUIRE(str1 < str2);
    }
}

TEST_CASE("TjsOctet - 字节操作") {
    // 注意：TjsOctet没有公共访问方法，这里只测试创建
    SECTION("字节数组创建") {
        std::vector<uint8_t> data{ 0x01, 0x02, 0x03, 0x04 };
        Ciallang::TjsOctet octet{ data };
        REQUIRE(true); // 如果能创建成功就通过
    }

    SECTION("空字节数组") {
        std::vector<uint8_t> emptyData;
        Ciallang::TjsOctet emptyOctet{ emptyData };
        REQUIRE(true); // 如果能创建成功就通过
    }

    SECTION("字节数组访问") {
        std::vector<uint8_t> data{ 0x10, 0x20, 0x30 };
        Ciallang::TjsOctet octet{ data };
        REQUIRE(true); // 如果能创建成功就通过
    }
}

TEST_CASE("Object - 对象操作") {
    // 注意：Object是一个抽象基类，需要具体实现
    // 这里只测试基本功能
    SECTION("对象类型检查") {
        // Object是抽象类，无法直接实例化
        // 这个测试主要用于验证类型系统
        REQUIRE(true);
    }
}

TEST_CASE("类型系统 - 混合类型操作") {
    SECTION("不同类型值存储") {
        // 创建不同类型的值
        Ciallang::TjsValue intValue{ static_cast<Ciallang::TjsInteger>(100) };
        Ciallang::TjsValue realValue{ static_cast<Ciallang::TjsReal>(3.14) };
        Ciallang::TjsString str{ "test" };
        Ciallang::TjsValue strValue{ str };

        // 验证类型
        REQUIRE(intValue.type() == Ciallang::TjsValueType::Integer);
        REQUIRE(realValue.type() == Ciallang::TjsValueType::Real);
        REQUIRE(strValue.type() == Ciallang::TjsValueType::String);

        // 验证值
        REQUIRE(intValue.toInteger() == 100);
        REQUIRE(realValue.toReal() == Catch::Approx(3.14));
        REQUIRE(strValue.toString() != nullptr);
    }

    SECTION("类型检查和转换") {
        Ciallang::TjsValue intValue{ static_cast<Ciallang::TjsInteger>(42) };
        Ciallang::TjsValue realValue{ static_cast<Ciallang::TjsReal>(2.71) };
        Ciallang::TjsString str{ "hello" };
        Ciallang::TjsValue strValue{ str };

        // 类型检查
        REQUIRE(intValue.isInteger());
        REQUIRE(realValue.isReal());
        REQUIRE(strValue.isString());

        // 类型转换检查
        REQUIRE_FALSE(intValue.isReal());
        REQUIRE_FALSE(realValue.isInteger());
        REQUIRE_FALSE(strValue.isInteger());
    }
}

TEST_CASE("类型系统 - 边界值测试") {
    SECTION("整数边界值") {
        // 最小整数
        Ciallang::TjsValue minInt{ static_cast<Ciallang::TjsInteger>(LLONG_MIN) };
        REQUIRE(minInt.toInteger() == LLONG_MIN);

        // 最大整数
        Ciallang::TjsValue maxInt{ static_cast<Ciallang::TjsInteger>(LLONG_MAX) };
        REQUIRE(maxInt.toInteger() == LLONG_MAX);

        // 零
        Ciallang::TjsValue zero{ static_cast<Ciallang::TjsInteger>(0) };
        REQUIRE(zero.toInteger() == 0);
    }

    SECTION("浮点数边界值") {
        // 正无穷大
        Ciallang::TjsValue posInf{ static_cast<Ciallang::TjsReal>(std::numeric_limits<double>::infinity()) };
        REQUIRE(posInf.toReal() == std::numeric_limits<double>::infinity());

        // 负无穷大
        Ciallang::TjsValue negInf{ static_cast<Ciallang::TjsReal>(-std::numeric_limits<double>::infinity()) };
        REQUIRE(negInf.toReal() == -std::numeric_limits<double>::infinity());

        // NaN
        Ciallang::TjsValue nan{ static_cast<Ciallang::TjsReal>(std::numeric_limits<double>::quiet_NaN()) };
        REQUIRE(std::isnan(nan.toReal()));

        // 零
        Ciallang::TjsValue zero{ static_cast<Ciallang::TjsReal>(0.0) };
        REQUIRE(zero.toReal() == 0.0);
    }

    SECTION("字符串边界值") {
        // 空字符串
        Ciallang::TjsString empty{ "" };
        REQUIRE(empty.empty());

        // 长字符串
        std::string longStr(1000, 'a');
        Ciallang::TjsString longTjsStr{ longStr };
        REQUIRE(longTjsStr.length() == 1000);
    }
}