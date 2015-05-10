#include <dirent.h>

#include "clang/Tooling/Tooling.h"

#include "eval.h"
#include "help.h"
#include "main.h"

using namespace clang;

const ASTContext** sources;
int num_sources;

bool static_init = false;

class ErrorCatcher : public DiagnosticConsumer {
public:
	ErrorCatcher() {};
	~ErrorCatcher() {};

	unsigned getNumErrors() {return 0;}
	unsigned getNumWarnings() {return 0;}

	void clear(void) {};
	void BeginSourceFile(const LangOptions&, const Preprocessor*) {}
	void EndSourceFile(void) {}
	void finish(void) {}
	bool IncludeInDiagnosticCounts() const { return false; }
	void HandleDiagnostic(DiagnosticsEngine::Level diaglevel, const Diagnostic &info) const{
		llvm::errs() << "DOUG DEBUG: got a diagnostic message:\n";
		SmallString<100> outStr;
		info.FormatDiagnostic(outStr);
		for(char c : outStr){
			llvm::errs() << c;
		}
		llvm::errs() << "\n";
	}
};

int main(int argc, const char ** argv) {
//	tooling::CommonOptionsParser argParser(argc, argv, MyHelp);
//	tooling::ClangTool tool(argParser.getCompilations(), argParser.getSourcePathList());

	IntrusiveRefCntPtr<DiagnosticIDs> diag_ids(new DiagnosticIDs());
	IntrusiveRefCntPtr<DiagnosticsEngine> engine(new DiagnosticsEngine(diag_ids, new DiagnosticOptions(), (DiagnosticConsumer*)new ErrorCatcher()));

	FileSystemOptions fsopts;

	if(argc < 3){
		llvm::errs() << "Missing arg\n";
		return 1;
	}

	std::vector<std::unique_ptr<ASTUnit> > ast_list;

	DIR* dir = opendir(argv[1]);
	if(dir == NULL){
		llvm::errs() << "Bad AST directory: "<<argv[1]<<"\n";
		return 1;
	}
	fsopts.WorkingDir += argv[1];
	while(struct dirent *entry = readdir(dir)){
		const char* name = entry->d_name;
		if(strstr(name, ".ast") == NULL) continue;

		std::unique_ptr<ASTUnit> u = ASTUnit::LoadFromASTFile(entry->d_name,engine,fsopts);
		printf("Read ASTUnit at loc %p from %s\n",(void*)u.get(),name);
		ast_list.push_back(std::move(u));
	}
	closedir(dir);

	const char** args = new const char*[2];
	args[0] = "-undef";
	args[1] = argv[2];
	ASTUnit* file = ASTUnit::LoadFromCommandLine(&args[0], &args[2], engine, ".");

	ast_list.push_back(std::unique_ptr<ASTUnit>(file));

	int n = ast_list.size();
	num_sources = n;
	sources = new const ASTContext*[n];
	local_vars = new std::unordered_map<std::string, lvalue>[n];

	int i = 0;
	for(auto &it : ast_list){
		const ASTUnit* u = it.get();
		if(u == nullptr){
			err_exit("There was a problem reading an AST file, quitting");
		}
		const ASTContext* c = &u->getASTContext();
		sources[i++] = c;
		RawType = c->UnknownAnyTy;
		BoolType = c->BoolTy;
		CharType = c->CharTy;
		UCharType = c->UnsignedCharTy;
		ShortType = c->ShortTy;
		UShortType = c->UnsignedShortTy;
		IntType = c->IntTy;
		UIntType = c->UnsignedIntTy;
		LongType = c->LongTy;
		ULongType = c->UnsignedLongTy;
		LongLongType = c->LongLongTy;
		ULongLongType = c->UnsignedLongLongTy;
		VoidType = c->VoidTy;
		VoidPtrType = c->getPointerType(c->VoidTy);
		BuiltinVaListType = c->getBuiltinVaListType();
	}

	for(i = 0; i < n; i++){
		StoreFuncImpls(i);
	}

	if(main_source == -1){
		llvm::errs() << "No main method found";
		return 0;
	}

	static_init = true;
	for(i = 0; i < n; i++){
		InitializeVars(i);
	}
	static_init = false;

	RunProgram();
	return 0;
}
