CLANG_LEVEL := ../../..

TOOLNAME = doug-interp

# No plugins, optimize startup time.
TOOL_NO_EXPORTS = 1

include $(CLANG_LEVEL)/../../Makefile.config

LINK_COMPONENTS := $(TARGETS_TO_BUILD) asmparser option
USEDLIBS = clangFrontend.a clangSerialization.a clangDriver.a clangTooling.a clangParse.a clangSema.a clangAnalysis.a clangEdit.a clangAST.a clangLex.a clangBasic.a

include $(CLANG_LEVEL)/Makefile
