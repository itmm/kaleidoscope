#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>

#include "KaleidoscopeJIT.h"

extern std::unique_ptr<llvm::LLVMContext> the_context;
extern std::unique_ptr<llvm::IRBuilder<>> builder;
extern std::unique_ptr<llvm::Module> the_module;
extern std::unique_ptr<llvm::legacy::FunctionPassManager> the_fpm;
extern std::unique_ptr<llvm::orc::KaleidoscopeJIT> the_jit;

void init_module_and_fpm();
