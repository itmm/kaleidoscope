#include <iostream>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/Support/TargetSelect.h>
#include <memory>

#include "ast.h"
#include "code.h"
#include "tok.h"


using namespace ast;

extern "C" double putchard(double v) {
	fputc(static_cast<char>(v), stderr);
	return 0.0;
}

static llvm::ExitOnError ExitOnErr;

static inline void handle_definition() {
	if (auto ast { parse_definition() }) {
		std::cerr << "parsed a function definition.\n";
		if (auto ir { ast->generate_code() }) {
			ir->print(llvm::errs());
			std::cerr << '\n';
			ExitOnErr(the_jit->addModule(llvm::orc::ThreadSafeModule(std::move(the_module), std::move(the_context))));
			init_module_and_fpm();
		}
	} else { next_tok(); }
}

static inline void handle_extern() {
	if (auto ast { parse_extern() }) {
		std::cerr << "parsed an extern.\n";
		if (auto ir { ast->generate_code() }) {
			ir->print(llvm::errs());
			std::cerr << '\n';
			ast::FunctionProtos[ast->name()] = std::move(ast);
		}
	} else { next_tok(); }
}

static void handle_top_level_expr() {
	if (auto ast { parse_top_level_expr() }) {
		std::cerr << "parsed a top-level expression.\n";
		if (auto ir { ast->generate_code() }) {
			ir->print(llvm::errs());
			auto rt { the_jit->getMainJITDylib().createResourceTracker() };
			auto tsm { llvm::orc::ThreadSafeModule(std::move(the_module), std::move(the_context)) };
			ExitOnErr(the_jit->addModule(std::move(tsm), rt));
			init_module_and_fpm();

			auto expr { ExitOnErr(the_jit->lookup("__anon_expr")) };
			assert(expr && "function not found");
			double (*fp)() = reinterpret_cast<double (*)()>(expr.getAddress());
			double got { fp() };
			std::cerr << "evaluated to: " << got << '\n';
			ExitOnErr(rt->remove());
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
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	the_jit = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
	init_module_and_fpm();
	mainloop();
	the_module->print(llvm::errs(), nullptr);
}
