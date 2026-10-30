/*
 * Copyright (c) 2024/12/18
 *
 * 类型系统单元测试（对准 Value 六类型动态值现代 API：
 * 显式构造 / is* 谓词 / as* 转换（TJS2 语义）/ 算术与位运算）
 */

#include <catch2/catch_all.hpp>

#include "types/Octet.hpp"
#include "types/Value.hpp"

using namespace cial;

TEST_CASE("类型系统 - 六类型构造与谓词") {
    SECTION("整数") {
        const Value value{ Integer{ 42 } };
        REQUIRE(value.type() == ValueType::Integer);
        REQUIRE(value.isInteger());
        REQUIRE_FALSE(value.isReal());
        REQUIRE_FALSE(value.isString());
        REQUIRE_FALSE(value.isOctet());
        REQUIRE_FALSE(value.isObject());
        REQUIRE_FALSE(value.isVoid());
    }

    SECTION("实数") {
        const Value value{ Real{ 3.14 } };
        REQUIRE(value.type() == ValueType::Real);
        REQUIRE(value.isReal());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isString());
    }

    SECTION("字符串") {
        const Value value{ "Hello World"_str };
        REQUIRE(value.type() == ValueType::String);
        REQUIRE(value.isString());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isReal());
    }

    SECTION("Void 默认构造") {
        const Value value{};
        REQUIRE(value.type() == ValueType::Void);
        REQUIRE(value.isVoid());
        REQUIRE_FALSE(value.isInteger());
        REQUIRE_FALSE(value.isObject());
    }

    SECTION("Octet") {
        const u8 bytes[]{ 0x01, 0x02, 0x03, 0x04 };
        const Value value{ Octet{ bytes, 4 } };
        REQUIRE(value.type() == ValueType::Octet);
        REQUIRE(value.isOctet());
        REQUIRE_FALSE(value.isString());
        REQUIRE_FALSE(value.isInteger());
    }
}

TEST_CASE("类型系统 - as* 转换的 TJS2 语义") {
    SECTION("整数原样取出") {
        const Value value{ Integer{ 42 } };
        REQUIRE(value.asInteger().unwrap() == 42);
    }

    SECTION("大整数不经 double 往返") {
        const Value value{ Integer{ std::numeric_limits<i64>::max() } };
        REQUIRE(value.asInteger().unwrap() == std::numeric_limits<i64>::max());
    }

    SECTION("Void 转 0") {
        const Value value{};
        REQUIRE(value.asInteger().unwrap() == 0);
    }

    SECTION("实数转整数截断") {
        const Value value{ Real{ 3.99 } };
        REQUIRE(value.asInteger().unwrap() == 3);
        REQUIRE(value.asReal().unwrap().value() == Catch::Approx(3.99));
    }

    SECTION("字符串解析为整数") {
        REQUIRE((Value{ "123"_str }).asInteger().unwrap() == 123);
        REQUIRE((Value{ "-12"_str }).asInteger().unwrap() == -12);
        REQUIRE((Value{ "abc"_str }).asInteger().isFailed());
    }

    SECTION("字符串原样取出") {
        const Value value{ "abc"_str };
        REQUIRE(value.asString().unwrap()->toStdStr() == "abc");
    }

    SECTION("toString：void → 空串、octet 抛错（原版 ToString，tjsVariant.cpp:411-445）") {
        Value voidValue{};
        REQUIRE(voidValue.toString().isOk());
        REQUIRE(voidValue.asString().unwrap()->isEmpty());

        Value intValue{ Integer{ 5 } };
        REQUIRE(intValue.toString().isOk());
        REQUIRE(intValue.asString().unwrap()->toStdStr() == "5");

        const u8 bytes[]{ 0x01 };
        Value octetValue{ Octet{ bytes, 1 } };
        REQUIRE(octetValue.toString().isFailed());
    }

    SECTION("octet 不参与算术转换") {
        const u8 bytes[]{ 0x01 };
        const Value value{ Octet{ bytes, 1 } };
        REQUIRE(value.asInteger().isFailed());
    }
}

TEST_CASE("类型系统 - 算术运算") {
    SECTION("整数四则") {
        const Value a{ Integer{ 10 } }, b{ Integer{ 5 } };
        REQUIRE(a.add(b).unwrap().asInteger().unwrap() == 15);
        REQUIRE(a.sub(b).unwrap().asInteger().unwrap() == 5);
        REQUIRE(a.mul(b).unwrap().asInteger().unwrap() == 50);
    }

    SECTION("除法为实数除、整除为独立运算（TJS2 语义）") {
        const Value a{ Integer{ 5 } }, b{ Integer{ 2 } };
        REQUIRE(a.div(b).unwrap().asReal().unwrap().value() == Catch::Approx(2.5));
        REQUIRE(a.idiv(b).unwrap().asInteger().unwrap() == 2);
        REQUIRE(a.mod(b).unwrap().asInteger().unwrap() == 1);
    }

    SECTION("实数与混合运算") {
        const Value a{ Real{ 10.5 } }, b{ Real{ 2.5 } };
        REQUIRE(a.add(b).unwrap().asReal().unwrap().value() == Catch::Approx(13.0));
        REQUIRE(a.mul(b).unwrap().asReal().unwrap().value() == Catch::Approx(26.25));

        const Value mixed{ Integer{ 10 } };
        REQUIRE(mixed.add(b).unwrap().asReal().unwrap().value() == Catch::Approx(12.5));
    }

    SECTION("字符串拼接") {
        REQUIRE((Value{ "a"_str }).add(Value{ "b"_str }).unwrap().asString().unwrap()->toStdStr() == "ab");
        REQUIRE((Value{ "n"_str }).add(Value{ Integer{ 1 } }).unwrap().asString().unwrap()->toStdStr() == "n1");
    }

    SECTION("除零报 DivideByZero") {
        const Value a{ Integer{ 1 } };
        REQUIRE(a.div(Value{ Integer{ 0 } }).isFailed());
        REQUIRE(a.idiv(Value{ Integer{ 0 } }).isFailed());
        REQUIRE(a.mod(Value{ Integer{ 0 } }).isFailed());
    }

    SECTION("octet 参与算术报 TypeError") {
        const u8 bytes[]{ 0x01 };
        const Value o{ Octet{ bytes, 1 } };
        REQUIRE(o.add(Value{ Integer{ 1 } }).isFailed());
    }
}

TEST_CASE("类型系统 - 位运算") {
    const Value a{ Integer{ 0b1100 } }, b{ Integer{ 0b1010 } };

    SECTION("与或非异或") {
        REQUIRE(a.bitwiseAnd(b).unwrap().asInteger().unwrap() == 0b1000);
        REQUIRE(a.bitwiseOr(b).unwrap().asInteger().unwrap() == 0b1110);
        REQUIRE(a.bitwiseXor(b).unwrap().asInteger().unwrap() == 0b0110);
    }

    SECTION("移位") {
        const Value one{ Integer{ 1 } };
        REQUIRE(one.bitwiseLeftShift(Value{ Integer{ 4 } }).unwrap().asInteger().unwrap() == 16);
        REQUIRE(Value{ Integer{ -16 } }.bitwiseRightShift(Value{ Integer{ 2 } }).unwrap().asInteger().unwrap() == -4);
    }

    SECTION("非整数操作数按 TJS2 语义截断") {
        REQUIRE(a.bitwiseAnd(Value{ Real{ 1.5 } }).unwrap().asInteger().unwrap() == 0);
    }
}

TEST_CASE("类型系统 - 拷贝与赋值独立性") {
    const Value original{ "payload"_str };
    const Value copy = original;
    REQUIRE(copy.asString().unwrap()->toStdStr() == "payload");
    // 拷贝不得共享可变负载：改副本不影响原值
    copy.asString().unwrap()->append("x"_str);
    REQUIRE(original.asString().unwrap()->length() == 7);

    Value moved{ Integer{ 7 } };
    const Value target = std::move(moved);
    REQUIRE(moved.isVoid());
    REQUIRE(target.asInteger().unwrap() == 7);
}
