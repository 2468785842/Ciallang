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

    template <size_t N>
    struct TjsValueName {
        char value[N]{};

        constexpr explicit TjsValueName(const char (&value)[N]) {
            std::copy_n(value, N, this->value);
        }
    };

    template <TjsValueName NAME>
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

    using TjsRealHelper = MakeTjsValueHelper<TjsValueName("real")>;
    using TjsIntegerHelper = MakeTjsValueHelper<TjsValueName("integer")>;
    using TjsStringHelper = MakeTjsValueHelper<TjsValueName("string")>;
    using TjsOctetHelper = MakeTjsValueHelper<TjsValueName("octet")>;
    using TjsObjectHelper = MakeTjsValueHelper<TjsValueName("object")>;

    static constexpr MakeTjsValueHelper<TjsValueName("void")> S_TjsVoidHelper{};
    using TjsVoidValueHelper = decltype(S_TjsVoidHelper);

    template<>
    inline void TjsVoidValueHelper::destroy(TjsValue &) const {
        // no nothing to do, beause the void type really not manager anything,
        // and no have error, void type is free
    }

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
