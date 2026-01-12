//
// Created by LiDong on 2026/1/12.
//

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("property - base read/write") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        var value;
        property p1 {
            setter(v) {
                value = v;
            }

            getter() {
                return value;
            }
        }
        p1 = 1;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("p1"_str) == 1);
}

TEST_CASE("property - read/write behavior") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        var log = "";
        var value;
        property p1 {
            setter(v) {
                log += "set(" + v + ");";
                value = v;
            }

            getter {
                log += "get();";
                return value;
            }
        }

        p1 = 10;          // 只调用 setter
        var a = p1;       // 只调用 getter
        p1 = p1 + 1;      // getter → setter
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("a"_str) == 10);
    REQUIRE(*vm.evalExpr<Integer>("value"_str) == 11);
    REQUIRE(**vm.evalExpr<String *>("log"_str) == "set(10);get();get();set(11);"_str);
}

TEST_CASE("property - only getter") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        property p_readonly {
            getter {
                return 42;
            }
        }

        var x = p_readonly;   // OK
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("x"_str) == 42);
}

TEST_CASE("property - only setter") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        var last;
        property p_writeonly {
            setter(v) {
                last = v;
            }
        }

        p_writeonly = 7;   // OK
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("last"_str) == 7);
}

// TODO: more tests
// 四、属性覆盖（shadow / override）
// property p3
// {
//     getter { return 1; }
// }
//
// inform(p3); // 1
//
// property p3   // 覆盖同名属性
// {
//     getter { return 2; }
// }
//
// inform(p3); // 2
//
//
// 👉 覆盖：属性像变量一样可被重新定义
//
// 五、属性对象 & 运算符
// 4️⃣ 取得属性对象，不触发 getter / setter
// var log = "";
//
// property p4
// {
//     setter(v)
//     {
//         log += "set;";
//     }
//
//     getter
//     {
//         log += "get;";
//         return 0;
//     }
// }
//
// var obj = &p4;   // 不应触发 get/set
//
// inform(log);     // ""
//
//
// 👉 覆盖：& 不触发控制器
//
// 5️⃣ 使用 * 运算符调用属性对象
// *p4 = 10;      // setter
// var v = *p4;   // getter
//
// inform(log);   // "set;get;"
//
//
// 👉 覆盖：脱离注册环境仍可调用控制器
//
// 六、属性对象重新注册
// var store;
//
// property p5
// {
//     setter(v) { store = v; }
//     getter { return store; }
// }
//
// var po = &p5;
//
// var obj = {};
// &obj.x = po;
//
// obj.x = 123;
// inform(obj.x); // 123
//
//
// 👉 覆盖：
//
// 属性对象跨对象注册
//
// 行为保持一致
//
// 七、局部变量 vs 全局注册的差异（重要边界）
// property p6
// {
//     getter { return 9; }
// }
//
// var a = &p6;  // 局部变量，属性对象
// inform(*a);   // OK，调用 getter
//
// globalProp = a; // 注册为全局成员
// inform(globalProp); // 调用 getter，而不是得到 Property 对象
//
//
// 👉 覆盖手册中的 Note
//
// 八、instanceof Property
// property p7
// {
//     getter { return 1; }
// }
//
// inform((&p7 instanceof "Property")); // true
// inform((p7 instanceof "Property"));  // false
//
//
// 👉 覆盖：
//
// 属性值 vs 属性对象 的类型差异
//
// 九、表达式中的隐式调用顺序（易错点）
// var trace = "";
//
// property p8
// {
//     setter(v)
//     {
//         trace += "S(" + v + ")";
//         value = v;
//     }
//     getter
//     {
//         trace += "G";
//         return value;
//     }
// }
//
// p8 = 1;
// p8 = p8 * 2 + p8;
//
// inform(trace);
// // 期望：
// // S(1) G G S(3)
//
//
// 👉 覆盖：
//
// 一个表达式中多次 getter
//
// setter 只在最终赋值调用一次
//
// 十、错误路径（健壮性）
// property p9
// {
//     setter(v)
//     {
//         if (v < 0)
//             error("negative not allowed");
//     }
// }
//
// p9 = -1; // 应抛出 setter 内错误
//
//
// 👉 覆盖：
//
// setter / getter 内异常传播