#include "StandardLibrary.hpp"
#include "ArrayLib.hpp"
#include "DateLib.hpp"
#include "DictLib.hpp"
#include "MathLib.hpp"
#include "RandomLib.hpp"
#include "RegExpLib.hpp"
#include "ScriptsLib.hpp"
#include "SerializeLib.hpp"
#include "StringLib.hpp"

namespace cial::stdlib {
    void registerStandardLibrary(Context &context) {
        registerStringLib(context);
        registerArrayLib(context);
        registerDictLib(context);
        registerRegExpLib(context);
        registerMathLib(context);
        registerRandomLib(context);
        registerDateLib(context);
        registerSerializeLib(context);
        registerScriptsLib(context);
    }
} // namespace cial::stdlib
