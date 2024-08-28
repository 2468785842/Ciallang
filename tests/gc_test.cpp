/*
 * Copyright (c) 2024/6/23 下午9:02
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#include <gtest/gtest.h>

#include "../src/gc/Generational.hpp"

using namespace Ciallang::GC;

class Dept final : public GCObject {
public:
    size_t id{ 0 };
    std::string name{ "dept_object" };

    constexpr size_t size() const noexcept override {
        return sizeof(Dept);
    }

    std::optional<std::vector<GCObject*>> getFields() const override { return {}; }

    GCObject* copyTo(uint8_t* to) override {
        return new(to) Dept{ *this };
    }
};

class Emp final : public GCObject {
public:
    size_t id{ 1 };
    std::string name{ "emp_object" };
    Dept* dept{ nullptr };

    std::optional<std::vector<GCObject*>> getFields() const override {
        if(!dept) return {};

        auto fields = std::vector<GCObject*>{};
        auto childrenFields = dept->getFields();
        if(childrenFields.has_value()) {
            fields.insert(
                fields.end(),
                childrenFields.value().begin(),
                childrenFields.value().end()
            );
        }
        fields.push_back(dept);
        return fields;
    }

    constexpr size_t size() const noexcept override {
        return sizeof(Emp);
    }

    GCObject* copyTo(uint8_t* to) override {
        return new(to) Emp{ *this };
    }

    ~Emp() override {
        fmt::println("{}", "");
    }
};


TEST(GCTest, TestMinor) {
    Generational gc{ 4700 };
    auto& roots = gc.allocateRoots();

    gc.printState();
    Emp* emp1 = gc.allocateNewSpace<Emp>();
    roots.push_back(emp1);
    Dept* dept1 = gc.allocateNewSpace<Dept>();

    gc.updatePtr(emp1, &emp1->dept, dept1);

    for(size_t i = 0; i < 6; ++i) {
        gc.allocateNewSpace<Emp>();
    }

    fmt::println("即将新生代GC...");
    gc.allocateNewSpace<Emp>();
    gc.printState();
}

TEST(GCTest, TestPromotion) {
    Generational gc{ 2000 };
    auto& roots = gc.allocateRoots();
    roots.push_back(gc.allocateNewSpace<Emp>());

    gc.printState();
    for(int i = 0; i < 31; ++i) {
        gc.allocateNewSpace<Emp>();
    }

    printf("emp1晋升\n");
    gc.allocateNewSpace<Emp>();
    gc.printState();
}

TEST(GCTest, TestPromotionMajorGC) {
    Generational gc{ 2048 * 16 };

    auto& roots = gc.allocateRoots();

    for(int j = 0; j < 12; j++) {
        auto* _emp1 = gc.allocateNewSpace<Emp>();
        _emp1->id = 666;
        roots.push_back(_emp1);

        //分配新对象，触发新生代GC
        for(int i = 0; i < 31; i++) {
            gc.allocateNewSpace<Emp>();
        }
    }

    printf("老年代GC\n");

    roots.push_back(gc.allocateNewSpace<Emp>());

    roots.erase(roots.begin(), roots.begin() + 10);

    gc.printState();
    // 分配新对象，触发新生代GC
    for(int i = 0; i < 32; i++) {
        gc.allocateNewSpace<Emp>();
    }

    LOG(INFO) << "FULL GC...";
    roots.clear();
    gc.collect();
    gc.printState();
}

TEST(GCTest, TestCrossGenRefGC) {
    Generational gc{ 2000 }; //分配后，实际可用1936
    auto& roots = gc.allocateRoots();

    roots.push_back(gc.allocateNewSpace<Emp>());

    //触发3次新生代GC，然后emp1会晋升至老年代
    for(int i = 0; i < 31; ++i) {
        gc.allocateNewSpace<Emp>();
    }

    LOG(INFO) << "emp1晋升";

    auto* tempEmp = gc.allocateNewSpace<Emp>();

    auto* dept1 = gc.allocateNewSpace<Dept>();

    //因为_emp1发生了晋升，所以此处的_emp1是复制前的对象，是一个过期的状态
    //使用复制后的新_emp1对象

    gc.updatePtr(tempEmp, &tempEmp->dept, dept1);

    LOG(INFO) << "FULL GC...";
    gc.collect();

    gc.printState();
}

TEST(GCTest, TestPromotionCrossGenRefGC) {
    Generational gc{ 2000 }; //分配后，实际可用1936
    auto& roots = gc.allocateRoots();

    auto* _emp1 = gc.allocateNewSpace<Emp>();
    roots.push_back(_emp1);

    //触发3次新生代GC，然后emp1会晋升至老年代
    for(int i = 0; i < 31; ++i) {
        gc.allocateNewSpace<Emp>();
    }

    //在_emp1晋升前，增加_emp1->dept的引用，创建跨代引用
    auto* dept1 = gc.allocateNewSpace<Dept>();

    //因为_emp1发生了晋升，所以此处的_emp1是复制前的对象，是一个过期的状态
    //使用复制后的新_emp1对象
    gc.updatePtr(roots[0], &dynamic_cast<Emp*>(roots[0])->dept, dept1);

    LOG(INFO) << "emp1晋升";

    //此时新生代内存不足，发生GC，_emp1已经经历3次GC，会晋升到老年代，但由于_dept还处于新生代，所以会在_rs中记录这条跨代引用
    gc.allocateNewSpace<Emp>();

    LOG(INFO) << "FULL GC...";
    gc.collect();

    gc.printState();
}
