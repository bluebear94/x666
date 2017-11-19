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
  x666::LineInfo li;
  x666::Parser p(&fh);
  while (true) {
    size_t byte = li.byte;
    x666::Token t = getNextToken(fh, li, byte);
    size_t index = t.index();
    std::cout << index << " ";
    switch (index) {
      case 0: {
        std::cout << "Identifier " << std::get<0>(t).name;
        break;
      }
      case 1: {
        std::cout << "String literal \"" << std::get<1>(t).str << "\"";
        break;
      }
      case 2: {
        std::cout << "Integer literal " << std::get<2>(t).n;
        break;
      }
      case 3: {
        std::cout << "Operator " << (int) std::get<3>(t);
        break;
      }
      case 4: {
        std::cout << "Newline";
        break;
      }
      case 5: {
        std::cout << "End of file.\n";
        break;
      }
      case 6: {
        std::cout << "** ERROR ENCOUNTERED BY LEXING **\n";
        const x666::LexError& le = std::get<6>(t);
        le.print(fh);
      }
    }
    if (index != 6 && index != 5) {
      std::cout << " @ bytes " << byte << " -- " << li.byte;
      size_t off = fh.tellg();
      fh.seekg(byte);
      char* s = new char[li.byte - byte];
      fh.read(s, li.byte - byte);
      s[fh.gcount()] = '\0';
      std::cout << " (" << s << ")\n";
      delete[] s;
      fh.seekg(off);
    }
    p.acceptToken(std::move(t));
    if (index == 5) break;
  }
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