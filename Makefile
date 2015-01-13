CLANG_LEVEL := ../../..

TOOLNAME = doug-interp

# No plugins, optimize startup time.
TOOL_NO_EXPORTS = 1

include $(CLANG_LEVEL)/../../Makefile.config

LINK_COMPONENTS := $(TARGETS_TO_BUILD) asmparser bitreader support mc option
USEDLIBS = clangFrontend.a clangSerialization.a clangDriver.a \
		clangTooling.a clangParse.a clangSema.a \
		clangStaticAnalyzerFrontend.a clangStaticAnalyzerCheckers.a \
		clangStaticAnalyzerCore.a clangAnalysis.a clangRewriteFrontend.a \
		clangRewrite.a clangEdit.a clangAST.a clangLex.a clangBasic.a \
		clangASTMatchers.a clangBasic.a

include $(CLANG_LEVEL)/Makefile
