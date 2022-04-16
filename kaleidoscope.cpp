#include "ast.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

int main() {
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	the_jit = std::make_unique<llvm::orc::KaleidoscopeJIT>();
	init_module_and_fpm();
	mainloop();
	the_module->print(llvm::errs(), nullptr);
}

