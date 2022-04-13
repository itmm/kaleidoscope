#include "tok.h"

#include <iostream>
#include <cstring>

std::string identifier { };
double value { 0.0 };

inline int get() { return std::cin.get(); }

int gettok() {
	static int last { ' ' };
	
	while (std::isspace(last)) { last = get(); }

	if (std::isalpha(last)) {
		identifier.clear();
		while (std::isalnum(last)) {
			identifier += last;
			last = get();
		}

		if (identifier == "def") { return tok_def; }
		if (identifier == "extern") { return tok_extern; }
		return tok_identifier;
	}

	if (std::isdigit(last) || last == '.') {
		std::string num;
		while (std::isdigit(last) || last == '.') {
			num += last;
			last = get();
		}
		value = std::stod(num);
		return tok_number;
	}

	if (last == '#') {
		while (last != EOF && last >= ' ') {
			last = get();
		}
		if (last != EOF) { return gettok(); }
	}

	if (last == EOF) { return tok_eof; }
	int res { last };
	last = get();
	return res;
}

int cur_tok { 0 };
int next_tok() {
	cur_tok = gettok();
	return cur_tok;
}
