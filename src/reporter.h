#pragma once
#include "ast.h"
#include <ostream>
#include <string>

namespace qtn {

void printDiagnostics(const TranslationUnit &tu, std::ostream &out,
                      bool color = true);

std::string diagsToJson(const TranslationUnit &tu);

std::string diagSummary(const TranslationUnit &tu);

} // namespace qtn