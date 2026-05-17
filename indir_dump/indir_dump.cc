#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"

// Remember to compile with `g -fno-discard-value-names` to get names (of the
// BB) and line values (for the src)

using namespace llvm;

namespace {

struct indirDump : public PassInfoMixin<indirDump> {

  // Checks if the instruction is NOT interesting
  bool notInteresting(Instruction &I) {
    switch (I.getOpcode()) {
    case Instruction::IndirectBr:
      return false;
    case Instruction::Call:
    case Instruction::Invoke:
    case Instruction::CallBr: {
      CallBase &call = cast<CallBase>(I);
      if (call.isIndirectCall())
        return false;
    }
    }
    return true;
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    LLVMContext &Ctx = M.getContext();
    const DataLayout &DL = M.getDataLayout();

    Type *IntptrTy = DL.getIntPtrType(Ctx);
    Type *VoidTy = Type::getVoidTy(Ctx);
    PointerType *PtrTy = PointerType::getUnqual(Ctx);
    Type *Int32Ty = Type::getInt32Ty(Ctx);

    FunctionType *LogFuncTy =
        FunctionType::get(VoidTy, {PtrTy, Int32Ty, IntptrTy}, false);
    FunctionCallee LogFunc = M.getOrInsertFunction("__log_indir", LogFuncTy);

    bool Modified = false;

    // Classic iter
    for (Function &F : M) {
      // Skips external function declarations
      if (F.isDeclaration())
        continue;

      for (BasicBlock &BB : F) {
        for (auto it = BB.begin(); it != BB.end();) {
          Instruction &I = *it++;

          if (notInteresting(I))
            continue;

          // errs() << "Found indirect jump in: " << F.getName() << "\n";

          IRBuilder<> IRB(&I);
          Value *TargetAddrInt = nullptr;

          if (IndirectBrInst *ibr = dyn_cast<IndirectBrInst>(&I)) {
            TargetAddrInt = IRB.CreatePtrToInt(ibr->getAddress(), IntptrTy);
          } else if (CallBase *call = dyn_cast<CallBase>(&I)) {
            TargetAddrInt =
                IRB.CreatePtrToInt(call->getCalledOperand(), IntptrTy);
          } else {
            // Should be unreachable
            llvm_unreachable(
                "InjectIndirCoverage: unexpected indirect instruction type");
          }

          if (TargetAddrInt) {
            std::string SrcInfoStr =
                F.getName().str() + "::" + BB.getName().str();

            // Inject call
            Value *SrcInfo = IRB.CreateGlobalString(SrcInfoStr, "src_info");
            unsigned LineNum = 0;
            if (const DebugLoc &Loc = I.getDebugLoc()) {
              LineNum = Loc.getLine();
            }

            Value *SrcLineVal = ConstantInt::get(Int32Ty, LineNum);
            IRB.CreateCall(LogFunc, {SrcInfo, SrcLineVal, TargetAddrInt});
            Modified = true;
          }
        }
      }
    }

    // Return `none()` if modified, `all()` otherwise
    return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // namespace

// Registration for the NPM
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "indirDump", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // opt support
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "indir-dump") {
                    MPM.addPass(indirDump());
                    return true;
                  }
                  return false;
                });
            // Pipeline support
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                  MPM.addPass(indirDump());
                });
          }};
}
