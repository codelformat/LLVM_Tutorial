#include "./src/EvalLLVM.h"

int main(int argc, char** argv)
{
    std::string program = R"(

      (printf "Value: %d\n" 42)
    
    )";

    EvalLLVM eval;
    eval.exec(program);
    return 0;
}