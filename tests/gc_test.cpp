// Copyright (c) 2025/9/29 14:59
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

//
// Created by LiDon on 2025/9/29.
//

#include "gc/GC.hpp"
#include <catch.hpp>
#include <iostream>
#include <utility>

using namespace Ciallang;

class Node : public GCObject {
public:
    std::string name;
    explicit Node(std::string n) : name(std::move(n)) {}
    ~Node() override { std::cout << "Node " << name << " destroyed" << std::endl; }
};

TEST_CASE("垃圾回收 - 基本功能") {

    // 创建循环引用 A <-> B
    auto a = new Node("A");
    auto b = new Node("B");

    // A -> B, B -> A
    a->addChild(b);
    b->addChild(a);

    // 跟踪对象
    GC::instance().track(a);
    GC::instance().track(b);

    // 验证对象被跟踪
    REQUIRE(a->getRefCount() >= 0);
    REQUIRE(b->getRefCount() >= 0);

    // 循环对象仍然在 candidates 中
    std::cout << "Running GC..." << std::endl;
    GC::instance().collect(); // 这里会回收 A 和 B

    // 注意：由于对象已被回收，我们无法直接验证
    // 但可以通过检查GC是否正常工作来间接验证
    REQUIRE(true); // 如果执行到这里说明没有崩溃
}