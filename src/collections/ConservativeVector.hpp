/*
 * Copyright (c) 2024/8/27 下午8:41
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
#pragma once
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Ciallang::Collections {
    template <typename T>
    class ConservativeVector {
    public:
        ConservativeVector()
            : _size(0), _capacity(1), _data(static_cast<T*>(operator new[](sizeof(T) * _capacity))) {
        }

        ~ConservativeVector() {
            clear();
            operator delete[](_data);
        }

        void pushBack(const T& value) {
            if(_size >= _capacity) {
                reserve(_capacity * 2); // 按倍数增加容量
            }
            new(&_data[_size++]) T(value); // 使用 placement new 创建对象
        }

        void pushBack(T&& value) {
            if(_size >= _capacity) {
                reserve(_capacity * 2);
            }
            new(&_data[_size++]) T(std::move(value)); // 使用 placement new 创建对象
        }

        void popBack() {
            if(_size == 0) {
                throw std::out_of_range("Vector is empty");
            }
            _data[--_size].~T(); // 显式调用析构函数

            // 保守释放策略：当大小小于容量的四分之一时才释放
            if(_size <= _capacity / 4) {
                reserve(_capacity / 2); // 按倍数减少容量
            }
        }

        size_t size() const {
            return _size;
        }

        size_t capacity() const {
            return _capacity;
        }

        T& operator[](size_t index) {
            if(index >= _size) {
                throw std::out_of_range("Index out of bounds");
            }
            return _data[index];
        }

        const T& operator[](size_t index) const {
            if(index >= _size) {
                throw std::out_of_range("Index out of bounds");
            }
            return _data[index];
        }

        // 调整大小
        void resize(const size_t new_size, const T& default_value = T()) {
            if(new_size > _capacity) {
                reserve(new_size); // 扩展容量以容纳新元素
            }

            if(new_size > _size) {
                for(size_t i = _size; i < new_size; ++i) {
                    new(&_data[i]) T(default_value); // 使用 placement new 初始化新元素
                }
            } else {
                for(size_t i = new_size; i < _size; ++i) {
                    _data[i].~T(); // 调用析构函数销毁对象
                }
            }

            _size = new_size; // 更新大小

            // 保守策略：如果缩小后的大小非常小，可以选择减少容量
            if(_size <= _capacity / 4) {
                reserve(_capacity / 2);
            }
        }

        void clear() {
            for(size_t i = 0; i < _size; ++i) {
                _data[i].~T(); // 显式调用析构函数销毁所有对象
            }
            _size = 0;
        }

        T& back() {
            if(_size == 0) {
                throw std::out_of_range("Vector is empty");
            }
            return _data[_size - 1];
        }

        const T& back() const {
            if(_size == 0) {
                throw std::out_of_range("Vector is empty");
            }
            return _data[_size - 1];
        }

        size_t empty() const {
            return _size == 0;
        }

    private:
        size_t _size;
        size_t _capacity;
        T* _data;

        void reserve(size_t new_capacity) {
            if(new_capacity < _size) {
                new_capacity = _size;
            }

            T* new_data = static_cast<T*>(operator new[](sizeof(T) * new_capacity));

            // 将数据从旧内存移动到新内存
            for(size_t i = 0; i < _size; ++i) {
                new(&new_data[i]) T(std::move(_data[i])); // 使用 placement new 移动对象
                _data[i].~T();                            // 销毁旧对象
            }

            operator delete[](_data);
            _data = new_data;
            _capacity = new_capacity;
        }
    };
}
