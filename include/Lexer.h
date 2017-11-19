#pragma once

#include <stdint.h>

#include <iosfwd>
#include <string>
#include <variant>

namespace x666 {
  struct LineInfo {
    LineInfo() : line(0), col(0), byte(0) {}
    size_t line, col;
    size_t byte;
  };
  struct Identifier {
    Identifier(char c) : name{c} {}
    std::string name;
  };
  struct StringLiteral {
    std::string str;
  };
  struct IntLiteral {
    IntLiteral(int64_t n) : n(n) {}
    int64_t n;
  };
  enum class Operator {
    leftBracket,
    rightBracket,
    leftSBracket,
    rightSBracket,
    plus,
    minus,
    times,
    divide,
    concat,
    assign,
  };
  struct Newline {};
  struct EndOfFile {};
  enum class LexErrorCode {
    integerOverflow,
    unknownOperator,
  };
  extern const char* lexErrorMessages[];
  struct LexError {
    LexError(LexErrorCode c, size_t start, size_t end) :
      c(c), start(start), end(end) {}
    LexErrorCode c;
    size_t start, end;
  };
  using Token = std::variant<
    Identifier,
    StringLiteral,
    IntLiteral,
    Operator,
    Newline,
    EndOfFile,
    LexError>;
  Token getNextToken(std::istream& fh, LineInfo& li, size_t& sot);
}