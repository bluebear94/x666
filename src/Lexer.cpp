#include "Lexer.h"

#include <ctype.h>

#include <iostream>
#include <limits>

namespace x666 {
  const char* lexErrorMessages[] = {
    "Integer is too big to fit type",
    "Unknown operator",
    "Operator doesn't belong in an expression",
    "Multiple expressions on a line",
    "Left operand missing",
    "Right operand missing",
    "Mismatched brackets",
    "This statement needs an expression after it",
    "This statement doesn't take an expression but got one",
  };
  const char* opsAsStrings[] = {
    "(", ")", "[", "]",
    "+", "-", "*", "/", "%",
    "~", "<-", "=", "<", ">",
    "/=", "<=", ">=", "??", "?&",
    "!!", "&>", "?", ":", "@", "@@",
    "@#", "!", "&", "|", "|*", "#", ",",
    "#>"
  };
  static int getChar(std::istream& fh, LineInfo& li) {
    int c = fh.get();
    if (c == '\n') {
      ++li.line;
      li.col = 0;
    } else {
      ++li.col;
    }
    ++li.byte;
    return c;
  }
  int getDigit(char c) {
    char cap = toupper(c);
    return isdigit(c) ? (c - '0') :
      isupper(cap) ? (cap - 'A' + 10) : -1;
  }
  // Check if (a * n + b) would overflow.
  // NEEDS a != 0
  bool wouldMAddOverflow(int32_t a, int64_t n, int32_t b) {
#if defined(__GNUC__) || defined(__GNUG__)
    // Use compiler intrinsics for checks
    return
      __builtin_smull_overflow(a, n, &n) ||
      __builtin_saddl_overflow(n, b, &n);
#else
    // First, check the result of (a * n)
    if (n > std::numeric_limits<int64_t>::max() / a) return true;
    if (n < std::numeric_limits<int64_t>::min() / a) return true;
    int64_t axn = a * n;
    if (b > 0 && n > std::numeric_limits<int64_t>::max() - b) return true;
    if (b < 0 && n > std::numeric_limits<int64_t>::min() - b) return true;
    return false;
#endif
  }
  std::string parseStringLiteral(std::istream& fh, LineInfo& li) {
    std::string s;
    while (true) {
      int c = getChar(fh, li);
      if (c == '\n' ||c == std::char_traits<char>::eof() || c == '\x22') {
        break;
      } else if (c == '\\') {
        // Escape sequence
        c = getChar(fh, li);
        switch (c) {
          case 'n': s += '\n'; break;
          case '\\': s += '\\'; break;
          case '\x22': s += '\x22'; break;
          default: s += c;
        }
      } else {
        s += (char) c;
      }
    }
    return s;
  }
  std::string unescape(const std::string& s) {
    std::string res;
    for (char c : s) {
      if (c == '\n') res += "\\n";
      else if (c == '\\') res += "\\\\";
      else if (c == '\x22') res += "\\\"";
      else res += c;
    }
    return res;
  }
  Token getNextToken(std::istream& fh, LineInfo& li) {
    int c;
    do {
      c = getChar(fh, li);
      if (c == '\n' || c == ';') return Newline();
      if (c == '#') {
        if (fh.peek() == '#') {
          // Comment syntax (tentative)
          fh.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
          li.col = 0;
          ++li.line;
          li.byte = fh.tellg();
          li.sot = li.byte;
          return Newline();
        }
      }
    } while (iswspace(c));
    if (fh.fail()) return EndOfFile();
    li.sot = li.byte - 1;
    bool negative = false;
    if (c == '-') { // Negative integers are handled specially
      c = fh.peek();
      if (isdigit(c)) {
        getChar(fh, li);
        negative = true;
      } else {
        return Operator::minus;
      }
    }
    if (isdigit(c)) {
      // This starts a numeric literal.
      int64_t n = (c - '0');
      size_t m = (n == 0) ? 0 : 1;
      if (negative) n = -n;
      size_t base = 10;
      while (true) {
        c = fh.peek();
        if (c == std::char_traits<char>::eof()) break;
        // Treat 0(h|d|o|b) specially
        // (We check for m instead of using n directly)
        // because we don't want "00h", for instance, matching...
        if (m == 0) {
          if (c == 'h') base = 16;
          else if (c == 'd') base = 12;
          else if (c == 'o') base = 8;
          else if (c == 'b') base = 2;
          m = 1;
          if (base != 10) {
            getChar(fh, li);
            continue;
          }
        }
        // If not a decimal digit or a letter within base bounds,
        // stop scanning
        int digit = getDigit(c);
        if (digit < 0 || (size_t) digit >= base) {
          break;
        }
        if (negative) digit = -digit;
        if (wouldMAddOverflow(base, n, digit)) {
          return LexError(LexErrorCode::integerOverflow, li);
        }
        n = base * n + digit;
        getChar(fh, li);
      }
      return IntLiteral(n);
    } else if (isalpha(c)) {
      // This starts an identifier.
      Identifier id((char) c);
      while (true) {
        c = fh.peek();
        if (c == std::char_traits<char>::eof()) break;
        if (!isalpha(c)) break;
        id.name += (char) c;
        getChar(fh, li);
      }
      return id;
    } else {
      switch (c) {
        case '+': return Operator::plus;
        case '*': return Operator::times;
        case '%': return Operator::modulo;
        case '~': return Operator::concat;
        case '(': return Operator::leftBracket;
        case ')': return Operator::rightBracket;
        case '[': return Operator::leftSBracket;
        case ']': return Operator::rightSBracket;
        case '=': return Operator::equal;
        case ':': return Operator::colon;
        case ',': return Operator::comma;
        case '#': {
          int c = fh.peek();
          if (c == '>') {
            getChar(fh, li);
            return Operator::print;
          }
          return Operator::length;
        }
        case '/': {
          if (fh.peek() == '=') {
            getChar(fh, li);
            return Operator::notEqual;
          }
          return Operator::divide;
        }
        case '<': {
          int c = fh.peek();
          if (c == '=') {
            getChar(fh, li);
            return Operator::lessEqual;
          } else if (c == '-') {
            getChar(fh, li);
            return Operator::assign;
          }
          return Operator::less;
        }
        case '>': {
          int c = fh.peek();
          if (c == '=') {
            getChar(fh, li);
            return Operator::greaterEqual;
          }
          return Operator::greater;
        }
        case '?': {
          int c = fh.peek();
          if (c == '?') {
            getChar(fh, li);
            return Operator::ifStmt;
          } else if (c == '&') {
            getChar(fh, li);
            return Operator::ifThenStmt;
          }
          return Operator::questionMark;
        }
        case '@': {
          int c = fh.peek();
          if (c == '#') {
            getChar(fh, li);
            return Operator::forStmt;
          } else if (c == '@') {
            getChar(fh, li);
            return Operator::repeatStmt;
          }
          return Operator::whileStmt;
        }
        case '&': {
          int c = fh.peek();
          if (c == '>') {
            getChar(fh, li);
            return Operator::endStmt;
          }
          return Operator::andStmt;
        }
        case '|': {
          int c = fh.peek();
          if (c == '*') {
            getChar(fh, li);
            return Operator::xorStmt;
          }
          return Operator::orStmt;
        }
        case '!': {
          int c = fh.peek();
          if (c == '!') {
            getChar(fh, li);
            return Operator::elseStmt;
          }
          return Operator::notStmt;
        }
        case '\x22': return StringLiteral(parseStringLiteral(fh, li));
      }
    }
    return LexError(LexErrorCode::unknownOperator, li);
  }
  void LexError::print(std::istream& fh) const {
    std::cout << "Error at line " << (li.line + 1);
    std::cout << " column " << (li.col + 1) << ": ";
    std::cout << lexErrorMessages[(int) c] << "\n";
    fh.clear();
    size_t off = fh.tellg();
    size_t lineend = li.byte;
    fh.seekg(lineend);
    while (true) {
      int c = fh.get();
      ++lineend;
      if (c == std::char_traits<char>::eof() || c == '\n')
        break;
    }
    fh.clear();
    size_t linestart = li.sot;
    fh.seekg(linestart);
    while (linestart > 0) {
      --linestart;
      fh.seekg(linestart);
      if (fh.peek() == '\n') {
        ++linestart;
        fh.get();
        break;
      }
    }
    // We should now be at the start of the first line.
    ssize_t lengthOfSnakeSigned = li.byte - li.sot + 1;
    fh.seekg(linestart);
    while (true) {
      std::string s;
      std::getline(fh, s);
      std::cout << s << "\n";
      if ((size_t) fh.tellg() >= lineend) break;
    }
    size_t lengthOfSnake = abs(lengthOfSnakeSigned);
    if (lengthOfSnakeSigned <= 0) {
      if (lengthOfSnake > li.col) lengthOfSnake = li.col;
      std::cout << std::string(li.col - lengthOfSnake, ' ');
      std::cout << std::string(lengthOfSnake, '~');
      std::cout << "^\n";
    } else {
      if (lengthOfSnake > li.col) lengthOfSnake = li.col;
      std::cout << std::string(li.col - lengthOfSnake, ' ');
      std::cout << "^";
      if (lengthOfSnake > 0)
        std::cout << std::string(lengthOfSnake - 1, '~');
      std::cout << "\n";
    }
    fh.seekg(off);
  }
}