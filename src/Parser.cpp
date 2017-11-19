#include "Parser.h"

#include <assert.h>
#include <iostream>

namespace x666 {
  // Precedences of operators by their ids
  // In general, <64 is treated specially
  // 0 => treated specially
  // 1 => not valid in expressions (statements only)
  // 2 => opening bracket
  // 3 => closing bracket
  // 3 LSBs in other cases:
  // 0 => binary, left-associative
  // 1 => binary, right-associative
  // 2 => unary, prefixing (nyi)
  // 4, 5 => like 0, 1 but can also be unary
  uint16_t precedences[] = {
    2, 3, 2, 3, // brackets
    0x400, 0x404, 0x500, 0x500, 0x500, // + - * / %
    0x400, 0x201, 0x300, 0x300, 0x300, // ~ <- = < 
    0x300, 0x300, 0x300, // /= <= >=
    1, 1, 1, 1, // ?? ?& !! &>
    0x381, 0x381, // ? :
    1, 1, 1, // @ @@ @#
    0x582, 0x280, 0x280, 0x280, // ! & | |*
    0x582, 0x180, // # ,
  };
  // Methods specific to Expression-trees
  Expression::~Expression() {}
  ExpressionPtr Expression::imbue(
      ExpressionPtr a,
      Operator o, size_t /*precedence*/,
      ExpressionPtr b) {
    return std::make_unique<BinaryOp>(std::move(a), std::move(b), o);
  }
  ExpressionPtr Expression::imbue(
      ExpressionPtr a,
      Operator o, size_t /*precedence*/) {
    return std::make_unique<UnaryOp>(std::move(a), o);
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
    size_t aprec = precedences[(size_t) a->o] >> 3;
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
  ExpressionPtr BinaryOp::imbue(
      ExpressionPtr ax,
      Operator o, size_t prec) {
    /*
        a->o        <- o
       /    \
      a->a  a->b
    */
    assert(ax->id() == 2);
    BinaryOp* a = dynamic_cast<BinaryOp*>(ax.get());
    size_t aprec = precedences[(size_t) a->o] >> 3;
    if (aprec >= prec) {
      /*
            o
           /
          a->o
         /    \
        a->a  a->b
      */
      return std::make_unique<UnaryOp>(std::move(ax), o);
    } else { // aprec < prec
      /*
          a->o
         /    \
        a->a   o
              /
             a->b
        (this case showing the trivial imbuement into a->b)
      */
      a->b = a->b->imbue(std::move(a->b), o, prec);
      return ax;
    }
  }
  ExpressionPtr UnaryOp::imbue(
      ExpressionPtr ax,
      Operator o, size_t prec,
      ExpressionPtr b) {
    /*
        a->o        <- o
       /                \
      a->a               b
    */
    assert(ax->id() == 3);
    UnaryOp* a = dynamic_cast<UnaryOp*>(ax.get());
    size_t aprec = precedences[(size_t) a->o] >> 3;
    if (aprec >= prec) {
      /*
            o--
           /   \
          a->o  b
         /
        a->a
      */
      return std::make_unique<BinaryOp>(std::move(ax), std::move(b), o);
    } else { // aprec < prec
      /*
            a->o
           /
          o--
         /   \
        a->a  b
        (this case showing the trivial imbuement into a->a)
      */
      a->a = a->a->imbue(std::move(a->a), o, prec, std::move(b));
      return ax;
    }
  }
  void Literal::trace() const {
    switch (val.index()) {
      case 0: std::cout << std::get<0>(val).name; break;
      case 1: std::cout << std::get<1>(val).n; break;
      case 2: std::cout << "\"" << unescape(std::get<2>(val).str) << "\"";
      break;
    }
  }
  void BinaryOp::trace() const {
    size_t prec = precedences[(size_t) o];
    ((prec & 1) == 0 ? a : b)->trace();
    std::cout << " " << opsAsStrings[(size_t) o] << " ";
    ((prec & 1) == 0 ? b : a)->trace();
  }
  void UnaryOp::trace() const {
    std::cout << opsAsStrings[(size_t) o];
    a->trace();
  }
  void Bracket::trace() const {
    std::cout << "(";
    ex->trace();
    std::cout << ")";
  }
  // ParserVisitor used in parseAST::parse()
  class ParserVisitor {
  public:
    ParserVisitor(Parser* p, const LineInfo& li) : p(p), li(li) {}
    bool operator()(Identifier&& i) {
      p->thisLine.push(std::make_unique<Literal>(std::move(i)));
      p->positions.push(li);
      return true;
    }
    bool operator()(StringLiteral&& i) {
      p->thisLine.push(std::make_unique<Literal>(std::move(i)));
      p->positions.push(li);
      return true;
    }
    bool operator()(IntLiteral&& i) {
      p->thisLine.push(std::make_unique<Literal>(std::move(i)));
      p->positions.push(li);
      return true;
    }
    void commitLine() {
      // Commit the current line
      if (p->thisLine.empty()) return;
      ExpressionPtr ex = std::move(p->thisLine.top());
      p->thisLine.pop();
      p->positions.pop();
      p->expressions.push_back(std::move(ex));
      if (!p->thisLine.empty()) {
        p->errorLog.emplace_back(
          LexErrorCode::multipleExpressions,
          p->getLastLineInfo());
        while (!p->thisLine.empty()) p->thisLine.pop();
        while (!p->positions.empty()) p->positions.pop();
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
    bool parseBinaryOp(const Operator& op, size_t& prec) {
      // Get the top AST
      if (p->thisLine.empty()) {
        // Oh no, we can't find anything before this
        if ((prec & 4) != 0) {
          prec |= 2;
          return false;
        } else {
          p->errorLog.emplace_back(
            LexErrorCode::noLeftOperand,
            p->li);
          return false;
        }
      }
      ExpressionPtr a = std::move(p->thisLine.top());
      p->thisLine.pop();
      // Now generate a new AST from a token
      size_t generatedExpressions = p->pushExpression();
      if (generatedExpressions != 1) {
        // Oh no, we can't find anything after this
        p->errorLog.emplace_back(
          LexErrorCode::noRightOperand,
          p->positions.top());
        p->positions.pop();
        return false;
      }
      ExpressionPtr b = std::move(p->thisLine.top());
      p->thisLine.pop();
      p->positions.pop();
      Expression* r = (prec & 1) == 0 ? a.get() : b.get();
      ExpressionPtr ex = (prec & 1) == 0 ?
        r->imbue(std::move(a), op, prec >> 3, std::move(b)) :
        r->imbue(std::move(b), op, prec >> 3, std::move(a));
      p->thisLine.push(std::move(ex));
      return true;
    }
    bool parseClosingBracket(const Operator& op) {
      bool matches = true;
      size_t openingHeight = 0;
      if (!p->brackets.empty()) {
        Parser::BracketEntry op2 = p->brackets.top();
        p->brackets.pop();
        if ((size_t) op2.bracket + 1 != (size_t) op)
          matches = false;
        openingHeight = op2.thisLineSize;
      }
      if (!matches) {
        p->errorLog.emplace_back(
          LexErrorCode::mismatchedBrackets,
          p->getLastLineInfo());
        return false;
      }
      ssize_t k = (ssize_t) p->thisLine.size() - (ssize_t) openingHeight;
      if (k < 0 || k > 1) {
        p->errorLog.emplace_back(
          LexErrorCode::multipleExpressions,
          p->getLastLineInfo());
        return false;
      } else if (k == 1) {
        ExpressionPtr ex = std::move(p->thisLine.top());
        p->thisLine.pop();
        p->thisLine.push(
          std::make_unique<Bracket>(
            std::move(ex), (Operator) ((size_t) op - 1)));
      } else { // k == 0
        p->thisLine.push(
          std::make_unique<Bracket>(
            nullptr, (Operator) ((size_t) op - 1)));
        p->positions.push(li);
      }
      return true;
    }
    bool operator()(Operator&& op) {
      size_t prec = precedences[(size_t) op];
      ExpressionPtr ex = nullptr;
      if (prec == 2) {
        // Opening bracket.
        p->brackets.push({ op, p->thisLine.size() });
        return false;
      } else if (prec == 3) {
        // Closing bracket.
        return parseClosingBracket(op);
      } else if (prec < 64) {
        p->errorLog.emplace_back(
          LexErrorCode::invalidOpInExpr,
          p->getLastLineInfo());
        return false;
      }
      if ((prec & 2) == 0) { // This is a binary operator
        parseBinaryOp(op, prec);
      }
      if ((prec & 2) != 0) { // This is a unary operator
        // Now generate a new AST from a token
        size_t generatedExpressions = p->pushExpression();
        if (generatedExpressions != 1) {
          // Oh no, we can't find anything after this
          p->errorLog.emplace_back(
            LexErrorCode::noRightOperand,
            p->getLastLineInfo());
          return false;
        }
        ExpressionPtr a = std::move(p->thisLine.top());
        p->thisLine.pop();
        Expression* ar = a.get();
        ex = ar->imbue(std::move(a), op, prec >> 3);
        p->thisLine.push(std::move(ex));
      }
    }
  private:
    Parser* p;
    LineInfo li;
  };
  Parser::Parser(std::istream* fh) : fh(fh) {}
  Token Parser::requestToken() {
    Token t = getNextToken(*fh, li);
    if (std::holds_alternative<LexError>(t))
      errorLog.push_back(std::get<LexError>(t));
    return t;
  }
  bool Parser::acceptToken(Token&& t) {
    return std::visit(ParserVisitor(this, li), std::move(t));
  }
  size_t Parser::pushExpression() {
    size_t oldBracketsHeight = brackets.size();
    size_t oldThisLineSize = thisLine.size();
    do {
      Token t = requestToken();
      if (std::holds_alternative<EndOfFile>(t) ||
          std::holds_alternative<Newline>(t)) {
        break;
      }
      acceptToken(std::move(t));
    } while (oldBracketsHeight != brackets.size());
    return thisLine.size() - oldThisLineSize;
  }
  void Parser::parse() {
    while (true) {
      Token t = requestToken();
      acceptToken(std::move(t));
      if (std::holds_alternative<EndOfFile>(t)) break;
      assert(thisLine.size() == positions.size());
    }
  }
  const LineInfo& Parser::getLastLineInfo() const {
    return !positions.empty() ? positions.top() : li;
  }
}