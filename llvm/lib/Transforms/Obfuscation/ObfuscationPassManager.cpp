#include "llvm/Transforms/Obfuscation/ObfuscationPassManager.h"
#include "llvm/Transforms/Obfuscation/Utils.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Transforms/Obfuscation/ObfuscationOptions.h"
#include "llvm/IR/Module.h"

#include <fstream>
#include <sstream>
#include <filesystem>


#define DEBUG_TYPE "ir-obfuscation"

using namespace llvm;

static cl::opt<bool>
EnableIRObfuscation("irobf", cl::init(false), cl::NotHidden,
                    cl::desc("Enable IR Code Obfuscation."),
                    cl::ZeroOrMore);


static cl::opt<bool>
EnableIndirectBr("irobf-indbr", cl::init(false), cl::NotHidden,
                 cl::desc("Enable IR Indirect Branch Obfuscation."),
                 cl::ZeroOrMore);
static cl::opt<uint32_t>
LevelIndirectBr("level-indbr", cl::init(0), cl::NotHidden,
                cl::desc("Set IR Indirect Branch Obfuscation Level, from 0 to 3."),
                cl::ZeroOrMore);


static cl::opt<bool>
EnableIndirectCall("irobf-icall", cl::init(false), cl::NotHidden,
                   cl::desc("Enable IR Indirect Call Obfuscation."),
                   cl::ZeroOrMore);
static cl::opt<uint32_t>
LevelIndirectCall("level-icall", cl::init(0), cl::NotHidden,
                  cl::desc("Set IR Indirect Call Obfuscation Level, from 0 to 3."),
                  cl::ZeroOrMore);


static cl::opt<bool> EnableIndirectGV(
    "irobf-indgv", cl::init(false), cl::NotHidden,
    cl::desc("Enable IR Indirect Global Variable Obfuscation."),
    cl::ZeroOrMore);
static cl::opt<uint32_t> LevelIndirectGV(
    "level-indgv", cl::init(0), cl::NotHidden,
    cl::desc("Set IR Indirect Global Variable Obfuscation Level, from 0 to 3."),
    cl::ZeroOrMore);


static cl::opt<bool> EnableIRFlattening(
    "irobf-fla", cl::init(false), cl::NotHidden,
    cl::desc("Enable IR Control Flow Flattening Obfuscation."), cl::ZeroOrMore);


static cl::opt<bool> EnableIRSubstitution(
    "irobf-sub", cl::init(false), cl::NotHidden,
    cl::desc("Enable Instruction Substitution Obfuscation."), cl::ZeroOrMore);

static cl::opt<uint32_t> LevelIRSubstitution(
    "level-sub", cl::init(0), cl::NotHidden,
    cl::desc("Set Instruction Substitution Obfuscation Additional Counter, from 0 to unlimited."),
    cl::ZeroOrMore);

static cl::opt<bool> EnableBogusFlow(
    "irobf-bcf", cl::init(false), cl::NotHidden,
    cl::desc("Enable Bogus Control Flow Obfuscation."), cl::ZeroOrMore);


static cl::opt<uint32_t> PercentageBogusFlow(
    "level-bcf", cl::init(80), cl::NotHidden,
    cl::desc("Set Bogus Control Flow Obfuscation Percentage, from 0 to 100."), cl::ZeroOrMore);


static cl::opt<bool>
EnableIRStringEncryption("irobf-cse", cl::init(false), cl::NotHidden,
                         cl::desc("Enable IR Constant String Encryption."),
                         cl::ZeroOrMore);


static cl::opt<bool>
EnableIRConstantIntEncryption("irobf-cie", cl::init(false), cl::NotHidden,
                              cl::desc(
                                  "Enable IR Constant Integer Encryption."),
                              cl::ZeroOrMore);
static cl::opt<uint32_t> LevelIRConstantIntEncryption(
    "level-cie", cl::init(0), cl::NotHidden,
    cl::desc("Set IR Constant Integer Encryption Level, from 0 to 3."),
    cl::ZeroOrMore);


static cl::opt<bool>
EnableIRConstantFPEncryption("irobf-cfe", cl::init(false), cl::NotHidden,
                             cl::desc("Enable IR Constant FP Encryption."),
                             cl::ZeroOrMore);

static cl::opt<uint32_t> LevelIRConstantFPEncryption(
    "level-cfe", cl::init(0), cl::NotHidden,
    cl::desc("Set IR Constant FP Encryption Level, from 0 to 3."),
    cl::ZeroOrMore);

static cl::opt<std::string>
    ArkariConfigPath("irobf-config", cl::init(std::string{}), cl::NotHidden,
                     cl::desc("Arkari config path."), cl::ZeroOrMore);

static cl::opt<std::string>
ObfuscationConfigPath("ollvm-config", cl::init(std::string{}), cl::NotHidden,
                 cl::desc("Function config path.\nFormat: >>>\n+sub,-fla,^bcf=80:\nfunction rules\n<<<"),
                 cl::ZeroOrMore);

namespace llvm {

struct ObfuscationPassManager : public ModulePass {
  static char            ID; // Pass identification
  SmallVector<Pass *> Passes;

  ObfuscationPassManager() : ModulePass(ID) {
    initializeObfuscationPassManagerPass(*PassRegistry::getPassRegistry());
  };

  StringRef getPassName() const override {
    return "Obfuscation Pass Manager";
  }

  bool doFinalization(Module &M) override {
    bool Change = false;
    for (Pass *P : Passes) {
      Change |= P->doFinalization(M);
      delete (P);
    }
    return Change;
  }

  void add(Pass *P) {
    Passes.push_back(P);
  }

  void writeModuleFunctionNameOfFile(Module &M) {
    if (ObfuscationConfigPath.getValue().empty() || !std::filesystem::exists(ObfuscationConfigPath.getValue())) {
      return;
    }
    std::stringstream logStream;
    logStream << "@=" << linkCurrentModuleSource() << "\n";
    for (Function &F : M) {
      logStream << "=" << F.getName().str() << "\n";
    }
    std::error_code EC;
    raw_fd_ostream fd_stream(ObfuscationConfigPath.getValue() + "-ollvm-pass-filter.txt", EC, sys::fs::OF_Append);
    if (!EC) {
      auto locker = fd_stream.lock();
      fd_stream << logStream.str() << "\n\n";
    }
  }

  bool run(Module &M) {
    linkCurrentModuleSource() = M.getModuleIdentifier();

    writeModuleFunctionNameOfFile(M);

    bool Change = false;
    for (Pass *P : Passes) {
      switch (P->getPassKind()) {
      case PassKind::PT_Function:
        Change |= runFunctionPass(M, (FunctionPass *)P);
        break;
      case PassKind::PT_Module:
        Change |= runModulePass(M, (ModulePass *)P);
        break;
      default:
        continue;
      }
    }

    linkCurrentModuleSource() = "";
    return Change;
  }

  bool runFunctionPass(Module &M, FunctionPass *P) {
    bool Changed = false;
    for (Function &F : M) {
      Changed |= P->runOnFunction(F);
    }
    return Changed;
  }

  bool runModulePass(Module &M, ModulePass *P) {
    return P->runOnModule(M);
  }

  static std::shared_ptr<ObfuscationOptions> getOptions() {
    auto Opt = ObfuscationOptions::readConfigFile(ArkariConfigPath);

    Opt->indBrOpt()->readOpt(EnableIndirectBr, LevelIndirectBr);
    Opt->iCallOpt()->readOpt(EnableIndirectCall, LevelIndirectCall);
    Opt->indGvOpt()->readOpt(EnableIndirectGV, LevelIndirectGV);
    Opt->flaOpt()->readOpt(EnableIRFlattening);
    Opt->subOpt()->readOpt(EnableIRSubstitution, LevelIRSubstitution);
    Opt->bcfOpt()->readOpt(EnableBogusFlow, PercentageBogusFlow);
    Opt->cseOpt()->readOpt(EnableIRStringEncryption);
    Opt->cieOpt()->readOpt(EnableIRConstantIntEncryption, LevelIRConstantIntEncryption);
    Opt->cfeOpt()->readOpt(EnableIRConstantFPEncryption, LevelIRConstantFPEncryption);

    Opt->loadFunctionConfig(ObfuscationConfigPath);
    return Opt;
  }

  bool runOnModule(Module &M) override {

    if (EnableIndirectBr || EnableIndirectCall || EnableIndirectGV ||
        EnableIRFlattening || EnableIRSubstitution || EnableBogusFlow ||
        EnableIRStringEncryption || EnableIRConstantIntEncryption ||
        EnableIRConstantFPEncryption || !ArkariConfigPath.empty()) {
      EnableIRObfuscation = true;
    }

    if (!EnableIRObfuscation) {
      return false;
    }

    const auto Options(getOptions());
    unsigned   pointerSize = M.getDataLayout().getTypeAllocSize(
        PointerType::getUnqual(M.getContext()));

    // 这个只能全局设置，给函数加注释是没用的，因为全部字符串都会储存在全局字符串表里
    if (Options->cseOpt()->isEnabled()) {
      add(llvm::createStringEncryptionPass(Options.get()));
    }

    add(llvm::createBogusControlFlow2Pass(Options.get()));

    add(llvm::createFlatteningPass(pointerSize, Options.get()));
    
    add(llvm::createSubstitutionPass(Options.get()));

    add(llvm::createConstantIntEncryptionPass(Options.get()));
    add(llvm::createConstantFPEncryptionPass(Options.get()));

    add(llvm::createIndirectCallPass(pointerSize, Options.get()));

    add(llvm::createIndirectBranchPass(pointerSize, Options.get()));
    
    add(llvm::createIndirectGlobalVariablePass(pointerSize, Options.get()));

    bool Changed = run(M);

    return Changed;
  }
};
} // namespace llvm

char ObfuscationPassManager::ID = 0;

ModulePass *llvm::createObfuscationPassManager() {
  return new ObfuscationPassManager();
}

INITIALIZE_PASS_BEGIN(ObfuscationPassManager, "irobf", "Enable IR Obfuscation",
                      false, false)
INITIALIZE_PASS_END(ObfuscationPassManager, "irobf", "Enable IR Obfuscation",
                      false, false)