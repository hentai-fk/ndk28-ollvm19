#include "llvm/Transforms/Obfuscation/Substitution.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Obfuscation/CryptoUtils.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Obfuscation/Utils.h"

using namespace llvm;


#define DEBUG_TYPE "sub"


#define NUMBER_ADD_SUBST 4
#define NUMBER_SUB_SUBST 3
#define NUMBER_AND_SUBST 2
#define NUMBER_OR_SUBST 2
#define NUMBER_XOR_SUBST 2


// Stats
STATISTIC(Add, "Add substitued");
STATISTIC(Sub, "Sub substitued");
// STATISTIC(Mul,  "Mul substitued");
// STATISTIC(Div,  "Div substitued");
// STATISTIC(Rem,  "Rem substitued");
// STATISTIC(Shi,  "Shift substitued");
STATISTIC(And, "And substitued");
STATISTIC(Or, "Or substitued");
STATISTIC(Xor, "Xor substitued");


struct Substitution : FunctionPass {
  static char ID;
  ObfuscationOptions *ArgsOptions;

  void (Substitution::*funcAdd[NUMBER_ADD_SUBST])(BinaryOperator *bo);
  void (Substitution::*funcSub[NUMBER_SUB_SUBST])(BinaryOperator *bo);
  void (Substitution::*funcAnd[NUMBER_AND_SUBST])(BinaryOperator *bo);
  void (Substitution::*funcOr[NUMBER_OR_SUBST])(BinaryOperator *bo);
  void (Substitution::*funcXor[NUMBER_XOR_SUBST])(BinaryOperator *bo);

  Substitution(ObfuscationOptions *argsOptions) : FunctionPass(ID) {
    this->ArgsOptions = argsOptions;

    funcAdd[0] = &Substitution::addNeg;
    funcAdd[1] = &Substitution::addDoubleNeg;
    funcAdd[2] = &Substitution::addRand;
    funcAdd[3] = &Substitution::addRand2;

    funcSub[0] = &Substitution::subNeg;
    funcSub[1] = &Substitution::subRand;
    funcSub[2] = &Substitution::subRand2;

    funcAnd[0] = &Substitution::andSubstitution;
    funcAnd[1] = &Substitution::andSubstitutionRand;

    funcOr[0] = &Substitution::orSubstitution;
    funcOr[1] = &Substitution::orSubstitutionRand;

    funcXor[0] = &Substitution::xorSubstitution;
    funcXor[1] = &Substitution::xorSubstitutionRand;
  }

  StringRef getPassName() const override { return {"Substitution"}; }

  bool runOnFunction(Function &F) override {
    const auto opt = ArgsOptions->toObfuscate(ArgsOptions->subOpt(), &F);
    if (!opt.isEnabled()) {
      return false;
    }
    return substitute(&F, opt.level());
  }

  bool substitute(Function *f, uint32_t obfAddition) {
    Function *tmp = f;

    // Loop for the number of time we run the pass on the function
    int times = 1 + obfAddition;
    bool checkChanged = false;
    do {
      for (Function::iterator bb = tmp->begin(); bb != tmp->end(); ++bb) {
        for (BasicBlock::iterator inst = bb->begin(); inst != bb->end();
             ++inst) {
          if (inst->isBinaryOp()) {
            switch (inst->getOpcode()) {
            case BinaryOperator::Add:
              // case BinaryOperator::FAdd:
              // Substitute with random add operation
              (this->*funcAdd[llvm::cryptoutils->get_range(NUMBER_ADD_SUBST)])(
                  cast<BinaryOperator>(inst));
              ++Add;
              checkChanged = true;
              break;
            case BinaryOperator::Sub:
              // case BinaryOperator::FSub:
              // Substitute with random sub operation
              (this->*funcSub[llvm::cryptoutils->get_range(NUMBER_SUB_SUBST)])(
                  cast<BinaryOperator>(inst));
              ++Sub;
              checkChanged = true;
              break;
            case BinaryOperator::Mul:
            case BinaryOperator::FMul:
              //++Mul;
              break;
            case BinaryOperator::UDiv:
            case BinaryOperator::SDiv:
            case BinaryOperator::FDiv:
              //++Div;
              break;
            case BinaryOperator::URem:
            case BinaryOperator::SRem:
            case BinaryOperator::FRem:
              //++Rem;
              break;
            case Instruction::Shl:
              //++Shi;
              break;
            case Instruction::LShr:
              //++Shi;
              break;
            case Instruction::AShr:
              //++Shi;
              break;
            case Instruction::And:
              (this->*funcAnd[llvm::cryptoutils->get_range(2)])(
                  cast<BinaryOperator>(inst));
              ++And;
              checkChanged = true;
              break;
            case Instruction::Or:
              (this->*funcOr[llvm::cryptoutils->get_range(2)])(
                  cast<BinaryOperator>(inst));
              ++Or;
              checkChanged = true;
              break;
            case Instruction::Xor:
              (this->*funcXor[llvm::cryptoutils->get_range(2)])(
                  cast<BinaryOperator>(inst));
              ++Xor;
              checkChanged = true;
              break;
            default:
              break;
            } // End switch
          } // End isBinaryOp
        } // End for basickblock
      } // End for Function
    } while (--times > 0); // for times
    return checkChanged;
  }

  // Implementation of a = b - (-c)
  void addNeg(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    // Create sub
    if (bo->getOpcode() == Instruction::Add) {
      op = BinaryOperator::CreateNeg(bo->getOperand(1), "", bo->getIterator());
      op = BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), op, "",
                                  bo);

      // Check signed wrap
      // op->setHasNoSignedWrap(bo->hasNoSignedWrap());
      // op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

      bo->replaceAllUsesWith(op);
    } /* else {
       op = BinaryOperator::CreateFNeg(bo->getOperand(1), "", bo);
       op = BinaryOperator::Create(Instruction::FSub, bo->getOperand(0), op, "",
                                   bo);
     }*/
  }

  // Implementation of a = -(-b + (-c))
  void addDoubleNeg(BinaryOperator *bo) {
    BinaryOperator *op, *op2 = NULL;
    UnaryOperator *op3, *op4;
    if (bo->getOpcode() == Instruction::Add) {
      op = BinaryOperator::CreateNeg(bo->getOperand(0), "", bo->getIterator());
      op2 = BinaryOperator::CreateNeg(bo->getOperand(1), "", bo->getIterator());
      op = BinaryOperator::Create(Instruction::Add, op, op2, "", bo);
      op = BinaryOperator::CreateNeg(op, "", bo->getIterator());
      bo->replaceAllUsesWith(op);
      // Check signed wrap
      // op->setHasNoSignedWrap(bo->hasNoSignedWrap());
      // op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());
    } else {
      op3 = UnaryOperator::CreateFNeg(bo->getOperand(0), "", bo);
      op4 = UnaryOperator::CreateFNeg(bo->getOperand(1), "", bo);
      op = BinaryOperator::Create(Instruction::FAdd, op3, op4, "", bo);
      op3 = UnaryOperator::CreateFNeg(op, "", bo);
      bo->replaceAllUsesWith(op3);
    }
  }

  // Implementation of  r = rand (); a = b + r; a = a + c; a = a - r
  void addRand(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    if (bo->getOpcode() == Instruction::Add) {
      Type *ty = bo->getType();
      ConstantInt *co = (ConstantInt *)ConstantInt::get(
          ty, llvm::cryptoutils->get_uint64_t());
      op = BinaryOperator::Create(Instruction::Add, bo->getOperand(0), co, "",
                                  bo);
      op = BinaryOperator::Create(Instruction::Add, op, bo->getOperand(1), "",
                                  bo);
      op = BinaryOperator::Create(Instruction::Sub, op, co, "", bo);

      // Check signed wrap
      // op->setHasNoSignedWrap(bo->hasNoSignedWrap());
      // op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

      bo->replaceAllUsesWith(op);
    }
    /* else {
        Type *ty = bo->getType();
        ConstantFP *co =
    (ConstantFP*)ConstantFP::get(ty,(float)llvm::cryptoutils->get_uint64_t());
        op =
    BinaryOperator::Create(Instruction::FAdd,bo->getOperand(0),co,"",bo); op =
    BinaryOperator::Create(Instruction::FAdd,op,bo->getOperand(1),"",bo); op =
    BinaryOperator::Create(Instruction::FSub,op,co,"",bo);
    } */
  }

  // Implementation of r = rand (); a = b - r; a = a + b; a = a + r
  void addRand2(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    if (bo->getOpcode() == Instruction::Add) {
      Type *ty = bo->getType();
      ConstantInt *co = (ConstantInt *)ConstantInt::get(
          ty, llvm::cryptoutils->get_uint64_t());
      op = BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), co, "",
                                  bo);
      op = BinaryOperator::Create(Instruction::Add, op, bo->getOperand(1), "",
                                  bo);
      op = BinaryOperator::Create(Instruction::Add, op, co, "", bo);

      // Check signed wrap
      // op->setHasNoSignedWrap(bo->hasNoSignedWrap());
      // op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

      bo->replaceAllUsesWith(op);
    }
    /* else {
        Type *ty = bo->getType();
        ConstantFP *co =
    (ConstantFP*)ConstantFP::get(ty,(float)llvm::cryptoutils->get_uint64_t());
        op =
    BinaryOperator::Create(Instruction::FAdd,bo->getOperand(0),co,"",bo); op =
    BinaryOperator::Create(Instruction::FAdd,op,bo->getOperand(1),"",bo); op =
    BinaryOperator::Create(Instruction::FSub,op,co,"",bo);
    } */
  }

  // Implementation of a = b + (-c)
  void subNeg(BinaryOperator *bo) {
    BinaryOperator *op = NULL;
    if (bo->getOpcode() == Instruction::Sub) {
      op = BinaryOperator::CreateNeg(bo->getOperand(1), "", bo->getIterator());
      op = BinaryOperator::Create(Instruction::Add, bo->getOperand(0), op, "",
                                  bo);
      // Check signed wrap
      // op->setHasNoSignedWrap(bo->hasNoSignedWrap());
      // op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());
    } else {
      auto op1 = UnaryOperator::CreateFNeg(bo->getOperand(1), "", bo);
      op = BinaryOperator::Create(Instruction::FAdd, bo->getOperand(0), op1, "",
                                  bo);
    }
    bo->replaceAllUsesWith(op);
  }

  // Implementation of  r = rand (); a = b + r; a = a - c; a = a - r
  void subRand(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    if (bo->getOpcode() == Instruction::Sub) {
      Type *ty = bo->getType();
      ConstantInt *co = (ConstantInt *)ConstantInt::get(
          ty, llvm::cryptoutils->get_uint64_t());
      op = BinaryOperator::Create(Instruction::Add, bo->getOperand(0), co, "",
                                  bo);
      op = BinaryOperator::Create(Instruction::Sub, op, bo->getOperand(1), "",
                                  bo);
      op = BinaryOperator::Create(Instruction::Sub, op, co, "", bo);

      // Check signed wrap
      // op->setHasNoSignedWrap(bo->hasNoSignedWrap());
      // op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

      bo->replaceAllUsesWith(op);
    }
    /* else {
        Type *ty = bo->getType();
        ConstantFP *co =
    (ConstantFP*)ConstantFP::get(ty,(float)llvm::cryptoutils->get_uint64_t());
        op =
    BinaryOperator::Create(Instruction::FAdd,bo->getOperand(0),co,"",bo); op =
    BinaryOperator::Create(Instruction::FSub,op,bo->getOperand(1),"",bo); op =
    BinaryOperator::Create(Instruction::FSub,op,co,"",bo);
    } */
  }

  // Implementation of  r = rand (); a = b - r; a = a - c; a = a + r
  void subRand2(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    if (bo->getOpcode() == Instruction::Sub) {
      Type *ty = bo->getType();
      ConstantInt *co = (ConstantInt *)ConstantInt::get(
          ty, llvm::cryptoutils->get_uint64_t());
      op = BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), co, "",
                                  bo);
      op = BinaryOperator::Create(Instruction::Sub, op, bo->getOperand(1), "",
                                  bo);
      op = BinaryOperator::Create(Instruction::Add, op, co, "", bo);

      // Check signed wrap
      // op->setHasNoSignedWrap(bo->hasNoSignedWrap());
      // op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

      bo->replaceAllUsesWith(op);
    }
    /* else {
        Type *ty = bo->getType();
        ConstantFP *co =
    (ConstantFP*)ConstantFP::get(ty,(float)llvm::cryptoutils->get_uint64_t());
        op =
    BinaryOperator::Create(Instruction::FSub,bo->getOperand(0),co,"",bo); op =
    BinaryOperator::Create(Instruction::FSub,op,bo->getOperand(1),"",bo); op =
    BinaryOperator::Create(Instruction::FAdd,op,co,"",bo);
    } */
  }

  // Implementation of a = b & c => a = (b^~c)& b
  void andSubstitution(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    // Create NOT on second operand => ~c
    op = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);

    // Create XOR => (b^~c)
    BinaryOperator *op1 =
        BinaryOperator::Create(Instruction::Xor, bo->getOperand(0), op, "", bo);

    // Create AND => (b^~c) & b
    op = BinaryOperator::Create(Instruction::And, op1, bo->getOperand(0), "",
                                bo);
    bo->replaceAllUsesWith(op);
  }

  // Implementation of a = a & b <=> ~(~a | ~b) & (r | ~r)
  void andSubstitutionRand(BinaryOperator *bo) {
    // Copy of the BinaryOperator type to create the random number with the
    // same type of the operands
    Type *ty = bo->getType();

    // r (Random number)
    ConstantInt *co =
        (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());

    // ~a
    BinaryOperator *op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);

    // ~b
    BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);

    // ~r
    BinaryOperator *opr = BinaryOperator::CreateNot(co, "", bo);

    // (~a | ~b)
    BinaryOperator *opa =
        BinaryOperator::Create(Instruction::Or, op, op1, "", bo);

    // (r | ~r)
    opr = BinaryOperator::Create(Instruction::Or, co, opr, "", bo);

    // ~(~a | ~b)
    op = BinaryOperator::CreateNot(opa, "", bo);

    // ~(~a | ~b) & (r | ~r)
    op = BinaryOperator::Create(Instruction::And, op, opr, "", bo);

    // We replace all the old AND operators with the new one transformed
    bo->replaceAllUsesWith(op);
  }

  // Implementation of a = a | b =>
  // a = (((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))) | (~(~a | ~b) & (r |
  // ~r))
  void orSubstitutionRand(BinaryOperator *bo) {

    Type *ty = bo->getType();
    ConstantInt *co =
        (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());

    // ~a
    BinaryOperator *op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);

    // ~b
    BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);

    // ~r
    BinaryOperator *op2 = BinaryOperator::CreateNot(co, "", bo);

    // ~a & r
    BinaryOperator *op3 =
        BinaryOperator::Create(Instruction::And, op, co, "", bo);

    // a & ~r
    BinaryOperator *op4 = BinaryOperator::Create(
        Instruction::And, bo->getOperand(0), op2, "", bo);

    // ~b & r
    BinaryOperator *op5 =
        BinaryOperator::Create(Instruction::And, op1, co, "", bo);

    // b & ~r
    BinaryOperator *op6 = BinaryOperator::Create(
        Instruction::And, bo->getOperand(1), op2, "", bo);

    // (~a & r) | (a & ~r)
    op3 = BinaryOperator::Create(Instruction::Or, op3, op4, "", bo);

    // (~b & r) | (b & ~r)
    op4 = BinaryOperator::Create(Instruction::Or, op5, op6, "", bo);

    // ((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))
    op5 = BinaryOperator::Create(Instruction::Xor, op3, op4, "", bo);

    // ~a | ~b
    op3 = BinaryOperator::Create(Instruction::Or, op, op1, "", bo);

    // ~(~a | ~b)
    op3 = BinaryOperator::CreateNot(op3, "", bo);

    // r | ~r
    op4 = BinaryOperator::Create(Instruction::Or, co, op2, "", bo);

    // ~(~a | ~b) & (r | ~r)
    op4 = BinaryOperator::Create(Instruction::And, op3, op4, "", bo);

    // (((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))) | (~(~a | ~b) & (r | ~r))
    op = BinaryOperator::Create(Instruction::Or, op5, op4, "", bo);
    bo->replaceAllUsesWith(op);
  }

  // Implementation of a = b | c => a = (b & c) | (b ^ c)
  void orSubstitution(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    // Creating first operand (b & c)
    op = BinaryOperator::Create(Instruction::And, bo->getOperand(0),
                                bo->getOperand(1), "", bo);

    // Creating second operand (b ^ c)
    BinaryOperator *op1 = BinaryOperator::Create(
        Instruction::Xor, bo->getOperand(0), bo->getOperand(1), "", bo);

    // final op
    op = BinaryOperator::Create(Instruction::Or, op, op1, "", bo);
    bo->replaceAllUsesWith(op);
  }

  // Implementation of a = a ^ b => a = (~a & b) | (a & ~b)
  void xorSubstitution(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    // Create NOT on first operand
    op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo); // ~a

    // Create AND
    op = BinaryOperator::Create(Instruction::And, bo->getOperand(1), op, "",
                                bo); // ~a & b

    // Create NOT on second operand
    BinaryOperator *op1 =
        BinaryOperator::CreateNot(bo->getOperand(1), "", bo); // ~b

    // Create AND
    op1 = BinaryOperator::Create(Instruction::And, bo->getOperand(0), op1, "",
                                 bo); // a & ~b

    // Create OR
    op = BinaryOperator::Create(Instruction::Or, op, op1, "",
                                bo); // (~a & b) | (a & ~b)
    bo->replaceAllUsesWith(op);
  }

  // implementation of a = a ^ b <=> (a ^ r) ^ (b ^ r) <=>
  // ((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))
  // note : r is a random number
  void xorSubstitutionRand(BinaryOperator *bo) {
    BinaryOperator *op = NULL;

    Type *ty = bo->getType();
    ConstantInt *co =
        (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());

    // ~a
    op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);

    // ~a & r
    op = BinaryOperator::Create(Instruction::And, co, op, "", bo);

    // ~r
    BinaryOperator *opr = BinaryOperator::CreateNot(co, "", bo);

    // a & ~r
    BinaryOperator *op1 = BinaryOperator::Create(
        Instruction::And, bo->getOperand(0), opr, "", bo);

    // ~b
    BinaryOperator *op2 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);

    // ~b & r
    op2 = BinaryOperator::Create(Instruction::And, op2, co, "", bo);

    // b & ~r
    BinaryOperator *op3 = BinaryOperator::Create(
        Instruction::And, bo->getOperand(1), opr, "", bo);

    // (~a & r) | (a & ~r)
    op = BinaryOperator::Create(Instruction::Or, op, op1, "", bo);

    // (~b & r) | (b & ~r)
    op1 = BinaryOperator::Create(Instruction::Or, op2, op3, "", bo);

    // ((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))
    op = BinaryOperator::Create(Instruction::Xor, op, op1, "", bo);
    bo->replaceAllUsesWith(op);
  }
};

char Substitution::ID = 0;
FunctionPass *llvm::createSubstitutionPass(ObfuscationOptions *argsOptions) {
  return new Substitution(argsOptions);
}