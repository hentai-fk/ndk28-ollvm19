#ifndef OBFUSCATION_OBFUSCATIONOPTIONS_H
#define OBFUSCATION_OBFUSCATIONOPTIONS_H

#include "llvm/Support/YAMLParser.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/SmallString.h"


namespace llvm {

SmallVector<std::string> readAnnotate(Function *f);

class ObfuscationOptions;

class ObfOpt {
protected:
  ObfuscationOptions* Owner;
  uint32_t    Enabled;
  uint32_t    Level;
  std::string AttributeName;

public:
  ObfOpt(ObfuscationOptions* owner, const std::string &attributeName) {
    this->Owner = owner;
    this->Enabled = false;
    this->Level = 0;
    this->AttributeName = attributeName;
  }

  void readOpt(const cl::opt<bool> &enableOpt) {
    if (enableOpt.getNumOccurrences()) {
      Enabled = enableOpt.getValue();
    }
  }

  void readOpt(const cl::opt<bool> &    enableOpt,
               const cl::opt<uint32_t> &levelOpt) {
    readOpt(enableOpt);
    if (levelOpt.getNumOccurrences()) {
      Level = levelOpt.getValue();
    }
  }

  void setEnable(bool enabled) {
    this->Enabled = enabled;
  }

  void setLevel(uint32_t level) {
    this->Level = level;
  }

  bool isEnabled() const {
    return this->Enabled;
  }

  uint32_t level() const {
    return this->Level;
  }

  ObfuscationOptions* owner() const {
    return Owner;
  }

  const std::string &attributeName() const {
    return this->AttributeName;
  }

  ObfOpt none() const {
    return ObfOpt(owner(), this->attributeName());
  }

};

class ObfuscationOptions {
protected:
  std::shared_ptr<ObfOpt> IndBrOpt = nullptr;
  std::shared_ptr<ObfOpt> ICallOpt = nullptr;
  std::shared_ptr<ObfOpt> IndGvOpt = nullptr;
  std::shared_ptr<ObfOpt> FlaOpt = nullptr;
  std::shared_ptr<ObfOpt> SubOpt = nullptr;
  std::shared_ptr<ObfOpt> BcfOpt = nullptr;
  std::shared_ptr<ObfOpt> CseOpt = nullptr;
  std::shared_ptr<ObfOpt> CieOpt = nullptr;
  std::shared_ptr<ObfOpt> CfeOpt = nullptr;

  std::vector<std::pair<std::vector<std::string>, std::string>> FunctionConfig;

public:
  SmallVector<std::shared_ptr<ObfOpt>> getAllOpt() const {
    return {
      IndBrOpt,
      ICallOpt,
      IndGvOpt,
      FlaOpt,
      SubOpt,
      BcfOpt,
      CseOpt,
      CieOpt,
      CfeOpt,
    };
  }

  ObfuscationOptions() {
    this->IndBrOpt = std::make_shared<ObfOpt>(this, "indbr");
    this->ICallOpt = std::make_shared<ObfOpt>(this, "icall");
    this->IndGvOpt = std::make_shared<ObfOpt>(this, "indgv");
    this->FlaOpt = std::make_shared<ObfOpt>(this, "fla");
    this->SubOpt = std::make_shared<ObfOpt>(this, "sub");
    this->BcfOpt = std::make_shared<ObfOpt>(this, "bcf");
    this->CseOpt = std::make_shared<ObfOpt>(this, "cse");
    this->CieOpt = std::make_shared<ObfOpt>(this, "cie");
    this->CfeOpt = std::make_shared<ObfOpt>(this, "cfe");
  }

  auto indBrOpt() const {
    return IndBrOpt;
  }

  auto iCallOpt() const {
    return ICallOpt;
  }

  auto indGvOpt() const {
    return IndGvOpt;
  }

  auto flaOpt() const {
    return FlaOpt;
  }

  auto subOpt() const {
    return SubOpt;
  }

  auto bcfOpt() const {
    return BcfOpt;
  }

  auto cseOpt() const {
    return CseOpt;
  }

  auto cieOpt() const {
    return CieOpt;
  }

  auto cfeOpt() const {
    return CfeOpt;
  }

  static std::shared_ptr<ObfuscationOptions> readConfigFile(
      const Twine &FileName);

  static ObfOpt toObfuscate(const std::shared_ptr<ObfOpt> &option, Function *f);

  void loadFunctionConfig(const Twine &configPathOpt);

};

}

#endif