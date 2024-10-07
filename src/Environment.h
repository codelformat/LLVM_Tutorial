

#ifndef Environment_h
#define Environment_h

#include <map>
#include <memory>
#include <string>

#include "llvm/IR/Value.h"
#include "Logger.h"

// 允许类的成员函数从类的实例内部安全地创建指向自身的 std::shared_ptr
class Environment : public std::enable_shared_from_this<Environment>
{
public:
    /**
     * 构造函数
     * @param record 存储变量名和对应LLVM值的映射
     * @param parent 可选的父环境，默认为nullptr
     */
    Environment(std::map<std::string, llvm::Value*> record,
                std::shared_ptr<Environment> parent = nullptr)
        : record_(record), parent_(parent)
    {
    }

    /**
     * 定义一个变量并存储它的值
     * @param name 变量名
     * @param value 变量对应的LLVM值
     * @return 返回存储的LLVM值
     */
    llvm::Value* define(const std::string& name, llvm::Value* value){
        record_[name] = value;  // 将变量名和值存入当前环境的记录中
        return value;  // 返回存储的值
    }

    /**
     * 查找变量的值
     * @param name 变量名
     * @return 返回查找到的LLVM值
     */
    llvm::Value* lookup(const std::string& name){
        return resolve(name)->record_[name];  // 使用resolve函数找到存储该变量的环境，并返回对应的值
    }


private:
    /**
     * 递归查找变量所在的环境
     * @param name 变量名
     * @return 返回一个包含该变量的环境
     */
    std::shared_ptr<Environment> resolve(const std::string& name){
        // 如果当前环境包含该变量，返回当前环境
        if (record_.count(name) > 0)
            return shared_from_this();  // 使用shared_from_this()返回当前对象的shared_ptr引用

        // 如果没有父环境，说明该变量未定义，抛出错误
        if (parent_ == nullptr)
            DIE << "Undefined variable " << name;  

        // 如果当前环境没有该变量，递归调用父环境进行查找
        return parent_->resolve(name);  
    }

    /**
     * 父环境的指针
     * 如果当前环境找不到变量，会递归查找父环境。
     */
    std::shared_ptr<Environment> parent_;

    /**
     * 当前环境中存储的变量记录
     * 键为变量名，值为对应的LLVM值
     */
    std::map<std::string, llvm::Value*> record_;
};

#endif