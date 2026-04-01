#include "lexer.h"
#include <cctype>
#include <unordered_map>

namespace qtn {

std::string SourceLoc::toString() const {
  return file + ":" + std::to_string(line) + ":" + std::to_string(column);
}

std::string Diagnostic::toString() const {
  std::string prefix = (level == DiagLevel::Error) ? "error" : "warning";
  return loc.toString() + ": " + prefix + ": " + message;
}

std::string_view tokenKindName(TokenKind k) {
  switch (k) {
  case TokenKind::Ident:
    return "identifier";
  case TokenKind::IntLit:
    return "integer literal";
  case TokenKind::FloatLit:
    return "float literal";
  case TokenKind::StringLit:
    return "string literal";
  case TokenKind::LBrace:
    return "'{'";
  case TokenKind::RBrace:
    return "'}'";
  case TokenKind::LParen:
    return "'('";
  case TokenKind::RParen:
    return "')'";
  case TokenKind::LBracket:
    return "'['";
  case TokenKind::RBracket:
    return "']'";
  case TokenKind::Semi:
    return "';'";
  case TokenKind::Colon:
    return "':'";
  case TokenKind::Comma:
    return "','";
  case TokenKind::Dot:
    return "'.'";
  case TokenKind::Star:
    return "'*'";
  case TokenKind::Eq:
    return "'='";
  case TokenKind::Lt:
    return "'<'";
  case TokenKind::Gt:
    return "'>'";
  case TokenKind::Question:
    return "'?'";
  case TokenKind::Directive:
    return "directive";
  case TokenKind::Attribute:
    return "attribute";
  case TokenKind::Eof:
    return "<eof>";
  default:
    return "keyword";
  }
}

bool Token::isKeyword() const {
  return kind >= TokenKind::KW_component && kind <= TokenKind::KW_QStringUtf8;
}

bool Token::isTypeStart() const {
  switch (kind) {
  case TokenKind::Ident:
  case TokenKind::KW_bool:
  case TokenKind::KW_byte:
  case TokenKind::KW_sbyte:
  case TokenKind::KW_short:
  case TokenKind::KW_ushort:
  case TokenKind::KW_int:
  case TokenKind::KW_uint:
  case TokenKind::KW_long:
  case TokenKind::KW_ulong:
  case TokenKind::KW_FP:
  case TokenKind::KW_FPVector2:
  case TokenKind::KW_FPVector3:
  case TokenKind::KW_FPMatrix:
  case TokenKind::KW_FPQuaternion:
  case TokenKind::KW_LayerMask:
  case TokenKind::KW_QString:
  case TokenKind::KW_QStringUtf8:
  case TokenKind::KW_entity_ref:
  case TokenKind::KW_player_ref:
  case TokenKind::KW_asset_ref:
  case TokenKind::KW_list:
  case TokenKind::KW_dictionary:
  case TokenKind::KW_hash_set:
  case TokenKind::KW_array:
  case TokenKind::KW_button:
  case TokenKind::KW_struct:
    return true;
  default:
    return false;
  }
}

static const std::unordered_map<std::string, TokenKind> kKeywords = {
    {"component", TokenKind::KW_component},
    {"singleton", TokenKind::KW_singleton},
    {"struct", TokenKind::KW_struct},
    {"enum", TokenKind::KW_enum},
    {"flags", TokenKind::KW_flags},
    {"union", TokenKind::KW_union},
    {"bitset", TokenKind::KW_bitset},
    {"input", TokenKind::KW_input},
    {"signal", TokenKind::KW_signal},
    {"event", TokenKind::KW_event},
    {"global", TokenKind::KW_global},
    {"asset", TokenKind::KW_asset},
    {"import", TokenKind::KW_import},
    {"using", TokenKind::KW_using},
    {"abstract", TokenKind::KW_abstract},
    {"synced", TokenKind::KW_synced},
    {"server", TokenKind::KW_server},
    {"client", TokenKind::KW_client},
    {"local", TokenKind::KW_local},
    {"remote", TokenKind::KW_remote},
    {"nothashed", TokenKind::KW_nothashed},
    {"button", TokenKind::KW_button},
    {"list", TokenKind::KW_list},
    {"List", TokenKind::KW_list},
    {"dictionary", TokenKind::KW_dictionary},
    {"Dictionary", TokenKind::KW_dictionary},
    {"hash_set", TokenKind::KW_hash_set},
    {"HashSet", TokenKind::KW_hash_set},
    {"array", TokenKind::KW_array},
    {"Array", TokenKind::KW_array},
    {"entity_ref", TokenKind::KW_entity_ref},
    {"EntityRef", TokenKind::KW_entity_ref},
    {"player_ref", TokenKind::KW_player_ref},
    {"PlayerRef", TokenKind::KW_player_ref},
    {"asset_ref", TokenKind::KW_asset_ref},
    {"AssetRef", TokenKind::KW_asset_ref},
    {"bool", TokenKind::KW_bool},
    {"Bool", TokenKind::KW_bool},
    {"Boolean", TokenKind::KW_bool},
    {"byte", TokenKind::KW_byte},
    {"Byte", TokenKind::KW_byte},
    {"sbyte", TokenKind::KW_sbyte},
    {"SByte", TokenKind::KW_sbyte},
    {"short", TokenKind::KW_short},
    {"Short", TokenKind::KW_short},
    {"ushort", TokenKind::KW_ushort},
    {"UShort", TokenKind::KW_ushort},
    {"int", TokenKind::KW_int},
    {"Int", TokenKind::KW_int},
    {"Int32", TokenKind::KW_int},
    {"uint", TokenKind::KW_uint},
    {"UInt", TokenKind::KW_uint},
    {"UInt32", TokenKind::KW_uint},
    {"long", TokenKind::KW_long},
    {"Long", TokenKind::KW_long},
    {"ulong", TokenKind::KW_ulong},
    {"ULong", TokenKind::KW_ulong},
    {"UInt64", TokenKind::KW_ulong},
    {"FP", TokenKind::KW_FP},
    {"FPVector2", TokenKind::KW_FPVector2},
    {"FPVector3", TokenKind::KW_FPVector3},
    {"FPMatrix", TokenKind::KW_FPMatrix},
    {"FPQuaternion", TokenKind::KW_FPQuaternion},
    {"LayerMask", TokenKind::KW_LayerMask},
    {"QString", TokenKind::KW_QString},
    {"QStringUtf8", TokenKind::KW_QStringUtf8},
};

Lexer::Lexer(std::string source, std::string filename)
    : src_(std::move(source)), file_(std::move(filename)) {}

char Lexer::peek(int offset) const {
  size_t idx = pos_ + static_cast<size_t>(offset);
  if (idx >= src_.size())
    return '\0';
  return src_[idx];
}

char Lexer::advance() {
  char c = src_[pos_++];
  if (c == '\n') {
    ++line_;
    col_ = 1;
  } else {
    ++col_;
  }
  return c;
}

void Lexer::emitError(SourceLoc loc, std::string msg) {
  diags_.push_back({DiagLevel::Error, std::move(loc), std::move(msg)});
}

Token Lexer::makeToken(TokenKind k, std::string lexeme, SourceLoc loc) {
  return Token{k, std::move(lexeme), std::move(loc)};
}

void Lexer::skipWhitespaceAndComments() {
  while (pos_ < src_.size()) {
    char c = peek();
    if (std::isspace(static_cast<unsigned char>(c))) {
      advance();
    } else if (c == '/' && peek(1) == '/') {
      while (pos_ < src_.size() && peek() != '\n')
        advance();
    } else if (c == '/' && peek(1) == '*') {
      SourceLoc start = here();
      advance();
      advance();
      bool closed = false;
      while (pos_ + 1 < src_.size()) {
        if (peek() == '*' && peek(1) == '/') {
          advance();
          advance();
          closed = true;
          break;
        }
        advance();
      }
      if (!closed) {
        emitError(start, "unterminated block comment");
        pos_ = src_.size();
      }
    } else {
      break;
    }
  }
}

Token Lexer::lexDirective(SourceLoc loc) {
  std::string text = "#";
  while (pos_ < src_.size() && peek() != '\n') {
    text += advance();
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    text.pop_back();
  return makeToken(TokenKind::Directive, text, loc);
}

Token Lexer::lexAttribute(SourceLoc loc) {
  std::string text = "[";
  int bracketDepth = 1;
  int parenDepth = 0;
  bool inString = false;

  advance();
  while (pos_ < src_.size()) {
    char c = advance();
    text += c;

    if (inString) {
      if (c == '\\' && pos_ < src_.size()) {
        text += advance();
      } else if (c == '"') {
        inString = false;
      }
      continue;
    }

    switch (c) {
    case '"':
      inString = true;
      break;
    case '(':
      ++parenDepth;
      break;
    case ')':
      --parenDepth;
      break;
    case '[':
      ++bracketDepth;
      break;
    case ']':
      --bracketDepth;
      if (bracketDepth == 0)
        return makeToken(TokenKind::Attribute, text, loc);
      break;
    case '\n':
      if (parenDepth == 0 && bracketDepth <= 1) {
        emitError(loc, "unterminated attribute");
        return makeToken(TokenKind::Attribute, text, loc);
      }
      break;
    default:
      break;
    }
  }
  emitError(loc, "unterminated attribute");
  return makeToken(TokenKind::Attribute, text, loc);
}

Token Lexer::lexIdent(SourceLoc loc) {
  std::string text;
  while (pos_ < src_.size()) {
    char c = peek();
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      text += advance();
    } else {
      break;
    }
  }
  auto it = kKeywords.find(text);
  if (it != kKeywords.end())
    return makeToken(it->second, text, loc);
  return makeToken(TokenKind::Ident, text, loc);
}

Token Lexer::lexNumber(SourceLoc loc) {
  std::string text;
  bool isFloat = false;

  if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
    text += advance();
    text += advance();
    while (pos_ < src_.size() &&
           std::isxdigit(static_cast<unsigned char>(peek())))
      text += advance();
    return makeToken(TokenKind::IntLit, text, loc);
  }

  while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(peek())))
    text += advance();

  if (pos_ < src_.size() && peek() == '.' &&
      std::isdigit(static_cast<unsigned char>(peek(1)))) {
    isFloat = true;
    text += advance();
    while (pos_ < src_.size() &&
           std::isdigit(static_cast<unsigned char>(peek())))
      text += advance();
  }
  if (pos_ < src_.size() && (peek() == 'f' || peek() == 'F')) {
    text += advance();
    isFloat = true;
  }
  return makeToken(isFloat ? TokenKind::FloatLit : TokenKind::IntLit, text,
                   loc);
}

Token Lexer::lexString(SourceLoc loc) {
  std::string text = "\"";
  advance();
  while (pos_ < src_.size()) {
    char c = peek();
    if (c == '"') {
      text += advance();
      break;
    }
    if (c == '\\') {
      text += advance();
      if (pos_ < src_.size())
        text += advance();
      continue;
    }
    if (c == '\n') {
      emitError(loc, "unterminated string literal");
      break;
    }
    text += advance();
  }
  return makeToken(TokenKind::StringLit, text, loc);
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> result;

  while (true) {
    skipWhitespaceAndComments();
    if (pos_ >= src_.size()) {
      result.push_back(makeToken(TokenKind::Eof, "", here()));
      break;
    }

    SourceLoc loc = here();
    char c = peek();

    if (c == '#') {
      advance();
      result.push_back(lexDirective(loc));
      continue;
    }

    if (c == '[') {
      char next = peek(1);
      if (std::isalpha(static_cast<unsigned char>(next)) || next == '_') {
        result.push_back(lexAttribute(loc));
        continue;
      }
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      result.push_back(lexIdent(loc));
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
      result.push_back(lexNumber(loc));
      continue;
    }

    if (c == '"') {
      result.push_back(lexString(loc));
      continue;
    }

    advance();
    switch (c) {
    case '{':
      result.push_back(makeToken(TokenKind::LBrace, "{", loc));
      break;
    case '}':
      result.push_back(makeToken(TokenKind::RBrace, "}", loc));
      break;
    case '(':
      result.push_back(makeToken(TokenKind::LParen, "(", loc));
      break;
    case ')':
      result.push_back(makeToken(TokenKind::RParen, ")", loc));
      break;
    case '[':
      result.push_back(makeToken(TokenKind::LBracket, "[", loc));
      break;
    case ']':
      result.push_back(makeToken(TokenKind::RBracket, "]", loc));
      break;
    case ';':
      result.push_back(makeToken(TokenKind::Semi, ";", loc));
      break;
    case ':':
      result.push_back(makeToken(TokenKind::Colon, ":", loc));
      break;
    case ',':
      result.push_back(makeToken(TokenKind::Comma, ",", loc));
      break;
    case '.':
      result.push_back(makeToken(TokenKind::Dot, ".", loc));
      break;
    case '*':
      result.push_back(makeToken(TokenKind::Star, "*", loc));
      break;
    case '=':
      result.push_back(makeToken(TokenKind::Eq, "=", loc));
      break;
    case '<':
      result.push_back(makeToken(TokenKind::Lt, "<", loc));
      break;
    case '>':
      result.push_back(makeToken(TokenKind::Gt, ">", loc));
      break;
    case '?':
      result.push_back(makeToken(TokenKind::Question, "?", loc));
      break;
    default:
      emitError(loc, std::string("unexpected character '") + c + "'");
      break;
    }
  }

  return result;
}

} // namespace qtn