#include "ast.h"
#include "tok.h"

#include <llvm/IR/Value.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <map>

namespace Ast {
	class Expression {
		public:
			virtual ~Expression() {}
			virtual llvm::Value *codegen() = 0;
	};

	class Number: public Expression {
			double value_;
		public:
			Number(double value): value_ { value } { }

			llvm::Value *codegen() override;
	};

	class Variable: public Expression {
			std::string name_;
		public:
			Variable(const std::string &name): name_ { name } { }

			llvm::Value *codegen() override;
	};

	class Binary: public Expression {
			char op_;
			std::unique_ptr<Expression> left_, right_;
		public:
			Binary(char op, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right):
				op_ { op }, left_ { std::move(left) }, right_ { std::move(right) }
			{ }

			llvm::Value *codegen() override;
	};

	class Call: public Expression {
			std::string callee_;
			std::vector<std::unique_ptr<Expression>> args_;
		public:
			Call(const std::string &callee, std::vector<std::unique_ptr<Expression>> args):
				callee_ { callee }, args_ { std::move(args) }
			{ }
			llvm::Value *codegen() override;
	};

	class Prototype: public Expression {
			std::string name_;
			std::vector<std::string> args_;
		public:
			Prototype(const std::string &name, std::vector<std::string> args):
				name_ { name }, args_ { std::move(args) }
			{ }

			llvm::Function *codegen() override;
			const std::string &name() const { return name_; }
	};

	class Function: public Expression {
			std::unique_ptr<Prototype> proto_;
			std::unique_ptr<Expression> body_;
		public:
			Function(std::unique_ptr<Prototype> proto, std::unique_ptr<Expression> body):
				proto_ { std::move(proto) }, body_ { std::move(body) }
			{ }
			llvm::Function *codegen() override;
	};
}

std::unique_ptr<Ast::Expression> log_error(const std::string &msg) {
	std::cerr << "log error: " << msg << '\n';
	return nullptr;
}

std::unique_ptr<Ast::Prototype> log_prototype_error(const std::string &msg) {
	log_error(msg);
	return nullptr;
}

llvm::Value *log_value_error(const std::string &msg) {
	log_error(msg);
	return nullptr;
}

static std::unique_ptr<llvm::LLVMContext> the_context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
std::unique_ptr<llvm::Module> the_module;
static std::unique_ptr<llvm::legacy::FunctionPassManager> the_fpm;
std::unique_ptr<llvm::orc::KaleidoscopeJIT> the_jit;

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

