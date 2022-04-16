#pragma once

#include <memory>
#include <string>
#include <vector>

#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

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

std::unique_ptr<Ast::Expression> log_error(const std::string &msg);
std::unique_ptr<Ast::Prototype> log_prototype_error(const std::string &msg);
