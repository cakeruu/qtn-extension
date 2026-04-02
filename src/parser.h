#pragma once
#include "ast.h"
#include "lexer.h"
#include <vector>

namespace qtn
{

class Parser
{
  public:
    explicit Parser(std::vector<Token> tokens, std::string filename = "<input>");
    TranslationUnit parse();

  private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::string file_;
    std::vector<Diagnostic> diags_;

    const Token &peek(int offset = 0) const;
    const Token &advance();
    bool at(TokenKind k) const;
    bool eat(TokenKind k);
    Token expect(TokenKind k, std::string_view what);
    std::string expectIdent(std::string_view what = "identifier");

    void error(const SourceLoc &loc, std::string msg);
    void errorAt(std::string msg);
    bool isTopLevelStart() const;
    void synchronize();

    NodePtr parseTopLevel();
    NodePtr parseComponent(bool singleton);
    NodePtr parseStruct();
    NodePtr parseEnum(bool isFlags);
    NodePtr parseUnion();
    NodePtr parseBitset();
    NodePtr parseInput();
    NodePtr parseSignal();
    NodePtr parseEvent();
    NodePtr parseGlobal();
    NodePtr parseAsset();
    NodePtr parseImport();
    NodePtr parseUsing();
    NodePtr parseDirective();

    void parseFieldList(std::vector<FieldDecl> &out);
    FieldDecl parseFieldRecovering(bool &ok);
    std::vector<Attribute> parseAttributes();
    TypeExpr parseType();
    TypeExpr parseGenericType(TypeExpr::Kind kind, const std::string &name,
                              const SourceLoc &loc);
    TypeExpr parseSizedStringType(const std::string &name, const SourceLoc &loc);
    SignalParam parseSignalParam();
    EnumValue parseEnumValue();
    UnionMember parseUnionMember();

    bool tryEatEventModifier(EventNode &ev);
};

} // namespace qtn