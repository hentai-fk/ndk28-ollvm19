#pragma once

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h" //new Pass
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Obfuscation/ObfuscationOptions.h"


// Namespace
namespace llvm {
class FunctionPass;
class ObfuscationOptions;

FunctionPass *createSubstitutionPass(ObfuscationOptions *argsOptions);
}