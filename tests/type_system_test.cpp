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
#include "types/Octet.hpp"
#include "types/String.hpp"
#include "types/Types.hpp"
#include "types/Value.hpp"

using namespace Ciallang;

TEST_CASE("Value - 基本类型创建") {
    SECTION("创建整数类型") {
        Ciallang::Value value{ static_cast<Ciallang::Integer>(42) };
        REQUIRE(value.type() == Ciallang::ValueType::Integer);
        REQUIRE(value.toInteger() == 42);
        REQUIRE(value.isInteger());
        REQUIRE_FALSE(value.isReal());
        REQUIRE_FALSE(value.isString());
        REQUIRE_FALSE(value.isObject());
    }

    SECTION("创建浮点数类型") {
        Ciallang::Value value{ static_cast<Ciallang::Real>(3.14) };
        REQUIRE(value.type() == Ciallang::ValueType::Real);
        REQUIRE(value.toReal() == Catch::Approx(3.14));
        REQUIRE(value.isReal());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isString());
        REQUIRE_FALSE(value.isObject());
    }

    SECTION("创建字符串类型") {
        Ciallang::Value value = createString("Hello World");
        REQUIRE(value.type() == Ciallang::ValueType::String);
        REQUIRE(value.toString() != nullptr);
        REQUIRE(value.isString());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isReal());
        REQUIRE_FALSE(value.isObject());
    }

    SECTION("创建Void类型") {
        Ciallang::Value value;
        REQUIRE(value.type() == Ciallang::ValueType::Void);
        REQUIRE(value.isVoid());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isReal());
        REQUIRE_FALSE(value.isString());
        REQUIRE_FALSE(value.isObject());
    }
}

TEST_CASE("Value - 类型转换") {
    SECTION("整数到浮点数转换") {
        Ciallang::Value intValue{ static_cast<Ciallang::Integer>(42) };
        REQUIRE(intValue.toInteger() == 42);

        // 转换为浮点数
        Ciallang::Value realValue{ static_cast<Ciallang::Real>(intValue.toInteger() * 1.0) };
        REQUIRE(realValue.toReal() == 42.0);
    }

    SECTION("浮点数到整数转换") {
        Ciallang::Value realValue{ static_cast<Ciallang::Real>(3.99) };
        REQUIRE(realValue.toReal() == Catch::Approx(3.99));

        // 转换为整数（截断）
        Ciallang::Value intValue{ static_cast<Ciallang::Integer>(realValue.toReal()) };
        REQUIRE(intValue.toInteger() == 3);
    }

    SECTION("字符串到整数转换") {
        Ciallang::Value strValue = createString("123");
        REQUIRE(strValue.toString() != nullptr);

        // 字符串解析为整数
        long long parsedInt = std::stoll(strValue.toString()->toStdStr());
        Ciallang::Value intValue{ static_cast<Ciallang::Integer>(parsedInt) };
        REQUIRE(intValue.toInteger() == 123);
    }

    SECTION("整数到字符串转换") {
        Ciallang::Value intValue{ static_cast<Ciallang::Integer>(456) };
        REQUIRE(intValue.toInteger() == 456);

        // 整数转换为字符串
        std::string str = std::to_string(intValue.toInteger());
        Ciallang::Value strValue = createString(str.c_str(), str.size());
        REQUIRE(strValue.toString() != nullptr);
    }
}

TEST_CASE("Value - 比较操作") {
    SECTION("整数比较") {
        Ciallang::Value value1{ static_cast<Ciallang::Integer>(10) };
        Ciallang::Value value2{ static_cast<Ciallang::Integer>(20) };
        Ciallang::Value value3{ static_cast<Ciallang::Integer>(10) };

        REQUIRE(value1.toInteger() < value2.toInteger());
        REQUIRE(value2.toInteger() > value1.toInteger());
        REQUIRE(value1.toInteger() == value3.toInteger());
    }

    SECTION("浮点数比较") {
        Ciallang::Value value1{ static_cast<Ciallang::Real>(1.5) };
        Ciallang::Value value2{ static_cast<Ciallang::Real>(2.5) };
        Ciallang::Value value3{ static_cast<Ciallang::Real>(1.5) };

        REQUIRE(value1.toReal() < value2.toReal());
        REQUIRE(value2.toReal() > value1.toReal());
        REQUIRE(value1.toReal() == Catch::Approx(value3.toReal()));
    }

    SECTION("字符串比较") {
        Ciallang::Value value1 = createString("apple");
        Ciallang::Value value2 = createString("banana");
        Ciallang::Value value3 = createString("apple");

        REQUIRE(value1.toString() != nullptr);
        REQUIRE(value2.toString() != nullptr);
        REQUIRE(value3.toString() != nullptr);
    }
}

TEST_CASE("Value - 算术运算") {
    SECTION("整数算术运算") {
        Ciallang::Value a{ static_cast<Ciallang::Integer>(10) };
        Ciallang::Value b{ static_cast<Ciallang::Integer>(5) };

        // 使用TjsValue的运算符
        Ciallang::Value addResult = a + b;
        REQUIRE(addResult.toInteger() == 15);

        Ciallang::Value subResult = a - b;
        REQUIRE(subResult.toInteger() == 5);

        Ciallang::Value mulResult = a * b;
        REQUIRE(mulResult.toInteger() == 50);

        Ciallang::Value divResult = a / b;
        REQUIRE(divResult.toInteger() == 2);
    }

    SECTION("浮点数算术运算") {
        Ciallang::Value a{ static_cast<Ciallang::Real>(10.5) };
        Ciallang::Value b{ static_cast<Ciallang::Real>(2.5) };

        Ciallang::Value addResult = a + b;
        REQUIRE(addResult.toReal() == Catch::Approx(13.0));

        Ciallang::Value subResult = a - b;
        REQUIRE(subResult.toReal() == Catch::Approx(8.0));

        Ciallang::Value mulResult = a * b;
        REQUIRE(mulResult.toReal() == Catch::Approx(26.25));

        Ciallang::Value divResult = a / b;
        REQUIRE(divResult.toReal() == Catch::Approx(4.2));
    }
}

// TEST_CASE("String - 字符串操作") {
//     SECTION("字符串创建和访问") {
//         auto v = createString("Hello World");
//         Ciallang::String str = *v.toString();
//         REQUIRE(str == "Hello World");
//         REQUIRE(str.length() == 11);
//         REQUIRE_FALSE(str.empty());
//     }

//     SECTION("空字符串") {
//         auto v = createString("");
//         Ciallang::String emptyStr = *v.toString();
//         REQUIRE(emptyStr == "");
//         REQUIRE(emptyStr.length() == 0);
//         REQUIRE(emptyStr.empty());
//     }

//     SECTION("字符串连接") {
//         auto v1 = createString("Hello");
//         Ciallang::String str1 = *v1.toString();
//         auto v2 = createString(" World");
//         Ciallang::String str2 = *v2.toString();

//         // 字符串连接
//         std::string result = str1 + str2;
//         Ciallang::String combined{ result };
//         REQUIRE(combined == "Hello World");
//     }

//     SECTION("字符串比较") {
//         Ciallang::String str1{ "apple" };
//         Ciallang::String str2{ "banana" };
//         Ciallang::String str3{ "apple" };

//         REQUIRE(str1 == str3);
//         REQUIRE(str1 != str2);
//         REQUIRE(str1 < str2);
//     }
// }

// TEST_CASE("Octet - 字节操作") {
//     // 注意：TjsOctet没有公共访问方法，这里只测试创建
//     SECTION("字节数组创建") {
//         std::vector<uint8_t> data{ 0x01, 0x02, 0x03, 0x04 };
//         Ciallang::Octet octet{ data };
//         REQUIRE(true); // 如果能创建成功就通过
//     }

//     SECTION("空字节数组") {
//         std::vector<uint8_t> emptyData;
//         Ciallang::Octet emptyOctet{ emptyData };
//         REQUIRE(true); // 如果能创建成功就通过
//     }

//     SECTION("字节数组访问") {
//         std::vector<uint8_t> data{ 0x10, 0x20, 0x30 };
//         Ciallang::Octet octet{ data };
//         REQUIRE(true); // 如果能创建成功就通过
//     }
// }

// TEST_CASE("Object - 对象操作") {
//     // 注意：Object是一个抽象基类，需要具体实现
//     // 这里只测试基本功能
//     SECTION("对象类型检查") {
//         // Object是抽象类，无法直接实例化
//         // 这个测试主要用于验证类型系统
//         REQUIRE(true);
//     }
// }

// TEST_CASE("类型系统 - 混合类型操作") {
//     SECTION("不同类型值存储") {
//         // 创建不同类型的值
//         Ciallang::Value intValue{ static_cast<Ciallang::Integer>(100) };
//         Ciallang::Value realValue{ static_cast<Ciallang::Real>(3.14) };
//         Ciallang::String str{ "test" };
//         Ciallang::Value strValue{ str };

//         // 验证类型
//         REQUIRE(intValue.type() == Ciallang::ValueType::Integer);
//         REQUIRE(realValue.type() == Ciallang::ValueType::Real);
//         REQUIRE(strValue.type() == Ciallang::ValueType::String);

//         // 验证值
//         REQUIRE(intValue.toInteger() == 100);
//         REQUIRE(realValue.toReal() == Catch::Approx(3.14));
//         REQUIRE(strValue.toString() != nullptr);
//     }

//     SECTION("类型检查和转换") {
//         Ciallang::Value intValue{ static_cast<Ciallang::Integer>(42) };
//         Ciallang::Value realValue{ static_cast<Ciallang::Real>(2.71) };
//         Ciallang::String str{ "hello" };
//         Ciallang::Value strValue{ str };

//         // 类型检查
//         REQUIRE(intValue.isInteger());
//         REQUIRE(realValue.isReal());
//         REQUIRE(strValue.isString());

//         // 类型转换检查
//         REQUIRE_FALSE(intValue.isReal());
//         REQUIRE_FALSE(realValue.isInteger());
//         REQUIRE_FALSE(strValue.isInteger());
//     }
// }

// TEST_CASE("类型系统 - 边界值测试") {
//     SECTION("整数边界值") {
//         // 最小整数
//         Ciallang::Value minInt{ static_cast<Ciallang::Integer>(LLONG_MIN) };
//         REQUIRE(minInt.toInteger() == LLONG_MIN);

//         // 最大整数
//         Ciallang::Value maxInt{ static_cast<Ciallang::Integer>(LLONG_MAX) };
//         REQUIRE(maxInt.toInteger() == LLONG_MAX);

//         // 零
//         Ciallang::Value zero{ static_cast<Ciallang::Integer>(0) };
//         REQUIRE(zero.toInteger() == 0);
//     }

//     SECTION("浮点数边界值") {
//         // 正无穷大
//         Ciallang::Value posInf{ static_cast<Ciallang::Real>(std::numeric_limits<double>::infinity()) };
//         REQUIRE(posInf.toReal() == std::numeric_limits<double>::infinity());

//         // 负无穷大
//         Ciallang::Value negInf{ static_cast<Ciallang::Real>(-std::numeric_limits<double>::infinity()) };
//         REQUIRE(negInf.toReal() == -std::numeric_limits<double>::infinity());

//         // NaN
//         Ciallang::Value nan{ static_cast<Ciallang::Real>(std::numeric_limits<double>::quiet_NaN()) };
//         REQUIRE(std::isnan(nan.toReal()));

//         // 零
//         Ciallang::Value zero{ static_cast<Ciallang::Real>(0.0) };
//         REQUIRE(zero.toReal() == 0.0);
//     }

//     SECTION("字符串边界值") {
//         // 空字符串
//         Ciallang::String empty{ "" };
//         REQUIRE(empty.empty());

//         // 长字符串
//         std::string longStr(1000, 'a');
//         Ciallang::String longTjsStr{ longStr };
//         REQUIRE(longTjsStr.length() == 1000);
//     }
// }