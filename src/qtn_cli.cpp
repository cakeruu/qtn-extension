#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "reporter.h"


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>


static void printHelp(std::ostream &out)
{
    out
        << "qtn - QTN file checker\n"
        << "\n"
        << "Usage:\n"
        << "  qtn [options] <file.qtn> [<file.qtn> ...]\n"
        << "\n"
        << "Options:\n"
        << "  --json       Print diagnostics as JSON array\n"
        << "  --no-color   Disable ANSI colors in text output\n"
        << "  -h, --help   Show this help\n"
        << "\n"
        << "Exit codes:\n"
        << "  0  No errors found\n"
        << "  1  Parse/lex errors found, bad args, or unreadable file\n"
        << "\n"
        << "Examples:\n"
        << "  qtn good.qtn\n"
        << "  qtn --json good.qtn bad.qtn\n"
        << "  qtn --no-color bad.qtn\n";
}

static std::string escJson(const std::string &s)
{
    std::string o;
    o.reserve(s.size() + 4);
    for (unsigned char c : s)
    {
        if (c == '"')
            o += "\\\"";
        else if (c == '\\')
            o += "\\\\";
        else if (c == '\n')
            o += "\\n";
        else if (c == '\r')
            o += "\\r";
        else if (c == '\t')
            o += "\\t";
        else if (c < 0x20)
        {
            char b[8];
            std::snprintf(b, 8, "\\u%04x", c);
            o += b;
        }
        else
            o += (char)c;
    }
    return o;
}

static std::string readFile(const std::string &path, bool &ok)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f)
    {
        ok = false;
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    ok = true;
    return ss.str();
}

static qtn::TranslationUnit processSource(const std::string &src, const std::string &filename)
{
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

int main(int argc, char *argv[])
{
    bool jsonMode = false;
    bool color = true;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--json") == 0)
        {
            jsonMode = true;
            continue;
        }
        if (std::strcmp(argv[i], "--no-color") == 0)
        {
            color = false;
            continue;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
        {
            printHelp(std::cout);
            return 0;
        }
        if (argv[i][0] == '-')
        {
            std::cerr << "qtn: unknown option '" << argv[i] << "'\n";
            std::cerr << "Try 'qtn --help' for usage.\n";
            return 1;
        }
        files.push_back(argv[i]);
    }

    if (files.empty())
    {
        printHelp(std::cerr);
        return 1;
    }

    int exitCode = 0;

    if (jsonMode)
    {
        bool first = true;
        std::cout << "[\n";
        for (const auto &path : files)
        {
            bool ok;
            auto src = readFile(path, ok);
            if (!ok)
            {
                if (!first)
                    std::cout << ",\n";
                std::cout << "  {\"level\":\"error\",\"file\":\"" << escJson(path)
                          << "\",\"line\":0,\"column\":0,"
                          << "\"message\":\"cannot open file\"}";
                first = false;
                exitCode = 1;
                continue;
            }
            auto tu = processSource(src, path);
            if (tu.hasErrors())
                exitCode = 1;

            for (const auto &d : tu.diags)
            {
                if (!first)
                    std::cout << ",\n";
                std::cout << "  {\n"
                          << "    \"level\": \"" << (d.level == qtn::DiagLevel::Error ? "error" : "warning") << "\",\n"
                          << "    \"file\": \"" << escJson(d.loc.file) << "\",\n"
                          << "    \"line\": " << d.loc.line << ",\n"
                          << "    \"column\": " << d.loc.column << ",\n"
                          << "    \"message\": \"" << escJson(d.message) << "\"\n"
                          << "  }";
                first = false;
            }
        }
        std::cout << "\n]\n";
    }
    else
    {
        for (const auto &path : files)
        {
            bool ok;
            auto src = readFile(path, ok);
            if (!ok)
            {
                std::cerr << "qtn: cannot open '" << path << "'\n";
                exitCode = 1;
                continue;
            }
            auto tu = processSource(src, path);
            if (tu.hasErrors())
                exitCode = 1;
            qtn::printDiagnostics(tu, std::cerr, color);
            if (!tu.diags.empty())
                std::cerr << path << ": " << qtn::diagSummary(tu) << "\n";
        }
    }

    return exitCode;
}