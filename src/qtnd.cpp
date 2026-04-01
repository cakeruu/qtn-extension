#include "ast.h"
#include "lexer.h"
#include "parser.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

static void printHelp(std::ostream &out) {
  out << "qtnd - QTN daemon\n"
      << "\n"
      << "Usage:\n"
      << "  qtnd [--stdio | --tcp <port> | --unix <path>]\n"
      << "\n"
      << "Options:\n"
      << "  --stdio      Read requests from stdin, write responses to stdout\n"
      << "  --tcp <port> Listen on 127.0.0.1:<port> (default: 9987)\n"
      << "  --unix <p>   Listen on UNIX domain socket path (not on Windows)\n"
      << "  -h, --help   Show this help\n"
      << "\n"
      << "Protocol (newline-delimited JSON):\n"
      << "  Request:\n"
      << "    "
         "{\"id\":1,\"action\":\"check\",\"filename\":\"foo.qtn\",\"content\":"
         "\"...\"}\n"
      << "    {\"id\":2,\"action\":\"ping\"}\n"
      << "    {\"id\":3,\"action\":\"quit\"}\n"
      << "  Response:\n"
      << "    "
         "{\"id\":1,\"ok\":true,\"hasErrors\":false,\"diagnostics\":[...]}\n"
      << "    {\"id\":2,\"ok\":true,\"pong\":true}\n"
      << "    {\"id\":3,\"ok\":true,\"quit\":true}\n";
}

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sock_t;
#define INVALID_SOCK INVALID_SOCKET
#define CLOSE_SOCK(s) closesocket(s)
#define SOCK_ERRNO WSAGetLastError()
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef int sock_t;
#define INVALID_SOCK (-1)
#define CLOSE_SOCK(s) ::close(s)
#define SOCK_ERRNO errno
#endif

namespace json {

static std::string esc(const std::string &s) {
  std::string o;
  o.reserve(s.size() + 4);
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      o += "\\\"";
      break;
    case '\\':
      o += "\\\\";
      break;
    case '\n':
      o += "\\n";
      break;
    case '\r':
      o += "\\r";
      break;
    case '\t':
      o += "\\t";
      break;
    default:
      if (c < 0x20) {
        char b[8];
        std::snprintf(b, 8, "\\u%04x", c);
        o += b;
      } else
        o += (char)c;
    }
  }
  return o;
}

struct Value {
  enum Type { Null, Bool, Number, String, Object, Array } type = Null;
  std::string str;
  bool b = false;
  std::vector<std::pair<std::string, Value>> obj;
  std::vector<Value> arr;
};

static void skipWS(const std::string &s, size_t &i) {
  while (i < s.size() && std::isspace((unsigned char)s[i]))
    ++i;
}

static Value parseValue(const std::string &s, size_t &i);

static std::string parseString(const std::string &s, size_t &i) {
  ++i;
  std::string out;
  while (i < s.size() && s[i] != '"') {
    if (s[i] == '\\') {
      ++i;
      if (i >= s.size())
        break;
      switch (s[i]) {
      case '"':
        out += '"';
        break;
      case '\\':
        out += '\\';
        break;
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      default:
        out += s[i];
        break;
      }
    } else {
      out += s[i];
    }
    ++i;
  }
  if (i < s.size())
    ++i;
  return out;
}

static Value parseValue(const std::string &s, size_t &i) {
  skipWS(s, i);
  Value v;
  if (i >= s.size())
    return v;
  char c = s[i];
  if (c == '"') {
    v.type = Value::String;
    v.str = parseString(s, i);
  } else if (c == '{') {
    v.type = Value::Object;
    ++i;
    while (i < s.size()) {
      skipWS(s, i);
      if (s[i] == '}') {
        ++i;
        break;
      }
      if (s[i] == ',') {
        ++i;
        continue;
      }
      std::string key = parseString(s, i);
      skipWS(s, i);
      if (i < s.size() && s[i] == ':')
        ++i;
      v.obj.push_back({key, parseValue(s, i)});
    }
  } else if (c == '[') {
    v.type = Value::Array;
    ++i;
    while (i < s.size()) {
      skipWS(s, i);
      if (s[i] == ']') {
        ++i;
        break;
      }
      if (s[i] == ',') {
        ++i;
        continue;
      }
      v.arr.push_back(parseValue(s, i));
    }
  } else if (c == 't') {
    v.type = Value::Bool;
    v.b = true;
    i += 4;
  } else if (c == 'f') {
    v.type = Value::Bool;
    v.b = false;
    i += 5;
  } else if (c == 'n') {
    v.type = Value::Null;
    i += 4;
  } else {
    v.type = Value::Number;
    while (i < s.size() &&
           (std::isdigit((unsigned char)s[i]) || s[i] == '-' || s[i] == '.' ||
            s[i] == 'e' || s[i] == 'E' || s[i] == '+'))
      v.str += s[i++];
  }
  return v;
}

static Value parse(const std::string &s) {
  size_t i = 0;
  return parseValue(s, i);
}

static const Value *get(const Value &obj, const std::string &key) {
  for (auto &kv : obj.obj)
    if (kv.first == key)
      return &kv.second;
  return nullptr;
}

static std::string getString(const Value &obj, const std::string &key,
                             const std::string &def = "") {
  auto *v = get(obj, key);
  if (v && v->type == Value::String)
    return v->str;
  return def;
}

static int getInt(const Value &obj, const std::string &key, int def = 0) {
  auto *v = get(obj, key);
  if (v && v->type == Value::Number)
    return std::stoi(v->str);
  return def;
}

} // namespace json

static qtn::TranslationUnit processSource(const std::string &src,
                                          const std::string &filename) {
  qtn::Lexer lex(src, filename);
  auto tokens = lex.tokenize();

  qtn::TranslationUnit lexTu;
  lexTu.filename = filename;
  for (auto &d : lex.diags())
    lexTu.diags.push_back(d);

  qtn::Parser parser(std::move(tokens), filename);
  auto tu = parser.parse();
  tu.diags.insert(tu.diags.begin(), lexTu.diags.begin(), lexTu.diags.end());
  return tu;
}

static std::string handleRequest(const std::string &line) {
  auto req = json::parse(line);
  if (req.type != json::Value::Object) {
    return "{\"ok\":false,\"error\":\"invalid JSON request\"}\n";
  }

  int id = json::getInt(req, "id", 0);
  std::string action = json::getString(req, "action");

  std::ostringstream resp;
  resp << "{\"id\":" << id << ",";

  if (action == "ping") {
    resp << "\"ok\":true,\"pong\":true}";
  } else if (action == "quit") {
    resp << "\"ok\":true,\"quit\":true}";
  } else if (action == "check") {
    std::string filename = json::getString(req, "filename", "<stdin>");
    std::string content = json::getString(req, "content");

    auto tu = processSource(content, filename);

    resp << "\"ok\":true,\"hasErrors\":" << (tu.hasErrors() ? "true" : "false")
         << ",\"diagnostics\":[\n";

    bool first = true;
    for (const auto &d : tu.diags) {
      if (!first)
        resp << ",\n";
      resp << "  {\"level\":\""
           << (d.level == qtn::DiagLevel::Error ? "error" : "warning")
           << "\",\"file\":\"" << json::esc(d.loc.file) << "\""
           << ",\"line\":" << d.loc.line << ",\"column\":" << d.loc.column
           << ",\"message\":\"" << json::esc(d.message) << "\"}";
      first = false;
    }
    resp << "\n]}";
  } else {
    resp << "\"ok\":false,\"error\":\"unknown action '" << json::esc(action)
         << "'\"}";
  }

  return resp.str() + "\n";
}

static void runStdio() {
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty())
      continue;
    auto resp = handleRequest(line);
    std::cout << resp << std::flush;
    if (resp.find("\"quit\":true") != std::string::npos)
      break;
  }
}

#ifndef _WIN32
static void handleClient(sock_t fd) {
  std::string buf;
  char tmp[4096];
  while (true) {
    ssize_t n = ::recv(fd, tmp, sizeof(tmp) - 1, 0);
    if (n <= 0)
      break;
    tmp[n] = '\0';
    buf += tmp;

    size_t pos;
    while ((pos = buf.find('\n')) != std::string::npos) {
      std::string line = buf.substr(0, pos);
      buf.erase(0, pos + 1);
      if (line.empty())
        continue;
      auto resp = handleRequest(line);
      ::send(fd, resp.data(), resp.size(), 0);
      if (resp.find("\"quit\":true") != std::string::npos) {
        CLOSE_SOCK(fd);
        return;
      }
    }
  }
  CLOSE_SOCK(fd);
}

static void runTCP(int port) {
  sock_t srv = ::socket(AF_INET, SOCK_STREAM, 0);
  if (srv == INVALID_SOCK) {
    perror("socket");
    exit(1);
  }

  int yes = 1;
  ::setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((uint16_t)port);

  if (::bind(srv, (sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
  }
  if (::listen(srv, 8) < 0) {
    perror("listen");
    exit(1);
  }

  std::cerr << "qtnd listening on 127.0.0.1:" << port << "\n";

  while (true) {
    sock_t client = ::accept(srv, nullptr, nullptr);
    if (client == INVALID_SOCK)
      continue;
    handleClient(client);
  }
}

static void runUnix(const std::string &path) {
  sock_t srv = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (srv == INVALID_SOCK) {
    perror("socket");
    exit(1);
  }

  ::unlink(path.c_str());

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

  if (::bind(srv, (sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
  }
  if (::listen(srv, 8) < 0) {
    perror("listen");
    exit(1);
  }

  std::cerr << "qtnd listening on unix:" << path << "\n";

  while (true) {
    sock_t client = ::accept(srv, nullptr, nullptr);
    if (client == INVALID_SOCK)
      continue;
    handleClient(client);
  }
}
#else
static void handleClientWin(SOCKET fd) {
  std::string buf;
  char tmp[4096];
  while (true) {
    int n = recv(fd, tmp, sizeof(tmp) - 1, 0);
    if (n <= 0)
      break;
    tmp[n] = '\0';
    buf += tmp;
    size_t pos;
    while ((pos = buf.find('\n')) != std::string::npos) {
      std::string line = buf.substr(0, pos);
      buf.erase(0, pos + 1);
      if (line.empty())
        continue;
      auto resp = handleRequest(line);
      send(fd, resp.data(), (int)resp.size(), 0);
      if (resp.find("\"quit\":true") != std::string::npos) {
        closesocket(fd);
        return;
      }
    }
  }
  closesocket(fd);
}

static void runTCP(int port) {
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);
  SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((u_short)port);
  if (bind(srv, (sockaddr *)&addr, sizeof(addr)) != 0) {
    std::cerr << "bind failed: " << WSAGetLastError() << "\n";
    exit(1);
  }
  listen(srv, 8);
  std::cerr << "qtnd listening on 127.0.0.1:" << port << "\n";
  while (true) {
    SOCKET c = accept(srv, nullptr, nullptr);
    if (c != INVALID_SOCKET)
      handleClientWin(c);
  }
}
static void runUnix(const std::string &) {
  std::cerr << "Unix sockets not supported on Windows; use --tcp\n";
  exit(1);
}
#endif

int main(int argc, char *argv[]) {
  enum class Mode { Stdio, TCP, Unix } mode = Mode::TCP;
  int port = 9987;
  std::string sockPath;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 ||
        std::strcmp(argv[i], "-h") == 0) {
      printHelp(std::cout);
      return 0;
    } else if (std::strcmp(argv[i], "--stdio") == 0) {
      mode = Mode::Stdio;
    } else if (std::strcmp(argv[i], "--tcp") == 0 && i + 1 < argc) {
      mode = Mode::TCP;
      port = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--unix") == 0 && i + 1 < argc) {
      mode = Mode::Unix;
      sockPath = argv[++i];
    } else {
      std::cerr << "qtnd: unknown argument '" << argv[i] << "'\n"
                << "Try 'qtnd --help' for usage.\n";
      return 1;
    }
  }

  switch (mode) {
  case Mode::Stdio:
    runStdio();
    break;
  case Mode::TCP:
    runTCP(port);
    break;
  case Mode::Unix:
    runUnix(sockPath);
    break;
  }
  return 0;
}