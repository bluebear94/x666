#include "Parser.h"

#include <assert.h>
#include <iostream>

namespace x666 {
  // Precedences of operators by their ids
  // 0 => treated specially
  // 1 => not valid in expressions (statements only)
  // 2 LSBs in other cases:
  // 0 => binary, left-associative
  // 1 => binary, right-associative
  // 2 => unary, prefixing (nyi)
  uint16_t precedences[] = {
    0, 0, 0, 0, // brackets
    400, 400, 500, 500, 500, // + - * / %
    400, 201, 300, 300, 300, // ~ <- = < 
    300, 300, 300, // /= <= >=
    1, 1, 1, 1, // ?? ?& !! &>
    341, 341, // ? :
    1, 1, 1, // @ @@ @#
    0, 240, 240, 240, // ! & | |*
    0, 140, // # ,
  };
  // Methods specific to Expression-trees
  Expression::~Expression() {}
  ExpressionPtr Expression::imbue(
      ExpressionPtr a,
      Operator o, size_t /*precedence*/,
      ExpressionPtr b) {
    return std::make_unique<BinaryOp>(std::move(a), std::move(b), o);
  }
  ExpressionPtr BinaryOp::imbue(
      ExpressionPtr ax,
      Operator o, size_t prec,
      ExpressionPtr b) {
    /*
        a->o        <- o
       /    \           \
      a->a  a->b         b
    */
    assert(ax->id() == 2);
    BinaryOp* a = dynamic_cast<BinaryOp*>(ax.get());
    size_t aprec = precedences[(size_t) a->o];
    if (aprec >= prec) {
      /*
            o--
           /   \
          a->o  b
         /    \
        a->a  a->b
      */
      return std::make_unique<BinaryOp>(std::move(ax), std::move(b), o);
    } else { // aprec < prec
      /*
          a->o
         /    \
        a->a   o--
              /   \
             a->b  b
        (this case showing the trivial imbuement into a->b)
      */
      a->b = a->b->imbue(std::move(a->b), o, prec, std::move(b));
      return ax;
    }
  }
  void Literal::trace() const {
    switch (val.index()) {
      case 0: std::cout << std::get<0>(val).name; break;
      case 1: std::cout << std::get<1>(val).n; break;
      case 2: std::cout << std::get<2>(val).str; break;
    }
  }
  void BinaryOp::trace() const {
    size_t prec = precedences[(size_t) o];
    ((prec & 1) == 0 ? a : b)->trace();
    std::cout << " " << opsAsStrings[(size_t) o] << " ";
    ((prec & 1) == 0 ? b : a)->trace();
  }
  // ParserVisitor used in parseAST::parse()
  class ParserVisitor {
  public:
    ParserVisitor(Parser* p) : p(p) {}
    bool operator()(Identifier&& i) {
      p->thisLine.push(std::make_unique<Literal>(std::move(i)));
      return true;
    }
    bool operator()(StringLiteral&& i) {
      p->thisLine.push(std::make_unique<Literal>(std::move(i)));
      return true;
    }
    bool operator()(IntLiteral&& i) {
      p->thisLine.push(std::make_unique<Literal>(std::move(i)));
      return true;
    }
    void commitLine() {
      // Commit the current line
      if (p->thisLine.empty()) return;
      ExpressionPtr ex = std::move(p->thisLine.top());
      p->thisLine.pop();
      p->expressions.push_back(std::move(ex));
      if (!p->thisLine.empty()) {
        p->errorLog.emplace_back(
          LexErrorCode::multipleExpressions,
          p->sot, p->li);
      }
      return;
    }
    bool operator()(Newline&&) {
      commitLine();
      return false;
    }
    bool operator()(EndOfFile&&) {
      commitLine();
      return false;
    }
    bool operator()(LexError&& e) {
      p->errorLog.push_back(e);
      return false;
    }
    bool operator()(Operator&& op) {
      size_t prec = precedences[(size_t) op];
      if (prec == 1 || prec == 0 /* nyi */) {
        p->errorLog.emplace_back(
          LexErrorCode::invalidOpInExpr,
          p->sot, p->li);
        return false;
      } else if ((prec & 2) == 0) {
        // Get the top AST
        if (p->thisLine.empty()) {
          // Oh no, we can't find anything after this
          p->errorLog.emplace_back(
            LexErrorCode::noLeftOperator,
            p->sot, p->li);
          return false;
        }
        ExpressionPtr a = std::move(p->thisLine.top());
        p->thisLine.pop();
        // Now generate a new AST from a token
        Token t = p->requestToken();
        if (std::holds_alternative<Newline>(t)) {
          // Oh no, we can't find anything after this
          p->errorLog.emplace_back(
            LexErrorCode::noRightOperator,
            p->sot, p->li);
          return false;
        }
        bool generatedExpression = p->acceptToken(std::move(t));
        if (!generatedExpression) {
          // Oh no, we can't find anything after this
          p->errorLog.emplace_back(
            LexErrorCode::noRightOperator,
            p->sot, p->li);
          return false;
        }
        ExpressionPtr b = std::move(p->thisLine.top());
        p->thisLine.pop();
        ExpressionPtr ex = (prec & 1) == 0 ?
          a->imbue(std::move(a), op, prec, std::move(b)) :
          b->imbue(std::move(b), op, prec, std::move(a));
        p->thisLine.push(std::move(ex));
        return true;
      }
    }
  private:
    Parser* p;
  };
  Parser::Parser(std::istream* fh) : fh(fh) {}
  Token Parser::requestToken() {
    return getNextToken(*fh, li, sot);
  }
  bool Parser::acceptToken(Token&& t) {
    return std::visit(ParserVisitor(this), std::move(t));
  }
  void Parser::parse() {
    while (true) {
      Token t = requestToken();
      if (std::holds_alternative<EndOfFile>(t)) break;
      acceptToken(std::move(t));
    }
  }
}