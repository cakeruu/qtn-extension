#include "parser.h"
#include <cassert>

namespace qtn
{

Parser::Parser(std::vector<Token> tokens, std::string filename)
    : tokens_(std::move(tokens)), file_(std::move(filename))
{
    if (tokens_.empty())
        tokens_.push_back(Token{ TokenKind::Eof, "", { file_, 1, 1 } });
}

const Token &Parser::peek(int offset) const
{
    size_t idx = pos_ + static_cast<size_t>(offset);
    if (idx >= tokens_.size())
        return tokens_.back();
    return tokens_[idx];
}

const Token &Parser::advance()
{
    const Token &t = tokens_[pos_];
    if (pos_ + 1 < tokens_.size())
        ++pos_;
    return t;
}

bool Parser::at(TokenKind k) const { return peek().kind == k; }

bool Parser::eat(TokenKind k)
{
    if (at(k))
    {
        advance();
        return true;
    }
    return false;
}

Token Parser::expect(TokenKind k, std::string_view what)
{
    if (at(k))
        return advance();
    error(peek().loc, std::string("expected ") + std::string(what) + ", got " +
                          std::string(tokenKindName(peek().kind)) + " '" +
                          peek().lexeme + "'");
    return Token{ k, "", peek().loc };
}

std::string Parser::expectIdent(std::string_view what)
{
    const Token &t = peek();
    // Allow keywords to be used as identifiers in name position
    if (t.kind == TokenKind::Ident || t.isKeyword())
    {
        return advance().lexeme;
    }
    error(t.loc, std::string("expected ") + std::string(what) + ", got " +
                     std::string(tokenKindName(t.kind)) + " '" + t.lexeme + "'");
    return "";
}

void Parser::error(const SourceLoc &loc, std::string msg)
{
    diags_.push_back({ DiagLevel::Error, loc, std::move(msg) });
}

void Parser::errorAt(std::string msg) { error(peek().loc, std::move(msg)); }

bool Parser::isTopLevelStart() const
{
    switch (peek().kind)
    {
    case TokenKind::KW_component:
    case TokenKind::KW_singleton:
    case TokenKind::KW_struct:
    case TokenKind::KW_enum:
    case TokenKind::KW_flags:
    case TokenKind::KW_union:
    case TokenKind::KW_bitset:
    case TokenKind::KW_input:
    case TokenKind::KW_signal:
    case TokenKind::KW_event:
    case TokenKind::KW_global:
    case TokenKind::KW_asset:
    case TokenKind::KW_import:
    case TokenKind::KW_using:
    case TokenKind::KW_abstract:
    case TokenKind::KW_synced:
    case TokenKind::KW_server:
    case TokenKind::KW_client:
    case TokenKind::Directive:
        return true;
    default:
        return false;
    }
}

void Parser::synchronize()
{
    while (!at(TokenKind::Eof))
    {
        if (at(TokenKind::Semi))
        {
            advance();
            return;
        }
        if (at(TokenKind::RBrace))
        {
            return;
        }
        if (isTopLevelStart())
        {
            return;
        }
        advance();
    }
}

TranslationUnit Parser::parse()
{
    TranslationUnit tu;
    tu.filename = file_;

    while (!at(TokenKind::Eof))
    {
        size_t prevPos = pos_;
        NodePtr node = parseTopLevel();
        if (node)
            tu.nodes.push_back(std::move(node));
        if (pos_ == prevPos && !at(TokenKind::Eof))
            advance();
    }

    for (auto &d : diags_)
        tu.diags.push_back(std::move(d));
    diags_.clear();
    return tu;
}

NodePtr Parser::parseTopLevel()
{
    const Token &t = peek();

    if (t.kind == TokenKind::Directive)
        return parseDirective();

    switch (t.kind)
    {
    case TokenKind::KW_component:
        return parseComponent(false);
    case TokenKind::KW_singleton:
        return parseComponent(true);
    case TokenKind::KW_struct:
        return parseStruct();
    case TokenKind::KW_enum:
        return parseEnum(false);
    case TokenKind::KW_flags:
        return parseEnum(true);
    case TokenKind::KW_union:
        return parseUnion();
    case TokenKind::KW_bitset:
        return parseBitset();
    case TokenKind::KW_input:
        return parseInput();
    case TokenKind::KW_signal:
        return parseSignal();
    case TokenKind::KW_event:
        return parseEvent();
    case TokenKind::KW_global:
        return parseGlobal();
    case TokenKind::KW_asset:
        return parseAsset();
    case TokenKind::KW_import:
        return parseImport();
    case TokenKind::KW_using:
        return parseUsing();
    case TokenKind::KW_abstract:
    case TokenKind::KW_synced:
    case TokenKind::KW_server:
    case TokenKind::KW_client:
        return parseEvent();
    default:
        error(t.loc, "unexpected token '" + t.lexeme + "' at top level");
        synchronize();
        return nullptr;
    }
}

void Parser::parseFieldList(std::vector<FieldDecl> &out)
{
    while (!at(TokenKind::RBrace) && !at(TokenKind::Eof) && !isTopLevelStart())
    {
        if (at(TokenKind::Directive))
        {
            parseDirective();
            continue;
        }

        size_t prevPos = pos_;
        bool ok = true;
        FieldDecl f = parseFieldRecovering(ok);
        if (ok)
        {
            out.push_back(std::move(f));
        }
        if (pos_ == prevPos)
            advance();
    }
}

FieldDecl Parser::parseFieldRecovering(bool &ok)
{
    FieldDecl f;
    f.loc = peek().loc;
    f.attrs = parseAttributes();
    if (!f.attrs.empty())
        f.loc = peek().loc;

    if (isTopLevelStart() || at(TokenKind::RBrace) || at(TokenKind::Eof))
    {
        ok = false;
        return f;
    }

    f.type = parseType();

    if (isTopLevelStart() || at(TokenKind::RBrace) || at(TokenKind::Eof))
    {
        error(peek().loc, "expected field name");
        ok = false;
        return f;
    }

    SourceLoc nameLoc = peek().loc;
    if (peek().kind == TokenKind::Ident || peek().isKeyword())
    {
        const Token &nameTok = advance();
        f.name = nameTok.lexeme;
        nameLoc = nameTok.loc;
    }
    else
    {
        error(peek().loc, "expected field name");
        ok = false;
        return f;
    }

    if (!at(TokenKind::Semi))
    {
        SourceLoc semiLoc = nameLoc;
        semiLoc.column += static_cast<int>(f.name.size());
        error(semiLoc, "expected ';' after field '" + f.name + "'");
        synchronize();
    }
    else
    {
        advance();
    }

    ok = true;
    return f;
}

NodePtr Parser::parseDirective()
{
    auto node = std::make_unique<DirectiveNode>();
    node->kind = Node::Kind::Directive;
    node->loc = peek().loc;
    node->text = advance().lexeme;
    return node;
}

NodePtr Parser::parseComponent(bool singleton)
{
    auto node = std::make_unique<ComponentNode>();
    node->kind = Node::Kind::Component;
    node->singleton = singleton;
    node->loc = peek().loc;

    if (singleton)
    {
        advance();
        expect(TokenKind::KW_component, "'component'");
    }
    else
    {
        advance();
    }

    node->name = expectIdent("component name");
    expect(TokenKind::LBrace, "'{'");
    parseFieldList(node->fields);
    expect(TokenKind::RBrace, "'}'");
    return node;
}

NodePtr Parser::parseStruct()
{
    auto node = std::make_unique<StructNode>();
    node->kind = Node::Kind::Struct;
    node->loc = peek().loc;
    advance();

    node->name = expectIdent("struct name");
    expect(TokenKind::LBrace, "'{'");
    parseFieldList(node->fields);
    expect(TokenKind::RBrace, "'}'");
    return node;
}

NodePtr Parser::parseEnum(bool isFlags)
{
    auto node = std::make_unique<EnumNode>();
    node->kind = isFlags ? Node::Kind::Flags : Node::Kind::Enum;
    node->isFlags = isFlags;
    node->loc = peek().loc;
    advance();

    node->name = expectIdent("enum name");

    if (eat(TokenKind::Colon))
        node->underlying = expectIdent("underlying type");

    expect(TokenKind::LBrace, "'{'");

    while (!at(TokenKind::RBrace) && !at(TokenKind::Eof) && !isTopLevelStart())
    {
        if (at(TokenKind::Comma))
        {
            advance();
            continue;
        }
        if (!at(TokenKind::Ident) && !peek().isKeyword())
        {
            error(peek().loc, "expected enum value name");
            synchronize();
            break;
        }
        node->values.push_back(parseEnumValue());
        if (!eat(TokenKind::Comma))
            break;
        if (at(TokenKind::RBrace))
            break; // trailing comma
    }

    expect(TokenKind::RBrace, "'}'");
    return node;
}

EnumValue Parser::parseEnumValue()
{
    EnumValue v;
    v.loc = peek().loc;
    v.name = expectIdent("enum value name");
    if (eat(TokenKind::Eq))
    {
        bool negative = false;
        if (!peek().lexeme.empty() && peek().lexeme[0] == '-')
        {
            advance();
            negative = true;
        }
        if (at(TokenKind::IntLit))
        {
            std::string raw = advance().lexeme;
            try
            {
                int64_t val = std::stoll(raw, nullptr, 0);
                v.value = negative ? -val : val;
            }
            catch (...)
            {
                error(peek().loc, "invalid integer value '" + raw + "'");
            }
        }
        else
        {
            error(peek().loc,
                  "expected integer value after '=', got '" + peek().lexeme + "'");
            if (!at(TokenKind::Comma) && !at(TokenKind::RBrace))
                advance();
        }
    }
    return v;
}

NodePtr Parser::parseUnion()
{
    auto node = std::make_unique<UnionNode>();
    node->kind = Node::Kind::Union;
    node->loc = peek().loc;
    advance();

    node->name = expectIdent("union name");
    expect(TokenKind::LBrace, "'{'");

    while (!at(TokenKind::RBrace) && !at(TokenKind::Eof))
    {
        if (isTopLevelStart() && !at(TokenKind::KW_struct))
            break;
        if (at(TokenKind::RBrace) || at(TokenKind::Eof))
            break;
        size_t prevPos = pos_;
        node->members.push_back(parseUnionMember());
        if (pos_ == prevPos)
        {
            advance();
            break;
        }
    }

    expect(TokenKind::RBrace, "'}'");
    return node;
}

UnionMember Parser::parseUnionMember()
{
    UnionMember m;
    m.loc = peek().loc;
    eat(TokenKind::KW_struct);
    m.structName = expectIdent("union member type");
    m.fieldName = expectIdent("union member name");
    if (!at(TokenKind::Semi))
    {
        error(peek().loc, "expected ';' after union member");
        synchronize();
    }
    else
    {
        advance();
    }
    return m;
}

NodePtr Parser::parseBitset()
{
    auto node = std::make_unique<BitsetNode>();
    node->kind = Node::Kind::Bitset;
    node->loc = peek().loc;
    advance();

    expect(TokenKind::LBracket, "'['");

    if (at(TokenKind::IntLit))
    {
        try
        {
            node->bits = std::stoi(advance().lexeme);
        }
        catch (...)
        {
            error(peek().loc, "invalid bitset size");
        }
    }
    else
    {
        error(peek().loc,
              "expected integer bitset size, got '" + peek().lexeme + "'");
        synchronize();
        return node;
    }

    expect(TokenKind::RBracket, "']'");
    node->name = expectIdent("bitset name");
    expect(TokenKind::Semi, "';'");
    return node;
}

NodePtr Parser::parseInput()
{
    auto node = std::make_unique<InputNode>();
    node->kind = Node::Kind::Input;
    node->loc = peek().loc;
    advance();

    expect(TokenKind::LBrace, "'{'");
    parseFieldList(node->fields);
    expect(TokenKind::RBrace, "'}'");
    return node;
}

NodePtr Parser::parseSignal()
{
    auto node = std::make_unique<SignalNode>();
    node->kind = Node::Kind::Signal;
    node->loc = peek().loc;
    advance();

    node->name = expectIdent("signal name");
    expect(TokenKind::LParen, "'('");

    while (!at(TokenKind::RParen) && !at(TokenKind::Eof) && !isTopLevelStart())
    {
        size_t prevPos = pos_;
        node->params.push_back(parseSignalParam());
        if (!eat(TokenKind::Comma))
            break;
        if (pos_ == prevPos)
        {
            advance();
            break;
        }
    }

    expect(TokenKind::RParen, "')'");
    expect(TokenKind::Semi, "';'");
    return node;
}

SignalParam Parser::parseSignalParam()
{
    SignalParam p;
    p.loc = peek().loc;
    p.type = parseType();
    p.name = expectIdent("parameter name");
    return p;
}

bool Parser::tryEatEventModifier(EventNode &ev)
{
    switch (peek().kind)
    {
    case TokenKind::KW_abstract:
        advance();
        ev.isAbstract = true;
        return true;
    case TokenKind::KW_synced:
        advance();
        ev.isSynced = true;
        return true;
    case TokenKind::KW_server:
        advance();
        ev.isServer = true;
        return true;
    case TokenKind::KW_client:
        advance();
        ev.isClient = true;
        return true;
    default:
        return false;
    }
}

NodePtr Parser::parseEvent()
{
    auto node = std::make_unique<EventNode>();
    node->kind = Node::Kind::Event;
    node->loc = peek().loc;

    int safety = 8;
    while (!at(TokenKind::KW_event) && !at(TokenKind::Eof) && safety-- > 0)
    {
        if (!tryEatEventModifier(*node))
        {
            error(peek().loc,
                  "expected event modifier or 'event', got '" + peek().lexeme + "'");
            synchronize();
            return node;
        }
    }

    expect(TokenKind::KW_event, "'event'");
    node->name = expectIdent("event name");

    if (eat(TokenKind::Colon))
        node->base = expectIdent("base event name");

    expect(TokenKind::LBrace, "'{'");
    parseFieldList(node->fields);
    expect(TokenKind::RBrace, "'}'");
    return node;
}

NodePtr Parser::parseGlobal()
{
    auto node = std::make_unique<GlobalNode>();
    node->kind = Node::Kind::Global;
    node->loc = peek().loc;
    advance();

    expect(TokenKind::LBrace, "'{'");
    parseFieldList(node->fields);
    expect(TokenKind::RBrace, "'}'");
    return node;
}

NodePtr Parser::parseAsset()
{
    auto node = std::make_unique<AssetNode>();
    node->kind = Node::Kind::Asset;
    node->loc = peek().loc;
    advance(); // 'asset'

    node->name = expectIdent("asset type name");
    if (!at(TokenKind::Semi))
    {
        error(peek().loc, "expected ';' after asset declaration");
        synchronize();
    }
    else
    {
        advance();
    }
    return node;
}

NodePtr Parser::parseImport()
{
    auto node = std::make_unique<ImportNode>();
    node->kind = Node::Kind::Import;
    node->loc = peek().loc;
    advance();

    if (eat(TokenKind::KW_struct))
    {
        node->isStruct = true;
        node->path = expectIdent("struct name");
        expect(TokenKind::LParen, "'('");
        if (at(TokenKind::IntLit))
        {
            try
            {
                node->structSize = std::stoi(advance().lexeme);
            }
            catch (...)
            {
                error(peek().loc, "invalid struct size");
            }
        }
        else
        {
            error(peek().loc, "expected integer struct size");
        }
        expect(TokenKind::RParen, "')'");
    }
    else
    {
        std::string path = expectIdent("import path");
        while (eat(TokenKind::Dot))
        {
            path += '.';
            path += expectIdent("import path component");
        }
        node->path = path;
    }

    expect(TokenKind::Semi, "';'");
    return node;
}

NodePtr Parser::parseUsing()
{
    auto node = std::make_unique<UsingNode>();
    node->kind = Node::Kind::Using;
    node->loc = peek().loc;
    advance();

    std::string ns = expectIdent("namespace");
    while (eat(TokenKind::Dot))
    {
        ns += '.';
        ns += expectIdent("namespace component");
    }
    node->ns = ns;
    expect(TokenKind::Semi, "';'");
    return node;
}

std::vector<Attribute> Parser::parseAttributes()
{
    std::vector<Attribute> attrs;
    while (at(TokenKind::Attribute))
    {
        Attribute a;
        a.loc = peek().loc;
        a.text = advance().lexeme;
        attrs.push_back(std::move(a));
    }
    return attrs;
}

TypeExpr Parser::parseType()
{
    SourceLoc loc = peek().loc;

    if (eat(TokenKind::KW_button))
    {
        TypeExpr t;
        t.kind = TypeExpr::Kind::Button;
        t.name = "button";
        t.loc = loc;
        return t;
    }

    if (at(TokenKind::KW_list))
        return parseGenericType(TypeExpr::Kind::List, "list", loc);
    if (at(TokenKind::KW_dictionary))
        return parseGenericType(TypeExpr::Kind::Dictionary, "dictionary", loc);
    if (at(TokenKind::KW_hash_set))
        return parseGenericType(TypeExpr::Kind::HashSet, "hash_set", loc);
    if (at(TokenKind::KW_QString))
        return parseSizedStringType("QString", loc);
    if (at(TokenKind::KW_QStringUtf8))
        return parseSizedStringType("QStringUtf8", loc);

    if (at(TokenKind::KW_array))
    {
        advance();
        TypeExpr t;
        t.kind = TypeExpr::Kind::Array;
        t.name = "array";
        t.loc = loc;
        expect(TokenKind::Lt, "'<'");
        t.args.push_back(parseType());
        expect(TokenKind::Gt, "'>'");
        expect(TokenKind::LBracket, "'['");
        if (at(TokenKind::IntLit))
            t.arraySize = advance().lexeme;
        else
            error(peek().loc, "expected array size");
        expect(TokenKind::RBracket, "']'");
        return t;
    }

    if (at(TokenKind::KW_asset_ref))
    {
        advance();
        TypeExpr t;
        t.kind = TypeExpr::Kind::AssetRef;
        t.loc = loc;
        expect(TokenKind::Lt, "'<'");
        t.name = expectIdent("asset type name");
        expect(TokenKind::Gt, "'>'");
        return t;
    }

    // Named type
    TypeExpr t;
    t.kind = TypeExpr::Kind::Named;
    t.loc = loc;
    const Token &tok = peek();
    if (tok.isTypeStart() || tok.kind == TokenKind::Ident)
    {
        t.name = advance().lexeme;
    }
    else
    {
        error(tok.loc, "expected type, got '" + tok.lexeme + "'");
        t.name = "?";
        return t;
    }

    while (at(TokenKind::Star))
    {
        advance();
        TypeExpr w;
        w.kind = TypeExpr::Kind::Pointer;
        w.loc = t.loc;
        w.args.push_back(std::move(t));
        t = std::move(w);
    }

    if (eat(TokenKind::Question))
    {
        TypeExpr w;
        w.kind = TypeExpr::Kind::Nullable;
        w.loc = t.loc;
        w.args.push_back(std::move(t));
        t = std::move(w);
    }

    return t;
}

TypeExpr Parser::parseGenericType(TypeExpr::Kind kind, const std::string &name,
                                  const SourceLoc &loc)
{
    advance();
    TypeExpr t;
    t.kind = kind;
    t.name = name;
    t.loc = loc;

    if (!at(TokenKind::Lt))
    {
        error(peek().loc, "expected '<' after '" + name + "'");
        return t;
    }
    expect(TokenKind::Lt, "'<'");
    t.args.push_back(parseType());
    if (kind == TypeExpr::Kind::Dictionary)
    {
        expect(TokenKind::Comma, "','");
        t.args.push_back(parseType());
    }
    expect(TokenKind::Gt, "'>'");
    return t;
}

TypeExpr Parser::parseSizedStringType(const std::string &name,
                                      const SourceLoc &loc)
{
    advance();

    TypeExpr t;
    t.kind = TypeExpr::Kind::Named;
    t.loc = loc;
    t.name = name;

    if (!at(TokenKind::Lt))
    {
        error(peek().loc, "expected '<' after '" + name + "'");
        return t;
    }

    expect(TokenKind::Lt, "'<'");

    if (at(TokenKind::IntLit))
    {
        const std::string sizeLit = advance().lexeme;
        t.name += "<" + sizeLit + ">";
    }
    else
    {
        error(peek().loc, "expected integer size for '" + name + "'");
        while (!at(TokenKind::Gt) && !at(TokenKind::Eof))
        {
            advance();
        }
    }

    expect(TokenKind::Gt, "'>'");
    return t;
}

} // namespace qtn