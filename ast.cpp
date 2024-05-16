#include "ast.h"
#include "tok.h"
#include "ast-expression.h"

namespace ast {
	Prototype_Ptr parse_prototype() {
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
		return std::make_unique<Prototype>(fn_name, std::move(arg_names));
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
