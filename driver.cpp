#include <iostream>

#include "ast.h"
#include "code.h"
#include "tok.h"

using namespace ast;

static inline void handle_definition() {
	if (auto ast { parse_definition() }) {
		std::cerr << "parsed a function definition.\n";
		if (auto ir { ast->generate_code() }) {
			ir->print(llvm::errs());
			std::cerr << '\n';
		}
	} else { next_tok(); }
}

static inline void handle_extern() {
	if (auto ast { parse_extern() }) {
		std::cerr << "parsed an extern.\n";
		if (auto ir { ast->generate_code() }) {
			ir->print(llvm::errs());
			std::cerr << '\n';
		}
	} else { next_tok(); }
}

static void handle_top_level_expr() {
	if (auto ast { parse_top_level_expr() }) {
		std::cerr << "parsed a top-level expression.\n";
		if (auto ir { ast->generate_code() }) {
			ir->print(llvm::errs());
			/*
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
			 */
			ir->removeFromParent();
		}
	} else { next_tok(); }
}

static inline void mainloop() {
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
	std::cerr << "> ";
	next_tok();
	/*
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	the_jit = std::make_unique<llvm::orc::KaleidoscopeJIT>();
	 */
	init_module_and_fpm();
	mainloop();
	the_module->print(llvm::errs(), nullptr);
}
