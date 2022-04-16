#include "ast.h"

#include <iostream>

std::unique_ptr<Ast::Expression> log_error(const std::string &msg) {
	std::cerr << "log error: " << msg << '\n';
	return nullptr;
}

std::unique_ptr<Ast::Prototype> log_prototype_error(const std::string &msg) {
	log_error(msg);
	return nullptr;
}

