#pragma once

#include <string>
#include <memory>
#include <vector>

namespace ast {
	class Expression {
		public:
			virtual ~Expression() = default;
	};

	using Expression_Ptr = std::unique_ptr<Expression>;

	Expression_Ptr log_expression_error(const char* message);

	class Number: public Expression {
			double value_;

		public:
			Number(double value): value_ { value } { }
	};

	class Variable: public Expression {
			std::string name_;

		public:
			Variable(const std::string& name): name_ { name } { }
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
	};

	class Call: public Expression {
			std::string callee_;
			std::vector<Expression_Ptr> args_;

		public:
			Call(const std::string& callee, std::vector<Expression_Ptr> args):
				callee_ { callee }, args_ { std::move(args) }
			{ }
	};

	class Prototype {
			std::string callee_;
			std::vector<std::string> args_;

		public:
			Prototype(const std::string& callee, std::vector<std::string> args):
				callee_ { callee }, args_ { std::move(args) }
			{ }
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
	};

	using Function_Ptr = std::unique_ptr<Function>;

	Expression_Ptr parse_expression();
};