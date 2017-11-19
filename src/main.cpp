#include <fstream>
#include <iostream>
#include <variant>

#include "Lexer.h"
#include "Parser.h"

int main(int argc, char** argv) {
  if (argc == 1) {
    std::cerr << "Please give a file name\n";
    return -1;
  }
  const char* fname = argv[1];
  std::fstream fh(fname);
  x666::Parser p(&fh);
  p.parse();
  if (p.errorLog.empty()) {
    std::cout << "Compilation succeeded\n";
    for (const x666::ExpressionPtr& ex : p.expressions) {
      ex->trace();
      std::cout << "\n";
    }
  } else {
    std::cout << "Parsing failed:\n";
    for (const x666::LexError& le : p.errorLog) {
      le.print(fh);
    }
  }
  return 0;
}