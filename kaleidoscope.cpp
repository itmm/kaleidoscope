#include "code.h"
#include "parse.h"
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

int main(int argc, char **argv) {
	llvm::InitLLVM x(argc, argv);
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	the_jit = std::make_unique<llvm::orc::KaleidoscopeJIT>();
	init_module_and_fpm();
	mainloop();
	the_module->print(llvm::errs(), nullptr);
}

