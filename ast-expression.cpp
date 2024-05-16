#include "ast-expression.h"
#include "tok.h"

#include <iostream>
#include <map>

namespace ast {
	Expression_Ptr log_expression_error(const char* message) {
		std::cerr << "Error: " << message << "\n";
		return nullptr;
	}

	Prototype_Ptr log_prototype_error(const char* message) {
		log_expression_error(message);
		return nullptr;
	}

	static Expression_Ptr parse_number_expression() {
		auto result { std::make_unique<Number>(value) };
		next_tok();
		return result;
	}

	static Expression_Ptr parse_paren_expression() {
		next_tok();
		auto inner { parse_expression() };
		if (cur_tok != ')') { return log_expression_error("expected ')'"); }
		next_tok();
		return inner;
	}

	static Expression_Ptr parse_identifier_expression() {
		auto name = identifier;
		next_tok();
		if (cur_tok != '(') { return std::make_unique<Variable>(name); };
		next_tok();
		std::vector<std::unique_ptr<Expression>> args;
		if (cur_tok != ')') {
			for (;;) {
				if (auto arg { parse_expression() }) {
					args.push_back(std::move(arg));
				} else { return nullptr; }
				if (cur_tok == ')') { break; }
				if (cur_tok != ',') {
					return log_expression_error("expected ')' or ',' in argument list");
				}
				next_tok();
			}
		}
		next_tok();
		return std::make_unique<Call>(name, std::move(args));
	}

	static Expression_Ptr parse_primary() {
		switch (cur_tok) {
			case tok_identifier:
				return parse_identifier_expression();
			case tok_number:
				return parse_number_expression();
			case '(':
				return parse_paren_expression();
			default:
				return log_expression_error("unknown token when expecting expression");
		}
	}

	static std::map<char, int> binary_op_precendence {
		{ '<', 10 }, { '+', 20 }, { '-', 20 }, { '*', 40 }
	};

	static int get_tok_precedence() {
		if (! isascii(cur_tok)) { return -1; }
		auto precedence { binary_op_precendence[cur_tok] };
		return precedence <= 0 ? -1 : precedence;
	}

	static Expression_Ptr parse_binary_op_right(int precendence, Expression_Ptr left) {
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
			left = std::make_unique<Binary>(op, std::move(left), std::move(right));
		}
	}

	Expression_Ptr parse_expression() {
		auto left { parse_primary() };
		if (! left) { return nullptr; }
		return parse_binary_op_right(0, std::move(left));
	}
}