#include "code.h"


std::unique_ptr<llvm::LLVMContext> the_context;
std::unique_ptr<llvm::IRBuilder<>> builder;
std::unique_ptr<llvm::Module> the_module;

/*
#include "ast.h"
#include "tok.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

#include <iostream>

llvm::Value *log_value_error(const std::string &msg) {
	log_error(msg);
	return nullptr;
}

static std::unique_ptr<llvm::legacy::FunctionPassManager> the_fpm;
std::unique_ptr<llvm::orc::KaleidoscopeJIT> the_jit;


*/

void init_module_and_fpm() {
	the_context = std::make_unique<llvm::LLVMContext>();
	builder = std::make_unique<llvm::IRBuilder<>>(*the_context);
	the_module = std::make_unique<llvm::Module>("kaleidoscope", *the_context);
	/*
	the_module->setDataLayout(the_jit->getTargetMachine().createDataLayout());
	the_fpm = std::make_unique<llvm::legacy::FunctionPassManager>(the_module.get());
	the_fpm->add(llvm::createInstructionCombiningPass());
	the_fpm->add(llvm::createReassociatePass());
	the_fpm->add(llvm::createGVNPass());
	the_fpm->add(llvm::createCFGSimplificationPass());
	the_fpm->doInitialization();
	 */
}