set(LLVM_LINK_COMPONENTS support)
set(LLVM_USED_LIBS clangTooling clangBasic clangAST)

add_clang_executable(doug-interp main.cpp)
target_link_libraries(doug-interp clangTooling clangBasic clangASTMatchers)

