#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#if LLVM_VERSION_MAJOR >= 21
#include "llvm/Plugins/PassPlugin.h"
#else
#include "llvm/Passes/PassPlugin.h"
#endif
// Remember to compile with `g -fno-discard-value-names` to get names (of the
// BB) and line values (for the src)

using namespace llvm;

namespace {

std::string getStableLocation(const Instruction &I) {
  const DebugLoc &Loc = I.getDebugLoc();
  // If no debug info
  if (!Loc) {
    return "UNKNOWN_FILE:0";
  }

  DILocation *DILoc = Loc.get();
  // Get root call site, no inlining
  while (DILoc->getInlinedAt()) {
    DILoc = DILoc->getInlinedAt();
  }

  // Extract base filename ("ldo.c" from "/src/lua/ldo.c")
  std::string Filename = DILoc->getFilename().str();
  size_t SlashIdx = Filename.find_last_of('/');
  if (SlashIdx != std::string::npos) {
    Filename = Filename.substr(SlashIdx + 1);
  }

  return Filename + ":" + std::to_string(DILoc->getLine());
}

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

    FunctionType *LogFuncTy =
        FunctionType::get(VoidTy, {PtrTy, IntptrTy}, false);
    FunctionCallee LogFunc = M.getOrInsertFunction("__log_indir", LogFuncTy);

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
            std::string SrcInfoStr = getStableLocation(I);

            // Inject call
            Value *SrcInfo = IRB.CreateGlobalString(SrcInfoStr, "src_info");

            IRB.CreateCall(LogFunc, {SrcInfo, TargetAddrInt});
          }
        }
      }
    }

    // Return `none()` if modified, `all()` otherwise
    return PreservedAnalyses::none();
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
