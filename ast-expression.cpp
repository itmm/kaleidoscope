#include "ast-expression.h"
#include "code.h"
#include "tok.h"

#include <iostream>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>
#include <map>

namespace ast {
	Expression_Ptr log_expression_error(const char* message) {
		std::cerr << "Error: " << message << "\n";
		return nullptr;
	}

	Prototype_Ptr log_prototype_error(const char* message) {
		log_expression_error(message);
		return nullptr;
	}

	static Expression_Ptr parse_number_expression() {
		auto result { std::make_unique<Number>(value) };
		next_tok();
		return result;
	}

	llvm::Value* Number::generate_code() {
		return llvm::ConstantFP::get(*the_context, llvm::APFloat(value_));
	}

	llvm::Value *log_value_error(const char* msg) {
		log_expression_error(msg);
		return nullptr;
	}

	static std::map<std::string, llvm::Value *> named_values;

	llvm::Value* Variable::generate_code() {
		auto value { named_values[name_] };
		if (! value) { return log_value_error("unknown variable name"); }
		return value;
	}

	static Expression_Ptr parse_paren_expression() {
		next_tok();
		auto inner { parse_expression() };
		if (cur_tok != ')') { return log_expression_error("expected ')'"); }
		next_tok();
		return inner;
	}

	static Expression_Ptr parse_identifier_expression() {
		auto name = identifier;
		next_tok();
		if (cur_tok != '(') { return std::make_unique<Variable>(name); };
		next_tok();
		std::vector<Expression_Ptr> args;
		if (cur_tok != ')') {
			for (;;) {
				if (auto arg { parse_expression() }) {
					args.push_back(std::move(arg));
				} else { return nullptr; }
				if (cur_tok == ')') { break; }
				if (cur_tok != ',') {
					return log_expression_error("expected ')' or ',' in argument list");
				}
				next_tok();
			}
		}
		next_tok();
		return std::make_unique<Call>(name, std::move(args));
	}

	static Expression_Ptr parse_if_expression() {
		next_tok();
		auto condition { parse_expression() };
		if (! condition) { return nullptr; }
		if (cur_tok != tok_then) { return log_expression_error("expected then"); }
		next_tok();
		auto then { parse_expression() };
		if (! then) { return nullptr; }
		if (cur_tok != tok_else) { return log_expression_error("expected else"); }
		next_tok();
		auto els { parse_expression() };
		if (! els) { return nullptr; }
		return std::make_unique<If>(std::move(condition), std::move(then), std::move(els));
	}

	llvm::Value* If::generate_code() {
		auto condition_value { condition_->generate_code() };
		if (! condition_value) { return nullptr; }
		condition_value = builder->CreateFCmpONE(condition_value, llvm::ConstantFP::get(*the_context, llvm::APFloat(0.0)), "ifcond");
		llvm::Function* the_function = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*the_context, "then", the_function);
		llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*the_context, "else");
		llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*the_context, "ifcont");
		builder->CreateCondBr(condition_value, then_bb, else_bb);
		builder->SetInsertPoint(then_bb);
		auto then_value { then_->generate_code() };
		if (! then_value) { return nullptr; }
		builder->CreateBr(merge_bb);
		then_bb = builder->GetInsertBlock();

		the_function->insert(the_function->end(), else_bb);
		builder->SetInsertPoint(else_bb);
		auto else_value { else_->generate_code() };
		if (! else_value) { return nullptr; }
		builder->CreateBr(merge_bb);
		else_bb = builder->GetInsertBlock();

		the_function->insert(the_function->end(), merge_bb);
		builder->SetInsertPoint(merge_bb);
		llvm::PHINode* pn = builder->CreatePHI(llvm::Type::getDoubleTy(*the_context), 2, "iftmp");
		pn->addIncoming(then_value, then_bb);
		pn->addIncoming(else_value, else_bb);
		return pn;
	}

	static Expression_Ptr parse_for_expression() {
		next_tok();
		if (cur_tok != tok_identifier) {
			return log_expression_error("expected identifier after for");
		}
		auto id_name = identifier;
		next_tok();
		if (cur_tok != '=')  { return log_expression_error("expected '=' after for"); }
		next_tok();
		auto start = parse_expression();
		if (! start) { return nullptr; }
		if (cur_tok != ',') {
			return log_expression_error("expected ',' after for start value");
		}
		next_tok();
		auto end = parse_expression();
		if (! end) { return nullptr; }
		Expression_Ptr step;
		if (cur_tok == ',') {
			next_tok();
			step = parse_expression();
			if (! step) { return nullptr; }
		}
		if (cur_tok != tok_in) { return log_expression_error("expected 'in' after for"); }
		next_tok();
		auto body = parse_expression();
		if (! body) { return nullptr; }
		return std::make_unique<For>(
			id_name, std::move(start), std::move(end), std::move(step), std::move(body)
		);
	}

	llvm::Value* For::generate_code() {
		auto start_val { start_->generate_code() };
		if (! start_val) { return nullptr; }
		auto the_function { builder->GetInsertBlock()->getParent() };
		auto preheader_bb { builder->GetInsertBlock() };
		auto loop_bb { llvm::BasicBlock::Create(*the_context, "loop", the_function) };
		builder->CreateBr(loop_bb);
		builder->SetInsertPoint(loop_bb);
		auto variable { builder->CreatePHI(llvm::Type::getDoubleTy(*the_context), 2, var_name_) };
		variable->addIncoming(start_val, preheader_bb);
		auto old_value { named_values[var_name_] };
		named_values[var_name_] = variable;
		if (! body_->generate_code()) { return nullptr; }
		llvm::Value* step_value;
		if (step_) {
			step_value = step_->generate_code();
			if (! step_value) { return nullptr; }
		} else { step_value = llvm::ConstantFP::get(*the_context, llvm::APFloat(1.0)); }
		auto next_var = builder->CreateFAdd(variable, step_value, "nextvar");
		auto end_condition { end_->generate_code() };
		if (! end_condition) { return nullptr; }
		end_condition = builder->CreateFCmpONE(end_condition, llvm::ConstantFP::get(*the_context, llvm::APFloat(0.0)), "loopcond");
		auto loop_end_bb = builder->GetInsertBlock();
		auto after_bb { llvm::BasicBlock::Create(*the_context, "afterloop", the_function) };
		builder->CreateCondBr(end_condition, loop_bb, after_bb);
		builder->SetInsertPoint(after_bb);
		variable->addIncoming(next_var, loop_end_bb);
		if (old_value) {
			named_values[var_name_] = old_value;
		} else {
			named_values.erase(var_name_);
		}
		return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*the_context));
	}

	static Expression_Ptr parse_primary() {
		switch (cur_tok) {
			case tok_identifier:
				return parse_identifier_expression();
			case tok_number:
				return parse_number_expression();
			case '(':
				return parse_paren_expression();
			case tok_if:
				return parse_if_expression();
			case tok_for:
				return parse_for_expression();
			default:
				return log_expression_error("unknown token when expecting expression");
		}
	}

	static std::map<char, int> binary_op_precendence {
		{ '<', 10 }, { '+', 20 }, { '-', 20 }, { '*', 40 }
	};

	static int get_tok_precedence() {
		if (! isascii(cur_tok)) { return -1; }
		auto precedence { binary_op_precendence[cur_tok] };
		return precedence <= 0 ? -1 : precedence;
	}

	static Expression_Ptr parse_binary_op_right(int precedence, Expression_Ptr left) {
		for (;;) {
			auto tok_prec { get_tok_precedence() };
			if (tok_prec < precedence) {
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
			left = std::make_unique<Binary>(op, std::move(left), std::move(right));
		}
	}

	std::map<std::string, Prototype_Ptr> FunctionProtos;

	static llvm::Function* get_function(std::string name) {
		if (auto* f = the_module->getFunction(name)) { return f; }
		auto fi { FunctionProtos.find(name) };
		if (fi != FunctionProtos.end()) {
			return fi->second->generate_code();
		}
		return nullptr;
	}

	llvm::Value* Binary::generate_code() {
		auto left { left_hand_side_->generate_code() };
		auto right { right_hand_side_->generate_code() };
		if (! left || ! right) { return nullptr; }

		switch (op_) {
			case '+': return builder->CreateFAdd(left, right, "addtemp");
			case '-': return builder->CreateFSub(left, right, "subtemp");
			case '*': return builder->CreateFMul(left, right, "multemp");
			case '<':
				left = builder->CreateFCmpULT(left, right, "cmptemp");
				return builder->CreateUIToFP(left, llvm::Type::getDoubleTy(*the_context), "booltemp");
			default: break;
		}

		auto* f { get_function(std::string("binary") + op_) };
		assert(f && "binary operator not found!");
		llvm::Value* ops[2] { left, right };
		return builder->CreateCall(f, ops, "binop");
	}

	llvm::Value* Call::generate_code() {
		auto callee { get_function(callee_) };
		if (! callee) { return log_value_error("unknown function referenced"); }
		if (callee->arg_size() != args_.size()) {
			return log_value_error("incorrect numbers of arguments passed");
		}
		std::vector<llvm::Value *> args;
		for (auto &arg : args_) {
			args.push_back(arg->generate_code());
			if (! args.back()) { return nullptr; }
		}
		return builder->CreateCall(callee, args, "calltmp");
	}

	Expression_Ptr parse_expression() {
		auto left { parse_primary() };
		if (! left) { return nullptr; }
		return parse_binary_op_right(0, std::move(left));
	}

	llvm::Function *Prototype::generate_code() {
		std::vector<llvm::Type *> doubles(args_.size(), llvm::Type::getDoubleTy(*the_context));
		auto ft { llvm::FunctionType::get(llvm::Type::getDoubleTy(*the_context), doubles, false) };
		auto f { llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name_, the_module.get()) };
		unsigned idx { 0 };
		for (auto &arg : f->args()) {
			arg.setName(args_[idx++]);
		}
		return f;
	}

	llvm::Function* Function::generate_code() {
		auto& p { *prototype_ };
		FunctionProtos[p.name()] = std::move(prototype_);
		auto fn { the_module->getFunction(p.name()) };
		if (! fn) { fn = p.generate_code(); };
		if (! fn) { return nullptr; }
		if (p.is_binary_op()) {
			binary_op_precendence[p.operator_name()] = p.binary_precedence();
		}

		auto bb { llvm::BasicBlock::Create(*the_context, "entry", fn) };
		builder->SetInsertPoint(bb);

		named_values.clear();
		for (auto &arg : fn->args()) {
			named_values[std::string(arg.getName())] = &arg;
		}
		if (auto retval { body_->generate_code() }) {
			builder->CreateRet(retval);
			llvm::verifyFunction(*fn);
			the_fpm->run(*fn);
			return fn;
		}
		fn->eraseFromParent();
		return nullptr;
	}
}