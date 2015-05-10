CInterp: an interpreter for C
===================


CInterp is a run-time interpreter that allows completely virtualized execution of programs written in C; no compilation is ever performed. Further, the behavior of the simulated program is independent to the host ABI, so code with machine-specific behavior will run the same regardless of the host architecture. Note that CInterp does not currently provide security guarantees; the interpreter is not hardened against malicious code and should not be run against untrusted code unless properly isolated.

Rationale
-------------
In contrast to more modern languages with managed and garbage collected memory, C programs are notoriously difficult and frustrating to debug, especially when incorrect memory access is involved. Complicating things further, many novice C programmers have a great difficulty in understanding how to correctly use and work with C pointers. This gives C a high barrier to entry in practice, and as a result, it causes C to be discouraged as a first language, in lieu of more debuggable options such as Java and Python.

However, this difficulty is somewhat artificial. While C has a reputation among programmers as a sort of high-level assembler ideal for low-level, byte-granular access, this is something of an entrenched historical artifact. Indeed, the C standard [for the record, this program is designed for compatibility with the N1570 draft, the closest document to the C11 standard that is freely available online] does not speak to any actual  target machine, but rather, defines an abstract C Virtual Machine, not at all unlike the Java or Python Virtual Machines, that specifies the behavior of C code. All a C compiler does is convert the code into an assembly that has identical behavior (with respect to external function calls and volatile memory access) to the behavior of the abstract machine. But due to historical use of C, compilers and runtimes for the language are expected to be thin and very close to the hardware.

The goal of the CInterp project is, in a general sense, to provide a runtime environment for the C language that allows for many debugging, safety, and determinism features normally not available to C programmers. In a more specific sense, the primary focus of the project was to create a back-end for the visualization and debugging of short-term C programs, to interface with the front-end of [OnlinePythonTutor](http://www.pythontutor.com), a project by Philip Guo at the University of Rochester with code available on GitHub [here](https://github.com/pgbovine/OnlinePythonTutor/). The idea is that, as with OnlinePythonTutor, an OnlineCTutor frontend can be developed to show the state of an abstract C machine in a line-by-line format. While many debuggers can provide something close to this interface, debuggers necessarily fail in many cases where the actual running semantics of the program don't behave well--for example, in the case of a buffer overflow or a pointer to freed memory, the information required to pre-preemptively detect this sort of issue is simply not present in the actual runtime memory of a C executable, and so debuggers will be of little help, only alerting you to when a segfault actually happens.

----------


High-level Architecture
-------------------
CInterp is fundamentally tied to the LLVM/Clang compiler project because of the very resourceful C++ API provided by the Clang LibTooling interface, which allows one to create standalone executables by compiling and linking code against the Clang source code itself. In particular, the Clang LibTooling interface provides access to the Abstract Syntax Trees (ASTs) outputted by the lexer and parser, alleviating the burden of having to actually parse any C code. The interface also provides many APIs with which to manipulate and extract information from nodes of the AST, enabling CInterp to run, with a few exceptions, as a straightforward recursive walking and real-time interpretation of the nodes: expressions are calculated via recursive calls based on operations, while function calls and loops are executed by CInterp as function calls and loops with some extra overhead.

Because the Clang AST is also used to generate debugging information about the program, it contains useful source-level information which makes line-level analysis of the code possible: each statement (implemented by the Clang API as `Stmt`, the fundamental executable code unit) is annotated with a source file and line/column number, so that in order to perform line-by-line execution, one only needs to check if the line number has changed before each `exec_stmt` call by the interpreter.

Other than the interfacing with the Clang API, the interpreter is organized into four main subsystems: the memory subsystem, the type subsystem, the execution subsystem, and the I/O subsystem. Originally, there was an additional subsystem, the casting subsystem, but as it turned out, almost all necessary casts are made explicit by the Clang ASTs, so that the casting system has been mostly folded into the other subsystems.

Memory Subsystem
-------------------

The memory subsystem is probably the place where CInterp differs the most from an actually running C program. In order to provide detection and correct visualization in the face of various erroneous memory conditions, such as buffer overflow, use-after-free, double-free, attempted modification of static strings, and de-referencing pointers into incorrect types, the memory subsystem could not simply provide a simple RAM-like access to the virtualized memory. Rather, there needed to be something to isolate each unit of memory used.

Reading the C standard, the solution became clear: the abstract C pointer type only guarantees that pointers exist and be valid in ranges of memory that have been allocated either statically, locally, or through calls to `malloc` (or one plus the allocated memory, so as to have an end pointer). Inspired by the memory architecture of 16-bit x86 processors, pointers in the CInterp memory subsystem consist of a segment and offset. Each segment corresponds to a block of memory allocated in the interpreter, along with some simple annotations about the block, such as whether it is statically allocated, locally allocated, or heap-allocated, the size of the memory allocation, and whether the allocation is still valid, or has expired/been freed.

In order to get ideal debugging information, the annotations for each memory block need to persist throughout the applications runtime, because the interpreter can never be certain as to whether freed or expired memory will be accessed again; unlike languages without raw memory access, C can do some funny stuff. Consider, for example, a C application that has very high memory use, and therefore needs to occasionally store parts of its memory elsewhere, such as on a local hard disk. However, to optimize disk read/write times, suppose that the developers decided that when memory is removed this way, it is compressed on the fly, and then written to the file. When the file is loaded, the raw memory, including any memory pointers, start existing again, meaning that in order to usefully detect an error, pointers that have been invalidated should be clearly invalid, rather than pointing to replaced memory. For this reason, the annotations are never deleted, and the segment, which serves as the memory block id, increments monotonically until a total of 2<sup>32</sup> memory blocks have been made, at which point the interpreter will no longer be able to allocate memory.

This design decision makes it difficult to use CInterp for anything other than short-lived programs, as the interpreter thus has a fundamental memory leak, and inability to deal with an unbounded stream of data. Memory blocks exist for every allocation, not just intercepted heap allocation via malloc, meaning that each method with parameters or local variables requires an allocation. In order to maintain efficiency during read and write operations that occur throughout the evaluation of almost any valid C program, a custom implementation of a red-black tree is used to map the block ids of active memory (memory that is still valid) into pointers to the corresponding memory blocks and annotation information. 

If one is willing to give up some debugging information pertaining to individual memory allocations, this memory overhead could be greatly reduced by coalescing freed and expired blocks into range trees, through which one could efficiently check why a given memory block was invalidated. If one is willing to give up even further debugging information, then the interpreter can actually be made to run non-terminating programs with event loops by allowing block ids to be reallocated sparingly after 2<sup>32</sup> have been used up. One natural approach to this situation might be to expand the block id space to 2<sup>64</sup>, which is high enough so as to be effectively infinite for any reasonably purpose. However, the snag here is the existence of the `intptr_t` typedef, which needs to be an integer type whose values correspond with possible pointers; a 64 bit segment and a 32 bit offset would require the largest integer type, `long long`, to be extended to at least 96 bits, which would make internal implementation of operations like multiplication and division much slower and more difficult. While `intptr_t` is technically an optional typedef, it would found to be too widely used (in particular by the BSD libc code, which is indirectly incorporated as described later) to be ignored by an interpreter.

In addition to the block level, useful debugging information is also added at the byte level. Each block is annotated with a series of memory tags, which correspond to the inferred type and relative location of each byte of memory based on heuristics of read and write operations, as well as the object tags (see the next section). These memory tags are intended to be most useful for visualization of raw memory, so that the entire stack and heap can be annotated with the variable and type the memory corresponds to, allowing (among other things) for easy identification of memory leaks.

Type Subsystem
-------------------
Almost every type in CInterp is implemented by a tagged object in runtime memory, inspired by the design of Python objects in CPython's garbage-collected memory. For objects that are locally created and not stored in the memory subsystem (for example, in the case of a nested expression with no variables; it would be helpful to think of these as stored in registers as opposed to memory), tagging is implemented by the `QualType` class, which is the highly intricate type system provided by the Clang API. However, when data needs to be written back to memory, a different tagging mechanism is needed as C does actually need to have a complete description of each object in memory so that operations like memcpy (or a custom version of memcpy, which is why intercepting library calls isn't a proper solution) work as intended.

When an object is stored in a memory block, the first 4 bytes are used as a tag, which corresponds (unlike `QualType`) only to coarse-grained types; for example, there is a single pointer tag that represents all pointer types, a single struct tag that represents all structs, etc. The tagging system is mainly useful for detecting undefined memory access operations; for example, when someone tries to read an int in a memory location where a long is stored. Additionally, the tagging system has a special flag to indicate that a stored value is uninitialized, so that if uninitialized data is ever actually used, an error will appear.

The extra tag does cause the `sizeof` operator to return different results for basic types than would be expected based on their bit width; for example, an emulated unsigned int can only represent a nonnegative value up to 2<sup>32</sup>, but `sizeof(unsigned int)` will resolve to 8 because of the tag. As it turns out, doing this is actually fine according to the C standard, with a couple caveats. C allows types to have both padding bits and trap representations. Padding bits are bits that do not actually correspond to the value of the variable. Trap representations are certain bit patterns for a type that it is not legal to for a type's memory representation to have. In other words, if a `long[]` is cast to an `int*`, and the program attempts to load an integer where a long was placed, rather than deferring to undefined behavior (related to the endianness of the host machine), the interpreter can fail and/or emit a debugging message.

There are only a couple of holes that need to be plugged, which come up after a careful review of the C standard for type representations. The `char*` type, unlike the other types, cannot have any padding bits; it is the fundamental unit, and its abstract value must exactly correspond to the bit pattern it has in memory. It makes a lot of sense that this works the way it does as otherwise methods like `memcpy` could never be possible in fully portable C code (it would always need to be a library call to non-C code), as the padding bits wouldn't necessarily be copied correctly.

The other major problem with tagging is that C requires the all-zero-bit representation to be valid (and zero) for many types, particularly integer types. In order to make this happen as little as possible, the zero tag is reserved, and its validity is only checked upon reading in types from the memory subsystem; when types are written back again, the non-zero tag is used.

The local objects discussed above are implemented by polymorphic C++ classes with the superclass `EmuVal`, corresponding to a single emulated value. Roughly speaking, the `EmuVal` objects correspond to the results of internal expressions in statements. `EmuVal` and its derived classes are all immutable, and quickly deleted after being used, so these do not need to interact with the memory subsystem except in the case that a value is being stored to a variable.

Each `EmuVal` keeps track of a status, either defined, undefined, or uninitialized, and propagates the undefined status through performed operations. The `EmuVal` class also has a `cast_to` method, which transforms one type of `EmuVal` to another. Each `EmuVal` also has a constructor that creates an `EmuVal` corresponding to a given memory block and offset, which works in the expected way.

Execution Subsystem
-------------------

As noted before, the core of the execution subsystem is simply the walking of the AST provided by Clang (technically, Clang provides one AST per file, so in general, we are walking a set of ASTs, but the difference is negligible after the first couple passes). First, an allocation pass is done through all input file to allocate memory blocks to each function and global variable, then a second pass is done to initialize each of these, either to a node in the AST for functions (so that dynamic function pointers work correctly), or to a memory block tagged as global. 

Finally, the main method is searched for and called, at which point further function calls are made recursively by the main routine `exec_stmt` until finally either `exit` is called (which is intercepted by the interpreter, as are a few special builtin functions needs to handle out-of-bound, see `external.cpp` for details), a `return` statement is made, or we simply hit the end of the main function.

Internally, the aforementioned `EmuVal` objects are used as rvalues, and a simple block+offset+type object aptly named `lvalue` is used to keep track of lvalues as they arise in expressions. The other two primary methods of the execution subsystem (in addition to `exec_stmt`) are `eval_lexpr` and `eval_rexpr`, which evaluate lvalue and rvalue expressions, respectively. Note that currently, this subsystem only implements a subset of the C standard, and on many unimplemented operations, the interpreter will simply halt.

From the "bottom end", which involves the root function calls made to a system (they can effectively be thought of as system calls to the interpreter), a special hack is used wherein the inline assembly directive `__asm__` is used with a string indicating the builtin CInterp function that needs to be called. Because `__asm__` is highly implementation and architecture-specific, doing this does not violate the standard or any reasonable assumptions about how calls happen. 

I/O Subsystem
-------------------
The I/O subsystem is, at this point, the least developed simply because there isn't yet a standard for interfacing with the OnlinePythonTutor json interface as advanced as there needs to be: the format does not allow for references to exist to internal sections of objects, because that doesn't happen in Python (essentially, everything is a pointer), which makes the format insufficient at the moment for dealing with structs and arrays, which are rather fundamental as far as C primitives go. In the meantime, a working json-based pseudo-format is used for output, although this does not actually work with the OnlinePythonTutor frontend at this time.

Communication happens on standard input and output for the executable, although this is very much able to change. Before and after each statement is executed, a call is made to check the current source line, and if it has changed, print the entire understanding of the system state, including all stack variables and memory blocks (even if unreferenced). In early testing, it was found that extraneous variables from various header files were clogging up useful output, so a check was done to only output relevant information if it derives from the provided input files (as opposed to provided system files and headers) or is pointed to by something from the provided input files.
