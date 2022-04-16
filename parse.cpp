#include "code.h"
#include "ast.h"
#include "tok.h"

#include <iostream>

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
		auto proto { std::make_unique<Ast::Prototype>("__anon_expr", std::vector<std::string>()) };
		return std::make_unique<Ast::Function>(std::move(proto), std::move(expr));
	}
	return nullptr;
}

void handle_definition() {
	if (auto ast { parse_definition() }) {
		if (auto ir { ast->codegen() }) {
			std::cerr << "parsed a function definition.\n";
			ir->print(llvm::errs());
			std::cerr << '\n';
		}
	} else { next_tok(); }
}

void handle_extern() {
	if (auto ast { parse_extern() }) {
		if (auto ir { ast->codegen() }) {
			std::cerr << "parsed an extern.\n";
			ir->print(llvm::errs());
			std::cerr << '\n';
		}
	} else { next_tok(); }
}

void handle_top_level_expr() {
	if (auto ast { parse_top_level_expr() }) {
		if (auto ir { ast->codegen() }) {
			ir->print(llvm::errs());
			auto h { the_jit->addModule(std::move(the_module)) };
			auto expr { the_jit->findSymbol("__anon_expr") };
			assert(expr && "function not found");
			auto addr { expr.getAddress() };
			assert(addr && "no address");
			double (*fp)() = (double (*)()) *addr;
			std::cerr << "addr " << (size_t) fp << "\n";
			double got { fp() };
			std::cerr << "evaluated to: " << got << '\n';
			the_jit->removeModule(h);
			init_module_and_fpm();
		}
	} else { next_tok(); }
}

void mainloop() {
	std::cerr << "> ";
	next_tok();
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

