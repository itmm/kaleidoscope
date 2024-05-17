#include "tok.h"

#include <iostream>

std::string identifier { };
double value { 0.0 };
static int last { ' ' };
static inline int get() { return std::cin.get(); }

static inline int parse_identifier() {
	identifier.clear();
	while (std::isalnum(last)) {
		identifier += last;
		last = get();
	}

	if (identifier == "def") { return tok_def; }
	if (identifier == "extern") { return tok_extern; }
	if (identifier == "if") { return tok_if; }
	if (identifier == "then") { return tok_then; }
	if (identifier == "else") { return tok_else; }

	return tok_identifier;
}

static inline int parse_number() {
	std::string num;
	while (std::isdigit(last) || last == '.') {
		num += last;
		last = get();
	}
	value = std::stod(num);
	return tok_number;
}

static int get_tok();

static inline int parse_comment() {
	while (last != EOF && last >= ' ') { last = get(); }
	return get_tok();
}

static int get_tok() {
	while (std::isspace(last)) { last = get(); }

	if (std::isalpha(last)) { return parse_identifier(); }

	if (std::isdigit(last) || last == '.') { return parse_number(); }

	if (last == '#') { return parse_comment(); }

	if (last == EOF) { return tok_eof; }

	int res { last };
	last = get();
	return res;
}

int cur_tok { 0 };

int next_tok() {
	cur_tok = get_tok();
	return cur_tok;
}
