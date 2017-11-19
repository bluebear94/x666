#include <fstream>
#include <iostream>
#include <variant>

#include "Lexer.h"

int main(int argc, char** argv) {
  if (argc == 1) {
    std::cerr << "Please give a file name\n";
    return -1;
  }
  const char* fname = argv[1];
  std::fstream fh(fname);
  x666::LineInfo li;
  while (true) {
    size_t byte = li.byte;
    x666::Token t = getNextToken(fh, li, byte);
    std::cout << t.index() << " ";
    switch (t.index()) {
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
        std::cout << "at line " << (li.line + 1);
        std::cout << " column " << (li.col + 1);
        size_t off = fh.tellg();
        fh.seekg(byte);
        const x666::LexError& le = std::get<6>(t);
        char* s = new char[le.end - le.start + 1];
        fh.read(s, le.end - le.start);
        s[fh.gcount()] = '\0';
        std::cout << " (" << s << ")\n";
        delete[] s;
        fh.seekg(off);
        std::cout << x666::lexErrorMessages[(int) le.c] << "\n";
      }
    }
    if (t.index() == 5) break;
    else if (t.index() != 6) {
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
  }
  return 0;
}