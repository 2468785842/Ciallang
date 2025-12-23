/*
 * Copyright (c) 2024/12/18
 *
 * GC单元测试 - 测试垃圾回收和循环引用检测
 */

#include <catch.hpp>
#include <utility>
#include <vector>

#include "gc/GC.hpp"

// 测试用的节点类，用于创建循环引用
class TestNode : public Ciallang::GCObject {
public:
    TestNode *next;
    std::string name;
    static int s_instanceCount;

    explicit TestNode(std::string n) : next(nullptr), name(std::move(n)) { s_instanceCount++; }
    ~TestNode() override { s_instanceCount--; }

    // 适配GC的子节点添加方法
    void setNext(TestNode *child) {
        next = child;
        if(child) {
            addChild(child);
        }
    }
};

int TestNode::s_instanceCount = 0;

TEST_CASE("GC - 基本垃圾回收") {
    SECTION("单对象跟踪") {
        Ciallang::GC &gc = Ciallang::GC::instance();

        // 创建对象
        auto obj = new TestNode("test1");
        REQUIRE(obj != nullptr);

        // 注册到GC
        gc.track(obj);
        REQUIRE(obj->getRefCount() >= 0);

        // 执行垃圾回收
        gc.collect();
        REQUIRE(TestNode::s_instanceCount == 0);
    }

    SECTION("多对象跟踪") {
        Ciallang::GC &gc = Ciallang::GC::instance();

        // 创建多个对象
        std::vector<TestNode *> objects;
        for(int i = 0; i < 5; ++i) {
            auto obj = new TestNode("test" + std::to_string(i));
            gc.track(obj);
            objects.push_back(obj);
        }

        // 执行垃圾回收
        gc.collect();
        REQUIRE(TestNode::s_instanceCount == 0);
    }
}

TEST_CASE("GC - 循环引用检测") {
    SECTION("简单循环引用") {
        Ciallang::GC &gc = Ciallang::GC::instance();

        // 创建两个节点形成循环引用
        auto node1 = new TestNode("node1");
        auto node2 = new TestNode("node2");

        node1->setNext(node2);
        node2->setNext(node1);

        // 注册到GC
        gc.track(node1);
        gc.track(node2);

        // 执行垃圾回收
        gc.collect();

        // 验证所有对象都被回收
        REQUIRE(TestNode::s_instanceCount == 0);
    }

    SECTION("复杂循环引用链") {
        Ciallang::GC &gc = Ciallang::GC::instance();

        // 创建三个节点形成循环引用链
        auto node1 = new TestNode("node1");
        auto node2 = new TestNode("node2");
        auto node3 = new TestNode("node3");

        node1->setNext(node2);
        node2->setNext(node3);
        node3->setNext(node1); // 形成循环

        // 注册到GC
        gc.track(node1);
        gc.track(node2);
        gc.track(node3);

        // 执行垃圾回收
        gc.collect();

        // 验证所有对象都被回收
        REQUIRE(TestNode::s_instanceCount == 0);
    }
}

TEST_CASE("GC - 边界情况") {
    SECTION("空GC回收") {
        Ciallang::GC &gc = Ciallang::GC::instance();

        // 没有注册任何对象时执行回收
        gc.collect();
        REQUIRE(TestNode::s_instanceCount == 0);
        REQUIRE(true); // 应该不会崩溃
    }

    SECTION("重复回收") {
        Ciallang::GC &gc = Ciallang::GC::instance();

        // 创建并跟踪对象
        auto obj = new TestNode("test");
        gc.track(obj);

        // 第一次回收
        gc.collect();

        // 第二次回收
        gc.collect();
        REQUIRE(TestNode::s_instanceCount == 0);
    }
}
