clang++ -o eval-llvm `llvm-config --cxxflags --ldflags --system-libs --libs core` -fexceptions eval-llvm.cpp

./eval-llvm

lli ./out.ll

echo $?

printf "\n"