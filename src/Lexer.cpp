#include "Lexer.h"

#include <ctype.h>

#include <iostream>
#include <limits>

namespace x666 {
  const char* lexErrorMessages[] = {
    "Integer is too big to fit type",
    "Unknown operator",
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
  Token getNextToken(std::istream& fh, LineInfo& li, size_t& sot) {
    int c;
    do {
      c = getChar(fh, li);
      if (c == '\n') return Newline();
    } while (iswspace(c));
    if (fh.fail()) return EndOfFile();
    sot = li.byte - 1;
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
          return LexError(LexErrorCode::integerOverflow, sot, li.byte + 1);
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
    }
    return LexError(LexErrorCode::unknownOperator, sot, li.byte + 1);
  }
}