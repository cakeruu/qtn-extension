#include "ast.h"

namespace qtn {

std::string TypeExpr::toString() const {
    switch (kind) {
    case Kind::Named:
        return name;
    case Kind::Pointer:
        return args.empty() ? "?" : args[0].toString() + "*";
    case Kind::Nullable:
        return args.empty() ? "?" : args[0].toString() + "?";
    case Kind::Button:
        return "button";
    case Kind::List:
        return "list<" + (args.empty() ? "" : args[0].toString()) + ">";
    case Kind::HashSet:
        return "hash_set<" + (args.empty() ? "" : args[0].toString()) + ">";
    case Kind::Array: {
        std::string s = "array<" + (args.empty() ? "" : args[0].toString()) + ">";
        if (arraySize) s += "[" + *arraySize + "]";
        return s;
    }
    case Kind::Dictionary: {
        std::string s = "dictionary<";
        if (args.size() >= 2) s += args[0].toString() + ", " + args[1].toString();
        s += ">";
        return s;
    }
    case Kind::AssetRef:
        return "asset_ref<" + name + ">";
    }
    return "?";
}

std::string Node::kindName() const {
    switch (kind) {
    case Kind::Component:  return "component";
    case Kind::Struct:     return "struct";
    case Kind::Enum:       return "enum";
    case Kind::Flags:      return "flags";
    case Kind::Union:      return "union";
    case Kind::Bitset:     return "bitset";
    case Kind::Input:      return "input";
    case Kind::Signal:     return "signal";
    case Kind::Event:      return "event";
    case Kind::Global:     return "global";
    case Kind::Asset:      return "asset";
    case Kind::Import:     return "import";
    case Kind::Using:      return "using";
    case Kind::Directive:  return "directive";
    }
    return "?";
}

bool TranslationUnit::hasErrors() const {
    for (auto& d : diags)
        if (d.level == DiagLevel::Error) return true;
    return false;
}

}