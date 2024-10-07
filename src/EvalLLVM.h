/**
 * Eval to LLVM IR Compiler
 */

#ifndef EVAL_LLVM_H
#define EVAL_LLVM_H

#include <iostream>
#include <regex>
#include <string>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

#include "./parser/EvalParser.h"

using syntax::EvalParser;

class EvalLLVM
{
public:
    EvalLLVM() : parser(std::make_unique<EvalParser>())
    {
        moduleInit();
        setupExternalFunctions();
    };

    /**
     * Executes a program
     */
    void exec(const std::string &program)
    {
        // 1. Parse the program into an AST
        auto ast = parser->parse(program);

        // 2. Compile the AST into LLVM IR
        compile(ast);

        // Print the LLVM IR
        module->print(llvm::outs(), nullptr);

        std::cout << "\n";

        // 3. Save the LLVM IR to a file
        saveModuleToFile("./out.ll");
    }
    void run();

private:
    void moduleInit()
    {
        ctx = std::make_unique<llvm::LLVMContext>();
        module = std::make_unique<llvm::Module>("EvalLLVM", *ctx);
        builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
    }

    void saveModuleToFile(const std::string &filename)
    {
        std::error_code ec;
        llvm::raw_fd_ostream outLL(filename, ec);
        module->print(outLL, nullptr);
        outLL.close();
    }

    void compile(const Exp &ast)
    {
        // 1. Create a main function
        fn = createFunction("main", llvm::FunctionType::get(/* return type*/ builder->getInt32Ty(),
                                                            /* isVarArg */ false));

        // 2. Compile the main body
        gen(ast);

        builder->CreateRet(builder->getInt32(0));
    }

    /**
     * Main Compile Loop
     */
    llvm::Value *gen(const Exp &ast)
    {

        switch (ast.type)
        {
        case ExpType::NUMBER:
            return builder->getInt32(ast.number);
        case ExpType::STRING:
        {
            auto re = std::regex("\\\\n");
            auto str = std::regex_replace(ast.string, re, "\n");
            // Create a global string pointer
            return builder->CreateGlobalStringPtr(str);
        }
        case ExpType::SYMBOL:
            // TODO: Implement symbol handling
            return builder->getInt32(0);
        case ExpType::LIST:
            auto tag = ast.list[0];

            if (tag.type == ExpType::SYMBOL)
            {
                auto op = tag.string;

                // ----------------------------------------
                // printf external call
                //
                // (printf "Value: %d" 42)
                // ----------------------------------------
                if (op == "printf")
                {
                    auto printfFn = module->getFunction("printf");

                    std::vector<llvm::Value *> args{};

                    for (int i = 1; i < ast.list.size(); i++)
                    {
                        args.push_back(gen(ast.list[i]));
                    }

                    return builder->CreateCall(printfFn, args);
                }
            }
        }

        // Unreachable
        return builder->getInt32(0);
    }

    /**
     * Define external functions
     */
    void setupExternalFunctions()
    {
        auto bytePtrTy = builder->getInt8Ty()->getPointerTo();

        module->getOrInsertFunction("printf",
                                    llvm::FunctionType::get(
                                        /* return type */ builder->getInt32Ty(),
                                        /* args */ bytePtrTy,
                                        /* isVarArg */ true));
    }

    /**
     * Create a main function
     */
    llvm::Function *createFunction(const std::string &name,
                                   llvm::FunctionType *type)
    {
        // Function prototype if it exists
        auto fn = module->getFunction(name);

        // If not, create it
        if (!fn)
        {
            fn = createFunctionProto(name, type);
        }

        createFunctionBlock(fn);
        return fn;
    }

    /**
     * Create a function prototype
     */
    llvm::Function *createFunctionProto(const std::string &name,
                                        llvm::FunctionType *type)
    {
        auto fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, *module);

        verifyFunction(*fn);
        return fn;
    }

    /**
     * Create a function block
     */
    void createFunctionBlock(llvm::Function *fn)
    {
        auto entry = createBasicBlock("entry", fn);
        builder->SetInsertPoint(entry);
    }

    /**
     * Create a basic block
     */
    llvm::BasicBlock *createBasicBlock(const std::string &name,
                                       llvm::Function *fn = nullptr)
    {
        return llvm::BasicBlock::Create(*ctx, name, fn);
    }

    /**
     * Parser
     */
    std::unique_ptr<EvalParser> parser;

    /**
     * Currently Compiling Function
     */
    llvm::Function *fn = nullptr;

    /**
     * LLVM Context
     */
    std::unique_ptr<llvm::LLVMContext> ctx;

    std::unique_ptr<llvm::Module> module;

    std::unique_ptr<llvm::IRBuilder<>> builder;
};

#endif