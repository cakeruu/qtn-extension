#include "reporter.h"
#include <cstdio>
#include <sstream>

namespace qtn
{

namespace
{
const char *kReset = "\033[0m";
const char *kBold = "\033[1m";
const char *kRed = "\033[31m";
const char *kYellow = "\033[33m";
const char *kCyan = "\033[36m";

std::string escape_json_string(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else
            {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}
} // namespace

void printDiagnostics(const TranslationUnit &tu, std::ostream &out,
                      bool color)
{
    for (const auto &d : tu.diags)
    {
        // file:line:col: level: message
        if (color)
        {
            out << kBold << d.loc.file << ':' << d.loc.line << ':' << d.loc.column
                << ':' << kReset;
            if (d.level == DiagLevel::Error)
                out << ' ' << kBold << kRed << "error:" << kReset;
            else
                out << ' ' << kBold << kYellow << "warning:" << kReset;
        }
        else
        {
            out << d.loc.file << ':' << d.loc.line << ':' << d.loc.column << ':';
            out << (d.level == DiagLevel::Error ? " error:" : " warning:");
        }
        out << ' ' << d.message << '\n';
    }
}

std::string diagsToJson(const TranslationUnit &tu)
{
    std::ostringstream os;
    os << "[\n";
    for (size_t i = 0; i < tu.diags.size(); ++i)
    {
        const auto &d = tu.diags[i];
        os << "  {\n";
        os << "    \"level\": \""
           << (d.level == DiagLevel::Error ? "error" : "warning") << "\",\n";
        os << "    \"file\": \"" << escape_json_string(d.loc.file) << "\",\n";
        os << "    \"line\": " << d.loc.line << ",\n";
        os << "    \"column\": " << d.loc.column << ",\n";
        os << "    \"message\": \"" << escape_json_string(d.message) << "\"\n";
        os << "  }";
        if (i + 1 < tu.diags.size())
            os << ',';
        os << '\n';
    }
    os << "]";
    return os.str();
}

std::string diagSummary(const TranslationUnit &tu)
{
    int errors = 0, warnings = 0;
    for (const auto &d : tu.diags)
    {
        if (d.level == DiagLevel::Error)
            ++errors;
        else
            ++warnings;
    }
    return std::to_string(errors) + " error(s), " + std::to_string(warnings) +
           " warning(s)";
}

} // namespace qtn