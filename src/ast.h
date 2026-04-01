#pragma once
#include "lexer.h"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace qtn {

struct TypeExpr;
struct FieldDecl;
struct Node;

using NodePtr = std::unique_ptr<Node>;
using NodeList = std::vector<NodePtr>;

struct TypeExpr {
  enum class Kind {
    Named,
    List,
    Dictionary,
    HashSet,
    Array,
    AssetRef,
    Pointer,
    Nullable,
    Button,
  };

  Kind kind = Kind::Named;
  std::string name;

  std::vector<TypeExpr> args;

  std::optional<std::string> arraySize;

  SourceLoc loc;

  std::string toString() const;
};

struct Attribute {
  std::string text;
  SourceLoc loc;
};

struct FieldDecl {
  std::vector<Attribute> attrs;
  TypeExpr type;
  std::string name;
  SourceLoc loc;
};

struct SignalParam {
  TypeExpr type;
  std::string name;
  SourceLoc loc;
};

struct UnionMember {
  std::string structName;
  std::string fieldName;
  SourceLoc loc;
};

struct EnumValue {
  std::string name;
  std::optional<int64_t> value;
  SourceLoc loc;
};

struct Node {
  enum class Kind {
    Component,
    Struct,
    Enum,
    Flags,
    Union,
    Bitset,
    Input,
    Signal,
    Event,
    Global,
    Asset,
    Import,
    Using,
    Directive,
  };

  Kind kind;
  SourceLoc loc;
  virtual ~Node() = default;
  virtual std::string kindName() const;
};

struct ComponentNode : Node {
  bool singleton = false;
  std::string name;
  std::vector<FieldDecl> fields;
};

struct StructNode : Node {
  std::string name;
  std::vector<FieldDecl> fields;
};

struct EnumNode : Node {
  bool isFlags = false;
  std::string name;
  std::optional<std::string> underlying;
  std::vector<EnumValue> values;
};

struct UnionNode : Node {
  std::string name;
  std::vector<UnionMember> members;
};

struct BitsetNode : Node {
  std::string name;
  int bits = 256;
};

struct InputNode : Node {
  std::vector<FieldDecl> fields;
};

struct SignalNode : Node {
  std::string name;
  std::vector<SignalParam> params;
};

struct EventNode : Node {
  bool isAbstract = false;
  bool isSynced = false;
  bool isServer = false;
  bool isClient = false;

  std::string name;
  std::optional<std::string> base;
  std::vector<FieldDecl> fields;
};

struct GlobalNode : Node {
  std::vector<FieldDecl> fields;
};

struct AssetNode : Node {
  std::string name;
};

struct ImportNode : Node {
  bool isStruct = false;
  std::string path;
  std::optional<int> structSize;
};

struct UsingNode : Node {
  std::string ns;
};

struct DirectiveNode : Node {
  std::string text;
};

struct TranslationUnit {
  std::string filename;
  NodeList nodes;
  std::vector<Diagnostic> diags;

  bool hasErrors() const;
};

} // namespace qtn