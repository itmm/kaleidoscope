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
/*
#include "KaleidoscopeJIT.h"

#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <llvm/IR/Value.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

llvm::Value *log_value_error(const std::string &msg) {
	log_error(msg);
	return nullptr;
}

static std::unique_ptr<llvm::LLVMContext> the_context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
static std::unique_ptr<llvm::Module> the_module;
static std::unique_ptr<llvm::legacy::FunctionPassManager> the_fpm;
static std::unique_ptr<llvm::orc::KaleidoscopeJIT> the_jit;

static std::map<std::string, llvm::Value *> named_values;

llvm::Value *Ast::Number::codegen() {
	return llvm::ConstantFP::get(*the_context, llvm::APFloat(value_));
}

llvm::Value *Ast::Variable::codegen() {
	auto value { named_values[name_] };
	if (! value) { return log_value_error("unknown variable name"); }
	return value;
}

llvm::Value *Ast::Binary::codegen() {
	auto left { left_->codegen() };
	auto right { right_->codegen() };
	if (! left || ! right) { return nullptr; }

	switch (op_) {
		case '+': return builder->CreateFAdd(left, right, "addtemp");
		case '-': return builder->CreateFSub(left, right, "subtemp");
		case '*': return builder->CreateFMul(left, right, "multemp");
		case '<':
			left = builder->CreateFCmpULT(left, right, "cmptemp");
			return builder->CreateUIToFP(left, llvm::Type::getDoubleTy(*the_context), "booltemp");
		default:
			return log_value_error("invalid binary operator");
	}
}

llvm::Value *Ast::Call::codegen() {
	auto callee { the_module->getFunction(callee_) };
	if (! callee) { return log_value_error("unknown function referenced"); }
	if (callee->arg_size() != args_.size()) {
		return log_value_error("incorrect numbers of arguments passed");
	}
	std::vector<llvm::Value *> args;
	for (auto &arg : args_) {
		args.push_back(arg->codegen());
		if (! args.back()) { return nullptr; }
	}
	return builder->CreateCall(callee, args, "calltmp");
}

llvm::Function *Ast::Prototype::codegen() {
	std::vector<llvm::Type *> doubles(args_.size(), llvm::Type::getDoubleTy(*the_context));
	auto ft { llvm::FunctionType::get(llvm::Type::getDoubleTy(*the_context), doubles, false) };
	auto f { llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name_, the_module.get()) };
	unsigned idx { 0 };
	for (auto &arg : f->args()) {
		arg.setName(args_[idx++].c_str());
	}
	return f;
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

void init_module_and_fpm() {
	the_context = std::make_unique<llvm::LLVMContext>();
	the_module = std::make_unique<llvm::Module>("kaleidoscope", *the_context);
	the_module->setDataLayout(the_jit->getTargetMachine().createDataLayout());
	builder = std::make_unique<llvm::IRBuilder<>>(*the_context);
	the_fpm = std::make_unique<llvm::legacy::FunctionPassManager>(the_module.get());
	the_fpm->add(llvm::createInstructionCombiningPass());
	the_fpm->add(llvm::createReassociatePass());
	the_fpm->add(llvm::createGVNPass());
	the_fpm->add(llvm::createCFGSimplificationPass());
	the_fpm->doInitialization();
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

*/
