#pragma once

#include <string>

enum Token {
	tok_eof = -1,
	tok_def = -2,
	tok_extern = -3,
	tok_identifier = -4,
	tok_number = -5
};

extern std::string identifier;
extern double value;

extern int cur_tok;
extern int next_tok();
