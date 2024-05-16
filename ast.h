#pragma once

#include "ast-expression.h"

namespace ast {
	Prototype_Ptr parse_prototype();
	Function_Ptr parse_definition();
	Prototype_Ptr parse_extern();
	Function_Ptr parse_top_level_expr();
}
