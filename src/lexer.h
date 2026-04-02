#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace qtn
{

struct SourceLoc
{
    std::string file;
    int line = 1;
    int column = 1;

    std::string toString() const;
};

enum class TokenKind
{
    Ident,
    IntLit,
    FloatLit,
    StringLit,

    LBrace,   // {
    RBrace,   // }
    LParen,   // (
    RParen,   // )
    LBracket, // [
    RBracket, // ]
    Semi,     // ;
    Colon,    // :
    Comma,    // ,
    Dot,      // .
    Star,     // *
    Eq,       // =
    Lt,       // <
    Gt,       // >
    Question, // ?

    KW_component,
    KW_singleton,
    KW_struct,
    KW_enum,
    KW_flags,
    KW_union,
    KW_bitset,
    KW_input,
    KW_signal,
    KW_event,
    KW_global,
    KW_asset,
    KW_import,
    KW_using,
    KW_abstract,
    KW_synced,
    KW_server,
    KW_client,
    KW_local,
    KW_remote,
    KW_nothashed,
    KW_button,
    KW_list,
    KW_dictionary,
    KW_hash_set,
    KW_array,
    KW_entity_ref,
    KW_player_ref,
    KW_asset_ref,
    KW_nullable, // T?
    KW_bool,
    KW_byte,
    KW_sbyte,
    KW_short,
    KW_ushort,
    KW_int,
    KW_uint,
    KW_long,
    KW_ulong,
    KW_FP,
    KW_FPVector2,
    KW_FPVector3,
    KW_FPMatrix,
    KW_FPQuaternion,
    KW_LayerMask,
    KW_QString,
    KW_QStringUtf8,

    Directive,
    Attribute,

    Eof,
    Unknown,
};

std::string_view tokenKindName(TokenKind k);

struct Token
{
    TokenKind kind = TokenKind::Unknown;
    std::string lexeme; // exact source text
    SourceLoc loc;

    bool is(TokenKind k) const { return kind == k; }
    bool isKeyword() const;
    bool isTypeStart() const;
};

enum class DiagLevel
{
    Warning,
    Error
};

struct Diagnostic
{
    DiagLevel level;
    SourceLoc loc;
    std::string message;

    std::string toString() const;
};

class Lexer
{
  public:
    explicit Lexer(std::string source, std::string filename = "<input>");

    std::vector<Token> tokenize();

    const std::vector<Diagnostic> &diags() const { return diags_; }

  private:
    std::string src_;
    std::string file_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    std::vector<Diagnostic> diags_;

    char peek(int offset = 0) const;
    char advance();
    void skipWhitespaceAndComments();
    Token makeToken(TokenKind k, std::string lexeme, SourceLoc loc);
    Token lexDirective(SourceLoc loc);
    Token lexAttribute(SourceLoc loc);
    Token lexIdent(SourceLoc loc);
    Token lexNumber(SourceLoc loc);
    Token lexString(SourceLoc loc);
    void emitError(SourceLoc loc, std::string msg);

    SourceLoc here() const { return { file_, line_, col_ }; }
};

} // namespace qtn