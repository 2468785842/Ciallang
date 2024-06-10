// Copyright (c) 2024/5/23 下午9:40
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

#pragma once

#include "../common/ConstExpr.hpp"

namespace Ciallang {
#if defined (LLONG_MAX)
    using TjsInteger = long long;
    using TjsReal = double;
#else
    using TjsInteger = int;
    using TjsReal = float;
#endif

    enum class TjsValueType {
        Void, // empty
        Object,
        String,
        Octet, // octet binary data
        Integer,
        Real
    };

    class TjsString;
    class TjsOctet;
    class TjsObject;

    class TjsValue;

    struct TjsValueHelper {
        static const TjsValueHelper* instance(TjsValueType type);
        virtual ~TjsValueHelper() = default;
        virtual void copy(const TjsValue& src, TjsValue& dest) const = 0;
        virtual void move(TjsValue& src, TjsValue& dest) const = 0;
        virtual void destroy(TjsValue& value) const = 0;
        [[nodiscard]] virtual const char* name() const = 0;
    };

    template <Common::Name NAME>
    struct MakeTjsValueHelper : TjsValueHelper {
        void copy(const TjsValue&, TjsValue&) const override {
                LOG(FATAL)
                        << "not implement function `copy` with type "
                        << _name;
        }

        void move(TjsValue&, TjsValue&) const override {
                LOG(FATAL)
                        << "not implement function `move` with type "
                        << _name;
        }

        void destroy(TjsValue&) const override {
                LOG(FATAL)
                        << "not implement function `destory` with type "
                        << _name;
        }

        [[nodiscard]] const char* name() const final {
            return _name;
        }

    private:
        const char* _name = NAME.value;
    };

    using TjsVoidHelper = MakeTjsValueHelper<Common::Name("void")>;
    using TjsRealHelper = MakeTjsValueHelper<Common::Name("real")>;
    using TjsIntegerHelper = MakeTjsValueHelper<Common::Name("integer")>;
    using TjsStringHelper = MakeTjsValueHelper<Common::Name("string")>;
    using TjsOctetHelper = MakeTjsValueHelper<Common::Name("octet")>;
    using TjsObjectHelper = MakeTjsValueHelper<Common::Name("object")>;

    static constexpr TjsVoidHelper S_TjsVoidHelper{};

    static constexpr TjsIntegerHelper S_TjsIntegerHelper{};
    static constexpr TjsRealHelper S_TjsRealHelper{};
    static constexpr TjsStringHelper S_TjsStringHelper{};
    static constexpr TjsOctetHelper S_TjsOctetHelper{};
    static constexpr TjsObjectHelper S_TjsObjectHelper{};

    static constexpr auto S_Helpers =
            frozen::make_unordered_map<TjsValueType, const TjsValueHelper*>({
                    { TjsValueType::Void, &S_TjsVoidHelper },
                    { TjsValueType::Integer, &S_TjsIntegerHelper },
                    { TjsValueType::Real, &S_TjsRealHelper },
                    { TjsValueType::String, &S_TjsStringHelper },
                    { TjsValueType::Octet, &S_TjsOctetHelper },
                    { TjsValueType::Object, &S_TjsObjectHelper }
            });


    inline const TjsValueHelper* TjsValueHelper::instance(const TjsValueType type) {
        const auto it = S_Helpers.find(type);
        if(it == S_Helpers.end()) {
            LOG(FATAL) << "not find helper!! type enum: 0x" << std::hex << static_cast<size_t>(type);
        }
        return it->second;
    }

}
