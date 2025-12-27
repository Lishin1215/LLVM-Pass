
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
// MathOpt - pow(x, gamma) -> exp(gamma * log(x)) vectorization
// ============================================================================

// 判斷是不是2.2, 1/2.2, 2.4, 1/2.4
// 生成 + 插入IR 
static Value *tryBuildGammaTransform(IRBuilder<> &B, Module *M, Value *X, double ExpVal) {
    // [0] 列出支援的gamma值
    const double gammas[] = {
        2.2, 1.0/2.2,      
        2.4, 1.0/2.4,  
    };
    
    // 確定ExpVal是不是可以換的值
    bool isSupported = false;
    for (double g : gammas) {
        if (fabs(ExpVal - g) < 0.001) { // 其實就是ExpVal == 2.2，（就算expval就是2.2, 但實際還是不等於 2.200001 vs 2.2000002)，所以才要用絕對值
            isSupported = true;
            break;
        }
    }
    
    if (!isSupported)
        return nullptr;
    
    Type *Ty = X->getType();

    // [1] 生成＋插入 log(x) 的IR
    Function *LogFn = Intrinsic::getDeclaration(M, Intrinsic::log, {Ty});
    Value *LogX = B.CreateCall(LogFn, {X}, "log_step");
    
    // [2] 生成＋插入 log(x)*2.2 ---> Mul(log(x),2.2) 的IR
    Value *MulVal = B.CreateFMul(LogX, ConstantFP::get(Ty, ExpVal), "mul_step");

    // [3] 生成＋插入 exp(Mul(log(x),2.2)) 的IR
    Function *ExpFn = Intrinsic::getDeclaration(M, Intrinsic::exp, {Ty});
    Value *Result = B.CreateCall(ExpFn, {MulVal}, "exp_step");
    
    return Result;
}

// check every instruction (IR), 抓出 指數 是“常數”或“global variable”的pow
// 生成+插入 "新的IR"（exp + log）
// 刪除 "舊的IR" (pow): (1) usage (2) IR本身
// 返回 "IR有沒有改"
static bool optimizeMathFunctions(Function &F) {
    bool Changed = false;
    SmallVector<std::pair<Instruction *, Value *>, 8> Replacements; // 存 <舊的IR, 新的IR>
    IRBuilder<> Builder(F.getContext()); // 產生IR的工具
    Module *M = F.getParent(); // 用來取得log, exp的IR

    errs() << "[MathOpt] Scanning for pow/gamma optimizations\n";

    // [1]
    for (auto &BB : F)
        // [2]
        for (auto &I : BB) {
            // [3]
            auto *CI = dyn_cast<CallInst>(&I); // Instruction強制轉型成"CallInst" -> true/false
            if (!CI) continue;

            // [3.5]
            if (!CI->isFast()) { // 判斷instruction有沒有開fast-math flag (透compile的command line去開，讓每個instruction可以用 isFast()這個function)
                errs() << "  [MathOpt] Skipping pow - no fast-math flags\n";
                continue;
            }

            // [4]
            StringRef Name = CI->getCalledFunction()->getName();
            if (!(Name.contains("pow") || Name.contains("powf") ||
                  Name.contains("llvm.pow")))
                continue;

            // [5]
            if (CI->arg_size() != 2) continue;

            // special case: 把exp從 (float)2.2 -> (double)2.2) 變成"單純 2.2"
            Value *X = CI->getOperand(0);
            Value *Exponent = CI->getOperand(1);
            
            if (auto *Cast = dyn_cast<FPExtInst>(Exponent)) // ex. pow(x, (float)2.2 -> (double)2.2))
                Exponent = Cast->getOperand(0); // 2.2(float)

            double ExpVal = 0.0; // 放exp的值
            Value *Result = nullptr;// 用來放“新的IR” (再放到vector中)

            Builder.SetInsertPoint(&I); // 紀錄 新的IR “要 插入的位置”
            Builder.setFastMathFlags(CI->getFastMathFlags()); // 讓 新的IR 也可以"用fast-math flag"

            // [6]
            if (auto *Kc = dyn_cast<ConstantFP>(Exponent)) {
                // [7]
                ExpVal = Kc->getValueAPF().convertToDouble();
                Result = tryBuildGammaTransform(Builder, M, X, ExpVal); // （上面的function）生成+插入 新的IR
                if (Result) {
                    errs() << "  [MathOpt] Found const gamma @" << Kc->getName() 
                            << " = " << ExpVal << "\n";
                }
            }
            // [8]
            else if (auto *Ld = dyn_cast<LoadInst>(Exponent)) { // 變數 就是 LoadInst
                if (auto *GV = dyn_cast<GlobalVariable>(Ld->getPointerOperand())) {
                    if (GV->hasInitializer()) { // 確認有無"初始值" (ex. #define gamma 1.3)
                        if (auto *InitC = dyn_cast<ConstantFP>(GV->getInitializer())) {
                            ExpVal = InitC->getValueAPF().convertToDouble();
                            Result = tryBuildGammaTransform(Builder, M, X, ExpVal); // （上面的function）生成+插入 新的IR
                            if (Result) {
                                errs() << "  [MathOpt] Found global gamma @" << GV->getName() 
                                       << " = " << ExpVal << "\n";
                            }
                        }
                    }
                }
            }

            if (Result) {
                // 有順利生成LLVM IR
                errs() << "  [MathOpt] pow(x, " << ExpVal << ") → exp(" << ExpVal 
                       << "*log(x)) [vectorizable]\n";
                Replacements.push_back({&I, Result}); // 把 <舊IR, 新IR> 放到vector
                Changed = true;
            } 
        }

    // 刪除“舊的LLVM IR” (1)usage (2)舊IR本身
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
// Main ImgOptPass - Combines Clamp + MathOpt optimizations
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

            // 2. Auto-inject into optimization pipeline (O3 only)
            PB.registerVectorizerStartEPCallback(
                [](FunctionPassManager &FPM, OptimizationLevel Level) {
                    if (Level == OptimizationLevel::O3) {
                        errs() << "[ImgOptPass] Injecting into pipeline (vectorizer-start, O3)\n";
                        FPM.addPass(ImgOptPass());
                    }
                });
        }
    };
}
