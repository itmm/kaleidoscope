#include "ast.h"
#include "tok.h"

#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <map>

namespace Ast {
	class Expression {
		public:
			virtual ~Expression() {}
	};

	class Number: public Expression {
			double value_;
		public:
			Number(double value): value_ { value } { }
	};

	class Variable: public Expression {
			std::string name_;
		public:
			Variable(const std::string &name): name_ { name } { }
	};

	class Binary: public Expression {
			char op_;
			std::unique_ptr<Expression> left_, right_;
		public:
			Binary(char op, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right):
				op_ { op }, left_ { std::move(left) }, right_ { std::move(right) }
			{ }
	};

	class Call: public Expression {
			std::string callee_;
			std::vector<std::unique_ptr<Expression>> args_;
		public:
			Call(const std::string &callee, std::vector<std::unique_ptr<Expression>> args):
				callee_ { callee }, args_ { std::move(args) }
			{ }
	};

	class Prototype: public Expression {
			std::string name_;
			std::vector<std::string> args_;
		public:
			Prototype(const std::string &name, std::vector<std::string> args):
				name_ { name }, args_ { std::move(args) }
			{ }

			const std::string &name() const { return name_; }
	};

	class Function: public Expression {
			std::unique_ptr<Prototype> proto_;
			std::unique_ptr<Expression> body_;
		public:
			Function(std::unique_ptr<Prototype> proto, std::unique_ptr<Expression> body):
				proto_ { std::move(proto) }, body_ { std::move(body) }
			{ }
	};
}

std::unique_ptr<Ast::Expression> log_error(const std::string &msg) {
	std::cerr << "log error: " << msg << '\n';
	return nullptr;
}

std::unique_ptr<Ast::Prototype> log_prototype_error(const std::string &msg) {
	log_error(msg);
	return nullptr;
}

std::unique_ptr<Ast::Expression> parse_number_expression() {
	auto result { std::make_unique<Ast::Number>(value) };
	next_tok();
	return result;
}

std::unique_ptr<Ast::Expression> parse_expression();

std::unique_ptr<Ast::Expression> parse_paren_expression() {
	next_tok();
	auto inner { parse_expression() };
	if (cur_tok != ')') { return log_error("expected ')'"); }
	next_tok();
	return inner;
}

std::unique_ptr<Ast::Expression> parse_identifier_expression() {
	auto name = identifier;
	next_tok();
	if (cur_tok != '(') { return std::make_unique<Ast::Variable>(name); };
	next_tok();
	std::vector<std::unique_ptr<Ast::Expression>> args;
	if (cur_tok != ')') {
		for (;;) {
			if (auto arg { parse_expression() }) {
				args.push_back(std::move(arg));
			} else { return nullptr; }
			if (cur_tok == ')') { break; }
			if (cur_tok != ',') {
				return log_error("expected ')' or ',' in argument list");
			}
			next_tok();
		}
	}
	next_tok();
	return std::make_unique<Ast::Call>(name, std::move(args));
}

std::unique_ptr<Ast::Expression> parse_primary() {
	switch (cur_tok) {
		case tok_identifier:
			return parse_identifier_expression();
		case tok_number:
			return parse_number_expression();
		case '(':
			return parse_paren_expression();
		default:
			return log_error("unknown token when expecting expression");
	}
}

static std::map<char, int> binary_op_precendence {
	{ '<', 10 }, { '+', 20 }, { '-', 20 }, { '*', 40 }
};

static int get_tok_precedence() {
	if (! isascii(cur_tok)) { return -1; }
	auto prec { binary_op_precendence[cur_tok] };
	return prec <= 0 ? -1 : prec;
}

std::unique_ptr<Ast::Expression> parse_binary_op_right(int precendence, std::unique_ptr<Ast::Expression> left) {
	for (;;) {
		auto tok_prec { get_tok_precedence() };
		if (tok_prec < precendence) {
			return left;
		}
		int op { cur_tok };
		next_tok();
		auto right { parse_primary() };
		if (! right) { return nullptr; }
		int next_prec { get_tok_precedence() };
		if (tok_prec < next_prec) {
			right = parse_binary_op_right(tok_prec + 1, std::move(right));
			if (! right) { return nullptr; }
		}
		left = std::make_unique<Ast::Binary>(op, std::move(left), std::move(right));
	}
}

std::unique_ptr<Ast::Expression> parse_expression() {
	auto left { parse_primary() };
	if (! left) { return nullptr; }
	return parse_binary_op_right(0, std::move(left));
}

std::unique_ptr<Ast::Prototype> parse_prototype() {
	if (cur_tok != tok_identifier) { return log_prototype_error("expected function name in prototype"); }
	auto fn_name { identifier };
	next_tok();
	if (cur_tok != '(') { return log_prototype_error("expected '(' in prototype"); }
	std::vector<std::string> arg_names;
	while (next_tok() == tok_identifier) {
		arg_names.push_back(identifier);
	}
	if (cur_tok != ')') { return log_prototype_error("expected ')' in prototype"); }
	next_tok();
	return std::make_unique<Ast::Prototype>(fn_name, std::move(arg_names));
}

std::unique_ptr<Ast::Function> parse_definition() {
	next_tok();
	auto proto { parse_prototype() };
	if (! proto) { return nullptr; }
	if (auto expr { parse_expression() }) {
		return std::make_unique<Ast::Function>(std::move(proto), std::move(expr));
	}
	return nullptr;
}

std::unique_ptr<Ast::Prototype> parse_extern() {
	next_tok();
	return parse_prototype();
}

std::unique_ptr<Ast::Function> parse_top_level_expr() {
	if (auto expr { parse_expression() }) {
		auto proto { std::make_unique<Ast::Prototype>("", std::vector<std::string>()) };
		return std::make_unique<Ast::Function>(std::move(proto), std::move(expr));
	}
	return nullptr;
}

void handle_definition() {
	if (parse_definition()) {
		std::cerr << "parsed a function definition.\n";
	} else { next_tok(); }
}

void handle_extern() {
	if (parse_extern()) {
		std::cerr << "parsed an extern.\n";
	} else { next_tok(); }
}

void handle_top_level_expr() {
	if (parse_top_level_expr()) {
		std::cerr << "parsed a top-level expression.\n";
	} else { next_tok(); }
}

void mainloop() {
	for (;;) {
		std::cerr << "> ";
		switch (cur_tok) {
			case tok_eof: return;
			case ';': next_tok(); break;
			case tok_def: handle_definition(); break;
			case tok_extern: handle_extern(); break;
			default: handle_top_level_expr(); break;
		}
	}
}

int main() {
	next_tok();
	mainloop();
}
