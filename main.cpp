#include <limits.h>
#include <unistd.h>
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "cast.h"
#include "debug.h"
#include "exit.h"
#include "help.h"
#include "main.h"
#include "mem.h"
#include "types.h"

using namespace clang;
using namespace llvm;

unsigned int currloc=0;

lvalue eval_lexpr(const Expr* e){
	if(isa<DeclRefExpr>(e)){
		const DeclRefExpr* expr = (const DeclRefExpr*)e;
		std::string name = expr->getDecl()->getNameAsString();
		for(auto stack_it = stack_vars.cbegin(); stack_it != stack_vars.cend(); ++stack_it){
			std::unordered_map<std::string, lvalue> map = *stack_it;
			auto it = map.find(name);
			if(it != map.end()){
				return it->second;
			}
		}
		auto it = global_vars.find(name);
		if(it == global_vars.end()){
			err_exit("Variable not found\n");
		} else {
			return it->second;
		}
	} else if(isa<ArraySubscriptExpr>(e)){
		const ArraySubscriptExpr* expr = (const ArraySubscriptExpr*)e;
		const EmuVal* base = eval_rexpr(expr->getBase());
		const EmuVal* idx = eval_rexpr(expr->getIdx());
		const Type* basetype = base->obj_type.getCanonicalType().getTypePtr();
		if(isa<PointerType>(basetype)){
			cant_handle();
		}
		const Type* t = idx->obj_type.getCanonicalType().getTypePtr();
		if(!isa<BuiltinType>(t) || (((const BuiltinType*)t)->getKind() != BuiltinType::Int)){
			cant_handle();
		}
		const EmuPtr* emup = (const EmuPtr*)base;
		mem_ptr p = mem_ptr(emup->u.block, emup->offset);
		size_t offset = p.offset + ((const EmuInt*)idx)->val;
		return lvalue(p.block, ((const PointerType*)basetype)->getPointeeType(), offset);
	}
	cant_handle();
}

// caller must free returned value
const EmuVal* eval_rexpr(const Expr* e){
	outs() << "DEBUG: eval_rexpr on\n";
	e->dump();
	if(isa<IntegerLiteral>(e)){
		const IntegerLiteral *obj = (const IntegerLiteral*)e;
		APInt i = obj->getValue();
		if(i.slt(EMU_MIN_INT) || i.sgt(EMU_MAX_INT)){
			outs() << "DEBUG: int too big or too small\n";
			cant_handle();
		}
		return new EmuInt(i.getSExtValue());
	} else if(isa<UnaryOperator>(e)){
		const UnaryOperator *obj = (const UnaryOperator*)e;
		switch(obj->getOpcode()){
		case UO_AddrOf:
		{
			lvalue arg = eval_lexpr(obj->getSubExpr());
			return new EmuPtr(arg.ptr);
		}
		case UO_Deref:
		case UO_Extension:
		case UO_Imag:
		case UO_Real:
		case UO_LNot:
		case UO_PostInc:
		case UO_PostDec:
		case UO_PreInc:
		case UO_PreDec:
		case UO_Plus:
		case UO_Minus:
		case UO_Not:
		default:
			llvm::outs() << "Got opcode " << obj->getOpcode() << "\n";
			cant_handle();
		}
	} else if(isa<BinaryOperator>(e)){
		const BinaryOperator* ex = (const BinaryOperator*)e;
		BinaryOperatorKind op = ex->getOpcode();

		// right always an rexpr
		const EmuVal *right = eval_rexpr(ex->getRHS());

		switch(op){
		case BO_Assign:
		{
			lvalue left = eval_lexpr(ex->getLHS());
			const EmuVal* ans = cast_to(right, left.type);
			if(ans != right) delete right;
			left.ptr.block->write(ans, left.ptr.offset);
			return ans;
		}
		case BO_LT:
		case BO_GT:
		case BO_LE:
		case BO_GE:
		case BO_EQ:
		case BO_NE:
		{
			const EmuVal *left = eval_rexpr(ex->getLHS());
			const Type* tl = left->obj_type.getCanonicalType().getTypePtr();
			const Type* tr = right->obj_type.getCanonicalType().getTypePtr();
			if(!isa<BuiltinType>(tl) || (((const BuiltinType*)tl)->getKind() != BuiltinType::Int)){
				cant_handle();
			}
			if(!isa<BuiltinType>(tr) || (((const BuiltinType*)tr)->getKind() != BuiltinType::Int)){
				cant_handle();
			}
			int32_t lval = ((const EmuInt*)left)->val;
			int32_t rval = ((const EmuInt*)right)->val;
			delete left;
			delete right;
			int32_t ans;
			if(op == BO_LT)     ans = (lval <  rval)?1:0;
			else if(op==BO_GT) ans = (lval >  rval)?1:0;
			else if(op==BO_LE) ans = (lval <= rval)?1:0;
			else if(op==BO_GE) ans = (lval >= rval)?1:0;
			else if(op==BO_EQ) ans = (lval == rval)?1:0;
			else if(op==BO_NE) ans = (lval != rval)?1:0;
			return new EmuInt(ans);
		}
		case BO_AddAssign:
		case BO_SubAssign:
		{
			lvalue left = eval_lexpr(ex->getLHS());
			const Type* tl = left.type.getCanonicalType().getTypePtr();
			const Type* tr = right->obj_type.getCanonicalType().getTypePtr();
			if(!isa<BuiltinType>(tl) || (((const BuiltinType*)tl)->getKind() != BuiltinType::Int)){
				cant_handle();
			}
			if(!isa<BuiltinType>(tr) || (((const BuiltinType*)tr)->getKind() != BuiltinType::Int)){
				cant_handle();
			}
			EmuInt* value = new EmuInt(STATUS_UNINITIALIZED);
			void* ptr = ((char*)left.ptr.block)+left.ptr.offset;
			value->set_to_repr(ptr);
			if(op == BO_AddAssign) value->val += ((const EmuInt*)right)->val;
			else                   value->val -= ((const EmuInt*)right)->val;
			value->dump_repr(ptr);
			delete right;
			return value;
		}
		case BO_PtrMemD:
		case BO_PtrMemI:
		case BO_Mul:
		case BO_Div:
		case BO_Rem:
		case BO_Add:
		case BO_Sub:
		case BO_Shl:
		case BO_Shr:
		case BO_And:
		case BO_Xor:
		case BO_Or:
		case BO_LAnd:
		case BO_LOr:
		case BO_MulAssign:
		case BO_DivAssign:
		case BO_RemAssign:
		case BO_ShlAssign:
		case BO_ShrAssign:
		case BO_AndAssign:
		case BO_XorAssign:
		case BO_OrAssign:
		case BO_Comma:
		default:
			cant_handle();
		}
	} else if(isa<CastExpr>(e)){
		const CastExpr* expr = (const CastExpr*)e;
		const Expr* sub = expr->getSubExpr();
		switch(expr->getCastKind()){
		case CK_LValueToRValue:
			return from_lvalue(eval_lexpr(sub));
		case CK_NoOp:
			return eval_rexpr(sub);
		default:
			cant_handle();
		}
	}
	cant_handle();
}

void exec_decl(const Decl* d){
	if(!isa<VarDecl>(d)){
		llvm::outs() << "Ignoring unrecognized declaration:\n";
		d->dump();
		return;
	}
	const VarDecl* decl = (const VarDecl*)d;
	const Expr* init = decl->getInit();
	const EmuVal* val;
	if(init == nullptr){
		val = blank_from_type(decl->getType());
	} else {
		const EmuVal* temp = eval_rexpr(init);
		val = cast_to(eval_rexpr(init), decl->getType());
		if(val != temp) delete temp;
	}

	mem_block* storage = new mem_block(MEM_TYPE_STACK, val);
	stack_vars.front().insert(std::pair<std::string,lvalue> (decl->getNameAsString(), lvalue(storage, val->obj_type, 0)));
}

// null if it just ended with no return call
// caller must free returned if not null
const EmuVal* exec_stmt(const SourceManager& src_mgr, const Stmt* s){
	unsigned int line = src_mgr.getSpellingLineNumber(s->getLocStart());
	if(line != currloc){
		currloc = line;
		debug_dump();
	}

	const EmuVal* retval = nullptr;
	if(isa<CompoundStmt>(s)){
		const CompoundStmt* stmt = (const CompoundStmt*)s;
		stack_vars.push_front(std::unordered_map<std::string, lvalue>());
		for(auto it = stmt->body_begin(); it != stmt->body_end(); it++){
			retval = exec_stmt(src_mgr, (const Stmt*)*it);
			if(retval != nullptr){
				break;
			}
		}
		stack_vars.pop_front();
	} else if(isa<DeclStmt>(s)){
		const DeclStmt* stmt = (const DeclStmt*)s;
		for(auto it = stmt->decl_begin(); it != stmt->decl_end(); it++){
			exec_decl(*it);
		}
	} else if(isa<Expr>(s)){
		eval_rexpr((const Expr*)s);
	} else if(isa<ReturnStmt>(s)){
		const ReturnStmt* stmt = (const ReturnStmt*)s;
		return eval_rexpr(stmt->getRetValue());
	} else if(isa<ForStmt>(s)){
		const ForStmt* stmt = (const ForStmt*)s;

		stack_vars.push_front(std::unordered_map<std::string, lvalue>());
		const Stmt* init = stmt->getInit();
		const Expr* cond = stmt->getCond();
		const Expr* inc = stmt->getInc();
		const Stmt* body = stmt->getBody();
		retval = exec_stmt(src_mgr,init);
		if(retval != nullptr){
			stack_vars.pop_front();
			return retval;
		}
		while(1){
			const EmuVal* temp = eval_rexpr(cond);
			bool z = is_scalar_zero(temp);
			delete temp;
			if(z) break;

			retval = exec_stmt(src_mgr, body);
			if(retval != nullptr){
				stack_vars.pop_front();
				return retval;
			}
			eval_rexpr(inc);
		}
		stack_vars.pop_front();
	} else {
		cant_handle();
	}
	return retval;
}

void StoreFuncImpl(const FunctionDecl *obj){
	auto it = global_vars.find(obj->getNameAsString());
	if(it != global_vars.end()) return;
	// now we ensure this is a new variable being declared

	// if this is just a declaration, ignore for now
	if(obj->getBody() == nullptr) return;

	uint32_t fid = new_fid();
	EmuFunc f(fid, obj->getType());
	mem_block *storage = new mem_block(MEM_TYPE_INVALID, &f);

	global_functions.insert(std::pair<uint32_t, const void*> (fid, obj));
	global_vars.insert(std::pair<std::string, lvalue> (obj->getNameAsString(), lvalue(storage, obj->getType(), 0)));
}

void InitializeVar(const ValueDecl *obj) {
	outs() << "DEBUG: initializevar called on " << obj->getName() << "\n";
	if(isa<FunctionDecl>(obj)){
		outs() << "DEBUG: initializing function " << obj->getName() << "\n";
		auto it = global_vars.find(obj->getNameAsString());
		if(it != global_vars.end()){
			// could mean it is declared again, or was defined
			EmuFunc f(STATUS_UNDEFINED, obj->getType());
			f.set_to_repr(it->second.ptr.block->data);
			auto it2 = global_functions.find(f.func_id);
			if(it2 != global_functions.end()){
				// if defined, just make it show up now
				it->second.ptr.block->memtype = MEM_TYPE_STATIC;
				return;
			}
			// if undefined, we've already taken care of it
			return;
		}
		// if there is no implementation, we know this is an external function
		uint32_t fid = new_fid();
		EmuFunc f(fid, obj->getType());
		mem_block *storage = new mem_block(MEM_TYPE_EXTERN, &f);
		global_vars.insert(std::pair<std::string, lvalue> (obj->getNameAsString(), lvalue(storage, obj->getType(), 0)));
		external_functions.insert(std::pair<uint32_t, std::string> (fid, obj->getNameAsString()));
	} else if(isa<VarDecl>(obj)){
		const VarDecl *v = (const VarDecl*)obj;
		const Expr* init = v->getInit();
		const EmuVal* val;
		auto it = global_vars.find(obj->getNameAsString());
		bool newdef = (it == global_vars.end());
		if(init == nullptr){
			// make sure it hasn't been declared before, we don't want to overwrite a definition with a declaration
			if(!newdef) return;

			val = blank_from_type(obj->getType());
		} else {
			val = cast_to(eval_rexpr(init), obj->getType());
		}

		mem_block* storage;
		if(newdef){
			storage = new mem_block(MEM_TYPE_GLOBAL, val);
		} else {
			storage = it->second.ptr.block;
		}
		global_vars.insert(std::pair<std::string,lvalue> (obj->getNameAsString(), lvalue(storage, val->obj_type, 0)));
	} else {
		outs() << "Ignoring unknown declaration:\n";
		obj->dump();
	}
	return;
}

class Interpreter : public ASTConsumer {
public:

virtual void HandleTranslationUnit(ASTContext &context) {
	IntType = context.IntTy;
	BoolType = context.BoolTy;
	RawType = context.UnknownAnyTy;
	RawPtrType = context.getPointerType(RawType);
	TranslationUnitDecl* file = context.getTranslationUnitDecl();
	file->dump();
	// first, we need to store implementations of the functions, in case a global initializer wants to call one
	for(auto it = file->decls_begin(); it != file->decls_end(); it++){
		const Decl* d = *it;
		if(isa<FunctionDecl>(d)){
			StoreFuncImpl((const FunctionDecl*)d);
		}
	}

	// stuff to track line numbers
	const SourceManager& src_mgr = file->getASTContext().getSourceManager();
	FileID mainfile = src_mgr.getMainFileID();

	// now, start initializing
	for(auto it = file->decls_begin(); it != file->decls_end(); it++){
		const Decl* d = *it;
		if(isa<ValueDecl>(d)){
			SourceLocation loc = d->getLocation();
			FileID from = src_mgr.getFileID(loc);
			if(from != mainfile){
				loc = src_mgr.getIncludeLoc(from);
			}
			unsigned int line = src_mgr.getSpellingLineNumber(loc);
			if(line != currloc){
				currloc = line;
				llvm::outs() << "Reached line " << line << "\n";
				debug_dump();
			}
			outs() << "Now calling initializevar\n";
			InitializeVar((const ValueDecl*)d);
		}
	}

	llvm::outs() << "DEBUG: now finding main\n";

	// At this point, let's find the main method and call it
	const FunctionDecl *func;
	{
		auto it = global_vars.find("main");
		if(it == global_vars.end()){
			err_exit("No main method declared\n");
		}
		EmuFunc f(STATUS_UNDEFINED, it->second.type);
		f.set_to_repr(it->second.ptr.block->data);
		auto it2 = global_functions.find(f.func_id);
		if(it2 == global_functions.end()){
			err_exit("No main method defined\n");
		}
		func = (const FunctionDecl*)it2->second;
	}
	unsigned int line = src_mgr.getSpellingLineNumber(func->getLocation());
	if(line != currloc){
		currloc = line;
		debug_dump();
	}
	QualType retqtype = func->getReturnType();
	const Type* rettype = retqtype.getTypePtr();
	if(!isa<BuiltinType>(rettype) || ((const BuiltinType*)rettype)->getKind() != BuiltinType::Int){
		err_exit("Return type for main must be an int");
	}

	stack_vars.push_front(std::unordered_map<std::string, lvalue>());

	auto it = func->param_begin();
	if(it != func->param_end()){
		ParmVarDecl *arg1 = *it;
		it++;
		auto it2 = it;
		if(it == func->param_end() || (++it) != func->param_end()){
			err_exit("Wrong number of arguments for main");
		}
		ParmVarDecl *arg2 = *it2;
		const Type* t1 = arg1->getType().getCanonicalType().getTypePtr();
		if(!isa<BuiltinType>(t1) || ((const BuiltinType*)t1)->getKind() != BuiltinType::Int){
			err_exit("Wrong type on first arg of main (should be int)");
		}
		const Type *t2 = arg2->getType().getCanonicalType().getTypePtr();
		bool b = isa<PointerType>(t2);
		if(!b){
			const Type* _t2 = ((const PointerType*)t2)->getPointeeType().getCanonicalType().getTypePtr();
			b = isa<BuiltinType>(_t2);
			if(!b){
				BuiltinType::Kind k = ((const BuiltinType*)_t2)->getKind();
				b = (k == BuiltinType::Char_U || k == BuiltinType::Char_S);
			}
		}
		if(b){
			err_exit("Wrong type on second arg of main (should be char*)");
		}
		err_exit("Main with params not yet supported");
	}

	const Stmt* mainbody = func->getBody();
	const EmuVal* retval = exec_stmt(src_mgr,mainbody);
	if(retval != nullptr){
		line = src_mgr.getSpellingLineNumber(mainbody->getLocEnd());
		if(line != currloc){
			currloc = line;
			debug_dump();
		}
	}

	do_exit((const EmuInt*)cast_to(retval, retqtype));
}
};

void do_exit(const EmuInt* retval){
	outs() << "Exit value: ";
	retval->print();
	exit_clean();
}

class InterpretAction : public ASTFrontendAction {
public:
std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, llvm::StringRef) {
	Interpreter* ans = new Interpreter();
	return std::unique_ptr<ASTConsumer>(ans);
}
};

int main(int argc, const char ** argv) {
	tooling::CommonOptionsParser argParser(argc, argv, MyHelp);
	tooling::ClangTool tool(argParser.getCompilations(), argParser.getSourcePathList());

	return tool.run(tooling::newFrontendActionFactory<InterpretAction>().get());
}
