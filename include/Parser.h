#pragma once

#include <iosfwd>
#include <memory>
#include <stack>
#include <vector>

#include "Lexer.h"

namespace x666 {
  class Expression {
  public:
    virtual ~Expression() = 0;
    Expression(Expression&& /*Expression*/) {}
    Expression() {}
    virtual size_t id() const = 0;
    /**
     * Imbue a binary operator and its other operand onto an expression.
     * a is the recipient, and it should also be the invoker.
     * b is the RHS for la operators and the LHS for ra operators.
     * 
     * Ex (give std::unique_ptr<Expression> a, b;):
     * std::unique_ptr<Expression> ex = a->imbue(a, op, prec, b);
     * After this call, a should no longer be used.
     */
    virtual std::unique_ptr<Expression> imbue(
      std::unique_ptr<Expression> a,
      Operator o, size_t precedence,
      std::unique_ptr<Expression> b);
    /**
     * Prints a representation of the expression to stdout.
     * BTW, did you know that `hack` means trace in Arka?
     */
    virtual void trace() const = 0;
  };
  using ExpressionPtr = std::unique_ptr<Expression>;
  class Literal : public Expression {
  public:
    using LiteralValue = std::variant<Identifier, IntLiteral, StringLiteral>;
    Literal(LiteralValue&& val) : val(std::move(val)) {}
    LiteralValue val;
    size_t id() const override { return 1; }
    void trace() const override;
  };
  class BinaryOp : public Expression {
  public:
    BinaryOp(ExpressionPtr a, ExpressionPtr b, Operator o) :
      a(std::move(a)), b(std::move(b)), o(o) {}
    // Note: a is LHS for left-associative operators
    // but RHS for right-associative operators
    ExpressionPtr a, b;
    Operator o;
    size_t id() const override { return 2; }
    ExpressionPtr imbue(
      ExpressionPtr ax,
      Operator o, size_t precedence,
      ExpressionPtr b) override;
    void trace() const override;
  };
  /**
   * A parser object.
   */
  class Parser {
  public:
    /**
     * Initialise the parser object.
     */
    Parser(std::istream* fh);
    void parse();
    /**
     * Accept a token (passed as a parameter)
     * and updates the state of the parser.
     */
    bool acceptToken(Token&& t);
    Token requestToken();
    ExpressionPtr parseExpression();
    std::vector<ExpressionPtr> expressions;
    std::stack<ExpressionPtr> thisLine;
    std::stack<LineInfo> positions;
    std::vector<LexError> errorLog;
    std::istream* fh;
    LineInfo li;
    size_t sot;
    friend class ParserVisitor;
  };
}