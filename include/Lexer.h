#pragma once

#include <stdint.h>

#include <iosfwd>
#include <string>
#include <variant>

namespace x666 {
  /**
   * Information about the current line and column.
   */
  struct LineInfo {
    LineInfo() : line(0), col(0), byte(0), sot(0) {}
    size_t line, col;
    size_t byte, sot;
  };
  /** An identifier token. */
  struct Identifier {
    Identifier(char c) : name{c} {}
    std::string name;
  };
  /** A string literal. */
  struct StringLiteral {
    StringLiteral(std::string&& s) : str(s) {}
    std::string str;
  };
  std::string unescape(const std::string& s);
  /** An integer literal (64-bit). */
  struct IntLiteral {
    IntLiteral(int64_t n) : n(n) {}
    int64_t n;
  };
  /** An operator. */
  enum class Operator {
    leftBracket,
    rightBracket,
    leftSBracket,
    rightSBracket,
    plus,
    minus,
    times,
    divide,
    modulo,
    concat,
    assign,
    equal,
    less,
    greater,
    notEqual,
    lessEqual,
    greaterEqual,
    ifStmt,
    ifThenStmt,
    elseStmt,
    endStmt,
    questionMark,
    colon,
    whileStmt,
    repeatStmt,
    forStmt,
    notStmt,
    andStmt,
    orStmt,
    xorStmt,
    length,
    comma,
    print,
  };
  /** Token to denote a newline. */
  struct Newline {};
  /** Token to denote the end of the file. */
  struct EndOfFile {};
  /** Enum of lex error codes. */
  enum class LexErrorCode {
    integerOverflow,
    unknownOperator,
    invalidOpInExpr,
    multipleExpressions,
    noLeftOperand,
    noRightOperand,
    mismatchedBrackets,
    statementNeedsExpression,
    statementHasExpression,
  };
  /** The array of lex error messages. */
  extern const char* lexErrorMessages[];
  extern const char* opsAsStrings[];
  /** A token to denote that a lexing error has occurred. */
  struct LexError {
    LexError(LexErrorCode c, const LineInfo& li) :
      c(c), li(li) {}
    LexErrorCode c;
    LineInfo li;
    void print(std::istream& fh) const;
  };
  using Token = std::variant<
    Identifier,
    StringLiteral,
    IntLiteral,
    Operator,
    Newline,
    EndOfFile,
    LexError>;
  /**
   * Get the next token from the file stream fh, updating li.
   * sot is a reference that will store the beginning of the
   * token read after this function returns.
   */
  Token getNextToken(std::istream& fh, LineInfo& li);
}