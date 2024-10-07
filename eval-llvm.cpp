#include "./src/EvalLLVM.h"

int main(int argc, char** argv)
{
    std::string program = R"(
    
    (var VERSION 45)

    // (begin
    //     (var VERSION "Hello")
    //     (printf "Version: %s\n\n" VERSION)
    
    (printf "Version: %d\n\n" VERSION)
    

    )";

    EvalLLVM eval;
    eval.exec(program);
    return 0;
}