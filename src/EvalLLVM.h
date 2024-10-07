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
#include "./Environment.h"

using syntax::EvalParser;
using Env = std::shared_ptr<Environment>;

class EvalLLVM
{
public:
    /**
     * 构造函数，初始化解析器、LLVM上下文、模块和IR构建器。
     * 还调用setupExternalFunctions()和setupGlobalEnvironment()来设置外部函数和全局环境。
     */
    EvalLLVM() : parser(std::make_unique<EvalParser>())
    {
        moduleInit();
        setupExternalFunctions();
        setupGlobalEnvironment();
    };

    /**
     * 执行传入的程序
     * @param program 传入的程序字符串
     */
    void exec(const std::string &program)
    {
        // 1. 将程序解析为AST
        auto ast = parser->parse("(begin " + program + ")");

        // 2. 将AST编译成LLVM IR
        compile(ast, GlobalEnv);

        // 打印LLVM IR
        module->print(llvm::outs(), nullptr);

        std::cout << "\n";

        // 3. 将LLVM IR保存到文件
        saveModuleToFile("./out.ll");
    }

    void run();

private:
    /**
     * 初始化LLVM模块、上下文和IR构建器
     */
    void moduleInit()
    {
        ctx = std::make_unique<llvm::LLVMContext>();
        module = std::make_unique<llvm::Module>("EvalLLVM", *ctx);
        builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
    }

    /**
     * 设置全局环境，定义一些全局变量
     */
    void setupGlobalEnvironment()
    {
        std::map<std::string, llvm::Value *> globalObject{
            {"VERSION", builder->getInt32(42)}};

        std::map<std::string, llvm::Value *> globalRec{};

        for (auto &entry : globalObject)
        {
            globalRec[entry.first] =
                createGlobalVariable(entry.first, (llvm::Constant *)entry.second);
        }

        GlobalEnv = std::make_shared<Environment>(globalRec, nullptr);
    }

    /**
     * 将生成的模块保存到文件
     * @param filename 文件名
     */
    void saveModuleToFile(const std::string &filename)
    {
        std::error_code ec;
        llvm::raw_fd_ostream outLL(filename, ec);
        module->print(outLL, nullptr);
        outLL.close();
    }

    /**
     * 编译AST为LLVM IR
     * @param ast 抽象语法树
     */
    void compile(const Exp &ast, Env env)
    {
        // 1. 创建main函数
        fn = createFunction(
            "main",
            llvm::FunctionType::get(builder->getInt32Ty(), false),
            GlobalEnv);

        // 创建全局变量VERSION
        createGlobalVariable("VERSION", builder->getInt32(42));

        // 2. 编译函数主体
        gen(ast, GlobalEnv);

        // 生成返回0的指令
        builder->CreateRet(builder->getInt32(0));
    }

    /**
     * Main Compile Loop
     * @param ast 抽象语法树
     * @param env 环境
     * @return 生成的LLVM值
     */
    llvm::Value *gen(const Exp &ast, Env env)
    {
        switch (ast.type)
        {
        case ExpType::NUMBER:
            // 处理数字类型，返回一个32位整数
            return builder->getInt32(ast.number);
        case ExpType::STRING:
        {
            // 处理字符串类型，替换换行符并创建全局字符串指针
            auto re = std::regex("\\\\n");
            auto str = std::regex_replace(ast.string, re, "\n");
            return builder->CreateGlobalStringPtr(str);
        }
        case ExpType::SYMBOL:
            // 处理布尔类型
            if (ast.string == "true")
            {
                return builder->getInt1(1);
            }
            else if (ast.string == "false")
            {
                return builder->getInt1(0);
            }
            else
            {
                // 处理变量
                auto varName = ast.string;
                auto value = env->lookup(varName);

                // 如果是全局变量，返回加载的变量值
                if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value))
                {
                    return builder->CreateLoad(globalVar->getInitializer()->getType(),
                                               globalVar, varName);
                }
            }
        case ExpType::LIST:
            // 处理列表类型
            auto tag = ast.list[0];

            if (tag.type == ExpType::SYMBOL)
            {
                auto op = tag.string;

                // 处理变量声明
                if (op == "var")
                {
                    auto varName = ast.list[1].string;
                    auto init = gen(ast.list[2], env);
                    return createGlobalVariable(varName, (llvm::Constant *)init)
                        ->getInitializer();
                }
                // 处理"begin"块
                else if (op == "begin")
                {
                    llvm::Value *blockRes;
                    for (auto i = 1; i < ast.list.size(); i++)
                    {
                        blockRes = gen(ast.list[i], env);
                    }
                    return blockRes;
                }
                // 处理"printf"函数调用
                else if (op == "printf")
                {
                    auto printfFn = module->getFunction("printf");
                    std::vector<llvm::Value *> args{};

                    for (int i = 1; i < ast.list.size(); i++)
                    {
                        args.push_back(gen(ast.list[i], env));
                    }

                    return builder->CreateCall(printfFn, args);
                }
            }
        }

        // 默认返回0
        return builder->getInt32(0);
    }

    /**
     * 创建全局变量
     * @param name 变量名
     * @param init 初始值
     * @return 创建的全局变量
     */
    llvm::GlobalVariable *createGlobalVariable(const std::string &name,
                                               llvm::Constant *init = nullptr)
    {
        module->getOrInsertGlobal(name, init->getType());
        auto variable = module->getNamedGlobal(name);
        variable->setAlignment(llvm::MaybeAlign(4));
        variable->setConstant(false);
        variable->setInitializer(init);
        return variable;
    }

    /**
     * 定义外部函数，如printf
     */
    void setupExternalFunctions()
    {
        auto bytePtrTy = builder->getInt8Ty()->getPointerTo();

        module->getOrInsertFunction("printf",
                                    llvm::FunctionType::get(
                                        builder->getInt32Ty(),
                                        bytePtrTy,
                                        true));
    }

    /**
     * 创建函数
     * @param name 函数名
     * @param type 函数类型
     * @param env 环境
     * @return 创建的函数
     */
    llvm::Function *createFunction(const std::string &name,
                                   llvm::FunctionType *type,
                                   Env env)
    {
        auto fn = module->getFunction(name);

        if (!fn)
        {
            fn = createFunctionProto(name, type, env);
        }

        createFunctionBlock(fn);
        return fn;
    }

    /**
     * 创建函数原型
     * @param name 函数名
     * @param type 函数类型
     * @param env 环境
     * @return 创建的函数原型
     */
    llvm::Function *createFunctionProto(const std::string &name,
                                        llvm::FunctionType *type,
                                        Env env)
    {
        // llvm::Function::ExternalLinkage 表示函数是外部链接的，即可以在模块外部定义
        // name 是函数的名称
        // *module 是包含该函数的模块
        auto fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, *module);

        // 验证函数
        // verifyFunction(*fn);

        // 将函数添加到环境中
        env->define(name, fn);
        return fn;
    }

    /**
     * 创建函数体
     * @param fn 函数指针
     */
    void createFunctionBlock(llvm::Function *fn)
    {
        auto entry = createBasicBlock("entry", fn);
        // 设置插入点为entry
        // 插入点是IRBuilder用于插入新指令的位置
        // 将当前的插入点设置为entry块的开始位置
        builder->SetInsertPoint(entry);
    }

    /**
     * 创建基本块
     * @param name 基本块名称
     * @param fn 所属函数
     * @return 创建的基本块
     */
    llvm::BasicBlock *createBasicBlock(const std::string &name,
                                       llvm::Function *fn = nullptr)
    {
        // 返回一个新创建的BasicBlock，其名称由name指定，并将其附加到函数fn中
        return llvm::BasicBlock::Create(*ctx, name, fn);
    }

    /**
     * 解析器
     */
    std::unique_ptr<EvalParser> parser;

    /**
     * 环境
     */
    std::shared_ptr<Environment> GlobalEnv;

    /**
     * 当前正在编译的函数
     */
    llvm::Function *fn = nullptr;

    /**
     * LLVM上下文
     */
    std::unique_ptr<llvm::LLVMContext> ctx;

    /**
     * LLVM模块
     */
    std::unique_ptr<llvm::Module> module;

    /**
     * LLVM IR 构建器
     */
    std::unique_ptr<llvm::IRBuilder<>> builder;
};

#endif // EVAL_LLVM_H