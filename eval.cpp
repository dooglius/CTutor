#include <limits.h>
#include <unistd.h>

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"

#include "cast.h"
#include "debug.h"
#include "eval.h"
#include "exit.h"
#include "external.h"
#include "help.h"
#include "main.h"
#include "mem.h"
#include "types.h"

#define BREAK_POINTER ((const EmuVal*)(-1))

using namespace clang;
using namespace llvm;

int main_source = -1;
int curr_source;
SourceLocation curr_loc;

static uint64_t apint_signed_repr(int64_t x){
	uint64_t* p = reinterpret_cast<uint64_t*>(&x);
	return *p;
}

static lvalue get_nonstack_var(std::string name){
	int source = curr_source;
	const auto it = local_vars[source].find(name);
	if(it != local_vars[source].end()){
		return it->second;
	}
	const auto it2 = global_vars.find(name);
	if(it2 != global_vars.end()){
		return local_vars[it2->second].find(name)->second;
	}
	return lvalue(nullptr, RawType, 0);
}

lvalue eval_lexpr(const Expr* e){
	if(isa<DeclRefExpr>(e)){
		const DeclRefExpr* expr = (const DeclRefExpr*)e;
		std::string name = expr->getDecl()->getNameAsString();
		const auto ret = stack_var_map.find(name);
		if(ret == stack_var_map.end()){
			return get_nonstack_var(name);			
		}
		const auto list = &ret->second;
		const auto item = list->back();
		lvalue ans = stack_vars[item.first][item.second].second;
		return ans;
	} else if(isa<ArraySubscriptExpr>(e)){
		const ArraySubscriptExpr* expr = (const ArraySubscriptExpr*)e;
		const EmuVal* base = eval_rexpr(expr->getBase());
		const EmuVal* idx = eval_rexpr(expr->getIdx());
		const Type* basetype = base->obj_type.getCanonicalType().getTypePtr();
		if(!basetype->isPointerType()){
			llvm::errs() << "\n\n";
			basetype->dump();
			cant_handle();
		}
		QualType t = idx->obj_type.getCanonicalType();
		if(!t->isIntegerType()){
			llvm::errs() << "\n\n";
			t.dump();
			cant_handle();
		}
		const EmuPtr* emup = (const EmuPtr*)base;
		mem_ptr p = mem_ptr(emup->u.block, emup->offset);
		QualType subtype = ((const PointerType*)basetype)->getPointeeType();

		const llvm::APInt idx_val = ((const EmuNumGeneric*)idx)->val;
		int64_t offset = ((int64_t)getSizeOf(subtype))*idx_val.getSExtValue() + p.offset;
		
		if(offset<0){
			err_exit("Got pointer to invalid location");
		}
		return lvalue(p.block, ((const PointerType*)basetype)->getPointeeType(), (size_t)offset);
	} else if(isa<StringLiteral>(e)){
		const StringLiteral *obj = (const StringLiteral*)e;
		if(obj->getKind() != StringLiteral::StringKind::Ascii){
			err_exit("Can't handle non-ascii strings");
		}
		// allocate string at this point
		mem_block *stringstorage = new mem_block(MEM_TYPE_STATIC, obj->getByteLength()+1);
		for(unsigned int i = 0; i <= obj->getLength(); i++){
			char c;
			if(i < obj->getLength()) c = (char)obj->getCodeUnit(i);
			else c = '\0';
			((char*)stringstorage->data)[i] = c;
		}
		return lvalue(stringstorage, e->getType(), 0);
	} else if(isa<ParenExpr>(e)){
		return eval_lexpr(((const ParenExpr*)e)->getSubExpr());
	}
	llvm::errs() << "DOUG DEBUG: eval_lexpr cant handle the following:\n";
	e->dump();
	cant_handle();
}

// caller must free returned value
const EmuVal* eval_rexpr(const Expr* e){
	errs() << "\nDEBUG: about to eval rexpr:\n";
	e->dump();

	if(isa<IntegerLiteral>(e)){
		const IntegerLiteral *obj = (const IntegerLiteral*)e;
		APInt i = obj->getValue();
		if(i.slt(EMU_MIN_INT) || i.sgt(EMU_MAX_INT)){
			e->dump();
			cant_handle();
		}
		return new EmuNum<NUM_TYPE_INT>(i);
	} else if(isa<CharacterLiteral>(e)){
		const CharacterLiteral *obj = (const CharacterLiteral*)e;
		unsigned int i = obj->getValue();
		if(i > 127){
			e->dump();
			cant_handle();
		}
		return new EmuNum<NUM_TYPE_CHAR>(new APInt(8, i, true));
	} else if(isa<UnaryOperator>(e)){
		const UnaryOperator *obj = (const UnaryOperator*)e;
		const Expr* sub = obj->getSubExpr();
		const auto op = obj->getOpcode();
		switch(op){
		case UO_AddrOf:
		{
			lvalue arg = eval_lexpr(sub);
			return new EmuPtr(arg.ptr, e->getType());
		}
		case UO_LNot:
		case UO_Minus:
		{
			const EmuVal* arg = eval_rexpr(sub);
			if(!arg->obj_type->isIntegerType()){
				cant_cast();
			}
			if(op == UO_LNot){
				return ((const EmuNumGeneric*)arg)->lnot();
			} else if (op == UO_Minus){
				return ((const EmuNumGeneric*)arg)->neg();
			}
		}
		case UO_Deref:
		case UO_Extension:
		case UO_Imag:
		case UO_Real:
		case UO_Not:
		case UO_PostInc:
		case UO_PostDec:
		case UO_PreInc:
		case UO_PreDec:
		case UO_Plus:
		default:
			llvm::errs() << "Got opcode " << obj->getOpcode() << "\n";
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
			const EmuVal* ans = right->cast_to(left.type);
			delete right;
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
			QualType tl = left->obj_type.getCanonicalType();
			QualType tr = right->obj_type.getCanonicalType();
			if(tl != IntType || tr != IntType){
				left->obj_type.dump();
				right->obj_type.dump();
				cant_handle();
			}
			const llvm::APInt* lval = &((const EmuNum<NUM_TYPE_INT>*)left)->val;
			llvm::APInt rval = ((const EmuNum<NUM_TYPE_INT>*)right)->val;
			int ans;
			if(lval->isNegative()){
				if(op == BO_LT)    ans = (lval->slt(rval))?1:0;
				else if(op==BO_GT) ans = (lval->sgt(rval))?1:0;
				else if(op==BO_LE) ans = (lval->sle(rval))?1:0;
				else if(op==BO_GE) ans = (lval->sge(rval))?1:0;
				else if(op==BO_EQ) ans = (lval->eq( rval))?1:0;
				else if(op==BO_NE) ans = (lval->ne( rval))?1:0;
			} else if(rval.isNegative()){
				if(op == BO_LT)    ans = 0;
				else if(op==BO_GT) ans = 1;
				else if(op==BO_LE) ans = 0;
				else if(op==BO_GE) ans = 1;
				else if(op==BO_EQ) ans = 0;
				else if(op==BO_NE) ans = 1;
			} else {
				if(op == BO_LT)    ans = (lval->ult(rval))?1:0;
				else if(op==BO_GT) ans = (lval->ugt(rval))?1:0;
				else if(op==BO_LE) ans = (lval->ule(rval))?1:0;
				else if(op==BO_GE) ans = (lval->uge(rval))?1:0;
				else if(op==BO_EQ) ans = (lval->eq( rval))?1:0;
				else if(op==BO_NE) ans = (lval->ne( rval))?1:0;
			}
			delete left;
			delete right;
			return new EmuNum<NUM_TYPE_INT>(APInt(32, apint_signed_repr(ans), true));
		}
		case BO_AddAssign:
		case BO_SubAssign:
		{
			lvalue left = eval_lexpr(ex->getLHS());
			QualType tl = left.type.getCanonicalType();
			QualType tr = right->obj_type.getCanonicalType();
			if(tl != IntType || tr != IntType){
				left.type.dump();
				right->obj_type.dump();
				cant_handle();
			}
			void* ptr = &((char*)left.ptr.block->data)[left.ptr.offset];
			size_t space = left.ptr.block->size;
			if(space < 4 || space-4 < left.ptr.offset){
				bad_memread();
			}
			const EmuNum<NUM_TYPE_INT> value(ptr);
			const EmuNum<NUM_TYPE_INT>* result;
			if(op == BO_AddAssign) result = value.add((const EmuNum<NUM_TYPE_INT>*)right);
			else                   result = value.sub((const EmuNum<NUM_TYPE_INT>*)right);
			left.ptr.block->write(result, left.ptr.offset);
			delete right;
			return result;
		}
		case BO_Add:
		case BO_Sub:
		case BO_Mul:
		case BO_Div:
		case BO_And:
		case BO_Or:
		{
			const EmuVal* left = eval_rexpr(ex->getLHS());
			if(!right->obj_type->isIntegerType()){
				right->obj_type.dump();
				cant_cast();
			}
			const EmuNumGeneric* trueright = (const EmuNumGeneric*)right;
			const EmuVal* retval;

			QualType tl = left->obj_type;
			// special case: add integer to pointer
			if(tl->isPointerType()){
				int n;
				if(op == BO_Add) n = trueright->val.getSExtValue();
				else if(op == BO_Sub) n = -trueright->val.getSExtValue();
				else err_exit("Undefined op on pointer");
				
				QualType sub = tl->getAs<PointerType>()->getPointeeType();
				int s = getSizeOf(sub);
				const EmuPtr* lp = (const EmuPtr*)left;
				retval = new EmuPtr(mem_ptr(lp->u.block,lp->offset+n*s), tl);
			} else if(tl->isIntegerType()){
				const EmuNumGeneric* trueleft = (const EmuNumGeneric*)left;
				if(op == BO_Add)      retval = trueleft->add(trueright);
				else if(op == BO_Sub) retval = trueleft->sub(trueright);
				else if(op == BO_Mul) retval = trueleft->mul(trueright);
				else if(op == BO_Div) retval = trueleft->div(trueright);
				else if(op == BO_Or) retval = trueleft->_or(trueright);
				else if(op == BO_And)retval = trueleft->_and(trueright);
				else cant_cast();
			} else {
				tl.dump();
				cant_cast();
			}

			delete left;
			delete right;

			return retval;
		}
		case BO_PtrMemD:
		case BO_PtrMemI:
		case BO_Rem:
		case BO_Shl:
		case BO_Shr:
		case BO_LAnd:
		case BO_Xor:
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
			e->dump();
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
		case CK_BitCast:
		{
			if(isa<ExplicitCastExpr>(e)){
				const ExplicitCastExpr* expr = (const ExplicitCastExpr*)e;
				return eval_rexpr(sub)->cast_to(expr->getTypeAsWritten());
			}
			// else ImplicitCastExpr
			return eval_rexpr(sub)->cast_to(e->getType());
		}
		case CK_IntegralCast:
		{
			return eval_rexpr(sub)->cast_to(expr->getType());
		}
		case CK_FunctionToPointerDecay:
		{
			lvalue l = eval_lexpr(sub);
			if(!l.type->isFunctionType()){
				e->dump();
				cant_cast();
			}
			return new EmuPtr(l.ptr, sources[curr_source]->getPointerType(l.type));
		}
		case CK_ArrayToPointerDecay:
		{
			lvalue l = eval_lexpr(sub);
			const EmuVal* ans = new EmuPtr(l.ptr, expr->getType());
			return ans;
		}
		case CK_BuiltinFnToFnPtr:
		{
			if(!isa<DeclRefExpr>(sub)){
				err_exit("Don't know how to convert builtin function");
			}
			std::string name = ((const DeclRefExpr*)sub)->getDecl()->getNameAsString();
			const EmuFunc* f = get_external_func(name, sub->getType());
			mem_block* ptr = new mem_block(MEM_TYPE_STATIC, f);
			delete f;
			return new EmuPtr(mem_ptr(ptr,0), expr->getType());
		}
		case CK_NullToPointer:
		{
			return new EmuPtr(mem_ptr(nullptr,0), expr->getType());
		}
		case CK_PointerToIntegral:
		{
			const EmuVal* ptr = eval_rexpr(sub);
			if(!ptr->obj_type->isPointerType()){
				err_exit("Expected pointer");
			}
			const EmuPtr* p = (const EmuPtr*)ptr;
			if(p->status != STATUS_DEFINED) cant_handle();
			uint64_t segment;
			uint64_t offset = p->offset;
			if(p->u.block == nullptr){
				segment = 0;
			} else {
				segment = p->u.block->id;
			}
			delete ptr;
			if((expr->getType()->getAs<BuiltinType>())->isSignedInteger()){
				return new EmuNum<NUM_TYPE_LONGLONG>(APInt(64, (segment << 32) + offset, true));
			} else {
				return new EmuNum<NUM_TYPE_ULONGLONG>(APInt(64, (segment << 32) + offset, false));				
			}
		}
		case CK_VectorSplat:
		case CK_IntegralToBoolean:
		case CK_IntegralToFloating:
		case CK_FloatingToIntegral:
		case CK_FloatingToBoolean:
		case CK_FloatingCast:
		case CK_CPointerToObjCPointerCast:
		case CK_BlockPointerToObjCPointerCast:
		case CK_AnyPointerToBlockPointerCast:
		case CK_ObjCObjectLValueCast:
		case CK_FloatingRealToComplex:
		case CK_FloatingComplexToReal:
		case CK_FloatingComplexToBoolean:
		case CK_FloatingComplexCast:
		case CK_FloatingComplexToIntegralComplex:
		case CK_IntegralRealToComplex:
		case CK_IntegralComplexToReal:
		case CK_IntegralComplexToBoolean:
		case CK_IntegralComplexCast:
		case CK_IntegralComplexToFloatingComplex:
		case CK_ARCProduceObject:
		case CK_ARCConsumeObject:
		case CK_ARCReclaimReturnedObject:
		case CK_ARCExtendBlockObject:
		case CK_AtomicToNonAtomic:
		case CK_NonAtomicToAtomic:
		case CK_CopyAndAutoreleaseBlockObject:
		case CK_ZeroToOCLEvent:
		case CK_AddressSpaceConversion:
		case CK_ReinterpretMemberPointer:
		case CK_UserDefinedConversion:
		case CK_ConstructorConversion:
		case CK_IntegralToPointer:
		case CK_PointerToBoolean:
		case CK_ToVoid:
		default:
			llvm::errs() << "\n\n";
			e->dump();
			cant_cast();
		}
	} else if(isa<CallExpr>(e)){
		const CallExpr* expr = (const CallExpr*)e;
		const Expr* const* args = expr->getArgs();
		const Expr* callee = expr->getCallee();

		llvm::errs() << "DOUG DEBUG: executing the following call:\n";
		callee->dump();

		const EmuVal* f = eval_rexpr(callee);
		if(f->status != STATUS_DEFINED || !f->obj_type->isFunctionPointerType()){
			f->obj_type.dump();
			err_exit("Calling an invalid function");
		}

		const EmuPtr* p = (const EmuPtr*)f;
		if(p->u.block->memtype == MEM_TYPE_EXTERN){
			err_exit("Tried to call an unimplemented function");
		}

		const EmuFunc* func = (const EmuFunc*)from_lvalue(lvalue(p->u.block, ((const PointerType*)p->obj_type.getTypePtr())->getPointeeType(), p->offset));
		uint32_t fid = func->func_id;
		
		const EmuVal* retval;

		add_stack_frame();
		if(fid < NUM_EXTERNAL_FUNCTIONS){
			if(is_lvalue_based_macro(fid)){
				// special handling for va_args stuff
				for(unsigned int i=0; i < expr->getNumArgs(); i++){
					const Expr* arg = args[i];
					while(isa<ImplicitCastExpr>(arg)){
						arg = ((const ImplicitCastExpr*)arg)->getSubExpr();
					}
					if(!isa<DeclRefExpr>(arg)){
						err_exit("Passed non-variable as lvalue to builtin macro");
					}
					std::string name = ((const DeclRefExpr*)arg)->getDecl()->getNameAsString();
					std::unordered_map<std::string,std::deque<std::pair<int,int> > >::const_iterator list = stack_var_map.find(name);
					if(list == stack_var_map.end()){
						err_exit("Can't find appropriate lvalue for macro");
					}
					const auto test = list->second;
					const auto item = test.back();
					const EmuVal* val = new EmuStackPos(item.first, item.second);
					mem_block* storage = new mem_block(MEM_TYPE_STACK, val);
					add_stack_var("", lvalue(storage,val->obj_type,0));
					delete val;
				}
			} else {
				// we are dealing with an external function
				for(unsigned int i=0; i < expr->getNumArgs(); i++){
					const EmuVal* val = eval_rexpr(args[i]);
					mem_block* storage = new mem_block(MEM_TYPE_STACK, val);
					add_stack_var("", lvalue(storage,val->obj_type,0));
					delete val;
				}
			}
			retval = call_external(fid);
		} else {
			const auto it = global_functions.find(fid);
			const FunctionDecl* defn = (const FunctionDecl*)it->second.second;

			for(unsigned int i=0; i < expr->getNumArgs(); i++){
				const EmuVal* val = eval_rexpr(args[i]);
				mem_block* storage = new mem_block(MEM_TYPE_STACK, val);
				std::string name;
				if(i >= defn->getNumParams()){
					name = ""; // relevant for later args of e.g. printf(char*, ...)
				} else {
					name = defn->getParamDecl(i)->getNameAsString();
				}
				llvm::errs() << "DOUG DEBUG: adding stack variable "<<name<<" for arg "<<i<<" of internal function call (numparams="<< defn->getNumParams() <<")\n";
				defn->dump();
				
				add_stack_var(name, lvalue(storage,val->obj_type,0));
				delete val;
			}

			int save = curr_source;
			curr_source = it->second.first;
			llvm::errs() << "DOUG DEBUG: actually executing:\n";
			defn->getBody()->dump();
			retval = exec_stmt(defn->getBody());
			llvm::errs() << "DOUG DEBUG: call returned with retval at "<<((const void*)retval)<<"\n";
			curr_source = save;
		}
		llvm::errs() << "DOUG DEBUG: popping frame leaving call\n";
		pop_stack_frame();
		return retval;
	} else if(isa<UnaryExprOrTypeTraitExpr>(e)){
		const UnaryExprOrTypeTraitExpr* expr = (const UnaryExprOrTypeTraitExpr*)e;
		switch(expr->getKind()){
		case UETT_SizeOf:
		{
			QualType qt = expr->getArgumentType();
			const EmuVal* fake = from_lvalue(lvalue(nullptr, qt, 0));
			uint64_t thesize = (uint64_t)fake->size();
			delete fake;
			return new EmuNum<NUM_TYPE_ULONG>(APInt(32, thesize, false));
		}
		case UETT_AlignOf:
		case UETT_VecStep:
		default:
			e->dump();
			cant_handle();
		}
	} else if(isa<InitListExpr>(e)){
		const InitListExpr* expr = (const InitListExpr*)e;
		unsigned int n = expr->getNumInits();
		QualType qt = expr->getType();
		if(qt->isArrayType()){
			const EmuPtr* array = (const EmuPtr*)from_lvalue(lvalue(nullptr, qt, 0));
			if(array->status != STATUS_DEFINED) cant_handle();
			size_t loc = 0;
			for(unsigned int i = 0; i < n; i++){
				const EmuVal* curr = eval_rexpr(expr->getInit(i));
				array->u.block->write(curr, loc);
				loc += curr->size();
				delete curr;
			}
			return array;
		} else if(qt->isStructureType()){
			unsigned int n = expr->getNumInits();
			const EmuVal** arr = new const EmuVal*[n];
			for(unsigned int i = 0; i < n; i++){
				arr[i] = eval_rexpr(expr->getInit(i));
			}
			return new EmuStruct(STATUS_DEFINED, qt, n, arr);
		}
		cant_handle();
	} else if(isa<ImplicitValueInitExpr>(e)){
		return zero_init(e->getType());
	} else if(isa<ParenExpr>(e)){
		return eval_rexpr(((const ParenExpr*)e)->getSubExpr());
	}
	e->dump();
	cant_handle();
}

void exec_decl(const Decl* d){
	if(!isa<VarDecl>(d)){
		llvm::errs() << "\n\nIgnoring unrecognized declaration:\n";
		d->dump();
		return;
	}
	const VarDecl* decl = (const VarDecl*)d;
	const Expr* init = decl->getInit();
	const EmuVal* val;
	if(init == nullptr){
		val = from_lvalue(lvalue(nullptr,decl->getType(),0));
	} else {
		const EmuVal* temp = eval_rexpr(init);
		val = eval_rexpr(init)->cast_to(decl->getType());
		delete temp;
	}

	mem_block* storage = new mem_block(MEM_TYPE_STACK, val);
	errs() << "DOUG DEBUG: new variable with name " << decl->getNameAsString() << "\n";
	add_stack_var(decl->getNameAsString(), lvalue(storage, val->obj_type, 0));
}

// null if it just ended with no return call
// caller must free returned if not null
const EmuVal* exec_stmt(const Stmt* s){
	curr_loc = s->getLocStart();
	debug_dump();

	errs() << "\n\nDEBUG: about to execute the following statement:\n";
	s->dump();
	errs() << "\n\n";

	const EmuVal* retval = nullptr;
	if(isa<CompoundStmt>(s)){
		const CompoundStmt* stmt = (const CompoundStmt*)s;
		add_stack_frame();
		for(auto it = stmt->body_begin(); it != stmt->body_end(); it++){
			retval = exec_stmt((const Stmt*)*it);
			if(retval != nullptr){
				break;
			}
		}
		llvm::errs() << "DOUG DEBUG: popping frame leaving compoundstmt\n";
		pop_stack_frame();
	} else if(isa<DeclStmt>(s)){
		const DeclStmt* stmt = (const DeclStmt*)s;
		for(auto it = stmt->decl_begin(); it != stmt->decl_end(); it++){
			exec_decl(*it);
		}
	} else if(isa<Expr>(s)){
		llvm::errs() << "DOUG DEBUG: the following is an expr:\n";
		s->dump();
		eval_rexpr((const Expr*)s);
	} else if(isa<ReturnStmt>(s)){
		const ReturnStmt* stmt = (const ReturnStmt*)s;
		return eval_rexpr(stmt->getRetValue());
	} else if(isa<IfStmt>(s)){
		const IfStmt* stmt = (const IfStmt*)s;
		const Expr* cond = stmt->getCond();
		const EmuVal* eval = eval_rexpr(cond);
		if(is_scalar_zero(eval)){
			const Stmt* next = stmt->getElse();
			if(next == nullptr) return nullptr;
			return exec_stmt(stmt->getElse());
		} else {
			return exec_stmt(stmt->getThen());
		}
	} else if(isa<ForStmt>(s)){
		const ForStmt* stmt = (const ForStmt*)s;
		add_stack_frame();
		const Stmt* init = stmt->getInit();
		const Expr* cond = stmt->getCond();
		const Expr* inc = stmt->getInc();
		const Stmt* body = stmt->getBody();
		retval = exec_stmt(init);
		if(retval == nullptr){
			while(1){
				const EmuVal* temp = eval_rexpr(cond);
				bool z = is_scalar_zero(temp);
				delete temp;
				if(z) break;

				retval = exec_stmt(body);
				if(retval != nullptr){
					break;
				}
				eval_rexpr(inc);
			}
		}
		llvm::errs() << "DOUG DEBUG: popping frame leaving for loop\n";
		pop_stack_frame();
	} else if(isa<NullStmt>(s)){
		// do nothing, naturally
	} else if(isa<SwitchStmt>(s)){
		add_stack_frame();
		const SwitchStmt* stmt = (const SwitchStmt*)s;
		const EmuVal* value = eval_rexpr(stmt->getCond());
		if(!value->obj_type->isIntegerType()) cant_cast();
		const EmuNumGeneric* val = (const EmuNumGeneric*)value;
		const Stmt* code = (const Stmt*)stmt->getBody();
		if(!isa<CompoundStmt>(code)) cant_handle();
		const CompoundStmt* sub = (const CompoundStmt*)code;
		auto it = sub->body_begin();
		for(; it != sub->body_end(); it++){
			const Stmt* curr = *it;
			if(isa<CaseStmt>(curr)){
				const CaseStmt* c = (const CaseStmt*)curr;
				const EmuVal* comp = eval_rexpr(c->getLHS());
				if(!comp->obj_type->isIntegerType()) cant_cast();
				const EmuNumGeneric* cmp = (const EmuNumGeneric*)comp;
				if(val->equals(cmp)){
					break;
				}
			}
		}
		if(it == sub->body_end()){
			// find the default statement
			for(it = sub->body_begin(); it != sub->body_end(); it++){
				if(isa<DefaultStmt>(*it)) break;
			}
		}
		for(; it != sub->body_end(); it++){
			const EmuVal* returned = exec_stmt(*it);
			if(returned != nullptr){
				if(returned != BREAK_POINTER) retval = returned;
				break;
			}
		}
		llvm::errs() << "DOUG DEBUG: popping frame leaving switch\n";
		pop_stack_frame();
	} else if(isa<SwitchCase>(s)){
		llvm::errs() << "DOUG DEBUG: detected switchcase\n";
		return exec_stmt(((const SwitchCase*)s)->getSubStmt());
	} else if(isa<BreakStmt>(s)){
		return BREAK_POINTER;
	} else {
		s->dump();
		cant_handle();
	}
	return retval;
}

static void StoreFuncImpl(const FunctionDecl *obj, int source){
	std::string name = obj->getNameAsString();

	// now we ensure this is a new function being declared
	auto it = local_vars[source].find(name);
	if(it != local_vars[source].end()) return;
	
	// if this is just a declaration, ignore for now
	if(!obj->isThisDeclarationADefinition()) return;

	uint32_t fid = new_fid();
	EmuFunc f(fid, obj->getType());
	mem_block *storage = new mem_block(MEM_TYPE_INVALID, &f);

	global_functions.insert(std::pair<uint32_t, std::pair<int, const void*> > (fid, std::pair<int, const void*> (source, obj)));
	local_vars[source].insert(std::pair<std::string, lvalue> (name, lvalue(storage, obj->getType(), 0)));

	Linkage l = obj->getFormalLinkage();
	if(l == ExternalLinkage || l == UniqueExternalLinkage){
		global_vars.insert(std::pair<std::string, int> (name, source));
		if(name.compare("main") == 0){
			main_source = source;
		}
	}
}

static void SetupVar(const ValueDecl *obj, int source) {
	std::string name = obj->getNameAsString();
	if(isa<FunctionDecl>(obj)){

		// try to get the definition
		lvalue l = get_nonstack_var(name);

		if(l.ptr.block != nullptr){
			// if it is defined, find the definition and unhide it
			EmuFunc f(l.ptr.block->data, obj->getType());

			if(f.status == STATUS_DEFINED){
				auto it2 = global_functions.find(f.func_id);
				if(it2 != global_functions.end()){
					l.ptr.block->memtype = MEM_TYPE_STATIC;
				}
			}
			return;
		}

		// if there is no implementation, we know this is an undefined function
		mem_block *storage = new mem_block(MEM_TYPE_EXTERN, (size_t)0);
		local_vars[source].insert(std::pair<std::string, lvalue> (obj->getNameAsString(), lvalue(storage, obj->getType(), 0)));
	} else if(isa<VarDecl>(obj)){
		const VarDecl *v = (const VarDecl*)obj;

		if(v->hasExternalStorage()) return;

		QualType qt = obj->getType();

		mem_block* storage = new mem_block(MEM_TYPE_GLOBAL, getSizeOf(qt));
		local_vars[source].insert(std::pair<std::string,lvalue> (name, lvalue(storage, qt, 0)));

		Linkage l = obj->getFormalLinkage();
		if(l == ExternalLinkage || l == UniqueExternalLinkage){
			global_vars.insert(std::pair<std::string, int> (name, source));
		}
	} else {
		errs() << "\n\nIgnoring unknown declaration:\n";
		obj->dump();
	}
	return;
}

static void InitializeVar(const VarDecl *v, int source) {
	if(v->hasExternalStorage()) return;

	const Expr* init = v->getInit();
	if(init == nullptr) return;

	curr_loc = v->getLocation();
	debug_dump();

	llvm::errs() << "DOUG DEBUG: about to resolve right hand of initialization:\n";
	init->dump();
	const EmuVal* temp = eval_rexpr(init);
	llvm::errs() << "DOUG DEBUG: about to cast obtained object ("<<((const void*)temp)<<") to the following decl:\n";
	v->dump();
	const EmuVal* val = temp->cast_to(v->getType());
	delete temp;

	std::string name = v->getNameAsString();
	lvalue loc = local_vars[source].find(name)->second;
	val->dump_repr(&((char*)loc.ptr.block->data)[loc.ptr.offset]);
	
	llvm::errs() << "DOUG DEBUG: variable "<<name<<" stored at block id "<<loc.ptr.block->id<<"\n";

	delete val;
	return;
}

// first, we need to store implementations of the functions, in case a global initializer wants to call one
void StoreFuncImpls(int source) {
	const TranslationUnitDecl* file = sources[source]->getTranslationUnitDecl();
	for(auto it = file->decls_begin(); it != file->decls_end(); it++){
		const Decl* d = *it;
		if(isa<FunctionDecl>(d)){
			StoreFuncImpl((const FunctionDecl*)d, source);
		}
	}
}

void InitializeVars(int source){
	const TranslationUnitDecl* file = sources[source]->getTranslationUnitDecl();
	curr_source = source;
	for(auto it = file->decls_begin(); it != file->decls_end(); it++){
		const Decl* d = *it;
		if(isa<ValueDecl>(d)){
			SetupVar((const ValueDecl*)d, source);
		}
	}
	for(auto it = file->decls_begin(); it != file->decls_end(); it++){
		const Decl* d = *it;
		if(isa<VarDecl>(d)){
			InitializeVar((const VarDecl*)d, source);
		}
	}
}

void RunProgram(void){
	// At this point, let's find the main method and call it
	const FunctionDecl *func;
	{
		int ms = main_source;
		if(ms == -1){
			err_exit("No main method declared");
		}
		printf("DOUG DEBUG: ms=%d\n",ms);
		auto it = local_vars[ms].find("main");
		EmuFunc f(it->second.ptr.block->data, it->second.type);
		auto it2 = global_functions.find(f.func_id);
		if(it2 == global_functions.end()){
			err_exit("No main method defined\n");
		}
		curr_source = ms;
		func = (const FunctionDecl*)it2->second.second;
	}
	curr_loc = func->getLocation();
	debug_dump();

	QualType retqtype = func->getReturnType();
	const Type* rettype = retqtype.getTypePtr();
	if(!isa<BuiltinType>(rettype) || ((const BuiltinType*)rettype)->getKind() != BuiltinType::Int){
		err_exit("Return type for main must be an int");
	}

	add_stack_frame();

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
	const EmuVal* retval = exec_stmt(mainbody);
	if(retval != nullptr){
		curr_loc = mainbody->getLocEnd();
		debug_dump();
	}

	do_exit((const EmuNum<NUM_TYPE_INT>*)retval->cast_to(retqtype));
}

void do_exit(const EmuNum<NUM_TYPE_INT>* retval){
	errs() << "\n\nDEBUG: Exit value: " << retval->val.toString(10, true) << "\n";
	exit_clean();
}
