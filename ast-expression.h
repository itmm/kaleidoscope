#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>

#include <llvm/IR/Value.h>

namespace ast {
	class Expression {
		public:
			virtual ~Expression() = default;
			virtual llvm::Value* generate_code() = 0;
	};

	using Expression_Ptr = std::unique_ptr<Expression>;

	Expression_Ptr log_expression_error(const char* message);

	class Number: public Expression {
			double value_;

		public:
			explicit Number(double value): value_ { value } { }
			llvm::Value* generate_code() override;
	};

	class Variable: public Expression {
			std::string name_;

		public:
			explicit Variable(std::string name): name_ { std::move(name) } { }
			llvm::Value* generate_code() override;
	};

	class Binary: public Expression {
			char op_;
			Expression_Ptr left_hand_side_;
			Expression_Ptr right_hand_side_;

		public:
			Binary(
				char op, Expression_Ptr left_hand_side, Expression_Ptr right_hand_side
			):
				op_ { op }, left_hand_side_ { std::move(left_hand_side) },
				right_hand_side_ { std::move(right_hand_side) }
			{ }
			llvm::Value* generate_code() override;
	};

	class Call: public Expression {
			std::string callee_;
			std::vector<Expression_Ptr> args_;

		public:
			Call(std::string callee, std::vector<Expression_Ptr> args):
				callee_ { std::move(callee) }, args_ { std::move(args) }
			{ }
			llvm::Value* generate_code() override;
	};

	class If: public Expression {
			Expression_Ptr condition_;
			Expression_Ptr then_;
			Expression_Ptr else_;

		public:
			If(Expression_Ptr condition, Expression_Ptr then, Expression_Ptr els):
				condition_ { std::move(condition) }, then_ { std::move(then) },
				else_ { std::move(els) }
			{ }

			llvm::Value * generate_code() override;
	};

	class For: public Expression {
			std::string var_name_;
			Expression_Ptr start_;
			Expression_Ptr end_;
			Expression_Ptr step_;
			Expression_Ptr body_;

		public:
			For(
				std::string  var_name, Expression_Ptr start, Expression_Ptr end,
				Expression_Ptr step, Expression_Ptr body
			):
				var_name_ { std::move(var_name) }, start_ { std::move(start) },
				end_ { std::move(end) }, step_ { std::move(step) }, body_ { std::move(body) }
			{ }

			llvm::Value * generate_code() override;
	};

	class Prototype {
			std::string name_;
			std::vector<std::string> args_;

		public:
			Prototype(std::string name, std::vector<std::string> args):
				name_ { std::move(name) }, args_ { std::move(args) }
			{ }

			[[nodiscard]] const std::string& name() const { return name_; }

			llvm::Function* generate_code();
	};

	using Prototype_Ptr = std::unique_ptr<Prototype>;

	Prototype_Ptr log_prototype_error(const char* message);

	class Function {
			Prototype_Ptr prototype_;
			Expression_Ptr body_;

		public:
			Function(Prototype_Ptr prototype, Expression_Ptr body):
				prototype_ { std::move(prototype) }, body_ { std::move(body) }
			{ }

			llvm::Function* generate_code();
	};

	using Function_Ptr = std::unique_ptr<Function>;

	Expression_Ptr parse_expression();

	extern std::map<std::string, Prototype_Ptr> FunctionProtos;
};