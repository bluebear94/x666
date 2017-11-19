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
     * Imbue a binary operator and its other operand into an expression.
     * a is the recipient, and it should also be the invoker.
     * b is the RHS for la operators and the LHS for ra operators.
     * prec should receive the entry in the precedence table,
     * right-shifted by 3.
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
     * Imbue a unary operator into an expression.
     * a is the recipient, and it should also be the invoker.
     * prec should receive the entry in the precedence table,
     * right-shifted by 3.
     */
    virtual std::unique_ptr<Expression> imbue(
      std::unique_ptr<Expression> a,
      Operator o, size_t precedence);
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
    ExpressionPtr imbue(
      ExpressionPtr ax,
      Operator o, size_t precedence) override;
    void trace() const override;
  };
  class UnaryOp : public Expression {
  public:
    UnaryOp(ExpressionPtr a, Operator o) :
      a(std::move(a)), o(o) {}
    // Note: a is LHS for left-associative operators
    // but RHS for right-associative operators
    ExpressionPtr a;
    Operator o;
    size_t id() const override { return 3; }
    ExpressionPtr imbue(
      ExpressionPtr ax,
      Operator o, size_t precedence,
      ExpressionPtr b) override;
    void trace() const override;
  };
  class Bracket : public Expression {
  public:
    Bracket(ExpressionPtr ex, Operator bracket) :
      ex(std::move(ex)), bracket(bracket) {}
    ExpressionPtr ex;
    Operator bracket;
    size_t id() const override { return 4; }
    void trace() const override;
  };
  struct Statement {
    ExpressionPtr ex;
    Operator statementOp;
    void trace() const;
  };
  /**
   * A parser object.
   */
  class Parser {
  public:
    struct BracketEntry {
      Operator bracket; // The opening bracket used
      size_t thisLineSize; // The size of thisLine when pushed
    };
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
    /**
     * Accept tokens from requestToken() until the opening
     * bracket is closed. Return the number of additional entries
     * on the thisLine stack.
     */
    size_t pushExpression();
    Token requestToken();
    ExpressionPtr parseExpression();
    const LineInfo& getLastLineInfo() const;
    std::vector<Statement> statements;
    std::stack<ExpressionPtr> thisLine;
    std::stack<LineInfo> positions;
    std::stack<BracketEntry> brackets;
    std::vector<LexError> errorLog;
    std::istream* fh;
    LineInfo li;
    // plus => no explicit statement
    // minus => already taken in a token
    Operator currentStatement;
    friend class ParserVisitor;
  };
}