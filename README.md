# SPYRE
David Wilson

"Spyre" is a programming language that began development in December, 2015.  This is
the fifth (or so) iteration of Spyre.  The previous iterations can be found at:
+ https://github.com/ForeverDev/spyre
+ https://github.com/ForeverDev/spyre2
+ https://github.com/ForeverDev/spyre3
+ https://github.com/ForeverDev/spyre4

Each version of Spyre is increasingly better than the last (as my knowledge about
programming languages has grown).  

Spyre is a statically typed, interpreted programming language.  There are two
numerical data types in Spyre: `int` and `float`.  Both have 64 bit storage.  `int` is
automatically signed (although I may add unsigned support in the future?).

Spyre's compiler is bootstrapping.  That is, the Spyre compiler is written 
in Spyre.  The way this works is I wrote (am in the process of writing) 
a very small Spyre compiler in "Spyre assembly" that converts a source file 
into "Spyre assembly".  Then, using that very small Spyre language, I can 
rewrite a more complex compiler.

Spyre source code compiles into a set of instructions targeted at the Spyre
Virtual Machine.  Here is the set of instructions: (NOTE: Currently, the 
instruction set is pretty messy and out of order, I plan on improving this
in the future when I don't need to add any more instructions)

MNEMONIC	| OPCODE	| OPERANDS  
----------- |:---------:| --------  
NOOP		| 00		|		    
IPUSH		| 01		| INT64	constant
IADD		| 02		|
ISUB		| 03		|
IMUL		| 04		|
IDIV		| 05		|
IMOD		| 06		|
SHL			| 07		|
SHR			| 08		|
AND			| 09		|
OR			| 0A		|
XOR			| 0B		|
NOT			| 0C		|
NEG			| 0D		|
IGT			| 0E		|
IGE			| 0F		|
ILT			| 10		|
ILE			| 11		|
ICMP		| 12		|
JNZ			| 13		| INT32 addr
JZ			| 14		| INT32 addr
JMP			| 15		| INT32 addr
CALL		| 16		| INT32 addr, INT32 nargs
IRET		| 17		| 
CCALL		| 18		| INT32 ptrToCFunctionName
FPUSH		| 19		| FLOAT64 constant
FADD		| 1A		|
FSUB		| 1B		|
FMUL		| 1C		|
FDIV		| 1D		|
FGT			| 1E		|
FGE			| 1F		|
FLT			| 20		|
FLE			| 21		|
FCMP		| 22		|
FRET		| 23		|
ILLOAD		| 24		| INT32 varOffsetAddress
ILSAVE		| 25		| INT32 varOffsetAddress
IARG		| 26		| INT32 argumentNumber
ILOAD		| 27		|
ISAVE		| 28		|
RES			| 29		| INT32 wordsToReserve
LEA			| 2A		| INT32 varOffsetAddress
IDER		| 2B		| 
ICINC		| 2C		| INT64 increment
CDER		| 2D		|
LOR			| 2E		|
LAND		| 2F		|
PADD		| 30		|
PSUB		| 31		|
LOG			| 32		| INT32 constant
VRET		| 33		|
DBON		| 34		|
DBOFF		| 35		|
DBDS		| 36		|
CJNZ		| 37		|
CJZ			| 38		|
CJMP		| 39		|
ILNSAVE		| 3A		| INT32 start, INT32 num
ILNLOAD		| 3B		| INT32 start, INT32 num

NOTE:	many of the instructions specific to ints/floats can be generalized
		(e.g. `ICMP`, `FCMP` can be generalized to `CMP`).  This will be
		done in the near future.
