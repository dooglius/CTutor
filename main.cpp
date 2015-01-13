#include <limits.h>
#include <unistd.h>
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "help.h"
#include "types.h"

using namespace clang;
using namespace llvm;

void exit_premature(void){
	llvm::outs() << "The interpreted doesn't know how to handle this\n";
	exit(0);
}

EmuVal* cast_to(EmuVal* orig, QualType t){
	const Type *ty = t.getTypePtr();
	if(isa<BuiltinType>(ty)){
		const BuiltinType *type = (const BuiltinType*)ty;
		switch(type->getKind()){
		case BuiltinType::Int:
			if(orig == nullptr) return new EmuInt(STATUS_UNINITIALIZED);
			if(orig->obj_type == EMU_TYPE_INT) return orig;
			return new EmuInt(STATUS_UNDEFINED);
		default:
			type->dump();
			exit_premature();
			return nullptr;
		}
	}
	ty->dump();
	exit_premature();
	return nullptr;
}

void add_global(llvm::StringRef name, EmuVal *val){
	llvm::outs() << "Adding global with name '" << name << "' and value ";
	val->print();
	llvm::outs() << "\n";
}

mem_item* eval_lexpr(Expr* e){
}

EmuVal* eval_rexpr(Expr* e){
	if(isa<IntegerLiteral>(e)){
		IntegerLiteral *obj = (IntegerLiteral*)e;
		APInt i = obj->getValue();
		if(i.slt(EMU_MIN_INT) || i.sgt(EMU_MAX_INT)){
			exit_premature();
		}

		return new EmuInt(i.getSExtValue());
	} else if(isa<UnaryOperator>(e)){
		UnaryOperator *obj = (UnaryOperator*)e;
		switch(obj->getOpcode()){
		case UO_Deref:
			= eval_lexpr(obj->getSubExpr());
			EmuPtr* ans = new EmuPtr();
		default:
			exit_premature();
		}
	}
	exit_premature();
	return nullptr;
}

class Initializer : public RecursiveASTVisitor<Initializer> {
public:
Initializer(){}

bool VisitVarDecl(VarDecl *obj) {
	if(!obj->hasGlobalStorage()) return true;

	Expr* init = obj->getInit();
	EmuVal *val;
	if(init == nullptr) {
		val = cast_to(nullptr,obj->getType());
	} else {
		val = cast_to(eval_rexpr(init),obj->getType());
	}
	add_global(obj->getName(), val);
	return true;
}
};


class Interpreter : public ASTConsumer {
public:

virtual void HandleTranslationUnit(ASTContext &context) {
	TranslationUnitDecl* file = context.getTranslationUnitDecl();
	file->dump();
	Initializer init;
	init.TraverseDecl(file);
}

private:
};

class InterpretAction : public ASTFrontendAction {
public:
virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, llvm::StringRef) {
	Interpreter* ans = new Interpreter();
	return std::unique_ptr<ASTConsumer>(ans);
}
};

int main(int argc, const char ** argv) {
	tooling::CommonOptionsParser argParser(argc, argv, MyHelp);
	tooling::ClangTool tool(argParser.getCompilations(), argParser.getSourcePathList());

	return tool.run(tooling::newFrontendActionFactory<InterpretAction>().get());
}
