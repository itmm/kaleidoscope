#include "code.h"


std::unique_ptr<llvm::LLVMContext> the_context;
std::unique_ptr<llvm::IRBuilder<>> builder;
std::unique_ptr<llvm::Module> the_module;

/*
#include "ast.h"
#include "tok.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
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

static std::map<std::string, llvm::Value *> named_values;

llvm::Value *Ast::Variable::codegen() {
	auto value { named_values[name_] };
	if (! value) { return log_value_error("unknown variable name"); }
	return value;
}

llvm::Function *Ast::Function::codegen() {
	auto fn { the_module->getFunction(proto_->name()) };
	if (! fn) { fn = proto_->codegen(); };
	if (! fn) { return nullptr; }
	if (! fn->empty()) {
		log_value_error("function cannot be redefined.");
		return nullptr;
	}
	auto bb { llvm::BasicBlock::Create(*the_context, "entry", fn) };
	builder->SetInsertPoint(bb);

	named_values.clear();
	for (auto &arg : fn->args()) {
		named_values[std::string(arg.getName())] = &arg;
	}
	if (auto retval { body_->codegen() }) {
		builder->CreateRet(retval);
		llvm::verifyFunction(*fn);
		the_fpm->run(*fn);
		return fn;
	}
	fn->eraseFromParent();
	return nullptr;
}
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