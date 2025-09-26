#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Transforms/Obfuscation/ObfuscationOptions.h"
#include "llvm/Transforms/Obfuscation/Utils.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Regex.h"
#include <fstream>
#include <regex>

using namespace llvm;

namespace llvm {

SmallVector<std::string> readAnnotate(Function *f) {
  SmallVector<std::string> annotations;

  auto *Annotations = f->getParent()->getGlobalVariable(
      "llvm.global.annotations");
  auto *C = dyn_cast_or_null<Constant>(Annotations);
  if (!C || C->getNumOperands() != 1)
    return annotations;

  C = cast<Constant>(C->getOperand(0));

  // Iterate over all entries in C and attach !annotation metadata to suitable
  // entries.
  for (auto &Op : C->operands()) {
    // Look at the operands to check if we can use the entry to generate
    // !annotation metadata.
    auto *OpC = dyn_cast<ConstantStruct>(&Op);
    if (!OpC || OpC->getNumOperands() < 2)
      continue;
    auto *Fn = dyn_cast<Function>(OpC->getOperand(0)->stripPointerCasts());
    if (Fn != f)
      continue;
    auto *StrC = dyn_cast<GlobalValue>(OpC->getOperand(1)->stripPointerCasts());
    if (!StrC)
      continue;
    auto *StrData = dyn_cast<ConstantDataSequential>(StrC->getOperand(0));
    if (!StrData)
      continue;
    annotations.emplace_back(StrData->getAsString());
  }
  return annotations;
}

std::shared_ptr<ObfuscationOptions> ObfuscationOptions::readConfigFile(
    const Twine &FileName) {

  std::shared_ptr<ObfuscationOptions> result = std::make_shared<
    ObfuscationOptions>();
  if (FileName.str().empty()) {
    return result;
  }
  if (!sys::fs::exists(FileName)) {
    errs() << "Config file doesn't exist: " << FileName << "\n";
    return result;
  }

  auto BufOrErr = MemoryBuffer::getFileOrSTDIN(FileName);
  if (const auto ErrCode = BufOrErr.getError()) {
    errs() << "Can not read config file: " << ErrCode.message() << "\n";
    return result;
  }

  const auto &    buf = *BufOrErr.get();
  llvm::SourceMgr sm;

  auto jsonRoot = json::parse(buf.getBuffer());
  if (!jsonRoot) {
    report_fatal_error(jsonRoot.takeError());
  }
  auto rootObj = jsonRoot->getAsObject();
  if (!rootObj) {
    errs() << "Json root is not an object.\n";
    return result;
  }

  static auto procObj = [](const std::shared_ptr<ObfOpt> &obfOpt,
                           const detail::DenseMapPair<
                             json::ObjectKey, json::Value> &
                           obj) ->bool {

    static auto procOptValue = [](const std::shared_ptr<ObfOpt> &obfOpt,
                                  const json::Value &            value) {
      if (auto optObj = value.getAsObject()) {
        if (auto enable = optObj->getBoolean("enable")) {
          obfOpt->setEnable(enable.value());
        }
        if (auto level = optObj->getInteger("level")) {
          obfOpt->setLevel(static_cast<uint32_t>(level.value()));
        }
      }
    };

    std::string key = obj.getFirst().str();
    auto &      value = obj.getSecond();

    if (key == obfOpt->attributeName()) {
      procOptValue(obfOpt, value);
      return true;
    }
    return false;
  };

  SmallVector<std::shared_ptr<ObfOpt>> allOpt = result->getAllOpt();
  for (auto &obj : *rootObj) {
    bool objHit = false;
    for (auto &opt : allOpt) {
      if ((objHit = procObj(opt, obj))) {
        break;
      }
    }
    if (!objHit) {
      errs() << "warning: unknown arkari config node: " << obj.getFirst().str() << '\n';
    }
  }
  return result;
}

static bool match_regex_full(const std::string& name, const std::string& rule) {
  return Regex(rule).match(name);
}

static void appendFuntionMatchRules(SmallVector<std::string>& list, const std::string& name, 
                                    std::vector<std::pair<std::vector<std::string>, std::string>>& functionConfig) {
  for (const auto& matchRule : functionConfig) {
    bool skipFound = false;
    bool anyFound = false;
    for (const auto& matchFunc : matchRule.first) {
      if (matchFunc[0] == '@') {
        if (matchFunc[1] == '!') {
          if (matchFunc[2] == '=' || match_regex_full(linkCurrentModuleSource(), matchFunc.substr(2))) {
            skipFound = true;
            break;
          }
        }
        else if (matchFunc[1] == '=' || match_regex_full(linkCurrentModuleSource(), matchFunc.substr(1))) {
          anyFound = true;
          break;
        }
      }
      else if (matchFunc[0] == '!') {
        if (matchFunc[1] == '=' || match_regex_full(name, matchFunc.substr(1))) {
          skipFound = true;
          break;
        }
      }
      else if (matchFunc[0] == '=' || match_regex_full(name, matchFunc)) {
        anyFound = true;
        break;
      }
    }
    if (!skipFound && anyFound) {
      list.push_back(matchRule.second);
    }
  }
}

ObfOpt ObfuscationOptions::toObfuscate(const std::shared_ptr<ObfOpt> &option,
                                       Function *                     f) {
  const auto attrEnable = "+" + option->attributeName();
  const auto attrDisable = "-" + option->attributeName();
  const auto attrLevel = "^" + option->attributeName();
  auto       result = option->none();
  if (f->isDeclaration()) {
    return result;
  }

  if (f->hasAvailableExternallyLinkage() != 0) {
    return result;
  }

  bool annotationEnableFound = option->isEnabled();
  bool annotationDisableFound = false;
  uint32_t annotationSetLevel = option->level();

  auto annotations = readAnnotate(f);
  appendFuntionMatchRules(annotations, f->getName().str(), option->owner()->FunctionConfig);

  if (!annotations.empty()) {
    for (const auto &annotation : annotations) {
      if (annotation.find(attrDisable) != std::string::npos) {
        annotationDisableFound = true;
        break;
      }
      if (annotation.find(attrEnable) != std::string::npos) {
        annotationEnableFound = true;
      }
      if (const auto levelPos = annotation.find(attrLevel); levelPos != std::string::npos) {
        if (const auto equalPos = annotation.find('=', levelPos + 1); equalPos != std::string::npos) {
          int32_t    level = 0;
          for (size_t i = levelPos + attrLevel.length(); i < equalPos; ++i) {
            if (annotation[i] != ' ') {
              level = -1;
              break;
            }
          }
          if (level == -1) {
            continue;
          }
          annotationSetLevel = std::strtoul(annotation.c_str() + equalPos + 1, 0, 0);
        }
      }
    }
  }

  result.setEnable(!annotationDisableFound && annotationEnableFound);
  result.setLevel(annotationSetLevel);
  return result;
}

static bool getPureLine(std::ifstream& input, std::string& line) {
  if (std::getline(input, line)) {
    if (*line.rbegin() == '\n') {
      line.resize(line.size() - 1);
    }
    if (*line.rbegin() == '\r') {
      line.resize(line.size() - 1);
    }
    return true;
  }
  return false;
}

void ObfuscationOptions::loadFunctionConfig(const Twine &configPathOpt) {
  if (configPathOpt.str().empty()) {
    return;
  }
  std::ifstream config(configPathOpt.str());
  if (!config) {
    errs() << "Config file doesn't exist: " << configPathOpt << "\n";
    return;
  }
  std::string line;
  std::string annotation;
  std::vector<std::string> function;
  std::regex annotationRule(R"(:\s*$)");
  while (getPureLine(config, line)) {
    if (line[0] == '#') {
      continue;
    }
    if (std::regex_search(line, annotationRule)) {
      if (!annotation.empty() && !function.empty()) {
        FunctionConfig.emplace_back(std::pair(function, annotation));
        function.clear();
      }
      annotation = line;
      continue;
    }
    if (!annotation.empty() && !line.empty()) {
      function.emplace_back(line);
    }
  }
  if (!annotation.empty() && !function.empty()) {
    FunctionConfig.emplace_back(std::pair(function, annotation));
  }
}

}