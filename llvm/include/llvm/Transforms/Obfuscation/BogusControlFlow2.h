#pragma once

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Obfuscation/ObfuscationOptions.h"

using namespace llvm;

namespace polaris {

struct BogusControlFlow2 {

  static bool isRequired() { return true; }
};

}; // namespace polaris


// Namespace
namespace llvm {
class FunctionPass;
class ObfuscationOptions;

FunctionPass* createBogusControlFlow2Pass(ObfuscationOptions *argsOptions);
}