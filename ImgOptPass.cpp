
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/ADT/StringSwitch.h"
#include <optional>
#include <cmath>
#include <cstdlib>
#include <string>
#include <algorithm>

using namespace llvm;


// ============================================================================
// [Part 2] MathOpt - pow(x, gamma) -> exp(gamma * log(x)) vectorization
// ============================================================================

// Helper: Check if gamma is supported and build log-exp transform if valid
// Returns the transformed IR value if gamma is supported, nullptr otherwise
static Value *tryBuildGammaTransform(IRBuilder<> &B, Module *M, Value *X, Value *Exponent, double ExpVal) {
    // Common gamma values
    const double gammas[] = {
        2.2, 1.0/2.2,      // sRGB / Rec.709
        2.4, 1.0/2.4,      // Adobe RGB
        1.8, 1.0/1.8,      // Apple RGB
        1.2,               // Gamma encoding
        1.0/3.0            // CIE Lab cube root
    };
    
    // Check if gamma is supported
    bool isSupported = false;
    for (double g : gammas) {
        if (fabs(ExpVal - g) < 0.001) {
            isSupported = true;
            break;
        }
    }
    
    if (!isSupported)
        return nullptr;
    
    // Build log-exp transformation: exp(gamma * log(x))
    Type *Ty = X->getType();
    Function *LogFn = Intrinsic::getDeclaration(M, Intrinsic::log, {Ty});
    Function *ExpFn = Intrinsic::getDeclaration(M, Intrinsic::exp, {Ty});
    
    Value *LogX = B.CreateCall(LogFn, {X}, "log_step");
    Value *MulVal = B.CreateFMul(LogX, Exponent, "mul_step");
    Value *Result = B.CreateCall(ExpFn, {MulVal}, "exp_step");
    
    return Result;
}

static bool optimizeMathFunctions(Function &F) {
    bool Changed = false;
    SmallVector<std::pair<Instruction *, Value *>, 8> Replacements;
    IRBuilder<> Builder(F.getContext());
    Module *M = F.getParent();

    errs() << "[MathOpt] Scanning for pow/gamma optimizations\n";

    for (auto &BB : F)
        for (auto &I : BB) {
            auto *CI = dyn_cast<CallInst>(&I);
            if (!CI) continue;

            Function *Callee = CI->getCalledFunction();
            if (!Callee) continue;

            StringRef Name = Callee->getName();
            if (!(Name.contains("pow") || Name.contains("powf") ||
                  Name.contains("llvm.pow")))
                continue;

            if (CI->arg_size() != 2) continue;

            // Check for fast-math flags
            FastMathFlags FMF;
            if (auto *FPMO = dyn_cast<FPMathOperator>(CI))
                FMF = FPMO->getFastMathFlags();
            
            if (!FMF.isFast()) {
                errs() << "  [MathOpt] Skipping pow - no fast-math flags\n";
                continue;
            }

            Builder.SetInsertPoint(&I);
            Builder.setFastMathFlags(FMF);
            
            Value *X = CI->getOperand(0);
            Value *Exponent = CI->getOperand(1);

            // Unified gamma value extraction logic
            Value *GammaSource = Exponent;
            
            // Strip fpext if exists
            if (auto *Cast = dyn_cast<FPExtInst>(Exponent))
                GammaSource = Cast->getOperand(0);

            double ExpVal = 0.0;
            Value *Result = nullptr;

            // Try 1: Direct constant
            if (auto *Kc = dyn_cast_or_null<ConstantFP>(GammaSource->stripPointerCasts())) {
                ExpVal = Kc->getValueAPF().convertToDouble();
                Result = tryBuildGammaTransform(Builder, M, X, Exponent, ExpVal);
            }
            // Try 2: Load from global variable (LLVM won't constant-propagate these)
            else if (auto *Ld = dyn_cast<LoadInst>(GammaSource)) {
                if (auto *GV = dyn_cast<GlobalVariable>(Ld->getPointerOperand())) {
                    if (GV->hasInitializer()) {
                        if (auto *InitC = dyn_cast<ConstantFP>(GV->getInitializer())) {
                            ExpVal = InitC->getValueAPF().convertToDouble();
                            Result = tryBuildGammaTransform(Builder, M, X, Exponent, ExpVal);
                            if (Result) {
                                errs() << "  [MathOpt] Found global gamma @" << GV->getName() 
                                       << " = " << ExpVal << "\n";
                            }
                        }
                    }
                }
            }

            if (Result) {
                errs() << "  [MathOpt] pow(x, " << ExpVal << ") → exp(" << ExpVal 
                       << "*log(x)) [vectorizable]\n";
                Replacements.push_back({&I, Result});
                Changed = true;
            } else if (ExpVal != 0.0) {
                errs() << "  [MathOpt] Skipping pow(x, " << ExpVal << ") - not a supported gamma value\n";
            }
        }

    for (auto &P : Replacements) {
        Instruction *Old = P.first;
        Value *NewV = P.second;
        Old->replaceAllUsesWith(NewV);
        Old->eraseFromParent();
    }

    errs() << (Changed ? "[MathOpt] Applied math optimizations\n"
                       : "[MathOpt] No math optimizations found\n");
    return Changed;
}

// ============================================================================
// [Part 3] Main ImgOptPass - Combines Clamp + MathOpt optimizations
// ============================================================================
struct ImgOptPass : public PassInfoMixin<ImgOptPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        errs() << "<<< [ImgOptPass] Processing function: " << F.getName() << " >>>\n";
        bool Changed = false;
        // Changed |= optimizeClamp(F);          // Clamp optimization disabled
        Changed |= optimizeMathFunctions(F);     // Only math optimization enabled
        if (Changed) {
            errs() << "[ImgOptPass] Changes applied\n";
            return PreservedAnalyses::none();
        }
        errs() << "[ImgOptPass] No changes\n";
        return PreservedAnalyses::all();
    }
};

// ============================================================================
// [Plugin Registration]
// ============================================================================
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "STB Image Optimization Pass", "1.0.0",
        [](PassBuilder &PB) {
            // 1. Register pass by name
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "img-opt") {
                        FPM.addPass(ImgOptPass());
                        return true;
                    }
                    return false;
                });

            // 2. Auto-inject into optimization pipeline (all levels)
            PB.registerVectorizerStartEPCallback(
                [](FunctionPassManager &FPM, OptimizationLevel Level) {
                    errs() << "[ImgOptPass] Injecting into pipeline (vectorizer-start)\n";
                    FPM.addPass(ImgOptPass());
                });
        }
    };
}
