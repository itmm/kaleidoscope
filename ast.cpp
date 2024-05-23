#include "ast.h"
#include "tok.h"
#include "ast-expression.h"

namespace ast {
	Prototype_Ptr parse_prototype() {
		std::string fn_name;
		unsigned kind = 0;
		unsigned binary_precedence = 30;

		switch (cur_tok) {
			case tok_identifier:
				fn_name = identifier;
				kind = 0;
				next_tok();
				break;
			case tok_binary:
				next_tok();
				if (!isascii(cur_tok)) {
					return log_prototype_error("expected binary operator");
				}
				fn_name = "binary";
				fn_name += static_cast<char>(cur_tok);
				kind = 2;
				next_tok();

				if (cur_tok == tok_number) {
					if (value < 1 || value > 100) {
						return log_prototype_error("invalid precedence: must be 1..100");
					}
					binary_precedence = static_cast<unsigned>(value);
					next_tok();
				}
				break;
			default:
				return log_prototype_error("expected function name in prototype");
		}

		if (cur_tok != '(') { return log_prototype_error("expected '(' in prototype"); }
		std::vector<std::string> arg_names;
		while (next_tok() == tok_identifier) {
			arg_names.push_back(identifier);
		}
		if (cur_tok != ')') { return log_prototype_error("expected ')' in prototype"); }
		next_tok();
		if (kind && arg_names.size() != kind) {
			return log_prototype_error("invalid number of arguments for operator");
		}
		return std::make_unique<Prototype>(
			fn_name, std::move(arg_names), kind != 0, binary_precedence
		);
	}

	Function_Ptr parse_definition() {
		next_tok();
		auto proto { parse_prototype() };
		if (! proto) { return nullptr; }
		if (auto expr { parse_expression() }) {
			return std::make_unique<Function>(std::move(proto), std::move(expr));
		}
		return nullptr;
	}

	Prototype_Ptr parse_extern() {
		next_tok();
		return parse_prototype();
	}

	Function_Ptr parse_top_level_expr() {
		if (auto expr { parse_expression() }) {
			auto proto { std::make_unique<Prototype>("__anon_expr", std::vector<std::string>()) };
			return std::make_unique<Function>(std::move(proto), std::move(expr));
		}
		return nullptr;
	}

}
