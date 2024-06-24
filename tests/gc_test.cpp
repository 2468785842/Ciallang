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

class Dept : public GCObject {
public:
    size_t id{ 0 };
    std::string name{ "dept_object" };

    ~Dept() override = default;

    size_t size() const noexcept override {
        return sizeof(Dept);
    }

    GCObject* copyTo(uint8_t* to) override {
        return new(to) Dept{ std::move(*this) };
    }
};

class Emp : public GCObject {
public:
    size_t id{ 1 };
    std::string name{ "emp_object" };
    Dept* dept{ nullptr };

    std::optional<std::vector<GCObject*>> getFields() const override {
        auto fields = std::vector<GCObject*>{};
        if(dept) {
            fields.push_back(dept);
            auto childrenFields = dept->getFields();
            if(childrenFields.has_value()) {
                fields.insert(
                    fields.end(),
                    childrenFields.value().begin(),
                    childrenFields.value().end()
                );
            }
        }
        return std::move(fields);
    }

    size_t size() const noexcept override {
        return sizeof(Emp);
    }

    GCObject* copyTo(uint8_t* to) override {
        return new(to) Emp{ std::move(*this) };
    }

    ~Emp() override = default;
};


TEST(GCTest, TestMinor) {
    Generational gc{ 4700 };
    auto& roots = gc.allocateRoots();

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
