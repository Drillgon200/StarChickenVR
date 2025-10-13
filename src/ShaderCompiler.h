#pragma once

#include "DrillLib.h"
#include "SerializeTools.h"
#include "SPIRV.h"

namespace ShaderCompiler {
using namespace SPIRV;

#define DSL_TOKENS X(NULL)\
	X(STRING)\
	X(BOOL)\
	X(I8)\
	X(I16)\
	X(I32)\
	X(I64)\
	X(U8)\
	X(U16)\
	X(U32)\
	X(U64)\
	X(F16)\
	X(F32)\
	X(F64)\
	X(IDENTIFIER)\
	X(LEFT_PAREN)\
	X(RIGHT_PAREN)\
	X(LEFT_BRACE)\
	X(RIGHT_BRACE)\
	X(LEFT_BRACKET)\
	X(RIGHT_BRACKET)\
	X(STAR)\
	X(SLASH)\
	X(PERCENT)\
	X(DOUBLE_PERCENT)\
	X(PLUS)\
	X(DOUBLE_PLUS)\
	X(MINUS)\
	X(DOUBLE_MINUS)\
	X(EQUAL)\
	X(DOUBLE_EQUAL)\
	X(LESS_THAN)\
	X(GREATER_THAN)\
	X(LESS_THAN_OR_EQUAL)\
	X(GREATER_THAN_OR_EQUAL)\
	X(BANG_EQUAL)\
	X(DOUBLE_LEFT_ARROW)\
	X(DOUBLE_RIGHT_ARROW)\
	X(TRIPLE_RIGHT_ARROW)\
	X(AMPERSAND)\
	X(DOUBLE_AMPERSAND)\
	X(BAR)\
	X(DOUBLE_BAR)\
	X(BANG)\
	X(TILDE)\
	X(CARAT)\
	X(COMMA)\
	X(COLON)\
	X(SEMICOLON)\
	X(DOT)\
	X(HASH)\
	X(QUESTION_MARK)\
	X(AT)\
	X(DOLLAR)\
	X(VOID)\
	X(RETURN)\
	X(IF)\
	X(ELSE)\
	X(FOR)\
	X(WHILE)\
	X(BREAK)\
	X(CONTINUE)\
	X(STRUCT)\
	X(NONUNIFORM)\
	X(DEMOTE)\
	X(TERMINATE)

enum TokenType {
#define X(name) TOKEN_##name,
	DSL_TOKENS
#undef X
	TOKEN_Count
};
StrA TOKEN_NAMES[]{
#define X(name) "TOKEN_" #name##a,
	DSL_TOKENS
#undef X
	"TOKEN_Count"a
};
struct Token {
	TokenType type;
	U32 lineNumber;
	U32 columnNumber;
	U32 columnLength;
	union {
		// Values for if this token type has a value associated
		StrA vStr;
		B32 vBool;
		I8 vI8;
		I16 vI16;
		I32 vI32;
		I64 vI64;
		U8 vU8;
		U16 vU16;
		U32 vU32;
		U64 vU64;
		U16 vF16; // No F16 type in C++
		F32 vF32;
		F64 vF64;
		StrA vIdentifier;
	};
	bool operator==(const Token& other) const {
		if (type != other.type) {
			return false;
		}
		switch (type) {
		case TOKEN_STRING: return vStr == other.vStr;
		case TOKEN_BOOL: return vBool == other.vBool;
		case TOKEN_I8: return vI8 == other.vI8;
		case TOKEN_I16: return vI16 == other.vI16;
		case TOKEN_I32: return vI32 == other.vI32;
		case TOKEN_I64: return vI64 == other.vI64;
		case TOKEN_U8: return vU8 == other.vU8;
		case TOKEN_U16: return vU16 == other.vU16;
		case TOKEN_U32: return vU32 == other.vU32;
		case TOKEN_U64: return vU64 == other.vU64;
		case TOKEN_F16: return vF16 == other.vF16;
		case TOKEN_F32: return vF32 == other.vF32;
		case TOKEN_F64: return vF64 == other.vF64;
		case TOKEN_IDENTIFIER: return vIdentifier == other.vIdentifier;
		default: return true;
		}
	}
	B32 has_value() {
		return type >= TOKEN_STRING && type <= TOKEN_IDENTIFIER;
	}
};
template<>
struct Hasher<Token> {
	U64 operator()(Token val) {
		U64 hash = 0;
		switch (val.type) {
		case TOKEN_STRING: hash = Hasher<StrA>{}(val.vStr); break;
		case TOKEN_BOOL: hash = Hasher<U32>{}(val.vBool); break;
		case TOKEN_I8: hash = Hasher<U32>{}(U32(val.vI8)); break;
		case TOKEN_I16: hash = Hasher<U32>{}(U32(val.vI16)); break;
		case TOKEN_I32: hash = Hasher<U32>{}(U32(val.vI32)); break;
		case TOKEN_I64: hash = Hasher<U64>{}(U64(val.vI64)); break;
		case TOKEN_U8:  hash = Hasher<U32>{}(val.vU8); break;
		case TOKEN_U16: hash = Hasher<U32>{}(val.vU16); break;
		case TOKEN_U32: hash = Hasher<U32>{}(val.vU32); break;
		case TOKEN_U64: hash = Hasher<U64>{}(val.vU64); break;
		case TOKEN_F16: hash = Hasher<U32>{}(val.vF16); break;
		case TOKEN_F32: hash = Hasher<U32>{}(bitcast<U32>(val.vF32)); break;
		case TOKEN_F64: hash = Hasher<U64>{}(bitcast<U64>(val.vF64)); break;
		case TOKEN_IDENTIFIER: hash = Hasher<StrA>{}(val.vIdentifier); break;
		default: break;
		}
		// Boost hash combine
		hash ^= Hasher<U32>{}(val.type) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
};

#undef DSL_TOKENS

typedef U32 TCId;
const TCId TC_INVALID_ID = U32_MAX;

const U32 MAX_SHADER_INTERFACE_LOCATIONS = 256;

enum ShaderType {
	SHADER_TYPE_NONE,
	SHADER_TYPE_VERTEX,
	SHADER_TYPE_FRAGMENT,
	SHADER_TYPE_COMPUTE,
	SHADER_TYPE_INTER_SHADER_INTERFACE
};

enum TypeClass : U16 {
	TYPE_CLASS_VOID,
	TYPE_CLASS_SCALAR,
	TYPE_CLASS_VECTOR,
	TYPE_CLASS_ARRAY,
	TYPE_CLASS_STRUCT,
	TYPE_CLASS_POINTER,
	TYPE_CLASS_IMAGE,
	TYPE_CLASS_SAMPLER,
	TYPE_CLASS_SAMPLED_IMAGE
};
struct PointerType;
struct Type {
	StrA typeName;
	TypeClass typeClass;
	B8 typeEmitted : 1;
	SpvId id;
	U32 sizeBytes;
	U32 alignment;
};
struct VectorType;
struct ScalarType {
	enum ScalarTypeClass : U8 {
		SCALAR_NONE,
		SCALAR_BOOL,
		SCALAR_INT,
		SCALAR_FLOAT
	};
	Type type;
	ScalarTypeClass scalarTypeClass;
	B8 isSigned;
	U32 width;
	VectorType* v2Type;
	VectorType* v3Type;
	VectorType* v4Type;
};
struct VectorType {
	Type type;
	ScalarType* componentType;
	U32 numComponents;
};
struct ArrayType {
	Type type;
	Type* elementType;
	I32 numElements; // -1 if unbounded
	SpvId numElementsSpvConstant; // SPV_NULL_ID if unbounded
};
struct ArrayTypeInfo {
	SpvId typeId;
	I32 numElements;

	bool operator==(const ArrayTypeInfo other) const {
		return typeId == other.typeId && numElements == other.numElements;
	}
};
template<>
struct Hasher<ArrayTypeInfo> {
	U64 operator()(const ArrayTypeInfo key) const {
		U64 hash = Hasher<U32>{}(U32(key.numElements));
		hash ^= Hasher<U32>{}(key.typeId) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
};
struct StructType {
	struct Member {
		StrA name;
		U32 index;
		U32 modifierCtxIdx;
		U32 byteOffset;
		Type* type;
	};
	Type type;
	ArenaArrayList<Member> members;
	TCId structTCCodeStart;
	U32 scopeIdx;
	U32 modifierCtxIdx;
	B8 endsInVariableLengthArray;
	B8 currentlyResolvingStruct;
	B8 structEvaluationComplete;

	void init(StrA name, U32 spvId, MemoryArena* arena, TCId tcCodeStart, U32 structScope, U32 structModifierContext) {
		type.typeName = name;
		type.typeClass = TYPE_CLASS_STRUCT;
		type.id = spvId;
		members.allocator = arena;
		structTCCodeStart = tcCodeStart;
		scopeIdx = structScope;
		modifierCtxIdx = structModifierContext;
	}
};
struct PointerType {
	Type type;
	Type* pointedAt;
	StorageClass storageClass;
};
struct ImageType {
	Type type;
	Type* sampledType; // Must be a scalar numerical type
	Dimensionality dimension;
	U32 depth : 2; // 0 = not depth, 1 = depth, 2 = unknown
	U32 arrayed : 1;
	U32 multisample : 1;
	U32 sampled : 2; // 0 = only known at runtime, 1 = compatible with sampling operations, 2 = compatible with read/write operations
	ImageFormat format;
	Type* sampledImageType;
};

// A scoped name acts as a unique identifier for a variable.
// You can have more than one variable with the same name as long as they're in different scopes.
struct ScopedName {
	StrA name;
	U32 scopeIndex;

	bool operator==(const ScopedName& other) const {
		return other.scopeIndex == scopeIndex && other.name == name;
	}
};
template<>
struct Hasher<ScopedName> {
	U64 operator()(const ScopedName& val) const {
		U64 hash = Hasher<StrA>{}(val.name);
		hash ^= Hasher<U32>{}(val.scopeIndex) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
};

struct Variable {
	ScopedName name; // Empty if temporary
	Type* type;
	SpvId id;
	B32 isGlobal : 1;
	B32 isFromInterfaceBlock : 1;
};
struct VariableAccessChain {
	Variable* base;
	Type* resultType;
	ArenaArrayList<U32> accessIndices;
	B32 isPtrAccessChain;
};
struct GlobalVariable {
	Variable* var;
	SpvId initializer;
	U32 modifierCtxIdx;
};

struct ProcedureIdentifier {
	StrA name;
	SpvId* argTypes;
	U32 numArgs;

	bool operator==(const ProcedureIdentifier& other) const {
		if (name != other.name || numArgs != other.numArgs) {
			return false;
		}
		for (U32 i = 0; i < numArgs; i++) {
			if (argTypes[i] != other.argTypes[i]) {
				return false;
			}
		}
		return true;
	}
};

template<>
struct Hasher<ProcedureIdentifier> {
	U64 operator()(const ProcedureIdentifier& val) const {
		// https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector
		U64 hash = val.numArgs;
		for (U32 i = 0; i < val.numArgs; i++) {
			hash ^= val.argTypes[i] + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		}
		hash ^= Hasher<StrA>{}(val.name) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
};
struct ProcedureType {
	ProcedureIdentifier identifier;
	Type* returnType;

	B32 operator==(const ProcedureType& other) const {
		if (returnType != other.returnType || identifier.numArgs != other.identifier.numArgs) {
			return false;
		}
		for (U32 i = 0; i < identifier.numArgs; i++) {
			if (identifier.argTypes[i] != other.identifier.argTypes[i]) {
				return false;
			}
		}
		return true;
	}
};
template<>
struct Hasher<ProcedureType> {
	U64 operator()(const ProcedureType& val) const {
		U64 hash = val.identifier.numArgs;
		for (U32 i = 0; i < val.identifier.numArgs; i++) {
			hash ^= val.identifier.argTypes[i] + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		}
		hash ^= Hasher<U32>{}(val.returnType->id) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
};
struct DSLCompiler;
struct Procedure {
	ProcedureType signature;
	ScopedName* argNames; // Only for user defined procedures
	Type** argTypes; // Only for user defined procedures
	SpvId id;
	SpvId (*call)(ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params);
	U64 userData;
};

struct ShaderEntrypoint {
	StrA name;
	ExecutionModel executionModel;
	SpvId id;
	U32 shaderSectionIdx;
	I32 localSizeX, localSizeY, localSizeZ; // Only relevant for a compute shader context
};

struct EnabledExtensions {
	Flags32 multiview : 1;
	Flags32 physicalStorageBuffer : 1;
	Flags32 runtimeDescriptorArray : 1;
	Flags32 shaderNonUniform : 1;
	Flags32 sampledImageArrayNonUniformIndexing : 1;
	Flags32 demoteToHelperInvocation : 1;
};

struct DSLCompileError {
	U32 lineNumber;
	U32 columnNumber;
	U32 errorByteNumber;
	U32 errorLength;
	StrA message;
};
typedef void (*DLSCompileErrorCallback)(DSLCompileError&);

// Type Checking Intermediate Representation
// Kind of like an AST, but closer to bytecode
enum TCOpType : U16 {
	// It may be a simplification to remove operators from this IR and have everything as an OP_CALL, where OP_ADD would be an OP_CALL to operator+
	TC_OP_ADD,
	TC_OP_ADDONE,
	TC_OP_SUB,
	TC_OP_SUBONE,
	TC_OP_NEG,
	TC_OP_MUL,
	TC_OP_DIV,
	TC_OP_REM,
	TC_OP_MOD,
	TC_OP_SHL,
	TC_OP_SHR,
	TC_OP_SAR,
	TC_OP_AND,
	TC_OP_XOR,
	TC_OP_OR,
	TC_OP_NOT,
	TC_OP_LOGICAL_NOT,
	TC_OP_REFERENCE_TO,
	TC_OP_DEREFERENCE,
	TC_OP_MARK_NONUNIFORM,
	TC_OP_LT,
	TC_OP_LE,
	TC_OP_GT,
	TC_OP_GE,
	TC_OP_EQ,
	TC_OP_NE,

	TC_OP_CALL,
	TC_OP_SUBSCRIPT,
	TC_OP_ACCESS_CHAIN,
	TC_OP_DECLARE,
	TC_OP_ASSIGN,
	TC_OP_LOAD,
	TC_OP_PROCEDURE_DEFINE,
	TC_OP_STRUCT_DEFINE,
	TC_OP_RETURN,
	TC_OP_BREAK,
	TC_OP_CONTINUE,
	TC_OP_CONDITIONAL_BRANCH,
	TC_OP_CONDITIONAL_LOOP,
	TC_OP_BLOCK_END,
	TC_OP_DEMOTE,
	TC_OP_TERMINATE,

	TC_OP_STRING,
	TC_OP_BOOL,
	TC_OP_I8,
	TC_OP_I16,
	TC_OP_I32,
	TC_OP_I64,
	TC_OP_U8,
	TC_OP_U16,
	TC_OP_U32,
	TC_OP_U64,
	TC_OP_F16,
	TC_OP_F32,
	TC_OP_F64,
	TC_OP_IDENTIFIER,

	TC_OP_SCOPE_BEGIN,
	TC_OP_SCOPE_END,
	TC_OP_MODIFIER_SCOPE_BEGIN,
	TC_OP_MODIFIER_SCOPE_END,

	TC_OP_TC_JMP // Jump current read pointer to this TCId
};
enum ValueClass : U8 {
	VALUE_CLASS_NONE,
	VALUE_CLASS_RUNTIME_VALUE,
	VALUE_CLASS_ACCESS_CHAIN,
	VALUE_CLASS_TYPE,
	VALUE_CLASS_PROCEDURE,
	VALUE_CLASS_SCOPED_NAME
};
struct TCOp {
	TCOpType opcode;
	ValueClass resultValueClass;
	B8 resultIsNonUniform;
	U32 srcLine;
	U32 srcColumn;
	U32 srcLength;
	union {
		Variable* variable;
		VariableAccessChain* accessChain;
		Type* type;
		Procedure* procedure;
		ScopedName scopedName;
	};
	union {
		StrA vStr;
		B32 vBool;
		I8 vI8;
		I16 vI16;
		I32 vI32;
		I64 vI64;
		U8 vU8;
		U16 vU16;
		U32 vU32;
		U64 vU64;
		U16 vF16; // No F16 type in C++
		F32 vF32;
		F64 vF64;
		StrA vIdentifier;

		U32 operands[3]; // For unary/binary/ternary operands (e.g. TC_OP_ADD would have the two TCIds to add together, where add means operator+)
		struct {
			// For call/subscript/access chain
			U32 extendedOperandsLength;
			TCId* extendedOperands;
		};
	};

	bool is_literal() {
		return opcode >= TC_OP_STRING && opcode <= TC_OP_IDENTIFIER;
	}
};

enum TerminationInsn : U32 {
	TERMINATION_INSN_NONE,
	TERMINATION_INSN_RETURN,
	TERMINATION_INSN_RETURN_VALUE,
	TERMINATION_INSN_KILL,
	TERMINATION_INSN_UNREACHABLE,
	TERMINATION_INSN_TERMINATE_INVOCATION,
	TERMINATION_INSN_BRANCH,
	TERMINATION_INSN_BRANCH_CONDITIONAL,
	TERMINATION_INSN_SWITCH
};

// Corresponds to a brace pair 
struct ScopeContext {
	const static U32 INVALID_PARENT_IDX = U32_MAX;
	U32 parentIdx;
	U32 thisScopeIdx;
	U32 shaderSectionIdx;
	ArenaHashMap<StrA, Variable*> varNameToVar;
	ArenaHashMap<StrA, U32> scopeNameToScopeIdx;
	ArenaHashMap<StrA, Type*> typeNameToType;
	ArenaHashMap<ProcedureIdentifier, Procedure*> procedureIdentifierToProcedure;

	void init(MemoryArena* arena, U32 parentIdxIn, U32 thisIdxIn, U32 shaderSectionIn) {
		varNameToVar.allocator = arena;
		scopeNameToScopeIdx.allocator = arena;
		typeNameToType.allocator = arena;
		parentIdx = parentIdxIn;
		thisScopeIdx = thisIdxIn;
		shaderSectionIdx = shaderSectionIn;
	}

	Variable* find_var(U32* scopeIdxOut, ScopeContext* allContexts, StrA name) {
		Variable** foundVar = varNameToVar.find(name);
		if (foundVar) {
			*scopeIdxOut = thisScopeIdx;
			return *foundVar;
		} else {
			return parentIdx == INVALID_PARENT_IDX ? nullptr : allContexts[parentIdx].find_var(scopeIdxOut, allContexts, name);
		}
	}
	U32 find_scope(ScopeContext* allContexts, StrA name) {
		U32* foundScope = scopeNameToScopeIdx.find(name);
		if (foundScope) {
			return *foundScope;
		} else {
			return parentIdx == INVALID_PARENT_IDX ? U32_MAX : allContexts[parentIdx].find_scope(allContexts, name);
		}
	}
	Type* find_type(ScopeContext* allContexts, StrA name) {
		Type** foundType = typeNameToType.find(name);
		if (foundType) {
			return *foundType;
		} else {
			return parentIdx == INVALID_PARENT_IDX ? nullptr : allContexts[parentIdx].find_type(allContexts, name);
		}
	}
	Procedure* find_procedure(ScopeContext* allContexts, ProcedureIdentifier procIden) {
		Procedure** foundProc = procedureIdentifierToProcedure.find(procIden);
		if (foundProc) {
			return *foundProc;
		} else {
			return parentIdx == INVALID_PARENT_IDX ? nullptr : allContexts[parentIdx].find_procedure(allContexts, procIden);
		}
	}
};
struct ModifierContext {
	U32 parentIndex;
	ArenaArrayList<Decoration> decorations;
	FunctionParameterAttributes functionParameterAttributes;
	SelectionControl selectionControl;
	BuiltIn builtIn;
	AccessQualifier accessQualifier;
	StorageClass storageClass;
	const static U32 DESCRIPTOR_SET_INVALID = U32_MAX;
	U32 descriptorSet;
	const static U32 BINDING_INVALID = U32_MAX;
	U32 binding;
	const static U32 LOCATION_INVALID = U32_MAX;
	U32 location;
	LoopControl loopControl;
	U32 loopControlArgs[NUM_LOOP_CONTROLS];
	MemoryOperands memoryOperands;
	U32 memoryOperandArgs[NUM_MEMORY_OPERANDS];
	FunctionControl functionControl;
	U32 alignmentDecoration;
	B32 isEntrypoint;
	ExecutionModel shaderExecutionModel;
	I32 localSizeX, localSizeY, localSizeZ;

	void init(MemoryArena* arena) {
		decorations.allocator = arena;
		selectionControl = SELECTION_CONTROL_NONE;
		builtIn = BUILTIN_Invalid;
		accessQualifier = ACCESS_QUALIFIER_Invalid;
		storageClass = STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER;
		shaderExecutionModel = EXECUTION_MODEL_Invalid;
		descriptorSet = DESCRIPTOR_SET_INVALID;
		binding = BINDING_INVALID;
		location = LOCATION_INVALID;
		alignmentDecoration = 1;
	}

	void init(ModifierContext& other) {
		*this = other;
		decorations = ArenaArrayList<Decoration>{ other.decorations.allocator };
		decorations.push_back_n(other.decorations.data, other.decorations.size);
	}

	void set_loop_control(LoopControlFlag flag, U32 arg) {
		if (flag == LOOP_CONTROL_NONE) {
			loopControl = 0;
			return;
		}
		U32 loopControlIndex = 31 - lzcnt32(flag);
		if (loopControlIndex < NUM_LOOP_CONTROLS) {
			loopControlArgs[loopControlIndex] = arg;
		}
		// I'd use == instead of & here but that triggers a compiler warning I'd rather not disable
		if (flag & LOOP_CONTROL_DONT_UNROLL) {
			loopControl &= ~(LOOP_CONTROL_UNROLL | LOOP_CONTROL_PEEL_COUNT | LOOP_CONTROL_PARTIAL_COUNT);
		} else if (flag & LOOP_CONTROL_UNROLL) {
			loopControl &= ~LOOP_CONTROL_DONT_UNROLL;
		} else if (flag & LOOP_CONTROL_PEEL_COUNT) {
			loopControl &= ~LOOP_CONTROL_DONT_UNROLL;
		} else if (flag & LOOP_CONTROL_PARTIAL_COUNT) {
			loopControl &= ~LOOP_CONTROL_DONT_UNROLL;
		}
		loopControl |= flag;
	}
	
	void get_memory_operand_args(ArenaArrayList<SpvDword>& result) {
		result.push_back(memoryOperands);
		if (memoryOperands & MEMORY_OPERAND_ALIGNED) result.push_back(memoryOperandArgs[31 - lzcnt32(MEMORY_OPERAND_ALIGNED)]);
		if (memoryOperands & MEMORY_OPERAND_MAKE_POINTER_AVAILABLE) result.push_back(memoryOperandArgs[31 - lzcnt32(MEMORY_OPERAND_MAKE_POINTER_AVAILABLE)]);
		if (memoryOperands & MEMORY_OPERAND_MAKE_POINTER_VISIBLE) result.push_back(memoryOperandArgs[31 - lzcnt32(MEMORY_OPERAND_MAKE_POINTER_VISIBLE)]);
	}
	void set_memory_operand(MemoryOperandFlag operand, U32 arg) {
		if (operand == MEMORY_OPERAND_NONE) {
			memoryOperands = 0;
			return;
		}
		U32 memoryOperandIndex = 31 - lzcnt32(operand);
		if (memoryOperandIndex < NUM_MEMORY_OPERANDS) {
			memoryOperandArgs[memoryOperandIndex] = arg;
		}
		memoryOperands |= operand;
	}

	void add_decoration(Decoration decoration) {
		if (!decorations.contains(decoration)) {
			decorations.push_back(decoration);
		}
	}
};

struct ShaderSection {
	ShaderType shaderType;
	ArenaArrayList<GlobalVariable> globals;
	U32 scopeIdx;

	void init(MemoryArena* arena, ShaderType sectionType, U32 scope) {
		shaderType = sectionType;
		globals.allocator = arena;
		scopeIdx = scope;
	}
};

struct DSLCompiler {
	MemoryArena* arena;
	const char* src;
	U32 srcLen;
	U32 currentLine;
	U32 lastColumn;
	U32 currentColumn;
	U32 currentByte;
	U32 tabColumnWidth;
	Token nextToken;

	ShaderType currentShaderType;

	SpvId nextSpvId;

	B32 compilerErrored;
	B32 panicFlag;

	ArenaArrayList<TCOp> tcCode; // Generated type checking IR gets put in here
	ArenaHashMap<Token, TCId> savedConstants;

	ArenaArrayList<ScopeContext> allScopes;
	ArenaArrayList<ModifierContext> allModifierContexts;
	U32 currentScope;
	U32 currentModifierContext;

	ArenaArrayList<ShaderEntrypoint> entrypoints;
	ArenaArrayList<TCId> allProcedureTCIds;
	ArenaArrayList<ShaderSection> shaderSections; // When a new section is defined (#shader ...), that means we use a new set of global vars for any entrypoints
	ArenaArrayList<ArenaArrayList<Variable*>> shaderInterfaceLocationGroups;
	ArenaArrayList<Type*> allTypes;
	ArenaHashMap<ProcedureType, SpvId> procedureTypeToSpvId;
	ArenaHashMap<ArrayTypeInfo, ArrayType*> arrayTypeInfoToArrayType;
	ArenaHashMap<U64, PointerType*> typeAndStorageClassToPointerType; // I didn't feel like making another hashable type for this so the key is storageClass << 32 | pointedAtTypeSpvId;

	ArenaArrayList<SpvDword> procedureSpvCode;
	ArenaArrayList<SpvDword> decorationSpvCode;

	EnabledExtensions enabledExtensions;
	Type* defaultTypeVoid, * defaultTypeBool, * defaultTypeI32, * defaultTypeU32, * defaultTypeF32, * defaultTypeSampler;
	SpvId addCarryStructTypeId;
	SpvId glsl450InstructionSet;

	DLSCompileErrorCallback errorCallback;

	void init(MemoryArena& arenaInput, const char* srcInput, U32 srcSizeInput) {
		arena = &arenaInput;
		tcCode.allocator = &arenaInput;
		savedConstants.allocator = &arenaInput;
		entrypoints.allocator = &arenaInput;
		allProcedureTCIds.allocator = &arenaInput;
		shaderSections.allocator = &arenaInput;
		shaderInterfaceLocationGroups.allocator = &arenaInput;
		allScopes.allocator = &arenaInput;
		allTypes.allocator = &arenaInput;
		allModifierContexts.allocator = &arenaInput;
		procedureTypeToSpvId.allocator = &arenaInput;
		arrayTypeInfoToArrayType.allocator = &arenaInput;
		typeAndStorageClassToPointerType.allocator = &arenaInput;
		procedureSpvCode.allocator = &arenaInput;
		decorationSpvCode.allocator = &arenaInput;
		allScopes.push_back_zeroed().init(arena, ScopeContext::INVALID_PARENT_IDX, 0, 0);
		allModifierContexts.clear();
		src = srcInput;
		srcLen = srcSizeInput;
		tabColumnWidth = 4;
		nextSpvId = 1;
		next_token();
	}

	void parse_error(StrA msg, B32 panic = false) {
		compilerErrored = true;
		print(strafmt(*arena, "Error reported: %. Line %, column %-%\n"a, msg, currentLine, lastColumn, currentColumn));
		if (panic) {
			while (nextToken.type != TOKEN_SEMICOLON && nextToken.type != TOKEN_LEFT_BRACE && nextToken.type != TOKEN_RIGHT_BRACE && nextToken.type != TOKEN_NULL) {
				next_token();
			}
		}
	}
	void generation_error(StrA msg, TCOp& failureToken) {
		compilerErrored = true;
		print(strafmt(*arena, "Error reported: %. Line %, column %-%\n"a, msg, failureToken.srcLine, failureToken.srcColumn, failureToken.srcColumn + failureToken.srcLength));
	}
	void generic_error(StrA msg) {
		compilerErrored = true;
		print(stracat(*arena, "Error reported: "a, msg, ".\n"a));
	}

	// TOKENIZER BEGIN //

	Token parse_number() {
		using namespace SerializeTools;
		StrA remaining{ src + currentByte, srcLen - currentByte };
		U32 base = 10;
		if (remaining.length >= 2 && remaining.str[0] == '0' && remaining.str[1] == 'x') {
			base = 16;
			remaining.str += 2, remaining.length -= 2;
		} else if (remaining.length >= 2 && remaining.str[0] == '0' && remaining.str[1] == 'b') {
			base = 2;
			remaining.str += 2, remaining.length -= 2;
		}
		U32 integerPartLength = 0;
		while (integerPartLength < remaining.length) {
			char c = remaining.str[integerPartLength];
			if (base == 10 && c >= '0' && c <= '9' ||
				base == 2 && c >= '0' && c <= 1 ||
				base == 16 && (c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F')) {
				integerPartLength++;
			} else {
				break;
			}
		}
		remaining.str += integerPartLength, remaining.length -= integerPartLength;
		B32 isNonInteger = false;
		U32 decimalPartLength = 0;
		if (remaining.length && remaining.str[0] == '.') {
			isNonInteger = true;
			remaining.str++, remaining.length--;
			while (decimalPartLength < remaining.length) {
				char c = remaining.str[decimalPartLength];
				if (c >= '0' && c <= '9') {
					decimalPartLength++;
				} else {
					break;
				}
			}
			remaining.str += decimalPartLength, remaining.length -= decimalPartLength;
		}
		if (isNonInteger && base != 10) {
			parse_error("Binary and hex floating point literals currently unsupported"a);
		}
		// Skip exponent
		if (remaining.length && remaining.str[0] == 'e') {
			remaining.str++, remaining.length--;
			if (remaining.length && remaining.str[0] == '-' || remaining.str[0] == '+') {
				remaining.str++, remaining.length--;
			}
			while (remaining.length && is_digit(remaining.str[0])) {
				remaining.str++, remaining.length--;
			}
		}
		StrA literalSuffix{ remaining.str, 0 };
		while (literalSuffix.length < remaining.length && is_alphadigit(literalSuffix.str[literalSuffix.length])) {
			literalSuffix.length++;
		}
		TokenType type = isNonInteger ? TOKEN_F32 : TOKEN_I32;
		if (literalSuffix.length) {
			literalSuffix = literalSuffix.uppercase(*arena);
			if (literalSuffix == "I8"a) { type = TOKEN_I8; }
			else if (literalSuffix == "I16"a) { type = TOKEN_I16; }
			else if (literalSuffix == "I32"a || literalSuffix == "I"a) { type = TOKEN_I32; }
			else if (literalSuffix == "I64"a) { type = TOKEN_I64; }
			else if (literalSuffix == "U8"a) { type = TOKEN_U8; }
			else if (literalSuffix == "U16"a) { type = TOKEN_U16; }
			else if (literalSuffix == "U32"a || literalSuffix == "U"a) { type = TOKEN_U32; }
			else if (literalSuffix == "U64"a) { type = TOKEN_U64; }
			else if (literalSuffix == "F16"a) { type = TOKEN_F16; }
			else if (literalSuffix == "F32"a || literalSuffix == "F"a) { type = TOKEN_F32; }
			else if (literalSuffix == "F64"a || literalSuffix == "D"a) { type = TOKEN_F64; }
			else { parse_error("Invalid literal suffix"a); }
		}
		Token token{ type };
		remaining = StrA{ src + currentByte, srcLen - currentByte };
		if (type == TOKEN_F16 || type == TOKEN_F32 || type == TOKEN_F64) {
			F64 num = 0.0;
			B32 success = parse_f64(&num, &remaining);
			switch (type) {
			case TOKEN_F16: parse_error("16 bit float not supported"a); break;
			case TOKEN_F32: token.vF32 = F32(num); break;
			case TOKEN_F64: token.vF64 = num; break;
			default: break;
			}
			if (!success) {
				parse_error("Float parse error"a);
			}
		} else {
			U64 num = 0;
			IntParseError parseError = parse_u64(&num, &remaining);
			U64 bound = I32_MAX;
			switch (type) {
			case TOKEN_I8: token.vI8 = I8(num); bound = I8_MAX; break;
			case TOKEN_I16: token.vI16 = I16(num); bound = I16_MAX; break;
			case TOKEN_I32: token.vI32 = I32(num); bound = I32_MAX; break;
			case TOKEN_I64: token.vI64 = I64(num); bound = I64_MAX; break;
			case TOKEN_U8: token.vU8 = U8(num); bound = U8_MAX; break;
			case TOKEN_U16: token.vU16 = U16(num); bound = U16_MAX; break;
			case TOKEN_U32: token.vU32 = U32(num); bound = U32_MAX; break;
			case TOKEN_U64: token.vU64 = U64(num); bound = U64_MAX; break;
			default: break;
			}
			if (parseError == INT_PARSE_OVERFLOW || num > bound) {
				parse_error("Integer too large for type"a);
			} else if (parseError != INT_PARSE_SUCCESS) {
				parse_error("Integer parse error"a);
			}
		}
		U32 byteDiff = U32(remaining.str - src) - currentByte;
		currentByte += byteDiff + U32(literalSuffix.length);
		currentColumn += byteDiff + U32(literalSuffix.length);
		return token;
	}

	StrA parse_string(char stringEndChar) {
		char* result = reinterpret_cast<char*>(arena->stackBase) + arena->stackPtr;
		U32 length = 0;
		while (currentByte < srcLen && src[currentByte] != stringEndChar) {
			char c = src[currentByte++];
			currentColumn++;
			if (c == '\\') {
				if (currentByte >= srcLen) {
					parse_error("Reached end of src while parsing string"a);
					return StrA{};
				}
				currentColumn++;
				switch (src[currentByte++]) {
				case '\'': result[length] = '\''; break;
				case '\"': result[length] = '\"'; break;
				case '\?': result[length] = '\?'; break;
				case '\\': result[length] = '\\'; break;
				case '\a': result[length] = '\a'; break;
				case '\b': result[length] = '\b'; break;
				case '\f': result[length] = '\f'; break;
				case '\n': result[length] = '\n'; break;
				case '\r': result[length] = '\r'; break;
				case '\t': result[length] = '\t'; break;
				case '\v': result[length] = '\v'; break;
				case 'x': {
					if (currentByte >= srcLen) {
						parse_error("Reached end of src while parsing string"a);
						return StrA{};
					}
					currentColumn++;
					c = src[currentByte++];
					U32 hexLiteral = 0;
					if (SerializeTools::is_hex_digit(c)) {
						hexLiteral = SerializeTools::hex_digit_to_u32(c);
						if (currentByte < srcLen && SerializeTools::is_hex_digit(src[currentByte])) {
							currentColumn++;
							hexLiteral = hexLiteral << 4 | SerializeTools::hex_digit_to_u32(src[currentByte++]);
						}
					} else {
						parse_error("Invalid hex literal"a);
					}
					result[length] = char(hexLiteral);
				} break;
				default: parse_error("Unknown escape in string literal"a);
				}
			} else {
				result[length] = c;
			}
			length++;
		}
		currentColumn++;
		if (src[currentByte++] != stringEndChar) {
			parse_error("Reached end of src while parsing string"a);
		}
		arena->stackPtr += length;
		return StrA{ result, length };
	}

	Token read_token() {
		while (true) {
			if (currentByte >= srcLen) {
				return Token{ TOKEN_NULL };
			}
			char nextChar = src[currentByte++];
			lastColumn = currentColumn++;
			char nextNextChar = currentByte >= srcLen ? '\0' : src[currentByte];
			switch (nextChar) {
			case '\n': currentLine++, currentColumn = 0; continue;
			case '\r':
			case '\v':
			case '\f':
			case ' ': continue;
			case '\t': currentColumn += tabColumnWidth - 1; continue;
			case '\0': return Token{ TOKEN_NULL };
			case '+': return nextNextChar == '+' ? (currentByte++, currentColumn++, Token{ TOKEN_DOUBLE_PLUS }) : Token{ TOKEN_PLUS };
			case '-': return nextNextChar == '-' ? (currentByte++, currentColumn++, Token{ TOKEN_DOUBLE_MINUS }) : Token{ TOKEN_MINUS };
			case '*': return Token{ TOKEN_STAR };
			case '/': {
				if (currentByte < srcLen && src[currentByte] == '/') {
					while (currentByte < srcLen && src[currentByte] != '\n') {
						currentByte++;
					}
					currentLine++, currentByte++;
					currentColumn = 0;
				} else if (currentByte < srcLen && src[currentByte] == '*') {
					currentByte++, currentColumn++;
					U32 commentDepth = 1;
					while (commentDepth && currentByte + 1 < srcLen) {
						if (src[currentByte] == '/' && src[currentByte + 1] == '*') {
							commentDepth++;
							currentByte += 2, currentColumn += 2;
						} else if (src[currentByte] == '*' && src[currentByte + 1] == '/') {
							commentDepth--;
							currentByte += 2, currentColumn += 2;
						} else {
							if (src[currentByte] == '\n') {
								currentLine++, currentColumn = 0;
							}
							currentByte++, currentColumn++;
						}
					}
				} else {
					return Token{ TOKEN_SLASH };
				}
			} break;
			case '%': return nextNextChar == '%' ? (currentByte++, currentColumn++, Token{ TOKEN_DOUBLE_PERCENT }) : Token{ TOKEN_PERCENT };
			case '=': return nextNextChar == '=' ? (currentByte++, currentColumn++, Token{ TOKEN_DOUBLE_EQUAL }) : Token{ TOKEN_EQUAL };
			case '<': return nextNextChar == '<' ? (currentByte++, currentColumn++, Token{ TOKEN_DOUBLE_LEFT_ARROW }) :
				             nextNextChar == '=' ? (currentByte++, currentColumn++, Token{ TOKEN_LESS_THAN_OR_EQUAL }) : Token{ TOKEN_LESS_THAN };
			case '>':
				if (nextNextChar == '>') {
					currentByte++, currentColumn++;
					nextNextChar = currentByte >= srcLen ? '\0' : src[currentByte];
					return nextNextChar == '>' ? (currentByte++, currentColumn++, Token{ TOKEN_TRIPLE_RIGHT_ARROW }) : Token{ TOKEN_DOUBLE_RIGHT_ARROW };
				} else {
					return nextNextChar == '=' ? (currentByte++, currentColumn++, Token{ TOKEN_GREATER_THAN_OR_EQUAL }) : Token{ TOKEN_GREATER_THAN };
				}
			case '&': return nextNextChar == '&' ? (currentByte++, currentColumn++, Token{ TOKEN_DOUBLE_AMPERSAND }) : Token{ TOKEN_AMPERSAND };
			case '|': return nextNextChar == '|' ? (currentByte++, currentColumn++, Token{ TOKEN_DOUBLE_BAR }) : Token{ TOKEN_BAR };
			case '!': return nextNextChar == '=' ? (currentByte++, currentColumn++, Token{ TOKEN_BANG_EQUAL }) : Token{ TOKEN_BANG };
			case '~': return Token{ TOKEN_TILDE };
			case '^': return Token{ TOKEN_CARAT };
			case '(': return Token{ TOKEN_LEFT_PAREN };
			case ')': return Token{ TOKEN_RIGHT_PAREN };
			case '{': return Token{ TOKEN_LEFT_BRACE };
			case '}': return Token{ TOKEN_RIGHT_BRACE };
			case '[': return Token{ TOKEN_LEFT_BRACKET };
			case ']': return Token{ TOKEN_RIGHT_BRACKET };
			case '"':
			case '\'': {
				Token result{ TOKEN_STRING };
				result.vStr = parse_string(nextChar);
				return result;
			}
			case ',': return Token{ TOKEN_COMMA };
			case ':': return Token{ TOKEN_COLON };
			case ';': return Token{ TOKEN_SEMICOLON };
			case '.': return Token{ TOKEN_DOT };
			case '#': return Token{ TOKEN_HASH };
			case '?': return Token{ TOKEN_QUESTION_MARK };
			case '@': return Token{ TOKEN_AT };
			case '$': return Token{ TOKEN_DOLLAR };
			default: {
				using namespace SerializeTools;
				if (is_digit(nextChar)) {
					currentByte--, currentColumn--;
					return parse_number();
				}
				Token result{ TOKEN_IDENTIFIER };
				currentByte--, currentColumn--;
				StrA iden{ src + currentByte, 0 };
				while (is_alphadigit(src[currentByte]) || src[currentByte] == '_') {
					currentByte++, currentColumn++, iden.length++;
				}
				if (iden.length == 0) {
					parse_error("Tried to parse identifier with zero length"a);
				}
				result.vIdentifier = iden;
				if (iden == "true"a) {
					result = Token{ TOKEN_BOOL };
					result.vBool = true;
					return result;
				} else if (iden == "false"a) {
					result = Token{ TOKEN_BOOL };
					result.vBool = false;
					return result;
				}
				else if (iden == "void"a)     { return Token{ TOKEN_VOID }; }
				else if (iden == "return"a)   { return Token{ TOKEN_RETURN }; }
				else if (iden == "if"a)       { return Token{ TOKEN_IF }; }
				else if (iden == "else"a)     { return Token{ TOKEN_ELSE }; }
				else if (iden == "for"a)      { return Token{ TOKEN_FOR }; }
				else if (iden == "while"a)    { return Token{ TOKEN_WHILE }; }
				else if (iden == "break"a)    { return Token{ TOKEN_BREAK }; }
				else if (iden == "continue"a) { return Token{ TOKEN_CONTINUE };} 
				else if (iden == "struct"a)   { return Token{ TOKEN_STRUCT }; }
				else if (iden == "nonuniform"a) { return Token{ TOKEN_NONUNIFORM }; }
				else if (iden == "demote"a) { return Token{ TOKEN_DEMOTE }; }
				else if (iden == "terminate"a) { return Token{ TOKEN_TERMINATE }; }
				else { return result; }
			}
			}
		}

		return Token{ TOKEN_NULL };
	}

	Token next_token() {
		Token token = nextToken;
		nextToken = read_token();
		nextToken.lineNumber = currentLine;
		nextToken.columnNumber = lastColumn;
		// Most text editors use one based indexing for columns
		nextToken.columnLength = currentColumn - lastColumn + 1;
		return token;
	}
	B32 try_consume(TokenType type) {
		if (nextToken.type == type) {
			next_token();
			return true;
		} else {
			return false;
		}
	}
	B32 try_consume(Token* token, TokenType type) {
		if (nextToken.type == type) {
			*token = next_token();
			return true;
		} else {
			return false;
		}
	}
	Token consume(TokenType type) {
		if (nextToken.type != type) {
			parse_error(strafmt(*arena, "Expected token %, got %"a, TOKEN_NAMES[type], TOKEN_NAMES[nextToken.type]), true);
			next_token();
			return Token{};
		}
		return next_token();
	}

	// TOKENIZER END //


	// EXPRESSION PARSER BEGIN //

	TCId tc_ternary(TCOpType type, U32 operand1, U32 operand2, U32 operand3, const Token& lineInfo) {
		TCOp op{};
		op.opcode = type;
		op.operands[0] = operand1;
		op.operands[1] = operand2;
		op.operands[2] = operand3;
		op.srcLine = lineInfo.lineNumber;
		op.srcColumn = lineInfo.columnNumber;
		op.srcLength = lineInfo.columnLength;
		tcCode.push_back(op);
		return TCId(tcCode.size - 1);
	}
	TCId tc_binary(TCOpType type, U32 operand1, U32 operand2, const Token& lineInfo) {
		return tc_ternary(type, operand1, operand2, TC_INVALID_ID, lineInfo);
	}
	TCId tc_unary(TCOpType type, U32 operand, const Token& lineInfo) {
		return tc_ternary(type, operand, TC_INVALID_ID, TC_INVALID_ID, lineInfo);
	}
	TCId tc_nullary(TCOpType type, const Token& lineInfo) {
		return tc_ternary(type, TC_INVALID_ID, TC_INVALID_ID, TC_INVALID_ID, lineInfo);
	}
	TCId tc_nary(TCOpType type, U32* operands, U32 operandCount, const Token& lineInfo) {
		TCOp& op = tcCode.push_back_zeroed();
		op.opcode = type;
		op.extendedOperands = operands;
		op.extendedOperandsLength = operandCount;
		op.srcLine = lineInfo.lineNumber;
		op.srcColumn = lineInfo.columnNumber;
		op.srcLength = lineInfo.columnLength;
		return TCId(tcCode.size - 1);
	}
	TCId tc_literal(Token lit) {
		TCId* litId = savedConstants.find(lit);
		if (litId) {
			return *litId;
		} else {
			TCOp op{};
			op.srcLine = lit.lineNumber;
			op.srcColumn = lit.columnNumber;
			op.srcLength = lit.columnLength;
			if (lit.type == TOKEN_IDENTIFIER) {
				op.opcode = TC_OP_IDENTIFIER;
				op.vIdentifier = lit.vIdentifier;
			} else {
				op.resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
				op.variable = arena->zalloc<Variable>(1);
				op.variable->id = nextSpvId++;
				switch (lit.type) {
				case TOKEN_STRING: parse_error("String unsupported"a); break;
				case TOKEN_BOOL: op.opcode = TC_OP_BOOL, op.vBool = lit.vBool, op.variable->type = defaultTypeBool; break;
				case TOKEN_I8: parse_error("I8 unsupported"a); break;
				case TOKEN_I16: parse_error("I16 unsupported"a); break;
				case TOKEN_I32: op.opcode = TC_OP_I32, op.vI32 = lit.vI32, op.variable->type = defaultTypeI32; break;
				case TOKEN_I64: parse_error("I64 unsupported"a); break;
				case TOKEN_U8: parse_error("U8 unsupported"a); break;
				case TOKEN_U16: parse_error("U16 unsupported"a); break;
				case TOKEN_U32: op.opcode = TC_OP_U32, op.vU32 = lit.vU32, op.variable->type = defaultTypeU32; break;
				case TOKEN_U64: parse_error("U64 unsupported"a); break;
				case TOKEN_F16: parse_error("F16 unsupported"a); break;
				case TOKEN_F32: op.opcode = TC_OP_F32, op.vF32 = lit.vF32, op.variable->type = defaultTypeF32; break;
				case TOKEN_F64: parse_error("F64 unsupported"a); break;
				default: parse_error("Supposed literal did not have literal type"a);
				}
			}
			tcCode.push_back(op);
			savedConstants.insert(lit, TCId(tcCode.size - 1));
			return TCId(tcCode.size - 1);
		}
	}
	SpvId spv_literal_index(U32 idx) {
		Token token{ TOKEN_U32 };
		token.vU32 = idx;
		return tcCode.data[tc_literal(token)].variable->id;
	}

	I32 get_precedence(TokenType type) {
		switch (type) {
		case TOKEN_DOUBLE_PLUS:
		case TOKEN_DOUBLE_MINUS:
		case TOKEN_NONUNIFORM:
			return 0;
		case TOKEN_STAR:
		case TOKEN_SLASH:
		case TOKEN_PERCENT:
		case TOKEN_DOUBLE_PERCENT:
			return 1;
		case TOKEN_PLUS:
		case TOKEN_MINUS:
			return 2;
		case TOKEN_DOUBLE_LEFT_ARROW:
		case TOKEN_DOUBLE_RIGHT_ARROW:
		case TOKEN_TRIPLE_RIGHT_ARROW:
			return 3;
		case TOKEN_AMPERSAND:
			return 4;
		case TOKEN_CARAT:
		case TOKEN_TILDE:
		case TOKEN_BANG:
			return 5;
		case TOKEN_BAR:
			return 6;
		case TOKEN_GREATER_THAN:
		case TOKEN_GREATER_THAN_OR_EQUAL:
		case TOKEN_LESS_THAN:
		case TOKEN_LESS_THAN_OR_EQUAL:
			return 7;
		case TOKEN_DOUBLE_EQUAL:
		case TOKEN_BANG_EQUAL:
			return 8;
		case TOKEN_DOUBLE_AMPERSAND:
			return 9;
		case TOKEN_DOUBLE_BAR:
			return 10;
		case TOKEN_QUESTION_MARK:
			return 11;
		case TOKEN_EQUAL:
			return 12;
		case TOKEN_COMMA:
			return 13;
		default:
			return -1;
		}
	}

	U32 begin_scope() {
		allScopes.push_back_zeroed();
		allScopes.back().init(arena, currentScope, allScopes.size - 1, shaderSections.size - 1);
		currentScope = allScopes.size - 1;
		tc_unary(TC_OP_SCOPE_BEGIN, currentScope, Token{});
		return currentScope;
	}
	void end_scope() {
		tc_nullary(TC_OP_SCOPE_END, Token{});
		currentScope = allScopes.data[currentScope].parentIdx;
	}

	U32 parse_expression_list(ArenaArrayList<TCId>& results, TokenType endToken) {
		U32 numItems = 0;
		while (nextToken.type != endToken) {
			results.push_back(parse_expression(get_precedence(TOKEN_COMMA)));
			numItems++;
			if (nextToken.type != TOKEN_COMMA) {
				break;
			}
			consume(TOKEN_COMMA);
		}
		return numItems;
	}

	TCId parse_unary(TCId priorValue) {
		if (priorValue != TC_INVALID_ID) {
			// postfix
			if (nextToken.type == TOKEN_LEFT_PAREN || nextToken.type == TOKEN_LEFT_BRACKET) {
				TokenType closingToken = nextToken.type == TOKEN_LEFT_PAREN ? TOKEN_RIGHT_PAREN : TOKEN_RIGHT_BRACKET;
				Token lastToken = next_token();
				ArenaArrayList<TCId> args{ arena };
				args.push_back(priorValue);
				parse_expression_list(args, closingToken);
				consume(closingToken);
				return parse_unary(tc_nary(closingToken == TOKEN_RIGHT_PAREN ? TC_OP_CALL : TC_OP_SUBSCRIPT, args.data, args.size, lastToken));
			} else if (nextToken.type == TOKEN_DOUBLE_PLUS || nextToken.type == TOKEN_DOUBLE_MINUS) {
				Token readToken = next_token();
				TCId oldVal = tc_unary(TC_OP_LOAD, priorValue, readToken);
				tc_binary(TC_OP_ASSIGN, priorValue, tc_unary(readToken.type == TOKEN_DOUBLE_PLUS ? TC_OP_ADDONE : TC_OP_SUBONE, priorValue, readToken), readToken);
				return parse_unary(oldVal);
			} else if (nextToken.type == TOKEN_DOT) {
				ArenaArrayList<TCId> identifierAccessChain{ arena };
				identifierAccessChain.push_back(priorValue);
				Token lastToken;
				while (nextToken.type == TOKEN_DOT) {
					consume(TOKEN_DOT);
					identifierAccessChain.push_back(tc_literal(lastToken = consume(TOKEN_IDENTIFIER)));
				}
				return parse_unary(tc_nary(TC_OP_ACCESS_CHAIN, identifierAccessChain.data, identifierAccessChain.size, lastToken));
			} else {
				return priorValue;
			}
		} else {
			// value/prefix
			if (nextToken.type == TOKEN_LEFT_PAREN) {
				consume(TOKEN_LEFT_PAREN);
				TCId result = parse_expression(I32_MAX);
				consume(TOKEN_RIGHT_PAREN);
				return parse_unary(result);
			} else if (nextToken.type == TOKEN_LEFT_BRACE) {
				return parse_scoped_block();
			} else if (nextToken.type == TOKEN_AT) {
				ArenaArrayList<TCId> procedureInfo{ arena };
				procedureInfo.push_back(); // Name
				procedureInfo.push_back(); // Total TCOps
				procedureInfo.push_back(); // Num return TCOps
				procedureInfo.push_back(); // Num param TCOps
				procedureInfo.push_back(); // Num body TCOps
				U32 parentScope = currentScope;
				U32 procScope = begin_scope();
				procedureInfo.push_back(procScope);
				procedureInfo.push_back(currentModifierContext);

				TCId result = tc_nary(TC_OP_PROCEDURE_DEFINE, procedureInfo.data, procedureInfo.size, consume(TOKEN_AT));
				U32 prevTotalTCCodeSize = tcCode.size;

				// Declarations in this TC code will be collected into param/return lists
				// Returns
				consume(TOKEN_LEFT_BRACKET);
				U32 prevTCCodeSize = tcCode.size;
				parse_expression(I32_MAX);
				procedureInfo.data[2] = tcCode.size - prevTCCodeSize;
				consume(TOKEN_RIGHT_BRACKET);
				// Params
				consume(TOKEN_LEFT_BRACKET);
				prevTCCodeSize = tcCode.size;
				parse_expression(I32_MAX);
				procedureInfo.data[3] = tcCode.size - prevTCCodeSize;
				consume(TOKEN_RIGHT_BRACKET);

				if (nextToken.type == TOKEN_IDENTIFIER) {
					allScopes.data[parentScope].scopeNameToScopeIdx.insert(nextToken.vIdentifier, procScope);
				}

				prevTCCodeSize = tcCode.size;
				procedureInfo.data[0] = nextToken.type == TOKEN_IDENTIFIER ? tc_literal(consume(TOKEN_IDENTIFIER)) : TC_INVALID_ID;
				if (nextToken.type == TOKEN_LEFT_BRACE) {
					consume(TOKEN_LEFT_BRACE);
					parse_block();
					consume(TOKEN_RIGHT_BRACE);
				}
				procedureInfo.data[4] = tcCode.size - prevTCCodeSize;
				procedureInfo.data[1] = tcCode.size - prevTotalTCCodeSize;
				end_scope();
				allProcedureTCIds.push_back(result);
				return parse_unary(result);
			} else if (nextToken.type == TOKEN_STRUCT) {
				Token structToken = consume(TOKEN_STRUCT);
				Token structName{};
				if (nextToken.type == TOKEN_IDENTIFIER) {
					structName = next_token();
				}

				consume(TOKEN_LEFT_BRACE);
				TCId result = tc_binary(TC_OP_STRUCT_DEFINE, 0, allTypes.size, structToken);
				StructType* newStruct = arena->zalloc<StructType>(1);
				allTypes.push_back(&newStruct->type);
				U32 prevTCCodeSize = tcCode.size;
				U32 structScopeId = begin_scope();
				U32 structModifierCtx = currentModifierContext;
				// All of the declarations present in this TC code will be gathered into the struct's fields.
				while (nextToken.type != TOKEN_RIGHT_BRACE && nextToken.type != TOKEN_NULL) {
					parse_expression(I32_MAX);
					consume(TOKEN_SEMICOLON);
				}
				end_scope();
				tcCode.data[result].operands[0] = tcCode.size - prevTCCodeSize;
				consume(TOKEN_RIGHT_BRACE);

				
				newStruct->init(structName.vIdentifier, nextSpvId++, arena, result, structScopeId, structModifierCtx);
				if (structName.vIdentifier.str) {
					allScopes.data[currentScope].typeNameToType.insert(structName.vIdentifier, &newStruct->type);
					allScopes.data[currentScope].scopeNameToScopeIdx.insert(structName.vIdentifier, structScopeId);
				}

				return parse_unary(result);
			} else if (nextToken.type == TOKEN_IF) {
				Token ifToken = consume(TOKEN_IF);
				TCId condition = parse_expression(I32_MAX);
				TCId ifStatement = tc_ternary(TC_OP_CONDITIONAL_BRANCH, condition, 0, 0, ifToken);
				TCId conditionTrueResult = parse_scoped_block();
				tc_nullary(TC_OP_BLOCK_END, Token{});

				TCId conditionFalseResult = TC_INVALID_ID;
				if (nextToken.type == TOKEN_ELSE) {
					consume(TOKEN_ELSE);
					if (nextToken.type == TOKEN_IF) {
						conditionFalseResult = parse_unary(TC_INVALID_ID);
					} else {
						conditionFalseResult = parse_scoped_block();
					}
				}
				tc_nullary(TC_OP_BLOCK_END, Token{});

				if ((conditionTrueResult == TC_INVALID_ID) != (conditionFalseResult == TC_INVALID_ID)) {
					parse_error("One if block yields value but other does not"a);
				}
				
				tcCode.data[ifStatement].operands[1] = conditionTrueResult;
				tcCode.data[ifStatement].operands[2] = conditionFalseResult;
				return parse_unary(ifStatement);
			} else if (nextToken.type == TOKEN_WHILE || nextToken.type == TOKEN_FOR) {
				Token loopToken = next_token();
				TCId runAfterLoop = TC_INVALID_ID;
				TCId runAfterLoopJmpBack = TC_INVALID_ID;
				if (loopToken.type == TOKEN_FOR) {
					begin_scope();
					parse_expression(I32_MAX);
					consume(TOKEN_SEMICOLON);
					TCId loopStatement = tc_unary(TC_OP_CONDITIONAL_LOOP, 0, loopToken);
					begin_scope();
					TCId condition = parse_expression(I32_MAX);
					tcCode.data[loopStatement].operands[0] = condition;
					consume(TOKEN_SEMICOLON);
					TCId jumpOverRunAfterLoop = tc_unary(TC_OP_TC_JMP, 0, Token{});
					runAfterLoop = tcCode.size;
					parse_expression(I32_MAX);
					runAfterLoopJmpBack = tc_unary(TC_OP_TC_JMP, 0, Token{});
					tcCode.data[jumpOverRunAfterLoop].operands[0] = tcCode.size;
				} else {
					TCId loopStatement = tc_unary(TC_OP_CONDITIONAL_LOOP, 0, Token{});
					begin_scope(); // Unclear if this scope is necessary
					TCId condition = parse_expression(I32_MAX);
					tcCode.data[loopStatement].operands[0] = condition;
				}
				tc_nullary(TC_OP_BLOCK_END, Token{});
				parse_scoped_block();
				end_scope();
				tc_nullary(TC_OP_BLOCK_END, Token{});
				if (runAfterLoop != TC_INVALID_ID) {
					tc_unary(TC_OP_TC_JMP, runAfterLoop, Token{});
					tcCode.data[runAfterLoopJmpBack].operands[0] = tcCode.size;
				}
				tc_nullary(TC_OP_BLOCK_END, Token{});

				if (loopToken.type == TOKEN_FOR) {
					end_scope();
				}
			} else if (nextToken.type == TOKEN_BREAK) {
				return tc_nullary(TC_OP_BREAK, consume(TOKEN_BREAK));
			} else if (nextToken.type == TOKEN_CONTINUE) {
				return tc_nullary(TC_OP_CONTINUE, consume(TOKEN_CONTINUE));
			} else if (nextToken.type == TOKEN_DEMOTE) {
				return tc_nullary(TC_OP_DEMOTE, consume(TOKEN_DEMOTE));
			} else if (nextToken.type == TOKEN_TERMINATE) {
				return tc_nullary(TC_OP_TERMINATE, consume(TOKEN_TERMINATE));
			} else if (nextToken.type == TOKEN_RETURN) {
				Token returnToken = consume(TOKEN_RETURN);
				ArenaArrayList<TCId> returnValues{ arena };
				parse_expression_list(returnValues, TOKEN_SEMICOLON);
				return tc_nary(TC_OP_RETURN, returnValues.data, returnValues.size, returnToken);
			} else if (nextToken.has_value()) {
				return parse_unary(tc_literal(next_token()));
			} else {
				TokenType op = nextToken.type;
				I32 precedence = get_precedence(op);
				if (precedence != -1) {
					Token opToken = next_token();
					TCId result = parse_unary(TC_INVALID_ID);
					switch (op) {
					case TOKEN_PLUS: return result;
					case TOKEN_MINUS: return tc_unary(TC_OP_NEG, result, opToken);
					case TOKEN_DOUBLE_PLUS:
					case TOKEN_DOUBLE_MINUS: {
						tc_binary(TC_OP_ASSIGN, result, tc_unary(nextToken.type == TOKEN_DOUBLE_PLUS ? TC_OP_ADDONE : TC_OP_SUBONE, result, opToken), opToken);
						return result;
					}
					case TOKEN_AMPERSAND: return tc_unary(TC_OP_REFERENCE_TO, result, opToken);
					case TOKEN_DOUBLE_AMPERSAND: return tc_unary(TC_OP_REFERENCE_TO, tc_unary(TC_OP_REFERENCE_TO, result, opToken), opToken);
					case TOKEN_CARAT: return tc_unary(TC_OP_DEREFERENCE, result, opToken);
					case TOKEN_TILDE: return tc_unary(TC_OP_NOT, result, opToken);
					case TOKEN_BANG: return tc_unary(TC_OP_LOGICAL_NOT, result, opToken);
					case TOKEN_NONUNIFORM: return tc_unary(TC_OP_MARK_NONUNIFORM, result, opToken);
					default: parse_error("Invalid unary prefix operator"a, true); break;
					}
				}
			}
		}
		return TC_INVALID_ID;
	}

	TCId parse_expression(I32 max) {
		TCId leftHandSide = parse_unary(TC_INVALID_ID);
		while (true) {
			TokenType binaryOp = nextToken.type;
			// TODO fix unary precedence
			if (nextToken.type == TOKEN_IDENTIFIER) {
				Token nameToken = next_token();
				TCId name = tc_literal(nameToken);
				ArenaArrayList<TCId> args{ arena };
				args.push_back(leftHandSide, name);
				if (nextToken.type == TOKEN_LEFT_BRACE) {
					consume(TOKEN_LEFT_BRACE);
					parse_expression_list(args, TOKEN_RIGHT_BRACE);
					consume(TOKEN_RIGHT_BRACE);
				}
				leftHandSide = tc_nary(TC_OP_DECLARE, args.data, args.size, nameToken);
				continue;
			}
			I32 precedence = get_precedence(binaryOp);
			if (precedence == -1 || precedence >= max) {
				break;
			}
			Token binaryOpToken = next_token();
			TCId rightHandSide = TC_INVALID_ID;
			if (binaryOp != TOKEN_DOUBLE_BAR && binaryOp != TOKEN_DOUBLE_AMPERSAND && binaryOp != TOKEN_QUESTION_MARK) {
				rightHandSide = parse_expression(precedence);
			}

			switch (binaryOp) {
			case TOKEN_PLUS: leftHandSide = tc_binary(TC_OP_ADD, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_MINUS: leftHandSide = tc_binary(TC_OP_SUB, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_STAR: leftHandSide = tc_binary(TC_OP_MUL, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_SLASH: leftHandSide = tc_binary(TC_OP_DIV, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_PERCENT: leftHandSide = tc_binary(TC_OP_REM, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_DOUBLE_PERCENT: leftHandSide = tc_binary(TC_OP_MOD, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_DOUBLE_EQUAL: leftHandSide = tc_binary(TC_OP_EQ, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_LESS_THAN: leftHandSide = tc_binary(TC_OP_LT, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_LESS_THAN_OR_EQUAL: leftHandSide = tc_binary(TC_OP_LE, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_GREATER_THAN: leftHandSide = tc_binary(TC_OP_GT, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_GREATER_THAN_OR_EQUAL: leftHandSide = tc_binary(TC_OP_GE, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_BANG_EQUAL: leftHandSide = tc_binary(TC_OP_NE, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_DOUBLE_LEFT_ARROW: leftHandSide = tc_binary(TC_OP_SHL, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_DOUBLE_RIGHT_ARROW: leftHandSide = tc_binary(TC_OP_SAR, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_TRIPLE_RIGHT_ARROW: leftHandSide = tc_binary(TC_OP_SHR, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_AMPERSAND: leftHandSide = tc_binary(TC_OP_AND, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_BAR: leftHandSide = tc_binary(TC_OP_OR, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_CARAT: leftHandSide = tc_binary(TC_OP_XOR, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_EQUAL: leftHandSide = tc_binary(TC_OP_ASSIGN, leftHandSide, rightHandSide, binaryOpToken); break;
			case TOKEN_DOUBLE_BAR: {
				Token boolToken{ TOKEN_BOOL };
				boolToken.vBool = true;
				TCId conditionResult = tc_ternary(TC_OP_CONDITIONAL_BRANCH, leftHandSide, tc_literal(boolToken), 0, binaryOpToken);
				tc_nullary(TC_OP_BLOCK_END, Token{});
				TCId conditionFalseResult = parse_expression(precedence);
				tc_nullary(TC_OP_BLOCK_END, Token{});

				if (conditionFalseResult == TC_INVALID_ID || leftHandSide == TC_INVALID_ID) {
					parse_error("Both sides must yield value in logical or"a);
				}

				tcCode.data[conditionResult].operands[2] = conditionFalseResult;
				leftHandSide = conditionResult;
			} break;
			case TOKEN_DOUBLE_AMPERSAND: {
				Token boolToken{ TOKEN_BOOL };
				boolToken.vBool = false;
				TCId conditionResult = tc_ternary(TC_OP_CONDITIONAL_BRANCH, leftHandSide, 0, tc_literal(boolToken), binaryOpToken);
				TCId conditionTrueResult = parse_expression(precedence);
				tc_nullary(TC_OP_BLOCK_END, Token{});
				tc_nullary(TC_OP_BLOCK_END, Token{});

				if (conditionTrueResult == TC_INVALID_ID || leftHandSide == TC_INVALID_ID) {
					parse_error("Both sides must yield value in logical and"a);
				}

				tcCode.data[conditionResult].operands[1] = conditionTrueResult;
				leftHandSide = conditionResult;
			} break;
			case TOKEN_QUESTION_MARK: {
				leftHandSide = tc_ternary(TC_OP_CONDITIONAL_BRANCH, leftHandSide, 0, 0, binaryOpToken);
				TCId conditionTrueResult = parse_expression(I32_MAX);
				tc_nullary(TC_OP_BLOCK_END, Token{});
				consume(TOKEN_COLON);
				TCId conditionFalseResult = parse_expression(I32_MAX);
				tc_nullary(TC_OP_BLOCK_END, Token{});

				if ((conditionTrueResult == TC_INVALID_ID) != (conditionFalseResult == TC_INVALID_ID)) {
					parse_error("One if block yields value but other does not"a);
				}

				tcCode.data[leftHandSide].operands[1] = conditionTrueResult;
				tcCode.data[leftHandSide].operands[2] = conditionFalseResult;
			} break;
			case TOKEN_COMMA: leftHandSide = rightHandSide; break;
			default: parse_error("Binay op not implemented"a); break;
			}
		}
		return leftHandSide;
	}

	// EXPRESSION PARSER END //

	
	TCId parse_block() {
		TCId lastStatementResult = TC_INVALID_ID;
		while (nextToken.type != TOKEN_NULL && nextToken.type != TOKEN_RIGHT_BRACE && nextToken.type != TOKEN_HASH) {
			// I thought it would be neat if everything was an expression, so here we are, only two statements
			if (nextToken.type == TOKEN_LEFT_BRACKET) {
				allModifierContexts.push_back().init(allModifierContexts.data[currentModifierContext]);
				allModifierContexts.back().parentIndex = currentModifierContext;
				currentModifierContext = allModifierContexts.size - 1;
				tc_unary(TC_OP_MODIFIER_SCOPE_BEGIN, currentModifierContext, nextToken);
				ModifierContext& modCtx = allModifierContexts.data[currentModifierContext];

				// Parse modifiers
				consume(TOKEN_LEFT_BRACKET);
				while (true) {
					if (nextToken.type == TOKEN_RIGHT_BRACKET) {
						break;
					}
					StrA modName = consume(TOKEN_IDENTIFIER).vIdentifier;
					if (modName == "builtin"a) {
						StrA builtinName = consume(TOKEN_IDENTIFIER).vIdentifier;
						BuiltIn builtIn =
							builtinName == "Position"a ? BUILTIN_POSITION :
							builtinName == "VertexIndex"a ? BUILTIN_VERTEX_INDEX :
							builtinName == "ViewIndex"a ? BUILTIN_VIEW_INDEX :
							builtinName == "FragCoord"a ? BUILTIN_FRAG_COORD :
							builtinName == "FragDepth"a ? BUILTIN_FRAG_DEPTH :
							builtinName == "GlobalInvocationId"a ? BUILTIN_GLOBAL_INVOCATION_ID :
							builtinName == "PointSize"a ? BUILTIN_POINT_SIZE :
							(parse_error("Invalid builtin"a), BuiltIn(0));

						modCtx.builtIn = builtIn;
						modCtx.add_decoration(DECORATION_BUILTIN);
					} else if (modName == "block"a) {
						modCtx.add_decoration(DECORATION_BLOCK);
					} else if (modName == "restrict"a) {
						modCtx.add_decoration(DECORATION_RESTRICT);
					} else if (modName == "nonwritable"a) {
						modCtx.add_decoration(DECORATION_NON_WRITABLE);
					} else if (modName == "nonreadable"a) {
						modCtx.add_decoration(DECORATION_NON_READABLE);
					} else if (modName == "flat"a) {
						modCtx.add_decoration(DECORATION_FLAT);
					} else if (modName == "alignment"a) {
						I32 alignment = consume(TOKEN_I32).vI32;
						if (alignment <= 0 || _mm_popcnt_u32(U32(alignment)) != 1) {
							parse_error(strafmt(*arena, "Bad alignment: %"a, alignment));
						}
						modCtx.alignmentDecoration = U32(alignment);
						modCtx.add_decoration(DECORATION_ALIGNMENT);
					} else if (modName == "set"a) {
						modCtx.descriptorSet = U32(consume(TOKEN_I32).vI32);
						modCtx.add_decoration(DECORATION_DESCRIPTOR_SET);
					} else if (modName == "binding"a) {
						modCtx.binding = U32(consume(TOKEN_I32).vI32);
						modCtx.add_decoration(DECORATION_BINDING);
					} else if (modName == "location"a) {
						modCtx.location = U32(consume(TOKEN_I32).vI32);
						if (modCtx.location >= MAX_SHADER_INTERFACE_LOCATIONS) {
							parse_error(strafmt(*arena, "Location must be less than %"a, MAX_SHADER_INTERFACE_LOCATIONS));
						}
						modCtx.add_decoration(DECORATION_LOCATION);
					} else if (modName == "localsize"a) {
						modCtx.localSizeX = consume(TOKEN_I32).vI32;
						modCtx.localSizeY = consume(TOKEN_I32).vI32;
						modCtx.localSizeZ = consume(TOKEN_I32).vI32;
					} else if (modName == "output"a) {
						modCtx.storageClass = STORAGE_CLASS_OUTPUT;
					} else if (modName == "input"a) {
						modCtx.storageClass = STORAGE_CLASS_INPUT;
					} else if (modName == "storage_buffer"a) {
						modCtx.storageClass = STORAGE_CLASS_STORAGE_BUFFER;
					} else if (modName == "uniform_buffer"a) {
						modCtx.storageClass = STORAGE_CLASS_UNIFORM;
					} else if (modName == "push_constant"a) {
						modCtx.storageClass = STORAGE_CLASS_PUSH_CONSTANT;
					} else if (modName == "uniform"a) {
						modCtx.storageClass = STORAGE_CLASS_UNIFORM_CONSTANT;
					} else if (modName == "entrypoint"a) {
						modCtx.isEntrypoint = true;
					} else if (modName == "accessalign"a) {
						I32 alignment = consume(TOKEN_I32).vI32;
						if (alignment <= 0 || _mm_popcnt_u32(U32(alignment)) != 1) {
							parse_error(strafmt(*arena, "Bad alignment: %"a, alignment));
						}
						modCtx.set_memory_operand(MEMORY_OPERAND_ALIGNED, U32(alignment));
					} else {
						parse_error(strafmt(*arena, "Unknown modifier: %"a, modName));
					}
					if (nextToken.type != TOKEN_COMMA) {
						break;
					}
					next_token();
				}
				consume(TOKEN_RIGHT_BRACKET);

				if (nextToken.type == TOKEN_LEFT_BRACE) {
					consume(TOKEN_LEFT_BRACE);
					lastStatementResult = parse_block();
					consume(TOKEN_RIGHT_BRACE);
				} else {
					lastStatementResult = parse_expression(I32_MAX);
					if (nextToken.type != TOKEN_SEMICOLON && nextToken.type != TOKEN_RIGHT_BRACE) {
						parse_error("Expression did not parse"a, true);
					}
				}
				
				currentModifierContext = allModifierContexts.data[currentModifierContext].parentIndex;
				tc_nullary(TC_OP_MODIFIER_SCOPE_END, Token{});
			} else {
				lastStatementResult = parse_expression(I32_MAX);
				if (nextToken.type != TOKEN_SEMICOLON && nextToken.type != TOKEN_RIGHT_BRACE) {
					parse_error("Expression did not parse"a, true);
				}
			}
			if (nextToken.type == TOKEN_SEMICOLON) {
				consume(TOKEN_SEMICOLON);
				lastStatementResult = TC_INVALID_ID;
			}
		}
		return lastStatementResult;
	}

	TCId parse_scoped_block() {
		consume(TOKEN_LEFT_BRACE);
		begin_scope();
		TCId result = parse_block();
		end_scope();
		consume(TOKEN_RIGHT_BRACE);
		return result;
	}

	void generate_type_checking_bytecode() {
		U32 version = 0;
		while (nextToken.type != TOKEN_NULL) {
			switch (nextToken.type) {
			case TOKEN_HASH: {
				next_token();
				Token directive = consume(TOKEN_IDENTIFIER);
				if (directive.vIdentifier != "version"a && version == 0) {
					parse_error("First hash directive must be version"a);
				}
				if (directive.vIdentifier == "version"a) {
					Token versionNum = consume(TOKEN_I32);
					if (versionNum.vI32 != 2) {
						parse_error("Can't compile this version"a);
					}
					if (version != 0) {
						parse_error("Version already defined"a);
					}
					version = U32(versionNum.vI32);
				} else if (directive.vIdentifier == "shader"a) {
					Token shaderType = consume(TOKEN_IDENTIFIER);
					allModifierContexts.push_back_zeroed().init(arena);
					currentModifierContext = allModifierContexts.size - 1;
					if (allScopes.data[currentScope].parentIdx != ScopeContext::INVALID_PARENT_IDX) {
						end_scope();
					}
					if (shaderType.vIdentifier == "vertex"a) {
						currentShaderType = SHADER_TYPE_VERTEX;
						allModifierContexts.data[currentModifierContext].shaderExecutionModel = EXECUTION_MODEL_VERTEX;
					} else if (shaderType.vIdentifier == "fragment"a) {
						currentShaderType = SHADER_TYPE_FRAGMENT;
						allModifierContexts.data[currentModifierContext].shaderExecutionModel = EXECUTION_MODEL_FRAGMENT;
					} else if (shaderType.vIdentifier == "compute"a) {
						currentShaderType = SHADER_TYPE_COMPUTE;
						allModifierContexts.data[currentModifierContext].shaderExecutionModel = EXECUTION_MODEL_GL_COMPUTE;
					} else {
						parse_error("Invalid shader type"a);
					}
					shaderSections.push_back_zeroed().init(arena, currentShaderType, allScopes.size);
					begin_scope();
				} else if (directive.vIdentifier == "interface"a) {
					allModifierContexts.push_back_zeroed().init(arena);
					currentModifierContext = allModifierContexts.size - 1;
					if (allScopes.data[currentScope].parentIdx != ScopeContext::INVALID_PARENT_IDX) {
						end_scope();
					}
					currentShaderType = SHADER_TYPE_INTER_SHADER_INTERFACE;
					shaderSections.push_back_zeroed().init(arena, currentShaderType, allScopes.size);
					begin_scope();
				} else if (directive.vIdentifier == "extension"a) {
					Token extensionType = consume(TOKEN_IDENTIFIER);
					if (extensionType.vIdentifier == "multiview"a) {
						enabledExtensions.multiview = true;
					} else if (extensionType.vIdentifier == "physical_storage_buffer"a) {
						enabledExtensions.physicalStorageBuffer = true;
					} else if (extensionType.vIdentifier == "runtime_descriptor_array"a) {
						enabledExtensions.runtimeDescriptorArray = true;
					} else if (extensionType.vIdentifier == "nonuniform"a) {
						enabledExtensions.shaderNonUniform = true;
					} else if (extensionType.vIdentifier == "sampled_image_array_nonuniform_indexing"a) {
						enabledExtensions.sampledImageArrayNonUniformIndexing = true;
					} else {
						parse_error("Unsupported extension type"a);
					}
				} else {
					parse_error("Unknown directive"a);
				}
			} break;
			default: {
				if (version == 0 || currentShaderType == SHADER_TYPE_NONE) {
					parse_error("Must have version and shader execution model before code"a);
					return;
				} else {
					parse_block();
				}
			} break;
			}
		}
		end_scope();
	}

	
	// DEFAULT TYPES BEGIN //

	// "uses too much stack space", it doesn't actually use too much stack space
#pragma warning(suppress : 6262)
	void register_default_types() {
	// Alright I'll admit I went a bit crazy with the macros here
	// I really should refactor this. This function alone takes ~0.4 seconds to compile. That's a third of the total compile time of Star Chicken!
#define ADD_MULTI_OP_TERNARY_OPERATOR(opName, resultTypePtr, argType1, argType2, argType3, code) {\
	SpvId* ternaryArgTypes = arena->alloc<SpvId>(3);\
	ternaryArgTypes[0] = argType1;\
	ternaryArgTypes[1] = argType2;\
	ternaryArgTypes[2] = argType3;\
	ProcedureType signature{ ProcedureIdentifier{ opName, ternaryArgTypes, 3 }, resultTypePtr };\
	scope.procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {\
		code\
	} }));\
}
#define ADD_MULTI_OP_BINARY_OPERATOR(opName, resultTypePtr, argType1, argType2, code) {\
	SpvId* binaryArgTypes = arena->alloc<SpvId>(2);\
	binaryArgTypes[0] = argType1;\
	binaryArgTypes[1] = argType2;\
	ProcedureType signature{ ProcedureIdentifier{ opName, binaryArgTypes, 2 }, resultTypePtr };\
	scope.procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {\
		code\
	} }));\
}
#define ADD_MULTI_OP_UNARY_OPERATOR(opName, resultTypePtr, argType, code) {\
	SpvId* unaryArgTypes = arena->alloc<SpvId>(1);\
	unaryArgTypes[0] = argType;\
	ProcedureType signature{ ProcedureIdentifier{ opName, unaryArgTypes, 1 }, resultTypePtr };\
	scope.procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {\
		code\
	} }));\
}
#define ADD_SINGLE_OP_BINARY_OPERATOR(opName, resultTypePtr, argType1, argType2, opFunc) {\
	SpvId* binaryArgTypes = arena->alloc<SpvId>(2);\
	binaryArgTypes[0] = argType1;\
	binaryArgTypes[1] = argType2;\
	ProcedureType signature{ ProcedureIdentifier{ opName, binaryArgTypes, 2 }, resultTypePtr };\
	scope.procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {\
		return opFunc(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params[0], params[1]);\
	} }));\
}
#define ADD_SINGLE_OP_UNARY_OPERATOR(opName, resultTypePtr, argType, opFunc) {\
	SpvId* unaryArgTypes = arena->alloc<SpvId>(1);\
	unaryArgTypes[0] = argType;\
	ProcedureType signature{ ProcedureIdentifier{ opName, unaryArgTypes, 1 }, resultTypePtr };\
	scope.procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {\
		return opFunc(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params[0]);\
	} }));\
}
#define ADD_DEFAULT_OPS(opTypeLetter, type, typeId, boolToUse)\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator+"a, type, typeId, typeId, op_##opTypeLetter##_add);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator-"a, type, typeId, typeId, op_##opTypeLetter##_sub);\
	ADD_SINGLE_OP_UNARY_OPERATOR("operator-"a, type, typeId, op_##opTypeLetter##_negate);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator*"a, type, typeId, typeId, op_##opTypeLetter##_mul);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator/"a, type, typeId, typeId, op_##opTypeLetter##_div);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator%"a, type, typeId, typeId, op_##opTypeLetter##_rem);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator%%"a, type, typeId, typeId, op_##opTypeLetter##_mod);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator=="a, boolToUse, typeId, typeId, op_##opTypeLetter##_equal);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator!="a, boolToUse, typeId, typeId, op_##opTypeLetter##_not_equal);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator<"a, boolToUse, typeId, typeId, op_##opTypeLetter##_less_than);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator>"a, boolToUse, typeId, typeId, op_##opTypeLetter##_greater_than);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator<="a, boolToUse, typeId, typeId, op_##opTypeLetter##_less_than_or_equal);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator>="a, boolToUse, typeId, typeId, op_##opTypeLetter##_greater_than_or_equal);\
	ADD_MULTI_OP_BINARY_OPERATOR("min"a, type, typeId, typeId, \
		return (op_##opTypeLetter##_min(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0], params[1])); \
	);\
	ADD_MULTI_OP_BINARY_OPERATOR("max"a, type, typeId, typeId, \
		return (op_##opTypeLetter##_max(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0], params[1])); \
	);\
	ADD_MULTI_OP_TERNARY_OPERATOR("clamp"a, type, typeId, typeId, typeId, \
		return (op_##opTypeLetter##_clamp(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0], params[1], params[2])); \
	);

#define ADD_DEFAULT_BIT_OPS(type, typeId)\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator>>"a, type, typeId, typeId, op_shift_right_logical);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator>>>"a, type, typeId, typeId, op_shift_right_arithmetic);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator<<"a, type, typeId, typeId, op_shift_left_logical);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator|"a, type, typeId, typeId, op_bit_or);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator^"a, type, typeId, typeId, op_bit_xor);\
	ADD_SINGLE_OP_BINARY_OPERATOR("operator&"a, type, typeId, typeId, op_bit_and);\
	ADD_SINGLE_OP_UNARY_OPERATOR("operator~"a, type, typeId, op_bit_not);\
	ADD_SINGLE_OP_UNARY_OPERATOR("bitreverse"a, type, typeId, op_bit_reverse);\
	ADD_SINGLE_OP_UNARY_OPERATOR("bitcount"a, type, typeId, op_bit_count);

#define ADD_DEFAULT_FLOAT_OPS(type, typeId)\
	ADD_MULTI_OP_UNARY_OPERATOR("length"a, f32Type, typeId, \
		return (op_length(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_BINARY_OPERATOR("distance"a, f32Type, typeId, typeId, \
		return (op_distance(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0], params[1])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("normalize"a, type, typeId, \
		return (op_normalize(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_SINGLE_OP_UNARY_OPERATOR("dpdx"a, type, typeId, op_dpdx);\
	ADD_SINGLE_OP_UNARY_OPERATOR("dpdy"a, type, typeId, op_dpdy);\
	ADD_SINGLE_OP_UNARY_OPERATOR("fwidth"a, type, typeId, op_fwidth);\
	ADD_MULTI_OP_UNARY_OPERATOR("sin"a, type, typeId, \
		return (op_f_sin(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("sin"a, type, typeId, \
		return (op_f_sin(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("cos"a, type, typeId, \
		return (op_f_cos(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("tan"a, type, typeId, \
		return (op_f_tan(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("asin"a, type, typeId, \
		return (op_f_asin(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("acos"a, type, typeId, \
		return (op_f_acos(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("atan"a, type, typeId, \
		return (op_f_atan(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_BINARY_OPERATOR("atan2"a, type, typeId, typeId, \
		return (op_f_atan2(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0], params[1])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("pow"a, type, typeId, \
		return (op_f_pow(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("exp"a, type, typeId, \
		return (op_f_exp(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("log"a, type, typeId, \
		return (op_f_log(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("sqrt"a, type, typeId, \
		return (op_f_sqrt(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);\
	ADD_MULTI_OP_UNARY_OPERATOR("invsqrt"a, type, typeId, \
		return (op_f_invsqrt(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
	);
	
	

		ScopeContext& scope = allScopes.data[currentScope];

		defaultTypeVoid = &arena->zalloc<ScalarType>(1)->type;
		defaultTypeVoid->typeClass = TYPE_CLASS_VOID;
		defaultTypeVoid->id = nextSpvId++;
		allTypes.push_back(defaultTypeVoid);
		
		ScalarType* boolScalarType = arena->zalloc<ScalarType>(1);
		boolScalarType->scalarTypeClass = ScalarType::SCALAR_BOOL;
		Type* boolType = &boolScalarType->type;
		boolType->typeClass = TYPE_CLASS_SCALAR;
		boolType->typeName = "Bool"a;
		defaultTypeBool = boolType;
		SpvId boolId = nextSpvId++;
		boolType->id =  boolId;
		allTypes.push_back(boolType);
		scope.typeNameToType.insert(boolType->typeName, boolType);
		ADD_MULTI_OP_UNARY_OPERATOR("Bool.<init>"a, boolType, boolId, return params[0];);

		Type* boolVectorTypes[3]{};
		StrA boolVectorTypeNames[3]{ "V2B"a, "V3B"a, "V4B"a };
		for (U32 i = 0; i < 3; i++) {
			VectorType& vec = *arena->zalloc<VectorType>(1);
			boolVectorTypes[i] = &vec.type;
			vec.componentType = boolScalarType;
			vec.numComponents = i + 2;
			vec.type.id = nextSpvId++;
			vec.type.typeClass = TYPE_CLASS_VECTOR;
			vec.type.typeName = boolVectorTypeNames[i];
			allTypes.push_back(&vec.type);
			scope.typeNameToType.insert(vec.type.typeName, &vec.type);
		}
		boolScalarType->v2Type = (VectorType*)boolVectorTypes[0];
		boolScalarType->v3Type = (VectorType*)boolVectorTypes[1];
		boolScalarType->v4Type = (VectorType*)boolVectorTypes[2];
		ADD_MULTI_OP_UNARY_OPERATOR("V2B.<init>"a, boolVectorTypes[0], boolVectorTypes[0]->id, return params[0];);
		ADD_MULTI_OP_UNARY_OPERATOR("V3B.<init>"a, boolVectorTypes[1], boolVectorTypes[1]->id, return params[0];);
		ADD_MULTI_OP_UNARY_OPERATOR("V4B.<init>"a, boolVectorTypes[2], boolVectorTypes[2]->id, return params[0];);
		ADD_SINGLE_OP_UNARY_OPERATOR("any"a, boolType, boolVectorTypes[0]->id, op_any);
		ADD_SINGLE_OP_UNARY_OPERATOR("any"a, boolType, boolVectorTypes[1]->id, op_any);
		ADD_SINGLE_OP_UNARY_OPERATOR("any"a, boolType, boolVectorTypes[2]->id, op_any);
		ADD_SINGLE_OP_UNARY_OPERATOR("all"a, boolType, boolVectorTypes[0]->id, op_all);
		ADD_SINGLE_OP_UNARY_OPERATOR("all"a, boolType, boolVectorTypes[1]->id, op_all);
		ADD_SINGLE_OP_UNARY_OPERATOR("all"a, boolType, boolVectorTypes[2]->id, op_all);

		ScalarType* i32ScalarType = arena->zalloc<ScalarType>(1);
		i32ScalarType->scalarTypeClass = ScalarType::SCALAR_INT;
		i32ScalarType->isSigned = true;
		i32ScalarType->width = 32;
		Type* i32Type = defaultTypeI32 = &i32ScalarType->type;
		i32Type->typeClass = TYPE_CLASS_SCALAR;
		i32Type->typeName = "I32"a;
		SpvId i32Id = nextSpvId++;
		i32Type->id = i32Id;
		i32Type->sizeBytes = 4;
		i32Type->alignment = 4;
		allTypes.push_back(i32Type);
		scope.typeNameToType.insert(i32Type->typeName, i32Type);
		ADD_DEFAULT_OPS(s, i32Type, i32Id, boolType);
		ADD_DEFAULT_BIT_OPS(i32Type, i32Id);

		ScalarType* u32ScalarType = arena->zalloc<ScalarType>(1);
		u32ScalarType->scalarTypeClass = ScalarType::SCALAR_INT;
		u32ScalarType->isSigned = false;
		u32ScalarType->width = 32;
		Type* u32Type = defaultTypeU32 = &u32ScalarType->type;
		u32Type->typeClass = TYPE_CLASS_SCALAR;
		u32Type->typeName = "U32"a;
		SpvId u32Id = nextSpvId++;
		u32Type->id = u32Id;
		u32Type->sizeBytes = 4;
		u32Type->alignment = 4;
		allTypes.push_back(u32Type);
		scope.typeNameToType.insert(u32Type->typeName, u32Type);
		ADD_DEFAULT_OPS(u, u32Type, u32Id, boolType);
		ADD_DEFAULT_BIT_OPS(u32Type, u32Id);

		ScalarType* f32ScalarType = arena->zalloc<ScalarType>(1);
		f32ScalarType->scalarTypeClass = ScalarType::SCALAR_FLOAT;
		f32ScalarType->width = 32;
		Type* f32Type = defaultTypeF32 = &f32ScalarType->type;
		f32Type->typeClass = TYPE_CLASS_SCALAR;
		f32Type->typeName = "F32"a;
		SpvId f32Id = nextSpvId++;
		f32Type->id = f32Id;
		f32Type->sizeBytes = 4;
		f32Type->alignment = 4;
		allTypes.push_back(f32Type);
		scope.typeNameToType.insert(f32Type->typeName, f32Type);
		ADD_DEFAULT_OPS(f, f32Type, f32Id, boolType);
		ADD_DEFAULT_FLOAT_OPS(f32Type, f32Id);

		ADD_MULTI_OP_UNARY_OPERATOR("I32.<init>"a, i32Type, i32Id, return params[0];);
		ADD_MULTI_OP_UNARY_OPERATOR("U32.<init>"a, u32Type, u32Id, return params[0];);
		ADD_MULTI_OP_UNARY_OPERATOR("F32.<init>"a, f32Type, f32Id, return params[0];);


#define ADD_UNARY_CASTS(i32GenType, u32GenType, f32GenType, i32GenTypeId, u32GenTypeId, f32GenTypeId, i32Name, u32Name, f32Name)\
		ADD_SINGLE_OP_UNARY_OPERATOR(i32Name ".<init>"a, i32GenType, u32GenTypeId, op_bitcast);\
		ADD_SINGLE_OP_UNARY_OPERATOR(i32Name ".<init>"a, i32GenType, f32GenTypeId, op_convert_f_to_s);\
		ADD_SINGLE_OP_UNARY_OPERATOR(u32Name ".<init>"a, u32GenType, i32GenTypeId, op_bitcast);\
		ADD_SINGLE_OP_UNARY_OPERATOR(u32Name ".<init>"a, u32GenType, f32GenTypeId, op_convert_f_to_u);\
		ADD_SINGLE_OP_UNARY_OPERATOR(f32Name ".<init>"a, f32GenType, i32GenTypeId, op_convert_s_to_f);\
		ADD_SINGLE_OP_UNARY_OPERATOR(f32Name ".<init>"a, f32GenType, u32GenTypeId, op_convert_u_to_f);

		ADD_UNARY_CASTS(i32Type, u32Type, f32Type, i32Id, u32Id, f32Id, "I32", "U32", "F32");

#define ADD_BINARY_VEC_SPLAT_OP(opName, vecType, numElements, scalarTypeId, opFunc)\
	ADD_MULTI_OP_BINARY_OPERATOR(opName, vecType, scalarTypeId, (vecType)->id,\
		SpvId constituents[4]{ params[0] COMMA params[0] COMMA params[0] COMMA params[0] };\
		SpvId xSplat = op_composite_construct(compiler.procedureSpvCode, proc.signature.returnType->id, compiler.nextSpvId++, constituents, numElements);\
		return opFunc(compiler.procedureSpvCode, proc.signature.returnType->id, compiler.nextSpvId++, xSplat, params[1]);\
	);\
	ADD_MULTI_OP_BINARY_OPERATOR(opName, vecType, (vecType)->id, scalarTypeId,\
		SpvId constituents[4]{ params[1] COMMA params[1] COMMA params[1] COMMA params[1] };\
		SpvId xSplat = op_composite_construct(compiler.procedureSpvCode, proc.signature.returnType->id, compiler.nextSpvId++, constituents, numElements);\
		return opFunc(compiler.procedureSpvCode, proc.signature.returnType->id, compiler.nextSpvId++, params[0], xSplat);\
	);

#define ADD_VECTOR_TYPE(opTypeLetter, typeNameStr, numElements, scalarType)\
	VectorType* v##numElements##scalarType##Type = arena->zalloc<VectorType>(1);\
	v##numElements##scalarType##Type->type.typeClass = TYPE_CLASS_VECTOR;\
	v##numElements##scalarType##Type->type.typeName = #typeNameStr##a;\
	SpvId v##numElements##scalarType##Id = nextSpvId++;\
	v##numElements##scalarType##Type->type.id = v##numElements##scalarType##Id;\
	v##numElements##scalarType##Type->type.sizeBytes = numElements * scalarType##Type->sizeBytes;\
	v##numElements##scalarType##Type->type.alignment = scalarType##Type->alignment;\
	v##numElements##scalarType##Type->componentType = scalarType##ScalarType;\
	v##numElements##scalarType##Type->numComponents = numElements;\
	allTypes.push_back(&v##numElements##scalarType##Type->type);\
	scope.typeNameToType.insert(v##numElements##scalarType##Type->type.typeName, &v##numElements##scalarType##Type->type);\
	scalarType##ScalarType->v##numElements##Type = v##numElements##scalarType##Type;\
	ADD_DEFAULT_OPS(opTypeLetter, &v##numElements##scalarType##Type->type, v##numElements##scalarType##Id, boolVectorTypes[numElements - 2])\
	ADD_BINARY_VEC_SPLAT_OP("operator+"a, &v##numElements##scalarType##Type->type, numElements, scalarType##Id, op_##opTypeLetter##_add);\
	ADD_BINARY_VEC_SPLAT_OP("operator-"a, &v##numElements##scalarType##Type->type, numElements, scalarType##Id, op_##opTypeLetter##_sub);\
	ADD_BINARY_VEC_SPLAT_OP("operator*"a, &v##numElements##scalarType##Type->type, numElements, scalarType##Id, op_##opTypeLetter##_mul);\
	ADD_BINARY_VEC_SPLAT_OP("operator/"a, &v##numElements##scalarType##Type->type, numElements, scalarType##Id, op_##opTypeLetter##_div);\
	ADD_BINARY_VEC_SPLAT_OP("operator%"a, &v##numElements##scalarType##Type->type, numElements, scalarType##Id, op_##opTypeLetter##_rem);\
	ADD_BINARY_VEC_SPLAT_OP("operator%%"a, &v##numElements##scalarType##Type->type, numElements, scalarType##Id, op_##opTypeLetter##_mod);\

#define ADD_VECTOR_INT_TYPE(opTypeLetter, typeNameStr, numElements, scalarType)\
	ADD_VECTOR_TYPE(opTypeLetter, typeNameStr, numElements, scalarType)\
	ADD_DEFAULT_BIT_OPS(&v##numElements##scalarType##Type->type, v##numElements##scalarType##Id)

#define ADD_VECTOR_FLOAT_TYPE(opTypeLetter, typeNameStr, numElements, scalarType)\
	ADD_VECTOR_TYPE(opTypeLetter, typeNameStr, numElements, scalarType)\
	ADD_DEFAULT_FLOAT_OPS(&v##numElements##scalarType##Type->type, v##numElements##scalarType##Id)\
	ADD_SINGLE_OP_BINARY_OPERATOR("dot"a, scalarType##Type, v##numElements##scalarType##Id, v##numElements##scalarType##Id, op_dot)

#define ADD_V2_CTORS(typeNameStr, vecType, scalarTypeId)\
		ADD_MULTI_OP_UNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, v2##scalarTypeId, return params[0];);\
		ADD_MULTI_OP_BINARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId, scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 2));\
		);\
		ADD_MULTI_OP_UNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId,\
			SpvId constituents[]{ params[0] COMMA params[0] };\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, constituents, 2));\
		);

#define ADD_V3_CTORS(typeNameStr, vecType, scalarTypeId)\
		ADD_MULTI_OP_UNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, v3##scalarTypeId, return params[0];);\
		ADD_MULTI_OP_BINARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, v2##scalarTypeId, scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 2));\
		);\
		ADD_MULTI_OP_BINARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId, v2##scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 2));\
		);\
		ADD_MULTI_OP_TERNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId, scalarTypeId, scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 3));\
		);\
		ADD_MULTI_OP_UNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId,\
			SpvId constituents[]{ params[0] COMMA params[0] COMMA params[0] };\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, constituents, 3));\
		);

#define ADD_V4_CTORS(typeNameStr, vecType, scalarTypeId)\
		ADD_MULTI_OP_UNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, v4##scalarTypeId, return params[0];);\
		ADD_MULTI_OP_BINARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, v3##scalarTypeId, scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 2));\
		);\
		ADD_MULTI_OP_BINARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId, v3##scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 2));\
		);\
		ADD_MULTI_OP_BINARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, v2##scalarTypeId, v2##scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 2));\
		);\
		ADD_MULTI_OP_TERNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, v2##scalarTypeId, scalarTypeId, scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 3));\
		);\
		ADD_MULTI_OP_TERNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId, v2##scalarTypeId, scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 3));\
		);\
		ADD_MULTI_OP_TERNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId, scalarTypeId, v2##scalarTypeId,\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 3));\
		);\
		{\
			SpvId* argTypes = arena->alloc<SpvId>(4);\
			argTypes[0] = argTypes[1] = argTypes[2] = argTypes[3] = scalarTypeId;\
			ProcedureType signature{ ProcedureIdentifier{ #typeNameStr ".<init>"a, argTypes, 4 }, &vecType->type };\
			scope.procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {\
				return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, 4));\
			} }));\
		}\
		ADD_MULTI_OP_UNARY_OPERATOR(#typeNameStr ".<init>"a, &vecType->type, scalarTypeId,\
			SpvId constituents[]{ params[0] COMMA params[0] COMMA params[0] COMMA params[0] };\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, constituents, 4));\
		);


		ADD_VECTOR_INT_TYPE(s, V2I32, 2, i32);
		ADD_VECTOR_INT_TYPE(s, V3I32, 3, i32);
		ADD_VECTOR_INT_TYPE(s, V4I32, 4, i32);
		ADD_VECTOR_INT_TYPE(u, V2U32, 2, u32);
		ADD_VECTOR_INT_TYPE(u, V3U32, 3, u32);
		ADD_VECTOR_INT_TYPE(u, V4U32, 4, u32);
		ADD_VECTOR_FLOAT_TYPE(f, V2F32, 2, f32);
		ADD_VECTOR_FLOAT_TYPE(f, V3F32, 3, f32);
		ADD_VECTOR_FLOAT_TYPE(f, V4F32, 4, f32);

		ADD_V2_CTORS(V2I32, v2i32Type, i32Id);
		ADD_V3_CTORS(V3I32, v3i32Type, i32Id);
		ADD_V4_CTORS(V4I32, v4i32Type, i32Id);
		ADD_V2_CTORS(V2U32, v2u32Type, u32Id);
		ADD_V3_CTORS(V3U32, v3u32Type, u32Id);
		ADD_V4_CTORS(V4U32, v4u32Type, u32Id);
		ADD_V2_CTORS(V2F32, v2f32Type, f32Id);
		ADD_V3_CTORS(V3F32, v3f32Type, f32Id);
		ADD_V4_CTORS(V4F32, v4f32Type, f32Id);

		ADD_UNARY_CASTS(&v2i32Type->type, &v2u32Type->type, &v2f32Type->type, v2i32Id, v2u32Id, v2f32Id, "V2I32", "V2U32", "V2F32");
		ADD_UNARY_CASTS(&v3i32Type->type, &v3u32Type->type, &v3f32Type->type, v3i32Id, v3u32Id, v3f32Id, "V3I32", "V3U32", "V3F32");
		ADD_UNARY_CASTS(&v4i32Type->type, &v4u32Type->type, &v4f32Type->type, v4i32Id, v4u32Id, v4f32Id, "V4I32", "V4U32", "V4F32");

		scope.typeNameToType.insert("V2F"a, &v2f32Type->type);
		scope.typeNameToType.insert("V3F"a, &v3f32Type->type);
		scope.typeNameToType.insert("V4F"a, &v4f32Type->type);
		scope.typeNameToType.insert("V2I"a, &v2i32Type->type);
		scope.typeNameToType.insert("V3I"a, &v3i32Type->type);
		scope.typeNameToType.insert("V4I"a, &v4i32Type->type);
		scope.typeNameToType.insert("V2U"a, &v2u32Type->type);
		scope.typeNameToType.insert("V3U"a, &v3u32Type->type);
		scope.typeNameToType.insert("V4U"a, &v4u32Type->type);

		glsl450InstructionSet = nextSpvId++;
		ADD_MULTI_OP_UNARY_OPERATOR("unpack_unorm4x8"a, &v4f32Type->type, u32Id, \
			return (op_unpack_unorm4x8(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0])); \
		);
		ADD_MULTI_OP_BINARY_OPERATOR("cross"a, &v3f32Type->type, v3f32Id, v3f32Id, \
			return (op_cross(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, compiler.glsl450InstructionSet, params[0], params[1])); \
		);

		addCarryStructTypeId = nextSpvId++;
		ADD_MULTI_OP_BINARY_OPERATOR("v2u_u64_add"a, &v2u32Type->type, v2u32Id, u32Id, \
			U32 idx0 = 0;\
			U32 idx1 = 1;\
			SpvId low = (op_composite_extract(codeOutput, compiler.defaultTypeU32->id, compiler.nextSpvId++, params[0], &idx0, 1));\
			SpvId high = (op_composite_extract(codeOutput, compiler.defaultTypeU32->id, compiler.nextSpvId++, params[0], &idx1, 1));\
			SpvId carried = (op_i_add_carry(codeOutput, compiler.addCarryStructTypeId, compiler.nextSpvId++, low, params[1]));\
			SpvId newLow = (op_composite_extract(codeOutput, compiler.defaultTypeU32->id, compiler.nextSpvId++, carried, &idx0, 1));\
			SpvId carry = (op_composite_extract(codeOutput, compiler.defaultTypeU32->id, compiler.nextSpvId++, carried, &idx1, 1));\
			SpvId readdCarry = (op_i_add(codeOutput, compiler.defaultTypeU32->id, compiler.nextSpvId++, high, carry));\
			SpvId v2Members[2]{ newLow COMMA readdCarry };\
			return (op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, v2Members, 2)); \
		);

		Type* samplerType = defaultTypeSampler = arena->zalloc<Type>(1);
		samplerType->typeClass = TYPE_CLASS_SAMPLER;
		samplerType->typeName = "Sampler"a;
		samplerType->id = nextSpvId++;
		allTypes.push_back(samplerType);
		scope.typeNameToType.insert(samplerType->typeName, samplerType);

#undef ADD_V4_CTORS
#undef ADD_V3_CTORS
#undef ADD_V2_CTORS
#undef ADD_VECTOR_FLOAT_TYPE
#undef ADD_VECTOR_INT_TYPE
#undef ADD_VECTOR_TYPE
#undef ADD_BINARY_VEC_SPLAT_OP
#undef ADD_UNARY_CASTS
#undef ADD_DEFAULT_FLOAT_OPS
#undef ADD_DEFAULT_BIT_OPS
#undef ADD_DEFAULT_OPS
#undef ADD_MULTI_OP_TERNARY_OPERATOR
#undef ADD_MULTI_OP_BINARY_OPERATOR
#undef ADD_MULTI_OP_UNARY_OPERATOR
#undef ADD_SINGLE_OP_BINARY_OPERATOR
#undef ADD_SINGLE_OP_UNARY_OPERATOR
	}

	// DEFAULT TYPES END //

	void resolve_struct(StructType& structType) {
		if (structType.structEvaluationComplete) {
			return;
		}
		TCOp& structOp = tcCode.data[structType.structTCCodeStart];
		if (structType.currentlyResolvingStruct) {
			generation_error("Circular struct references"a, structOp);
			return;
		}
		structType.currentlyResolvingStruct = true;
		U32 numTCOps = tcCode.data[structType.structTCCodeStart].operands[0];
		ArenaArrayList<VarDeclaration> varDeclarations{ arena };
		get_var_declaration_list(varDeclarations, structType.scopeIdx, tcCode.data + structType.structTCCodeStart + 1, tcCode.data + structType.structTCCodeStart + 1 + numTCOps);
		U32 structSize = 0;
		U32 structMaxAlignment = 0;
		for (VarDeclaration& decl : varDeclarations) {
			if (structType.endsInVariableLengthArray) {
				generation_error("Can't have another member after variable length array"a, structOp);
			}
			U32 alignment = defaultTypeF32->alignment; // Everything gets scalar packing
			structSize = ALIGN_HIGH(structSize, alignment);
			structType.members.push_back(StructType::Member{ decl.name, structType.members.size, decl.modifierCtxIdx, structSize, decl.type });
			if (decl.type->typeClass == TYPE_CLASS_POINTER) {
				if (((PointerType*)decl.type)->storageClass != STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
					generation_error("Can't have logical pointer type inside struct"a, structOp);
				}
			}
			structSize += decl.type->sizeBytes;
			
			structMaxAlignment = max(structMaxAlignment, alignment);
			if (decl.type->typeClass == TYPE_CLASS_ARRAY && reinterpret_cast<ArrayType*>(decl.type)->numElements == -1 ||
				decl.type->typeClass == TYPE_CLASS_STRUCT && reinterpret_cast<StructType*>(decl.type)->endsInVariableLengthArray) {
				structType.endsInVariableLengthArray = true;
			} else {
				ASSERT(decl.type->sizeBytes != 0, "Size must not be 0"a);
			}
			if (!structType.endsInVariableLengthArray && decl.type->sizeBytes == 0) {
				generation_error("Struct member must have defined bit pattern"a, structOp);
			}
		}
		if (structType.endsInVariableLengthArray) {
			structSize = U32_MAX;
		} else {
			structSize = ALIGN_HIGH(structSize, structMaxAlignment);
			SpvId* argTypes = arena->alloc<SpvId>(structType.members.size);
			ProcedureType signature{ ProcedureIdentifier{ stracat(*arena, structType.type.typeName, ".<init>"a), argTypes, structType.members.size }, &structType.type };
			for (StructType::Member& member : structType.members) {
				*argTypes++ = member.type->id;
			}
			allScopes.data[structType.scopeIdx].procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {
				return op_composite_construct(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params, proc.signature.identifier.numArgs);
			} }));

			signature.identifier.numArgs = 1;
			signature.identifier.argTypes = arena->alloc<SpvId>(1);
			signature.identifier.argTypes[0] = structType.type.id;
			allScopes.data[structType.scopeIdx].procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {
				return params[0];
			} }));
		}
		structType.type.sizeBytes = structSize;
		structType.type.alignment = max(structType.type.alignment, structMaxAlignment);
		structOp.resultValueClass = VALUE_CLASS_TYPE;
		structOp.type = &structType.type;
		structType.structEvaluationComplete = true;
	}
	void emit_type_definition(ArenaArrayList<SpvDword>& output, Type* type) {
		if (type->typeEmitted) {
			return;
		}
		switch (type->typeClass) {
		case TYPE_CLASS_VOID: {
			op_type_void(output, type->id);
		} break;
		case TYPE_CLASS_SCALAR: {
			ScalarType& scalarType = *reinterpret_cast<ScalarType*>(type);
			if (scalarType.scalarTypeClass == ScalarType::SCALAR_BOOL) {
				op_type_bool(output, type->id);
			} else if (scalarType.scalarTypeClass == ScalarType::SCALAR_INT) {
				op_type_int(output, type->id, scalarType.width, scalarType.isSigned ? 1u : 0u);
			} else if (scalarType.scalarTypeClass == ScalarType::SCALAR_FLOAT) {
				op_type_float(output, type->id, scalarType.width);
			}
		} break;
		case TYPE_CLASS_VECTOR: {
			VectorType& vectorType = *reinterpret_cast<VectorType*>(type);
			emit_type_definition(output, &vectorType.componentType->type);
			op_type_vector(output, type->id, vectorType.componentType->type.id, vectorType.numComponents);
		} break;
		case TYPE_CLASS_ARRAY: {
			ArrayType& arrayType = *reinterpret_cast<ArrayType*>(type);
			emit_type_definition(output, arrayType.elementType);
			if (arrayType.numElements < 0) {
				op_type_runtime_array(output, type->id, arrayType.elementType->id);
			} else {
				op_type_array(output, type->id, arrayType.elementType->id, arrayType.numElementsSpvConstant);
			}
		} break;
		case TYPE_CLASS_STRUCT: {
			StructType& structType = *reinterpret_cast<StructType*>(type);
			SpvId* memberTypeIds = arena->alloc<SpvId>(structType.members.size);
			SpvId* nextMemberTypeId = memberTypeIds;
			for (StructType::Member& member : structType.members) {
				if (member.type->typeClass != TYPE_CLASS_POINTER) {
					emit_type_definition(output, member.type);
					*nextMemberTypeId++ = member.type->id;
				} else {
					ASSERT(((PointerType*)member.type)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER, "Structs must not contain non physical pointers"a);
					Type* v2uType = &((ScalarType*)defaultTypeU32)->v2Type->type;
					emit_type_definition(output, v2uType);
					*nextMemberTypeId++ = v2uType->id;
				}
			}
			op_type_struct(output, type->id, memberTypeIds, structType.members.size);
		} break;
		case TYPE_CLASS_POINTER: {
			PointerType& ptrType = *reinterpret_cast<PointerType*>(type);
			emit_type_definition(output, ptrType.pointedAt);
			op_type_pointer(output, type->id, ptrType.storageClass, ptrType.pointedAt->id);
		} break;
		case TYPE_CLASS_IMAGE: {
			ImageType& imgType = *reinterpret_cast<ImageType*>(type);
			op_type_image(output, imgType.type.id, imgType.sampledType->id, imgType.dimension, imgType.depth, imgType.arrayed, imgType.multisample, imgType.sampled, imgType.format);
			if (imgType.sampledImageType) {
				op_type_sampled_image(output, imgType.sampledImageType->id, imgType.type.id);
			}
		} break;
		case TYPE_CLASS_SAMPLER: {
			op_type_sampler(output, type->id);
		} break;
		case TYPE_CLASS_SAMPLED_IMAGE: {
			// Handled in TYPE_CLASS_IMAGE case
		} break;
		}
		type->typeEmitted = true;
	}
	void do_binary_procedure(TCOp* op, StrA operatorName) {
		TCOp& operandA = tcCode.data[op->operands[0]];
		TCOp& operandB = tcCode.data[op->operands[1]];
		Type* typeA, * typeB;
		SpvId resultA = load_val(&typeA, operandA);
		SpvId resultB = load_val(&typeB, operandB);
		if (resultA == SPV_NULL_ID || resultB == SPV_NULL_ID) {
			generation_error(strafmt(*arena, "Operator \"%\" not defined for non runtime values"a, operatorName), *op);
		}
		if (compilerErrored) {
			return;
		}
		SpvId operandTypes[2]{ typeA->id, typeB->id };
		if (Procedure* proc = allScopes.data[currentScope].find_procedure(allScopes.data, ProcedureIdentifier{ operatorName, operandTypes, ARRAY_COUNT(operandTypes) })) {
			op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
			op->variable = arena->zalloc<Variable>(1);
			op->variable->type = proc->signature.returnType;
			SpvId operands[2]{ resultA, resultB };
			op->variable->id = proc->call(procedureSpvCode, *proc, *this, operands);
		} else {
			generation_error("No suitable operator overload found for types"a, *op);
		}
	}
	void do_unary_procedure(TCOp* op, StrA operatorName) {
		TCOp& operand = tcCode.data[op->operands[0]];
		Type* type;
		SpvId result = load_val(&type, operand);
		if (result == SPV_NULL_ID) {
			generation_error(strafmt(*arena, "Operator \"%\" not defined for non runtime values"a, operatorName), *op);
		}
		if (compilerErrored) {
			return;
		}
		if (Procedure* proc = allScopes.data[currentScope].find_procedure(allScopes.data, ProcedureIdentifier{ operatorName, &type->id, 1 })) {
			op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
			op->variable = arena->zalloc<Variable>(1);
			op->variable->type = proc->signature.returnType;
			op->variable->id = proc->call(procedureSpvCode, *proc, *this, &result);
		} else {
			generation_error("No suitable operator overload found for types"a, *op);
		}
	}
	PointerType* get_pointer_to(Type* type, StorageClass storageClass) {
		PointerType** existingPtrType = typeAndStorageClassToPointerType.find(U64(storageClass) << 32 | type->id);
		if (existingPtrType) {
			return *existingPtrType;
		} else {
			if (storageClass == STORAGE_CLASS_Invalid) {
				generic_error(strafmt(*arena, "Invalid storage class when getting pointer to %, specify one"a, type->typeName));
			}
			PointerType* ptrType = arena->zalloc<PointerType>(1);
			ptrType->pointedAt = type;
			ptrType->type.id = nextSpvId++;
			ptrType->type.typeClass = TYPE_CLASS_POINTER;
			ptrType->storageClass = storageClass;
			if (storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
				enabledExtensions.physicalStorageBuffer = true;
				ptrType->type.sizeBytes = sizeof(U64);
			}
			allTypes.push_back(&ptrType->type);
			typeAndStorageClassToPointerType.insert(U64(storageClass) << 32 | type->id, ptrType);
			return ptrType;
		}
	}
	ArrayType* get_array_of(Type* type, I32 count) {
		ArrayType** arrTypePtr = arrayTypeInfoToArrayType.find(ArrayTypeInfo{ type->id, count });
		if (arrTypePtr) {
			return *arrTypePtr;
		} else {
			SpvId countSpvConstant = count > 0 ? spv_literal_index(U32(count)) : SPV_NULL_ID;
			ArrayType* arrType = arena->zalloc<ArrayType>(1);
			arrType->type.id = nextSpvId++;
			arrType->type.sizeBytes = count > 0 ? type->sizeBytes * count : 0;
			arrType->type.alignment = type->alignment;
			arrType->type.typeClass = TYPE_CLASS_ARRAY;
			arrType->elementType = type;
			arrType->numElements = count;
			arrType->numElementsSpvConstant = countSpvConstant;
			arrayTypeInfoToArrayType.insert(ArrayTypeInfo{ type->id, count }, arrType);
			allTypes.push_back(&arrType->type);
			if (type->typeClass == TYPE_CLASS_IMAGE) {
				enabledExtensions.runtimeDescriptorArray = true;
			}
			return arrType;
		}
	}
	SpvId load_val_ptr(Type** resultType, SpvId val, PointerType* type){
		ModifierContext& modCtx = allModifierContexts.data[currentModifierContext];
		bool modCtxHasAligned = modCtx.memoryOperands & MEMORY_OPERAND_ALIGNED;
		if (type->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER && !modCtxHasAligned) {
			// Default alignment to 4, should work with all scalar types
			modCtx.set_memory_operand(MEMORY_OPERAND_ALIGNED, 4);
		}
		ArenaArrayList<SpvDword> memoryOperandArgs{};
		SpvDword memoryOperandArgsData[16];
		memoryOperandArgs.wrap(memoryOperandArgsData, 0, 16, *arena);
		modCtx.get_memory_operand_args(memoryOperandArgs);
		if (!modCtxHasAligned) { modCtx.memoryOperands &= ~MEMORY_OPERAND_ALIGNED; }
		*resultType = type->pointedAt;
		SpvId loadedTypeId = type->pointedAt->id;
		if (type->pointedAt->typeClass == TYPE_CLASS_POINTER && ((PointerType*)type->pointedAt)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
			// Load physical pointers as V2U
			loadedTypeId = ((ScalarType*)defaultTypeU32)->v2Type->type.id;
		}
		return op_load(procedureSpvCode, loadedTypeId, nextSpvId++, val, memoryOperandArgs.data, memoryOperandArgs.size);
	}
	SpvId load_val(Type** resultType, TCOp& srcOp) {
		if (srcOp.resultValueClass == VALUE_CLASS_RUNTIME_VALUE) {
			/*
			if (srcOp.variable->type->typeClass == TYPE_CLASS_POINTER) {
				SpvId loadFrom = srcOp.variable->id;
				if (((PointerType*)srcOp.variable->type)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
					// Physical pointers are stored as V2U
					loadFrom = op_bitcast(procedureSpvCode, srcOp.variable->type->id, nextSpvId++, loadFrom);
				}
				SpvId loaded = load_val_ptr(resultType, loadFrom, reinterpret_cast<PointerType*>(srcOp.variable->type));
				if (srcOp.resultIsNonUniform) {
					op_decorate(decorationSpvCode, loaded, DECORATION_NON_UNIFORM, nullptr, 0);
				}
				return loaded;
			} else {
				*resultType = srcOp.variable->type;
				return srcOp.variable->id;
			}
			*/
			*resultType = srcOp.variable->type;
			return srcOp.variable->id;
		} else if (srcOp.resultValueClass == VALUE_CLASS_ACCESS_CHAIN) {
			VariableAccessChain* accessChain = srcOp.accessChain;
			Type* accessChainPhysicalResultType = accessChain->resultType;
			if (accessChainPhysicalResultType->typeClass == TYPE_CLASS_POINTER && ((PointerType*)accessChainPhysicalResultType)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
				accessChainPhysicalResultType = &((ScalarType*)defaultTypeU32)->v2Type->type;
			}
			if (accessChain->base->type->typeClass == TYPE_CLASS_POINTER) {
				PointerType* ptrType = get_pointer_to(accessChainPhysicalResultType, reinterpret_cast<PointerType*>(accessChain->base->type)->storageClass);
				SpvId accessChainResult = accessChain->base->id;
				if (((PointerType*)accessChain->base->type)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
					// Physical pointers are stored as V2U
					accessChainResult = op_bitcast(procedureSpvCode, accessChain->base->type->id, nextSpvId++, accessChainResult);
				}
				if (accessChain->isPtrAccessChain) {
					accessChainResult = op_ptr_access_chain(procedureSpvCode, ptrType->type.id, nextSpvId++, accessChainResult, accessChain->accessIndices.data[0], accessChain->accessIndices.data + 1, accessChain->accessIndices.size - 1);
				} else {
					if (!accessChain->accessIndices.empty()) {
						accessChainResult = op_access_chain(procedureSpvCode, ptrType->type.id, nextSpvId++, accessChainResult, accessChain->accessIndices.data, accessChain->accessIndices.size);
					}
				}
				SpvId loaded = load_val_ptr(resultType, accessChainResult, get_pointer_to(accessChain->resultType, reinterpret_cast<PointerType*>(accessChain->base->type)->storageClass));
				if (srcOp.resultIsNonUniform) {
					op_decorate(decorationSpvCode, accessChainResult, DECORATION_NON_UNIFORM, nullptr, 0);
					op_decorate(decorationSpvCode, loaded, DECORATION_NON_UNIFORM, nullptr, 0);
				}
				return loaded;
			} else {
				*resultType = accessChain->resultType;
				if (accessChain->accessIndices.empty()) {
					return accessChain->base->id;
				} else {
					SpvId loaded = op_composite_extract(procedureSpvCode, accessChainPhysicalResultType->id, nextSpvId++, accessChain->base->id, accessChain->accessIndices.data, accessChain->accessIndices.size);
					if (srcOp.resultIsNonUniform) {
						op_decorate(decorationSpvCode, loaded, DECORATION_NON_UNIFORM, nullptr, 0);
					}
					return loaded;
				}
			}
		} else if (srcOp.opcode == TC_OP_IDENTIFIER) {
			U32 varScope = U32_MAX;
			Variable* var = allScopes.data[currentScope].find_var(&varScope, allScopes.data, srcOp.vIdentifier);
			if (var) {
				*resultType = var->type;
				return var->id;
			} else {
				generation_error(strafmt(*arena, "Could not find variable by name %"a, srcOp.vIdentifier), srcOp);
			}
		} else {
			generation_error("Can't load from non Value"a, srcOp);
		}
		*resultType = nullptr;
		return SPV_NULL_ID;
	}
	Type* load_type(TCOp& srcOp) {
		if (srcOp.resultValueClass == VALUE_CLASS_TYPE) {
			return srcOp.type;
		} else if (srcOp.opcode == TC_OP_IDENTIFIER) {
			StrA name = srcOp.vIdentifier;
			Type* foundType = allScopes.data[currentScope].find_type(allScopes.data, name);
			if (!foundType && name.starts_with("Image"a)) {
				// Image types have a lot of optional switches, I thought it would be easier to do them in code rather than register a bunch of extra types
				// Format (where parens are required and brackets optional) is Image(SampledType)(Dimension)[Depth][Array][Multisampled][Sampled][Format]
				// SampledType is default float
				// Example: ImageF322DNonDepthArrayMultisampledSampledR11FG11FB10F
				// Example: Image2DSampled
				ImageType imageType{};
				StrA cfg = name.skip(I64("Image"a.length));	

				if (cfg.consume("U32"a)) {
					imageType.sampledType = defaultTypeU32;
				} else if (cfg.consume("I32"a)) {
					imageType.sampledType = defaultTypeI32;
				} else if (cfg.consume("F32"a)) {
					imageType.sampledType = defaultTypeF32;
				} else {
					imageType.sampledType = defaultTypeF32;
				}

				if (cfg.consume("1D"a)) { imageType.dimension = DIMENSIONALITY_1D; }
				else if (cfg.consume("2D"a)) { imageType.dimension = DIMENSIONALITY_2D; }
				else if (cfg.consume("3D"a)) { imageType.dimension = DIMENSIONALITY_3D; }
				else if (cfg.consume("Cube"a)) { imageType.dimension = DIMENSIONALITY_CUBE; }
				//else if (cfg.consume("Rect"a)) { imageType.dimension = DIMENSIONALITY_RECT; } // rect appears to be some legacy OpenGL cruft we don't need
				else if (cfg.consume("Buffer"a)) { imageType.dimension = DIMENSIONALITY_BUFFER; }
				else if (cfg.consume("SubpassData"a)) { imageType.dimension = DIMENSIONALITY_SUBPASS_DATA; }
				else { imageType.dimension = DIMENSIONALITY_Invalid; }

				if (cfg.consume("Depth"a)) { imageType.depth = 1; }
				else if (cfg.consume("NonDepth"a)) { imageType.depth = 0; }
				else { imageType.depth = 2; } // Unknown

				if (cfg.consume("Array"a)) { imageType.arrayed = true; }

				if (cfg.consume("Multisampled"a)) { imageType.multisample = true; }

				if (cfg.consume("Sampled"a)) { imageType.sampled = 1; }
				else if (cfg.consume("Storage"a)) { imageType.sampled = 2; }

				if (cfg.length && cfg[0] == 'R') {
					if (cfg.consume("RGBA32F"a)) { imageType.format = IMAGE_FORMAT_RGBA32F; }
					else if (cfg.consume("RGBA16F"a)) { imageType.format = IMAGE_FORMAT_RGBA16F; }
					else if (cfg.consume("R32F"a)) { imageType.format = IMAGE_FORMAT_R32F; }
					else if (cfg.consume("RGBA8"a)) { imageType.format = IMAGE_FORMAT_RGBA8; }
					else if (cfg.consume("RGBA8SNORM"a)) { imageType.format = IMAGE_FORMAT_RGBA8SNORM; }
					else if (cfg.consume("RG32F"a)) { imageType.format = IMAGE_FORMAT_RG32F; }
					else if (cfg.consume("RG16F"a)) { imageType.format = IMAGE_FORMAT_RG16F; }
					else if (cfg.consume("R11FG11FB10F"a)) { imageType.format = IMAGE_FORMAT_R11FG11FB10F; }
					else if (cfg.consume("R16F"a)) { imageType.format = IMAGE_FORMAT_R16F; }
					else if (cfg.consume("RGBA16"a)) { imageType.format = IMAGE_FORMAT_RGBA16; }
					else if (cfg.consume("RGB10A2"a)) { imageType.format = IMAGE_FORMAT_RGB10A2; }
					else if (cfg.consume("RG16"a)) { imageType.format = IMAGE_FORMAT_RG16; }
					else if (cfg.consume("RG8"a)) { imageType.format = IMAGE_FORMAT_RG8; }
					else if (cfg.consume("R16"a)) { imageType.format = IMAGE_FORMAT_R16; }
					else if (cfg.consume("R8"a)) { imageType.format = IMAGE_FORMAT_R8; }
					else if (cfg.consume("RGBA16SNORM"a)) { imageType.format = IMAGE_FORMAT_RGBA16SNORM; }
					else if (cfg.consume("RG16SNORM"a)) { imageType.format = IMAGE_FORMAT_RG16SNORM; }
					else if (cfg.consume("RG8SNORM"a)) { imageType.format = IMAGE_FORMAT_RG8SNORM; }
					else if (cfg.consume("R16SNORM"a)) { imageType.format = IMAGE_FORMAT_R16SNORM; }
					else if (cfg.consume("R8SNORM"a)) { imageType.format = IMAGE_FORMAT_R8SNORM; }
					else if (cfg.consume("RGBA32I"a)) { imageType.format = IMAGE_FORMAT_RGBA32I; }
					else if (cfg.consume("RGBA16I"a)) { imageType.format = IMAGE_FORMAT_RGBA16I; }
					else if (cfg.consume("RGBA8I"a)) { imageType.format = IMAGE_FORMAT_RGBA8I; }
					else if (cfg.consume("R32I"a)) { imageType.format = IMAGE_FORMAT_R32I; }
					else if (cfg.consume("RG32I"a)) { imageType.format = IMAGE_FORMAT_RG32I; }
					else if (cfg.consume("RG16I"a)) { imageType.format = IMAGE_FORMAT_RG16I; }
					else if (cfg.consume("RG8I"a)) { imageType.format = IMAGE_FORMAT_RG8I; }
					else if (cfg.consume("R16I"a)) { imageType.format = IMAGE_FORMAT_R16I; }
					else if (cfg.consume("R8I"a)) { imageType.format = IMAGE_FORMAT_R8I; }
					else if (cfg.consume("RGBA32UI"a)) { imageType.format = IMAGE_FORMAT_RGBA32UI; }
					else if (cfg.consume("RGBA16UI"a)) { imageType.format = IMAGE_FORMAT_RGBA16UI; }
					else if (cfg.consume("RGBA8UI"a)) { imageType.format = IMAGE_FORMAT_RGBA8UI; }
					else if (cfg.consume("R32UI"a)) { imageType.format = IMAGE_FORMAT_R32UI; }
					else if (cfg.consume("RGB10A2UI"a)) { imageType.format = IMAGE_FORMAT_RGB10A2UI; }
					else if (cfg.consume("RG32UI"a)) { imageType.format = IMAGE_FORMAT_RG32UI; }
					else if (cfg.consume("RG16UI"a)) { imageType.format = IMAGE_FORMAT_RG16UI; }
					else if (cfg.consume("RG8UI"a)) { imageType.format = IMAGE_FORMAT_RG8UI; }
					else if (cfg.consume("R16UI"a)) { imageType.format = IMAGE_FORMAT_R16UI; }
					else if (cfg.consume("R8UI"a)) { imageType.format = IMAGE_FORMAT_R8UI; }
					else if (cfg.consume("R64UI"a)) { imageType.format = IMAGE_FORMAT_R64UI; }
					else if (cfg.consume("R64I"a)) { imageType.format = IMAGE_FORMAT_R64I; }
				}

				if (cfg.length == 0 && imageType.dimension != DIMENSIONALITY_Invalid) {
					imageType.type.typeClass = TYPE_CLASS_IMAGE;
					imageType.type.typeName = name;
					imageType.type.id = nextSpvId++;
					if (imageType.dimension != DIMENSIONALITY_BUFFER && imageType.dimension != DIMENSIONALITY_SUBPASS_DATA) {
						if (imageType.sampled == 2) { // Storage image
							SpvId coordTypeId = imageType.dimension == DIMENSIONALITY_1D ? defaultTypeU32->id :
								imageType.dimension == DIMENSIONALITY_2D ? reinterpret_cast<ScalarType*>(defaultTypeU32)->v2Type->type.id :
								imageType.dimension == DIMENSIONALITY_3D || imageType.dimension == DIMENSIONALITY_CUBE ? reinterpret_cast<ScalarType*>(defaultTypeU32)->v3Type->type.id :
								reinterpret_cast<ScalarType*>(defaultTypeU32)->v4Type->type.id;

							SpvId* argTypes = arena->alloc<SpvId>(2);
							argTypes[0] = imageType.type.id;
							argTypes[1] = coordTypeId;
							ProcedureType signature{ ProcedureIdentifier{ "read_image"a, argTypes, 2 }, &reinterpret_cast<ScalarType*>(imageType.sampledType)->v4Type->type };
							allScopes.data[0].procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {
								return op_image_read(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params[0], params[1]);
							} }));

							argTypes = arena->alloc<SpvId>(3);
							argTypes[0] = imageType.type.id;
							argTypes[1] = coordTypeId;
							argTypes[2] = reinterpret_cast<ScalarType*>(imageType.sampledType)->v4Type->type.id;
							signature = ProcedureType{ ProcedureIdentifier{ "write_image"a, argTypes, 3 }, defaultTypeVoid };
							allScopes.data[0].procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {
								op_image_write(codeOutput, params[0], params[1], params[2]);
								return SPV_NULL_ID;
							} }));

							argTypes = arena->alloc<SpvId>(2);
							argTypes[0] = imageType.type.id;
							argTypes[1] = coordTypeId;
							signature = ProcedureType{ ProcedureIdentifier{ "operator[]"a, argTypes, 2 }, &reinterpret_cast<ScalarType*>(imageType.sampledType)->v4Type->type };
							allScopes.data[0].procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {
								return op_image_read(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params[0], params[1]);
							} }));
						} else { // Sampled image
							imageType.sampledImageType = arena->zalloc<Type>(1);
							imageType.sampledImageType->typeClass = TYPE_CLASS_SAMPLED_IMAGE;
							imageType.sampledImageType->id = nextSpvId++;

							SpvId* argTypes = arena->alloc<SpvId>(2);
							argTypes[0] = imageType.type.id;
							argTypes[1] = defaultTypeSampler->id;
							ProcedureType signature{ ProcedureIdentifier{ "SampledImage"a, argTypes, 2 }, imageType.sampledImageType };
							allScopes.data[0].procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {
								return op_sampled_image(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params[0], params[1]);
							} }));

							SpvId coordTypeId = imageType.dimension == DIMENSIONALITY_1D ? defaultTypeF32->id :
								imageType.dimension == DIMENSIONALITY_2D ? reinterpret_cast<ScalarType*>(defaultTypeF32)->v2Type->type.id :
								imageType.dimension == DIMENSIONALITY_3D || imageType.dimension == DIMENSIONALITY_CUBE ? reinterpret_cast<ScalarType*>(defaultTypeF32)->v3Type->type.id :
								reinterpret_cast<ScalarType*>(defaultTypeF32)->v4Type->type.id;

							argTypes = arena->alloc<SpvId>(2);
							argTypes[0] = imageType.sampledImageType->id;
							argTypes[1] = coordTypeId;
							signature = ProcedureType{ ProcedureIdentifier{ "sample_image"a, argTypes, 2 }, &reinterpret_cast<ScalarType*>(imageType.sampledType)->v4Type->type };
							allScopes.data[0].procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {
								return op_image_sample_implicit_lod(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, params[0], params[1]);
							} }));

							argTypes = arena->alloc<SpvId>(3);
							argTypes[0] = imageType.type.id;
							argTypes[1] = defaultTypeSampler->id;
							argTypes[2] = coordTypeId;
							signature = ProcedureType{ ProcedureIdentifier{ "operator[]"a, argTypes, 3 }, &reinterpret_cast<ScalarType*>(imageType.sampledType)->v4Type->type };
							allScopes.data[0].procedureIdentifierToProcedure.insert(signature.identifier, &(*arena->alloc<Procedure>(1) = Procedure{ signature, nullptr, nullptr, SPV_NULL_ID, [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) -> SpvId {
								SpvId sampledImageType = SpvId(proc.userData);
								SpvId sampledImage = op_sampled_image(codeOutput, sampledImageType, compiler.nextSpvId++, params[0], params[1]);
								return op_image_sample_implicit_lod(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, sampledImage, params[2]);
							}, imageType.sampledImageType->id }));
						}
						
					}
					ImageType* resultType = arena->alloc<ImageType>(1);
					*resultType = imageType;
					allTypes.push_back(&resultType->type);
					allScopes.data[0].typeNameToType.insert(name, &resultType->type);

					foundType = &resultType->type;
				}
			}
			return foundType;
		} else {
			return nullptr;
		}
	}
	void store_val_ptr(TCOp& dstOp, SpvId dstPtrId, PointerType* ptrType, SpvId srcOp, Type* srcOpType) {
		if (ptrType->pointedAt == srcOpType) {
			if (ptrType->pointedAt->typeClass == TYPE_CLASS_POINTER && ((PointerType*)ptrType->pointedAt)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
				ptrType = get_pointer_to(&((ScalarType*)defaultTypeU32)->v2Type->type, ptrType->storageClass);
			}

			ModifierContext& modCtx = allModifierContexts.data[currentModifierContext];
			bool modCtxHasAligned = modCtx.memoryOperands & MEMORY_OPERAND_ALIGNED;
			if (ptrType->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER && !modCtxHasAligned) {
				// Default alignment to 4, should work with all scalar types
				modCtx.set_memory_operand(MEMORY_OPERAND_ALIGNED, 4);
			}
			ArenaArrayList<SpvDword> memoryOperandArgs{};
			SpvDword memoryOperandArgsData[16];
			memoryOperandArgs.wrap(memoryOperandArgsData, 0, 16, *arena);
			modCtx.get_memory_operand_args(memoryOperandArgs);
			if (!modCtxHasAligned) { modCtx.memoryOperands &= ~MEMORY_OPERAND_ALIGNED; }
			op_store(procedureSpvCode, dstPtrId, srcOp, memoryOperandArgs.data, memoryOperandArgs.size);
		} else {
			generation_error("Store type mismatch"a, dstOp);
		}
	}
	void store_val(TCOp& dst, SpvId srcOp, Type* srcType) {
		if (dst.resultValueClass == VALUE_CLASS_RUNTIME_VALUE) {
			if (dst.variable->type->typeClass == TYPE_CLASS_POINTER) {
				PointerType* ptrType = reinterpret_cast<PointerType*>(dst.variable->type);
				SpvId dstPtrId = dst.variable->id;
				if (ptrType->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
					// Physical pointers are stored as V2U
					dstPtrId = op_bitcast(procedureSpvCode, ptrType->type.id, nextSpvId++, dstPtrId);
				}
				store_val_ptr(dst, dstPtrId, ptrType, srcOp, srcType);
			} else {
				generation_error("Can't store to non lvalue"a, dst);
			}
		} else if (dst.resultValueClass == VALUE_CLASS_ACCESS_CHAIN) {
			VariableAccessChain* accessChain = dst.accessChain;
			if (accessChain->base->type->typeClass == TYPE_CLASS_POINTER) {
				PointerType* ptrType = get_pointer_to(accessChain->resultType, reinterpret_cast<PointerType*>(accessChain->base->type)->storageClass);
				PointerType* physicalPtrType = ptrType;
				if (ptrType->pointedAt->typeClass == TYPE_CLASS_POINTER && ((PointerType*)ptrType->pointedAt)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
					physicalPtrType = get_pointer_to(&((ScalarType*)defaultTypeU32)->v2Type->type, ptrType->storageClass);
				}
				SpvId accessChainResult = accessChain->base->id;
				if (((PointerType*)accessChain->base->type)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
					// Physical pointers are stored as V2U
					accessChainResult = op_bitcast(procedureSpvCode, accessChain->base->type->id, nextSpvId++, accessChainResult);
				}
				if (accessChain->isPtrAccessChain) {
					accessChainResult = op_ptr_access_chain(procedureSpvCode, physicalPtrType->type.id, nextSpvId++, accessChainResult, accessChain->accessIndices.data[0], accessChain->accessIndices.data + 1, accessChain->accessIndices.size - 1);
				} else {
					accessChainResult = op_access_chain(procedureSpvCode, physicalPtrType->type.id, nextSpvId++, accessChainResult, accessChain->accessIndices.data, accessChain->accessIndices.size);
				}
				store_val_ptr(dst, accessChainResult, ptrType, srcOp, srcType);
			} else {
				if (accessChain->resultType == srcType) {
					accessChain->base->id = op_composite_insert(procedureSpvCode, accessChain->base->type->id, nextSpvId++, srcOp, accessChain->base->id, accessChain->accessIndices.data, accessChain->accessIndices.size);
				} else {
					generation_error("Store type mismatch"a, dst);
				}
			}
		} else if (dst.opcode == TC_OP_IDENTIFIER) {
			U32 varScope = U32_MAX;
			Variable* var = allScopes.data[currentScope].find_var(&varScope, allScopes.data, dst.vIdentifier);
			if (var) {
				if (var->type == srcType) {
					var->id = srcOp;
				} else {
					generation_error("Store type mismatch"a, dst);
				}
			} else {
				generation_error(strafmt(*arena, "Could not find variable by name %"a, dst.vIdentifier), dst);
			}
		} else {
			generation_error("Can't store to non lvalue"a, dst);
		}
	}

	SpvId try_implicit_bool_cast(Type** type, SpvId val) {
		if (*type == defaultTypeU32) {
			SpvId u32_0 = tcCode.data[tc_literal(Token{ TOKEN_U32 })].variable->id;
			*type = defaultTypeBool;
			return op_u_not_equal(procedureSpvCode, defaultTypeBool->id, nextSpvId++, val, u32_0);
		} else if (*type == defaultTypeI32) {
			SpvId i32_0 = tcCode.data[tc_literal(Token{ TOKEN_I32 })].variable->id;
			*type = defaultTypeBool;
			return op_i_not_equal(procedureSpvCode, defaultTypeBool->id, nextSpvId++, val, i32_0);
		} else if (*type == defaultTypeF32) {
			SpvId f32_0 = tcCode.data[tc_literal(Token{ TOKEN_F32 })].variable->id;
			*type = defaultTypeBool;
			return op_f_not_equal(procedureSpvCode, defaultTypeBool->id, nextSpvId++, val, f32_0);
		} else if ((*type)->typeClass == TYPE_CLASS_POINTER && ((PointerType*)*type)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
			// val is actually a V2U
			SpvId u32_0 = tcCode.data[tc_literal(Token{ TOKEN_U32 })].variable->id;
			*type = defaultTypeBool;
			SpvId u32Constituents[]{ u32_0, u32_0 };
			SpvId v2u_0 = op_composite_construct(procedureSpvCode, ((ScalarType*)defaultTypeU32)->v2Type->type.id, nextSpvId++, u32Constituents, 2);
			SpvId vNotEqual = op_u_not_equal(procedureSpvCode, ((ScalarType*)defaultTypeBool)->v2Type->type.id, nextSpvId++, val, v2u_0);
			return op_any(procedureSpvCode, defaultTypeBool->id, nextSpvId++, vNotEqual);
		}
		return val;
	}

	struct VarDeclaration {
		Type* type;
		StrA name;
		U32 modifierCtxIdx;
		U32 scope;
	};
	void get_var_declaration_list(ArenaArrayList<VarDeclaration>& result, U32 scopeCtxIdx, TCOp* start, TCOp* end) {
		U32 oldScope = currentScope;
		currentScope = scopeCtxIdx;
		for (TCOp* op = start; op != end; op++) {
			switch (op->opcode) {
			case TC_OP_STRUCT_DEFINE: {
				U32 numDeclarationOps = op->operands[0];
				U32 structTypeIdx = op->operands[1];
				resolve_struct(*reinterpret_cast<StructType*>(allTypes.data[structTypeIdx]));
				op += numDeclarationOps;
			} break;
			case TC_OP_PROCEDURE_DEFINE: {
				U32 totalTCOps = op->extendedOperands[1];
				op += totalTCOps;
			} break;
			case TC_OP_REFERENCE_TO: {
				TCOp& operand = tcCode.data[op->operands[0]];
				if (Type* type = load_type(operand)) {
					op->resultValueClass = VALUE_CLASS_TYPE;
					op->type = &get_pointer_to(type, allModifierContexts.data[currentModifierContext].storageClass)->type;
				} else {
					generation_error("Can't reference non type"a, operand);
				}
			} break;
			case TC_OP_SUBSCRIPT: {
				TCOp& typeOperand = tcCode.data[op->extendedOperands[0]];
				Type* baseType = load_type(typeOperand);
				TCId* args = op->extendedOperands + 1;
				U32 numArgs = op->extendedOperandsLength - 1;
				if (!baseType) {
					generation_error("Can't reference non type"a, typeOperand);
				} else if (numArgs > 1) {
					generation_error("Can't create array type with more than one argument"a, *op);
				} else {
					I32 arrayLen = -1;
					if (numArgs != 0) {
						TCOp& lenOperand = tcCode.data[args[0]];
						if (lenOperand.opcode == TC_OP_I32) {
							arrayLen = lenOperand.vI32;
						} else {
							generation_error("Array type length must be an I32"a, lenOperand);
						}
						if (arrayLen <= 0) {
							generation_error("Must have positive array length"a, lenOperand);
						}
					}
					op->resultValueClass = VALUE_CLASS_TYPE;
					op->type = &get_array_of(baseType, arrayLen)->type;
				}
			} break;
			case TC_OP_ACCESS_CHAIN: {
				U32 accessScope = currentScope;
				TCId* accessArgs = op->extendedOperands;
				U32 numAccessArgs = op->extendedOperandsLength;
				if (tcCode.data[accessArgs[0]].opcode != TC_OP_IDENTIFIER) {
					generation_error("Access chain start must be identifier in this context"a, *op);
					break;
				}
				for (U32 i = 0; i < numAccessArgs - 1; i++) {
					TCOp& arg = tcCode.data[accessArgs[i]];
					U32 nextScope = allScopes.data[accessScope].find_scope(allScopes.data, arg.vIdentifier);
					if (nextScope == U32_MAX) {
						generation_error("Failed to find scope"a, arg);
					} else {
						accessScope = nextScope;
					}
				}
				if (Type* type = allScopes.data[accessScope].find_type(allScopes.data, tcCode.data[accessArgs[numAccessArgs - 1]].vIdentifier)) {
					op->resultValueClass = VALUE_CLASS_TYPE;
					op->type = type;
				} else {
					generation_error("Failed to find type"a, tcCode.data[accessArgs[numAccessArgs - 1]]);
				}
			} break;
			case TC_OP_DECLARE: {
				Type* declarationType = load_type(tcCode.data[op->extendedOperands[0]]);
				TCOp& declarationName = tcCode.data[op->extendedOperands[1]];
				if (!declarationType) {
					generation_error("Declaration type invalid"a, tcCode.data[op->extendedOperands[0]]);
				}
				if (declarationName.opcode != TC_OP_IDENTIFIER) {
					generation_error("Declaration name must be an identifier"a, declarationName);
				}
				if (shaderSections.empty()) {
					generation_error("Must define shader section before declaring a global"a, *op);
				}
				if (compilerErrored) {
					break;
				}
				result.push_back(VarDeclaration{ declarationType, declarationName.vIdentifier, currentModifierContext, currentScope });
			} break;
			case TC_OP_SCOPE_BEGIN: currentScope = op->operands[0]; break;
			case TC_OP_SCOPE_END: currentScope = allScopes.data[currentScope].parentIdx;  break;
			case TC_OP_MODIFIER_SCOPE_BEGIN: currentModifierContext = op->operands[0]; break;
			case TC_OP_MODIFIER_SCOPE_END: currentModifierContext = allModifierContexts.data[currentModifierContext].parentIndex; break;
			case TC_OP_TC_JMP: op = &tcCode.data[op->operands[0]]; continue;
			default: {
				if (!op->is_literal()) {
					generation_error("Invalid op for context"a, *op);
				}
			} break;
			}
		}
		currentScope = oldScope;
	}
	void type_checking_bytecode_to_spirv() {
		ArenaArrayList<SpvId> callArgs{ arena };
		ArenaArrayList<SpvId> callTypes{ arena };
		ArenaArrayList<SpvDword> memoryOperandArgs{ arena };
		ArenaArrayList<VarDeclaration> varDeclarations{ arena };
		callArgs.reserve(64);
		callTypes.reserve(64);
		memoryOperandArgs.reserve(64);
		varDeclarations.reserve(64);

		// This is a massive hack. There are a few instructions that may need extra literals (like 0 for comparisons or 0/1/2/3 for swizzle indices)
		// tcCode must not be moved while interpreting it, so reserve an extra 128 spaces
		// Fix this!
		tcCode.reserve(tcCode.size + 128);

		// Resolve types
		for (Type* type : allTypes) {
			if (type->typeClass != TYPE_CLASS_STRUCT) {
				continue;
			}
			resolve_struct(*reinterpret_cast<StructType*>(type));
		}

		// Resolve globals
		varDeclarations.clear();
		get_var_declaration_list(varDeclarations, 0, tcCode.data, tcCode.end());
		for (VarDeclaration& decl : varDeclarations) {
			if (decl.type->typeClass != TYPE_CLASS_POINTER) {
				generic_error(strafmt(*arena, "Global var \"%\" must be declared as pointer"a, decl.name));
			}
			ScopeContext& scope = allScopes.data[decl.scope];
			if (shaderSections.data[scope.shaderSectionIdx].shaderType == SHADER_TYPE_INTER_SHADER_INTERFACE) {
				continue;
			}
			Variable* var = arena->zalloc<Variable>(1);
			var->name = ScopedName{ decl.name, decl.scope };
			var->type = decl.type;
			var->id = nextSpvId++;
			var->isGlobal = true;
			shaderSections.data[scope.shaderSectionIdx].globals.push_back(GlobalVariable{ var, SPV_NULL_ID, decl.modifierCtxIdx });
			allScopes.data[decl.scope].varNameToVar.insert(decl.name, var);
		}
		for (VarDeclaration& decl : varDeclarations) {
			ScopeContext& scope = allScopes.data[decl.scope];
			if (shaderSections.data[scope.shaderSectionIdx].shaderType != SHADER_TYPE_INTER_SHADER_INTERFACE) {
				continue;
			}
			ArenaArrayList<Variable*>& locationGroup = shaderInterfaceLocationGroups.push_back_zeroed();
			locationGroup.allocator = arena;
			for (ShaderSection& shaderSection : shaderSections) {
				if (shaderSection.shaderType == SHADER_TYPE_INTER_SHADER_INTERFACE) {
					continue;
				}
				Variable* var = arena->zalloc<Variable>(1);
				var->name = ScopedName{ decl.name, decl.scope };
				var->type = &get_pointer_to(reinterpret_cast<PointerType*>(decl.type)->pointedAt, shaderSection.shaderType == SHADER_TYPE_VERTEX ? STORAGE_CLASS_OUTPUT : STORAGE_CLASS_INPUT)->type;
				var->id = nextSpvId++;
				var->isGlobal = true;
				var->isFromInterfaceBlock = true;
				GlobalVariable globalVar{ var, SPV_NULL_ID, decl.modifierCtxIdx };
				shaderSection.globals.push_back(globalVar);
				allScopes.data[shaderSection.scopeIdx].varNameToVar.insert(decl.name, var);
				shaderSections.data[scope.shaderSectionIdx].globals.push_back(globalVar);
				locationGroup.push_back(var);
			}
		}

		// Resolve procedure declarations
		for (TCId procId : allProcedureTCIds) {
			TCOp& procTCOp = tcCode.data[procId];
			StrA name = tcCode.data[procTCOp.extendedOperands[0]].vIdentifier;
			//U32 totalTCOps = procTCOp.extendedOperands[1];
			U32 numReturnTCOps = procTCOp.extendedOperands[2];
			U32 numParamTCOps = procTCOp.extendedOperands[3];
			//U32 numBodyTCOps = procTCOp.extendedOperands[4];
			U32 scopeIdx = procTCOp.extendedOperands[5];
			U32 modifierCtxIdx = procTCOp.extendedOperands[6];

			varDeclarations.clear();
			get_var_declaration_list(varDeclarations, scopeIdx, tcCode.data + procId + 1, tcCode.data + procId + 1 + numReturnTCOps);
			U32 returnCount = varDeclarations.size;
			get_var_declaration_list(varDeclarations, scopeIdx, tcCode.data + procId + 1 + numReturnTCOps, tcCode.data + procId + 1 + numReturnTCOps + numParamTCOps);
			U32 paramCount = varDeclarations.size - returnCount;

			SpvId* paramTypeIds = arena->alloc<SpvId>(paramCount);
			Type** paramTypes = arena->alloc<Type*>(paramCount);
			ScopedName* paramNames = arena->alloc<ScopedName>(paramCount);
			for (U32 i = 0; i < paramCount; i++) {
				paramTypeIds[i] = varDeclarations.data[returnCount + i].type->id;
				paramTypes[i] = varDeclarations.data[returnCount + i].type;
				paramNames[i] = ScopedName{ varDeclarations.data[returnCount + i].name, varDeclarations.data[returnCount + i].scope };
			}

			Procedure* proc = arena->zalloc<Procedure>(1);
			proc->id = nextSpvId++;
			proc->signature.returnType = returnCount == 0 ? defaultTypeVoid : varDeclarations.data[0].type;
			proc->signature.identifier.name = name;
			proc->signature.identifier.numArgs = paramCount;
			proc->signature.identifier.argTypes = paramTypeIds;
			proc->argNames = paramNames;
			proc->argTypes = paramTypes;
			proc->call = [](ArenaArrayList<SpvDword>& codeOutput, Procedure& proc, DSLCompiler& compiler, SpvDword* params) {
				return op_function_call(codeOutput, proc.signature.returnType->id, compiler.nextSpvId++, proc.id, params, proc.signature.identifier.numArgs);
			};

			if (!procedureTypeToSpvId.contains(proc->signature)) {
				procedureTypeToSpvId.insert(proc->signature, nextSpvId++);
			}
			if (!name.is_empty()) {
				ScopeContext& parentScope = allScopes.data[allScopes.data[scopeIdx].parentIdx];
				parentScope.procedureIdentifierToProcedure.insert(proc->signature.identifier, proc);
			}
			ModifierContext& ctx = allModifierContexts.data[modifierCtxIdx];
			if (ctx.isEntrypoint) {
				entrypoints.push_back(ShaderEntrypoint{ name, ctx.shaderExecutionModel, proc->id, allScopes.data[scopeIdx].shaderSectionIdx, ctx.localSizeX, ctx.localSizeY, ctx.localSizeZ});
			}

			procTCOp.resultValueClass = VALUE_CLASS_PROCEDURE;
			procTCOp.procedure = proc;
		}

		// Resolve code
		for (TCId procId : allProcedureTCIds) {
			TCOp& procTCOp = tcCode.data[procId];
			Procedure& proc = *procTCOp.procedure;
			StrA name = tcCode.data[procTCOp.extendedOperands[0]].vIdentifier;
			//U32 totalTCOps = procTCOp.extendedOperands[1];
			U32 numReturnTCOps = procTCOp.extendedOperands[2];
			U32 numParamTCOps = procTCOp.extendedOperands[3];
			U32 numBodyTCOps = procTCOp.extendedOperands[4];
			U32 scopeIdx = procTCOp.extendedOperands[5];
			U32 modifierCtxIdx = procTCOp.extendedOperands[6];

			// Figure out which phis to insert and where
			struct BasicBlock {
				BasicBlock* dominatorContext;
				ArenaHashMap<ScopedName, B8> assignedVariables;
				ArenaHashMap<ScopedName, B8> phisNeeded;
				U32 numIncomingEdges;
				U32 currentControlFlowEdge;
				TerminationInsn terminationInsn;
				SpvId blockId;
				U32 loopHeaderPhiOffset; // Phis are added to the loop header before we know what they will contain. This stores the offset so we can patch them later.
				// Array of phi instruction arguments. One entry of numIncomingEdges * { varId, parentBlockId } per scoped name in phisNeeded, laid out linearly
				SpvDword* phiData;

				void init(MemoryArena* arena, BasicBlock* dominator, SpvId id) {
					dominatorContext = dominator;
					assignedVariables.allocator = arena;
					assignedVariables.clear();
					phisNeeded.allocator = arena;
					phisNeeded.clear();
					numIncomingEdges = 0;
					currentControlFlowEdge = 0;
					terminationInsn = TERMINATION_INSN_NONE;
					blockId = id;
					phiData = nullptr;
				}
				void assign_var(StrA name, U32 scopeIdx) {
					assignedVariables.insert(ScopedName{ name, scopeIdx }, 0);
				}
				bool has_var(StrA name, U32 scopeIdx) {
					return assignedVariables.contains(ScopedName{ name, scopeIdx }) || (dominatorContext ? dominatorContext->has_var(name, scopeIdx) : false);
				}
				void add_to_merge_context(BasicBlock* mergeContext, BasicBlock* mergeDominator) {
					BasicBlock* toAddCtx = this;
					while (toAddCtx && toAddCtx != mergeDominator) {
						for (U32 i = 0; i < toAddCtx->assignedVariables.capacity; i++) {
							if (toAddCtx->assignedVariables.keys[i] == toAddCtx->assignedVariables.emptyKey) {
								continue;
							}
							ScopedName assignedVarName = toAddCtx->assignedVariables.keys[i];
							if (mergeDominator->has_var(assignedVarName.name, assignedVarName.scopeIndex)) {
								mergeContext->phisNeeded.insert(assignedVarName, 0);
								mergeContext->assignedVariables.insert(assignedVarName, 0);
							}
						}
						toAddCtx = toAddCtx->dominatorContext;
					}
					mergeContext->numIncomingEdges++;
				}
				void add_context_to_phi_data(DSLCompiler& compiler, SpvId fromBlockId) {
					if (numIncomingEdges < 2) {
						return;
					}
					if (!phiData) {
						phiData = compiler.arena->zalloc<SpvDword>(phisNeeded.size * numIncomingEdges * (sizeof(SpvDword) + sizeof(SpvDword)));
					}
					U32 currentVar = 0;
					for (U32 i = 0; i < phisNeeded.capacity; i++) {
						ScopedName varName = phisNeeded.keys[i];
						if (varName == phisNeeded.emptyKey) {
							continue;
						}
						U32 phiResultIdx = (currentVar * numIncomingEdges + currentControlFlowEdge) * 2;
						currentVar++;
						Variable** var = compiler.allScopes.data[varName.scopeIndex].varNameToVar.find(varName.name);
						if (!var) {
							compiler.generic_error(stracat(*compiler.arena, "Phi generation failed; could not find variable "a, varName.name));
							continue;
						}
						phiData[phiResultIdx] = (*var)->id;
						phiData[phiResultIdx + 1] = fromBlockId;
					}
					currentControlFlowEdge++;
				}
				void add_default_context_to_phi_data(DSLCompiler& compiler) {
					if (numIncomingEdges < 2) {
						return;
					}
					if (!phiData) {
						phiData = compiler.arena->zalloc<SpvDword>(phisNeeded.size * numIncomingEdges * (sizeof(SpvDword) + sizeof(SpvDword)));
					}
					U32 currentVar = 0;
					for (U32 i = 0; i < phisNeeded.capacity; i++) {
						ScopedName varName = phisNeeded.keys[i];
						if (varName == phisNeeded.emptyKey) {
							continue;
						}
						Variable** var = compiler.allScopes.data[varName.scopeIndex].varNameToVar.find(varName.name);
						if (!var) {
							compiler.generic_error(stracat(*compiler.arena, "Phi generation failed; could not find variable "a, varName.name));
							continue;
						}
						for (U32 ctrlFlowEdge = 0; ctrlFlowEdge < numIncomingEdges; ctrlFlowEdge++) {
							U32 phiResultIdx = (currentVar * numIncomingEdges + ctrlFlowEdge) * 2;
							phiData[phiResultIdx] = (*var)->id;
						}
						currentVar++;
					}
				}
				void rewind_first_branch_path(DSLCompiler& compiler) {
					if (numIncomingEdges < 2) {
						return;
					}
					U32 currentVar = 0;
					for (U32 i = 0; i < phisNeeded.capacity; i++) {
						ScopedName varName = phisNeeded.keys[i];
						if (varName == phisNeeded.emptyKey) {
							continue;
						}
						Variable** var = compiler.allScopes.data[varName.scopeIndex].varNameToVar.find(varName.name);
						if (!var) {
							compiler.generic_error(stracat(*compiler.arena, "Phi generation failed; could not find variable "a, varName.name));
							continue;
						}
						U32 secondCtrlFlowEdge = 1;
						U32 phiResultIdx = (currentVar * numIncomingEdges + secondCtrlFlowEdge) * 2;
						// Use the defaults set in add_default_context_to_phi_data to figure out what the variable was before this branch was taken
						(*var)->id = phiData[phiResultIdx];
						currentVar++;
					}
				}
				void generate_loop_header_phis(DSLCompiler& compiler) {
					if (!phiData) {
						phiData = compiler.arena->zalloc<SpvDword>(phisNeeded.size * numIncomingEdges * (sizeof(SpvDword) + sizeof(SpvDword)));
					}
					loopHeaderPhiOffset = compiler.procedureSpvCode.size;
					for (U32 i = 0; i < phisNeeded.capacity; i++) {
						ScopedName varName = phisNeeded.keys[i];
						if (varName == phisNeeded.emptyKey) {
							continue;
						}
						Variable** var = compiler.allScopes.data[varName.scopeIndex].varNameToVar.find(varName.name);
						if (!var) {
							continue;
						}
						(*var)->id = op_phi(compiler.procedureSpvCode, (*var)->type->id, compiler.nextSpvId++, phiData, numIncomingEdges);
					}
				}
				void patch_loop_header_phis(DSLCompiler& compiler) {
					U32 phiWordCount = 3 + numIncomingEdges * 2;
					U32 currentVar = 0;
					for (U32 i = 0; i < phisNeeded.capacity; i++) {
						ScopedName varName = phisNeeded.keys[i];
						if (varName == phisNeeded.emptyKey) {
							continue;
						}
						Variable** var = compiler.allScopes.data[varName.scopeIndex].varNameToVar.find(varName.name);
						if (!var) {
							continue;
						}
						(*var)->id = patch_phi(compiler.procedureSpvCode, loopHeaderPhiOffset + phiWordCount * currentVar, &phiData[currentVar * numIncomingEdges * 2], numIncomingEdges);
						currentVar++;
					}
				}
				void generate_phis(DSLCompiler& compiler) {
					if (numIncomingEdges < 2) {
						return;
					}
					U32 currentVar = 0;
					for (U32 i = 0; i < phisNeeded.capacity; i++) {
						ScopedName varName = phisNeeded.keys[i];
						if (varName == phisNeeded.emptyKey) {
							continue;
						}
						U32 phiResultIdx = currentVar * numIncomingEdges * 2;
						currentVar++;
						Variable** var = compiler.allScopes.data[varName.scopeIndex].varNameToVar.find(varName.name);
						if (!var) {
							continue;
						}
						(*var)->id = op_phi(compiler.procedureSpvCode, (*var)->type->id, compiler.nextSpvId++, &phiData[phiResultIdx], numIncomingEdges);
					}
				}
			};
			struct ControlFlowConstruct {
				ControlFlowConstruct* parent;
				union {
					struct {
						BasicBlock* branchTrueCtx;
						BasicBlock* branchFalseCtx;
						BasicBlock* branchTrueEndCtx; // So we can properly generate the phi for if statements that yield values
					};
					struct {
						BasicBlock* loopHeaderCtx;
						BasicBlock* loopBodyCtx;
						BasicBlock* loopContinueCtx;
					};
				};
				BasicBlock* mergeCtx;
				TCOp* controlFlowHeader;
				U8 blockIndex; // The current block we're at in parsing (eg for a branch this is 0 while parsing the true block and 1 while parsing the false block)

				bool is_loop() {
					return controlFlowHeader->opcode == TC_OP_CONDITIONAL_LOOP;
				}
			};
			ArenaArrayList<U32> scopesToRemoveDummyVariableFrom{ arena };
			Variable phiDummyVariable{};
			ArenaArrayList<ControlFlowConstruct*> allControlFlowConstructs{ arena };
			ControlFlowConstruct* lastControlFlowConstruct = nullptr;
			ControlFlowConstruct* lastLoopConstruct = nullptr;
			BasicBlock* currentBasicBlock = arena->zalloc<BasicBlock>(1);
			currentBasicBlock->init(arena, nullptr, nextSpvId++);
			BasicBlock* firstBasicBlock = currentBasicBlock;

			currentScope = scopeIdx;
			scopesToRemoveDummyVariableFrom.push_back(currentScope);
			currentModifierContext = modifierCtxIdx;
			TCOp* op = &tcCode.data[procId + 1 + numReturnTCOps + numParamTCOps];
			TCOp* opEnd = op + numBodyTCOps;
			while (op != opEnd) {
				switch (op->opcode) {
				case TC_OP_CONDITIONAL_BRANCH: {
					ControlFlowConstruct* newConstruct = arena->zalloc<ControlFlowConstruct>(1);
					newConstruct->parent = lastControlFlowConstruct;
					newConstruct->mergeCtx = arena->zalloc<BasicBlock>(1);
					newConstruct->mergeCtx->init(arena, currentBasicBlock, nextSpvId++);
					newConstruct->controlFlowHeader = op;
					lastControlFlowConstruct = newConstruct;
					allControlFlowConstructs.push_back(newConstruct);

					currentBasicBlock = newConstruct->branchTrueCtx = arena->zalloc<BasicBlock>(1);
					newConstruct->branchTrueCtx->init(arena, newConstruct->mergeCtx->dominatorContext, nextSpvId++);
					newConstruct->branchTrueEndCtx = newConstruct->branchTrueCtx;
					newConstruct->branchFalseCtx = arena->zalloc<BasicBlock>(1);
					newConstruct->branchFalseCtx->init(arena, newConstruct->mergeCtx->dominatorContext, nextSpvId++);
				} break;
				case TC_OP_CONDITIONAL_LOOP: {
					ControlFlowConstruct* newConstruct = arena->zalloc<ControlFlowConstruct>(1);
					newConstruct->parent = lastControlFlowConstruct;
					newConstruct->loopHeaderCtx = arena->zalloc<BasicBlock>(1);
					newConstruct->loopHeaderCtx->init(arena, currentBasicBlock, nextSpvId++);
					newConstruct->loopBodyCtx = arena->zalloc<BasicBlock>(1);
					newConstruct->loopBodyCtx->init(arena, newConstruct->loopHeaderCtx, nextSpvId++);
					newConstruct->loopContinueCtx = arena->zalloc<BasicBlock>(1);
					newConstruct->loopContinueCtx->init(arena, newConstruct->loopHeaderCtx, nextSpvId++);
					newConstruct->mergeCtx = arena->zalloc<BasicBlock>(1);
					newConstruct->mergeCtx->init(arena, newConstruct->loopHeaderCtx, nextSpvId++);
					newConstruct->controlFlowHeader = op;
					lastLoopConstruct = lastControlFlowConstruct = newConstruct;
					allControlFlowConstructs.push_back(newConstruct);

					currentBasicBlock->add_to_merge_context(lastControlFlowConstruct->loopHeaderCtx, lastControlFlowConstruct->loopHeaderCtx->dominatorContext);

					currentBasicBlock = arena->zalloc<BasicBlock>(1);
					currentBasicBlock->init(arena, newConstruct->loopHeaderCtx, nextSpvId++);
				} break;
				case TC_OP_BLOCK_END: {
					if (lastControlFlowConstruct->is_loop()) {
						if (lastControlFlowConstruct->blockIndex == 0) {
							currentBasicBlock = lastControlFlowConstruct->loopBodyCtx;
						} else if (lastControlFlowConstruct->blockIndex == 1) {
							currentBasicBlock->add_to_merge_context(lastControlFlowConstruct->loopContinueCtx, lastControlFlowConstruct->loopHeaderCtx);
							currentBasicBlock = lastControlFlowConstruct->loopContinueCtx;
						} else if (lastControlFlowConstruct->blockIndex == 2) {
							currentBasicBlock->add_to_merge_context(lastControlFlowConstruct->loopHeaderCtx, lastControlFlowConstruct->loopHeaderCtx->dominatorContext);
							lastControlFlowConstruct->loopHeaderCtx->add_to_merge_context(lastControlFlowConstruct->mergeCtx, lastControlFlowConstruct->loopHeaderCtx);
						}
					} else { // branch
						if (currentBasicBlock->terminationInsn == TERMINATION_INSN_NONE) {
							currentBasicBlock->add_to_merge_context(lastControlFlowConstruct->mergeCtx, lastControlFlowConstruct->mergeCtx->dominatorContext);
						}
						if (lastControlFlowConstruct->blockIndex == 0) {
							lastControlFlowConstruct->branchTrueEndCtx = currentBasicBlock;
							currentBasicBlock = arena->zalloc<BasicBlock>(1);
							currentBasicBlock->init(arena, lastControlFlowConstruct->mergeCtx->dominatorContext, nextSpvId++);
						}
					}
					lastControlFlowConstruct->blockIndex++;
					if (lastControlFlowConstruct->blockIndex == 2 && !lastControlFlowConstruct->is_loop() ||
						lastControlFlowConstruct->blockIndex == 3 && lastControlFlowConstruct->is_loop()) {
						if (lastControlFlowConstruct->mergeCtx->numIncomingEdges == 0) {
							lastControlFlowConstruct->mergeCtx->terminationInsn = TERMINATION_INSN_UNREACHABLE;
						}
						currentBasicBlock = lastControlFlowConstruct->mergeCtx;
						if (lastControlFlowConstruct == lastLoopConstruct) {
							do {
								lastLoopConstruct = lastLoopConstruct->parent;
							} while (lastLoopConstruct && !lastLoopConstruct->is_loop());
						}
						lastControlFlowConstruct = lastControlFlowConstruct->parent;
					}
				} break;
				case TC_OP_BREAK: {
					if (!lastLoopConstruct) {
						generation_error("Can't break without loop body"a, *op);
						break;
					}
					currentBasicBlock->add_to_merge_context(lastLoopConstruct->mergeCtx, lastLoopConstruct->loopHeaderCtx);
					currentBasicBlock->terminationInsn = TERMINATION_INSN_BRANCH;
				} break;
				case TC_OP_CONTINUE: {
					if (!lastLoopConstruct) {
						generation_error("Can't continue without loop body"a, *op);
						break;
					}
					currentBasicBlock->add_to_merge_context(lastLoopConstruct->loopContinueCtx, lastLoopConstruct->loopHeaderCtx);
					currentBasicBlock->terminationInsn = TERMINATION_INSN_BRANCH;
				} break;
				case TC_OP_RETURN: {
					currentBasicBlock->terminationInsn = TERMINATION_INSN_RETURN;
				} break;
				case TC_OP_TERMINATE: {
					currentBasicBlock->terminationInsn = TERMINATION_INSN_TERMINATE_INVOCATION;
				} break;
				case TC_OP_STRUCT_DEFINE: {
					U32 numDeclarationOps = op->extendedOperands[1];
					op += numDeclarationOps;
				} break;
				case TC_OP_PROCEDURE_DEFINE: {
					U32 totalOps = op->extendedOperands[1];
					op += totalOps;
				} break;
				case TC_OP_REFERENCE_TO: {
					TCOp& operand = tcCode.data[op->operands[0]];
					Type* type = load_type(operand);
					if (!type) {
						generation_error("Can't reference non type"a, operand);
					} else {
						op->resultValueClass = VALUE_CLASS_TYPE;
						op->type = &get_pointer_to(type, allModifierContexts.data[currentModifierContext].storageClass)->type;
					}
				} break;
				case TC_OP_SUBSCRIPT: {
					TCOp& arrayOperand = tcCode.data[op->extendedOperands[0]];
					if (arrayOperand.opcode == TC_OP_IDENTIFIER) {
						StrA varName = arrayOperand.vIdentifier;
						U32 varScope = U32_MAX;
						Variable* var = allScopes.data[currentScope].find_var(&varScope, allScopes.data, varName);
						if (var && !var->isGlobal) {
							// This may incorrectly identify an assign to this variable if operator[] is overloaded, but it should be ok to have redundant phis
							op->resultValueClass = VALUE_CLASS_SCOPED_NAME;
							op->scopedName = ScopedName{ varName, varScope };
						}
					} else if (arrayOperand.resultValueClass == VALUE_CLASS_SCOPED_NAME) {
						op->scopedName = arrayOperand.scopedName;
					}
				} break;
				case TC_OP_ACCESS_CHAIN: {
					U32 accessScope = currentScope;
					TCId* accessArgs = op->extendedOperands;
					U32 numAccessArgs = op->extendedOperandsLength;
					if (tcCode.data[accessArgs[0]].opcode != TC_OP_IDENTIFIER) {
						break;
					}
					U32 i = 0;
					for (; i < numAccessArgs - 1; i++) {
						TCOp& arg = tcCode.data[accessArgs[i]];
						U32 nextScope = allScopes.data[accessScope].find_scope(allScopes.data, arg.vIdentifier);
						if (nextScope == U32_MAX) {
							break;
						} else {
							accessScope = nextScope;
						}
					}
					StrA varName = tcCode.data[accessArgs[i]].vIdentifier;
					U32 varScope = U32_MAX;
					Variable* var = allScopes.data[accessScope].find_var(&varScope, allScopes.data, varName);
					if (var && !var->isGlobal) {
						op->resultValueClass = VALUE_CLASS_SCOPED_NAME;
						op->scopedName = ScopedName{ varName, varScope };
					}
				} break;
				case TC_OP_DECLARE: {
					TCOp& declarationName = tcCode.data[op->extendedOperands[1]];
					if (declarationName.opcode != TC_OP_IDENTIFIER) {
						generation_error("Declaration name must be an identifier"a, declarationName);
					}
					if (!declarationName.vIdentifier.is_empty()) {
						allScopes.data[currentScope].varNameToVar.insert(declarationName.vIdentifier, &phiDummyVariable);
						currentBasicBlock->assign_var(declarationName.vIdentifier, currentScope);
					}
				} break;
				case TC_OP_ASSIGN: {
					TCOp& assignTo = tcCode.data[op->operands[0]];
					if (assignTo.resultValueClass == VALUE_CLASS_SCOPED_NAME) {
						currentBasicBlock->assign_var(assignTo.scopedName.name, assignTo.scopedName.scopeIndex);
					} else if (assignTo.opcode == TC_OP_IDENTIFIER) {
						U32 varScopeIdx = 0;
						if (Variable* var = allScopes.data[currentScope].find_var(&varScopeIdx, allScopes.data, assignTo.vIdentifier)) {
							currentBasicBlock->assign_var(assignTo.vIdentifier, varScopeIdx);
						}
					}
				} break;
				case TC_OP_SCOPE_BEGIN: currentScope = op->operands[0]; scopesToRemoveDummyVariableFrom.push_back(currentScope); break;
				case TC_OP_SCOPE_END: currentScope = allScopes.data[currentScope].parentIdx;  break;
				case TC_OP_MODIFIER_SCOPE_BEGIN: currentModifierContext = op->operands[0]; break;
				case TC_OP_MODIFIER_SCOPE_END: currentModifierContext = allModifierContexts.data[currentModifierContext].parentIndex; break;
				case TC_OP_TC_JMP: op = &tcCode.data[op->operands[0]]; continue;
				default: break; // ignore other ops
				}
				op++;
			}
			for (U32 scopeId : scopesToRemoveDummyVariableFrom) {
				allScopes.data[scopeId].varNameToVar.clear();
			}

			lastControlFlowConstruct = nullptr;
			lastLoopConstruct = nullptr;

			// Emit code
			op_function(procedureSpvCode, proc.signature.returnType->id, proc.id, allModifierContexts.data[modifierCtxIdx].functionControl, *procedureTypeToSpvId.find(proc.signature));
			for (U32 i = 0; i < proc.signature.identifier.numArgs; i++) {
				Variable* var = arena->zalloc<Variable>(1);
				var->name = proc.argNames[i];
				var->type = proc.argTypes[i];
				var->id = nextSpvId++;
				allScopes.data[currentScope].varNameToVar.insert(proc.argNames[i].name, var);
				op_function_parameter(procedureSpvCode, var->type->id, var->id);
			}
			U32 controlFlowConstructIndex = 0;
			lastControlFlowConstruct = nullptr;
			lastLoopConstruct = nullptr;
			currentBasicBlock = firstBasicBlock;
			op_label(procedureSpvCode, currentBasicBlock->blockId);
			for (op = &tcCode.data[procId + 1 + numReturnTCOps + numParamTCOps]; op != opEnd;) {
				switch (op->opcode) {
				case TC_OP_ADD: do_binary_procedure(op, "operator+"a); break;
				case TC_OP_ADDONE: do_unary_procedure(op, "operator++"a); break;
				case TC_OP_SUB: do_binary_procedure(op, "operator-"a); break;
				case TC_OP_SUBONE: do_unary_procedure(op, "operator--"a); break;
				case TC_OP_NEG: do_unary_procedure(op, "operator-"a); break;
				case TC_OP_MUL: do_binary_procedure(op, "operator*"a); break;
				case TC_OP_DIV: do_binary_procedure(op, "operator/"a); break;
				case TC_OP_REM: do_binary_procedure(op, "operator%"a); break;
				case TC_OP_MOD: do_binary_procedure(op, "operator%%"a); break;
				case TC_OP_SHL: do_binary_procedure(op, "operator<<"a); break;
				case TC_OP_SHR: do_binary_procedure(op, "operator>>"a); break;
				case TC_OP_SAR: do_binary_procedure(op, "operator>>>"a); break;
				case TC_OP_AND: do_binary_procedure(op, "operator&"a); break;
				case TC_OP_XOR: do_binary_procedure(op, "operator^"a); break;
				case TC_OP_OR: do_binary_procedure(op, "operator|"a); break;
				case TC_OP_NOT: do_unary_procedure(op, "operator~"a); break;
				case TC_OP_LOGICAL_NOT: {
					Type* valType;
					SpvId valId = load_val(&valType, tcCode.data[op->operands[0]]);
					if (valType) {
						valId = try_implicit_bool_cast(&valType, valId);
						if (valType == defaultTypeBool) {
							op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
							op->variable = arena->zalloc<Variable>(1);
							op->variable->type = defaultTypeBool;
							op->variable->id = op_logical_not(procedureSpvCode, defaultTypeBool->id, nextSpvId++, valId);
						} else {
							generation_error("Logical not must operate on bool"a, *op);
						}
					}
				} break;
				case TC_OP_REFERENCE_TO: {
					TCOp& operand = tcCode.data[op->operands[0]];
					Type* loadedType = load_type(operand);
					if (loadedType) {
						op->resultValueClass = VALUE_CLASS_TYPE;
						op->type = &get_pointer_to(loadedType, allModifierContexts.data[currentModifierContext].storageClass)->type;
					} else {
						do_unary_procedure(op, "operator&"a);
					}
				} break;
				case TC_OP_DEREFERENCE: {
					TCOp& operand = tcCode.data[op->operands[0]];
					if (operand.opcode == TC_OP_IDENTIFIER) {
						U32 varScope = U32_MAX;
						if (Variable* var = allScopes.data[currentScope].find_var(&varScope, allScopes.data, operand.vIdentifier)) {
							op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
							op->variable = var;
						} else {
							generation_error(strafmt(*arena, "Could not find variable by name %"a, operand.vIdentifier), operand);
						}
					} else if (operand.resultValueClass == VALUE_CLASS_RUNTIME_VALUE) {
						op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
						op->variable = operand.variable;
					} else if (operand.resultValueClass == VALUE_CLASS_ACCESS_CHAIN) {
						op->resultValueClass = VALUE_CLASS_ACCESS_CHAIN;
						op->accessChain = operand.accessChain;
					}
					if (op->resultValueClass == VALUE_CLASS_RUNTIME_VALUE && op->variable->type->typeClass == TYPE_CLASS_POINTER) {
						VariableAccessChain* accessChain = arena->zalloc<VariableAccessChain>(1);
						accessChain->base = op->variable;
						accessChain->resultType = reinterpret_cast<PointerType*>(op->variable->type)->pointedAt;
						accessChain->accessIndices.allocator = arena;
						op->resultValueClass = VALUE_CLASS_ACCESS_CHAIN;
						op->accessChain = accessChain;
					} else if (op->resultValueClass == VALUE_CLASS_ACCESS_CHAIN && op->accessChain->resultType->typeClass == TYPE_CLASS_POINTER) {
						op->accessChain->accessIndices.push_back(spv_literal_index(0));
						op->accessChain->resultType = reinterpret_cast<PointerType*>(op->accessChain->resultType)->pointedAt;
					} else {
						generation_error("Can't dereference non pointer"a, *op);
					}
					op->resultIsNonUniform = operand.resultIsNonUniform;
				} break;
				case TC_OP_MARK_NONUNIFORM: {
					enabledExtensions.shaderNonUniform = true;
					Type* valType;
					SpvId valId = load_val(&valType, tcCode.data[op->operands[0]]);
					if (valType) {
						op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
						op->variable = arena->zalloc<Variable>(1);
						op->variable->type = valType;
						op->variable->id = valId;
						op->resultIsNonUniform = true;
						if (!tcCode.data[op->operands[0]].resultIsNonUniform) {
							op_decorate(decorationSpvCode, valId, DECORATION_NON_UNIFORM, nullptr, 0);
						}
					}
				} break;
				case TC_OP_LT: do_binary_procedure(op, "operator<"a); break;
				case TC_OP_LE: do_binary_procedure(op, "operator<="a); break;
				case TC_OP_GT: do_binary_procedure(op, "operator>"a); break;
				case TC_OP_GE: do_binary_procedure(op, "operator>="a); break;
				case TC_OP_EQ: do_binary_procedure(op, "operator=="a); break;
				case TC_OP_NE: do_binary_procedure(op, "operator!="a); break;
				case TC_OP_CALL:
				case TC_OP_SUBSCRIPT: {
					TCOp& toCall = tcCode.data[op->extendedOperands[0]];
					U32 numArgs = op->extendedOperandsLength - 1;
					Type* calledType = load_type(tcCode.data[op->extendedOperands[0]]);
					if (op->opcode == TC_OP_SUBSCRIPT && calledType) {
						if (numArgs > 1) {
							generation_error("Array declaration must not have more than one size"a, *op);
						}
						TCOp& arraySize = tcCode.data[op->extendedOperands[1]];
						if (numArgs == 1 && arraySize.opcode != TC_OP_I32) {
							generation_error("Array must be created with I32 size"a, arraySize);
						}
						op->resultValueClass = VALUE_CLASS_TYPE;
						op->type = &get_array_of(calledType, numArgs == 0 ? -1 : arraySize.vI32)->type;
						break;
					}
					if (op->opcode == TC_OP_CALL && calledType && calledType->typeClass == TYPE_CLASS_POINTER) {
						if (reinterpret_cast<PointerType*>(calledType)->storageClass != STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
							generation_error("Cast to pointer must be physical pointer type"a, *op);
						}
						if (numArgs != 1) {
							generation_error("Pointer constructor must have 1 arg"a, *op);
							break;
						}
						Type* argType;
						SpvId argId = load_val(&argType, tcCode.data[op->extendedOperands[1]]);
						if (argType != &((ScalarType*) defaultTypeU32)->v2Type->type &&
							(argType->typeClass != TYPE_CLASS_POINTER || ((PointerType*)argType)->storageClass != STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER)) {
							generation_error("Pointer must be constructed with V2U32 or other pointer"a, *op);
						}
						op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
						op->variable = arena->zalloc<Variable>(1);
						op->variable->type = calledType;
						op->variable->id = argId;
						break;
					}

					U32 namedVarScope = U32_MAX;
					Variable* namedVar = nullptr;
					if (toCall.opcode == TC_OP_IDENTIFIER) {
						namedVar = allScopes.data[currentScope].find_var(&namedVarScope, allScopes.data, toCall.vIdentifier);
					}
					if (toCall.resultValueClass == VALUE_CLASS_RUNTIME_VALUE && toCall.variable->type->typeClass == TYPE_CLASS_ARRAY ||
						toCall.resultValueClass == VALUE_CLASS_ACCESS_CHAIN && toCall.accessChain->resultType->typeClass == TYPE_CLASS_ARRAY ||
						namedVar && namedVar->type->typeClass == TYPE_CLASS_ARRAY) {
						if (op->opcode != TC_OP_SUBSCRIPT) {
							generation_error("Array must be subscripted"a, tcCode.data[op->extendedOperands[1]]);
						}
						Type* accessType = nullptr;
						SpvId accessId = load_val(&accessType, tcCode.data[op->extendedOperands[1]]);
						I32 accessLiteral = 0;
						if (accessType != defaultTypeI32 && accessType != defaultTypeU32) {
							generation_error("Array must be subscripted with integer type"a, tcCode.data[op->extendedOperands[1]]);
						}
						if ((toCall.resultValueClass == VALUE_CLASS_RUNTIME_VALUE ? toCall.variable->type : toCall.accessChain->base->type)->typeClass != TYPE_CLASS_POINTER) {
							if (tcCode.data[op->extendedOperands[1]].opcode != TC_OP_I32) {
								generation_error("Non pointer array must be accessed with I32 constant"a, tcCode.data[op->extendedOperands[1]]);
							}
							accessLiteral = tcCode.data[op->extendedOperands[1]].vI32;
						}
						// Make access chain
						if (toCall.resultValueClass == VALUE_CLASS_RUNTIME_VALUE || namedVar) {
							Variable* var = namedVar ? namedVar : toCall.variable;
							VariableAccessChain* accessChain = arena->zalloc<VariableAccessChain>(1);
							accessChain->base = var;
							accessChain->resultType = var->type->typeClass == TYPE_CLASS_POINTER ? reinterpret_cast<PointerType*>(var->type)->pointedAt : var->type;
							accessChain->accessIndices.allocator = arena;
							op->resultValueClass = VALUE_CLASS_ACCESS_CHAIN;
							op->accessChain = accessChain;
							op->accessChain->accessIndices.push_back(accessChain->base->type->typeClass == TYPE_CLASS_POINTER ? accessId : accessLiteral);
						} else {
							toCall.accessChain->accessIndices.push_back(toCall.accessChain->base->type->typeClass == TYPE_CLASS_POINTER ? accessId : accessLiteral);
							toCall.accessChain->resultType = reinterpret_cast<ArrayType*>(toCall.accessChain->resultType)->elementType;
							op->resultValueClass = VALUE_CLASS_ACCESS_CHAIN;
							op->accessChain = toCall.accessChain;
						}
						op->resultIsNonUniform = bool(toCall.resultIsNonUniform | tcCode.data[op->extendedOperands[1]].resultIsNonUniform);
						break;
					}

					if (toCall.resultValueClass == VALUE_CLASS_RUNTIME_VALUE && toCall.variable->type->typeClass == TYPE_CLASS_POINTER && ((PointerType*)toCall.variable->type)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER ||
						toCall.resultValueClass == VALUE_CLASS_ACCESS_CHAIN && toCall.accessChain->resultType->typeClass == TYPE_CLASS_POINTER && ((PointerType*)toCall.accessChain->resultType)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER ||
						namedVar && namedVar->type->typeClass == TYPE_CLASS_POINTER && ((PointerType*)namedVar->type)->storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {

						Type* baseType = nullptr;
						SpvId baseId = load_val(&baseType, toCall);
						Type* accessType = nullptr;
						SpvId accessId = load_val(&accessType, tcCode.data[op->extendedOperands[1]]);
						if (accessType != defaultTypeI32 && accessType != defaultTypeU32) {
							generation_error("Array must be subscripted with integer type"a, tcCode.data[op->extendedOperands[1]]);
						}
						Variable* var = nullptr;
						if (toCall.resultValueClass == VALUE_CLASS_RUNTIME_VALUE || namedVar) {
							var = namedVar ? namedVar : toCall.variable;
						} else {
							var = arena->zalloc<Variable>(1);
							var->type = baseType;
							var->id = baseId;
						}
						VariableAccessChain* accessChain = arena->zalloc<VariableAccessChain>(1);
						accessChain->base = var;
						accessChain->resultType = var->type->typeClass == TYPE_CLASS_POINTER ? reinterpret_cast<PointerType*>(var->type)->pointedAt : var->type;
						accessChain->accessIndices.allocator = arena;
						op->resultValueClass = VALUE_CLASS_ACCESS_CHAIN;
						op->accessChain = accessChain;
						op->accessChain->accessIndices.push_back(accessId);
						op->accessChain->isPtrAccessChain = true;
						break;
					}

					callArgs.clear();
					callTypes.clear();
					callArgs.reserve(op->extendedOperandsLength);
					callTypes.reserve(op->extendedOperandsLength);
					for (U32 i = 1; i < op->extendedOperandsLength; i++) {
						Type* argType = nullptr;
						SpvId argResult = load_val(&argType, tcCode.data[op->extendedOperands[i]]);
						if (argResult != SPV_NULL_ID) {
							callArgs.push_back(argResult);
							callTypes.push_back(argType->id);
						}
					}
					Procedure* procToCall = nullptr;
					if (toCall.opcode == TC_OP_IDENTIFIER) {
						if (calledType) {
							procToCall = allScopes.data[currentScope].find_procedure(allScopes.data, ProcedureIdentifier{ stracat(*arena, calledType->typeName, ".<init>"a), callTypes.data, callTypes.size});
						} else {
							procToCall = allScopes.data[currentScope].find_procedure(allScopes.data, ProcedureIdentifier{ toCall.vIdentifier, callTypes.data, callTypes.size });
						}
					} else if (toCall.resultValueClass == VALUE_CLASS_SCOPED_NAME) {
						procToCall = allScopes.data[toCall.scopedName.scopeIndex].find_procedure(allScopes.data, ProcedureIdentifier{ toCall.scopedName.name, callTypes.data, callTypes.size });
					} else if (toCall.resultValueClass == VALUE_CLASS_PROCEDURE) {
						procToCall = toCall.procedure;
					}
					if (!procToCall) {
						// No direct procedures, try to find an overloaded call operator
						Type* argType = nullptr;
						SpvId argResult = load_val(&argType, toCall);
						if (argType) {
							callArgs.insert(0, argResult);
							callTypes.insert(0, argType->id);
							procToCall = allScopes.data[currentScope].find_procedure(allScopes.data, ProcedureIdentifier{ op->opcode == TC_OP_SUBSCRIPT ? "operator[]"a : "operator()"a, callTypes.data, callTypes.size});
						}
					}
					if (procToCall) {
						if (callArgs.size != procToCall->signature.identifier.numArgs) {
							generation_error("Must have same number of args as procedure parameters"a, *op);
						} else {
							op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
							op->variable = arena->zalloc<Variable>(1);
							op->variable->type = procToCall->signature.returnType;
							op->variable->id = procToCall->call(procedureSpvCode, *procToCall, *this, callArgs.data);
						}
					} else {
						generation_error("Could not resolve procedure"a, *op);
					}
				} break;
				case TC_OP_ACCESS_CHAIN: {
					U32 accessScope = currentScope;
					TCId* accessArgs = op->extendedOperands;
					U32 numAccessArgs = op->extendedOperandsLength;
					U32 i = 0;
					if (tcCode.data[accessArgs[0]].opcode == TC_OP_IDENTIFIER) {
						for (; i < numAccessArgs - 1; i++) {
							TCOp& arg = tcCode.data[accessArgs[i]];
							U32 nextScope = allScopes.data[accessScope].find_scope(allScopes.data, arg.vIdentifier);
							if (nextScope == U32_MAX) {
								break;
							} else {
								accessScope = nextScope;
							}
						}
					}
					Variable* var = nullptr;
					Type* type = nullptr;
					VariableAccessChain* accessChain = nullptr;
					TCOp& firstAccessArg = tcCode.data[accessArgs[i]];
					if (firstAccessArg.opcode == TC_OP_IDENTIFIER) {
						StrA iden = firstAccessArg.vIdentifier;
						U32 varScope = U32_MAX;
						var = allScopes.data[accessScope].find_var(&varScope, allScopes.data, iden);
						if (!var) {
							type = allScopes.data[accessScope].find_type(allScopes.data, iden);
							if (!type) {
								if (i != numAccessArgs) {
									generation_error(strafmt(*arena, "Unknown identifier: %"a, iden), firstAccessArg);
									generation_error("Reached access chain result while entries still remain in access chain"a, firstAccessArg);
								}
								op->resultValueClass = VALUE_CLASS_SCOPED_NAME;
								op->scopedName = ScopedName{ iden, accessScope };
								break;
							}
						}
					} else if (firstAccessArg.resultValueClass == VALUE_CLASS_RUNTIME_VALUE) {
						var = firstAccessArg.variable;
					} else if (firstAccessArg.resultValueClass == VALUE_CLASS_TYPE) {
						type = firstAccessArg.type;
					} else if (firstAccessArg.resultValueClass == VALUE_CLASS_ACCESS_CHAIN) {
						accessChain = firstAccessArg.accessChain;
					} else {
						generation_error("Access chain failed"a, firstAccessArg);
						break;
					}
					if (type) {
						if (i != numAccessArgs) {
							generation_error("Reached access chain result while entries still remain in access chain"a, firstAccessArg);
						}
						op->resultValueClass = VALUE_CLASS_TYPE;
						op->type = type;
						break;
					}
					i++;
					if (var && i == numAccessArgs) {
						op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
						op->variable = var;
						break;
					}

					Type* prevType = nullptr;
					if (var) {
						prevType = var->type;
						if (prevType->typeClass == TYPE_CLASS_POINTER) {
							prevType = reinterpret_cast<PointerType*>(prevType)->pointedAt;
						}
						accessChain = arena->zalloc<VariableAccessChain>(1);
						accessChain->base = var;
						accessChain->resultType = prevType;
						accessChain->accessIndices.allocator = arena;
					} else {
						prevType = accessChain->resultType;
					}
					op->resultValueClass = VALUE_CLASS_ACCESS_CHAIN;
					op->accessChain = accessChain;
					SpvId loadedId = SPV_NULL_ID; // If the lvalue access chain is broken by a load (eg vector swizzle) accessChain is set to null and this is set to the result value
					for (; i < numAccessArgs; i++) {
						TCOp& currentAccessArg = tcCode.data[accessArgs[i]];
						StrA accessName = currentAccessArg.vIdentifier;
						if (prevType->typeClass == TYPE_CLASS_STRUCT) {
							StructType* structType = reinterpret_cast<StructType*>(prevType);
							for (StructType::Member& member : structType->members) {
								if (member.name == accessName) {
									prevType = member.type;
									if (accessChain) {
										accessChain->resultType = prevType;
										accessChain->accessIndices.push_back(accessChain->base->type->typeClass == TYPE_CLASS_POINTER ? spv_literal_index(member.index) : member.index);
									} else {
										loadedId = op_composite_extract(procedureSpvCode, prevType->id, nextSpvId++, loadedId, &member.index, 1);
									}
									goto memberFound;
								}
							}
							generation_error("Member not found"a, currentAccessArg);
						memberFound:;
						} else if (prevType->typeClass == TYPE_CLASS_VECTOR) {
							VectorType* vecType = reinterpret_cast<VectorType*>(prevType);
							if (accessName.length > vecType->numComponents) {
								generation_error("Vector access must not access more members than vector size"a, currentAccessArg);
								break;
							}
							U32 indices[4]{};
							for (U32 constituent = 0; constituent < accessName.length; constituent++) {
								switch (accessName.str[constituent]) {
								case 'x':
								case 'r':
								case 's': indices[constituent] = 0; break;
								case 'y':
								case 'g':
								case 't': indices[constituent] = 1; break;
								case 'z':
								case 'b':
								case 'p': indices[constituent] = 2; break;
								case 'w':
								case 'a':
								case 'q': indices[constituent] = 3; break;
								default: generation_error("Unknown vector access index"a, currentAccessArg);
								}
								if (indices[constituent] >= vecType->numComponents) {
									generation_error("Can't access this index for vector size"a, currentAccessArg);
								}
							}
							if (accessName.length == 1) {
								prevType = &vecType->componentType->type;
								if (accessChain) {
									accessChain->accessIndices.push_back(accessChain->base->type->typeClass == TYPE_CLASS_POINTER ? spv_literal_index(indices[0]) : indices[0]);
									accessChain->resultType = prevType;
								} else {
									loadedId = op_composite_extract(procedureSpvCode, prevType->id, nextSpvId++, loadedId, &indices[0], 1);
								}
							} else {
								Type* accessResultType = nullptr;
								SpvId accessResult = accessChain ? load_val(&accessResultType, *op) : loadedId;
								VectorType* resultType =
									accessName.length == 2 ? vecType->componentType->v2Type :
									accessName.length == 3 ? vecType->componentType->v3Type :
									vecType->componentType->v4Type;
								accessResult = op_vector_shuffle(procedureSpvCode, resultType->type.id, nextSpvId++, accessResult, accessResult, indices, U32(accessName.length));

								accessChain = nullptr;
								prevType = &resultType->type;
								loadedId = accessResult;
							}
						} else {
							generation_error("Can't access chain this type"a, currentAccessArg);
						}
					}
					if (!accessChain) {
						op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
						Variable* resultVar = arena->zalloc<Variable>(1);
						resultVar->type = prevType;
						resultVar->id = loadedId;
						op->variable = resultVar;
					}
				} break;
				case TC_OP_DECLARE: {
					TCOp& typeTCOp = tcCode.data[op->extendedOperands[0]];
					Type* type = nullptr;
					if (typeTCOp.resultValueClass == VALUE_CLASS_TYPE) {
						type = typeTCOp.type;
					} else if (typeTCOp.opcode == TC_OP_IDENTIFIER) {
						type = allScopes.data[currentScope].find_type(allScopes.data, typeTCOp.vIdentifier);
					} else {
						generation_error("Could not resolve type for object creation"a, typeTCOp);
						break;
					}
					StrA varName = tcCode.data[op->extendedOperands[1]].vIdentifier;
					TCId* args = op->extendedOperands + 2;
					U32 argsLength = op->extendedOperandsLength - 2;
					callArgs.clear();
					callTypes.clear();
					callArgs.reserve(argsLength);
					callTypes.reserve(argsLength);
					for (U32 i = 0; i < argsLength; i++) {
						TCOp& arg = tcCode.data[args[i]];
						Type* spvType = nullptr;
						SpvId spvArg = load_val(&spvType, arg);
						if (spvArg != SPV_NULL_ID) {
							callArgs.push_back(spvArg);
							callTypes.push_back(spvType->id);
						} else {
							generation_error("Constituent must be a value"a, arg);
						}
					}
					//TODO handle array init
					U32 scopeToFindInitIn = 0;
					if (type->typeClass == TYPE_CLASS_STRUCT) {
						scopeToFindInitIn = reinterpret_cast<StructType*>(type)->scopeIdx;
					}
					Procedure* procToCall = allScopes.data[scopeToFindInitIn].find_procedure(allScopes.data, ProcedureIdentifier{ stracat(*arena, type->typeName, ".<init>"a), callTypes.data, callTypes.size });
					SpvId resultId = SPV_NULL_ID;
					if (procToCall) {
						ASSERT(procToCall->signature.returnType == type, "Init return type did not match"a);
						resultId = procToCall->call(procedureSpvCode, *procToCall, *this, callArgs.data);
					} else if (callTypes.size == 1 && callTypes.data[0] == type->id) {
						resultId = callArgs.data[0];
					} else {
						generation_error("Can't initialize type with arguments"a, *op);
					}
					if (resultId != SPV_NULL_ID) {
						op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
						op->variable = arena->zalloc<Variable>(1);
						op->variable->name = ScopedName{ varName, currentScope };
						op->variable->type = type;
						op->variable->id = resultId;
						if (varName.str) {
							allScopes.data[currentScope].varNameToVar.insert(varName, op->variable);
						}
					}
				} break;
				case TC_OP_ASSIGN: {
					TCOp& assignToOp = tcCode.data[op->operands[0]];
					TCOp& assignFromOp = tcCode.data[op->operands[1]];
					Type* loadedType;
					SpvId loadedId = load_val(&loadedType, assignFromOp);
					if (loadedId == SPV_NULL_ID) {
						generation_error("Can't assign a non Value"a, assignFromOp);
					}
					store_val(assignToOp, loadedId, loadedType);
					if (assignToOp.resultValueClass == VALUE_CLASS_RUNTIME_VALUE) {
						op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
						op->variable = assignToOp.variable;
					} else if (assignToOp.resultValueClass == VALUE_CLASS_ACCESS_CHAIN) {
						op->resultValueClass = VALUE_CLASS_ACCESS_CHAIN;
						op->accessChain = assignToOp.accessChain;
					} else if (assignToOp.opcode == TC_OP_IDENTIFIER) {
						U32 varScope = U32_MAX;
						if (Variable* var = allScopes.data[currentScope].find_var(&varScope, allScopes.data, assignToOp.vIdentifier)) {
							op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
							op->variable = var;
						}
					}
				} break;
				case TC_OP_LOAD: {
					TCOp& toLoadOp = tcCode.data[op->operands[0]];
					op->resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
					op->variable = arena->zalloc<Variable>(1);
					op->variable->id = load_val(&op->variable->type, toLoadOp);
				} break;
				case TC_OP_PROCEDURE_DEFINE: {
					U32 numTCOps = op->extendedOperands[1];
					op += numTCOps;
				} break;
				case TC_OP_STRUCT_DEFINE: {
					// Structs have already been processed, skip it
					TCId opCount = op->operands[1];
					op += opCount;
				} break;
				case TC_OP_RETURN: {
					TCId* returnArgIds = op->extendedOperands;
					U32 numReturnArgs = op->extendedOperandsLength;
					if (numReturnArgs == 0) {
						op_return(procedureSpvCode);
					} else if (numReturnArgs == 1) {
						Type* returnedType = nullptr;
						op_return_value(procedureSpvCode, load_val(&returnedType, tcCode.data[returnArgIds[0]]));
						if (returnedType != proc.signature.returnType) {
							generation_error("Returned type did not match procedure return type"a, *op);
						}
					} else {
						generation_error("Multiple returns currently unsupported"a, *op);
					}
					//if ((op + 1)->opcode != TC_OP_BLOCK_END && ((op + 1)->opcode != TC_OP_SCOPE_END || (op + 2)->opcode != TC_OP_BLOCK_END)) {
						//generation_error("Return must be the last instruction in a block"a, *op);
					//}
					currentBasicBlock->terminationInsn = numReturnArgs == 0 ? TERMINATION_INSN_RETURN : TERMINATION_INSN_RETURN_VALUE;
				} break;
				case TC_OP_BREAK: {
					if (lastLoopConstruct) {
						lastLoopConstruct->mergeCtx->add_context_to_phi_data(*this, currentBasicBlock->blockId);
						op_branch(procedureSpvCode, lastLoopConstruct->mergeCtx->blockId);
						currentBasicBlock->terminationInsn = TERMINATION_INSN_BRANCH;
						//if ((op + 1)->opcode != TC_OP_BLOCK_END && ((op + 1)->opcode != TC_OP_SCOPE_END || (op + 2)->opcode != TC_OP_BLOCK_END)) {
							//generation_error("Break must be the last instruction in a block"a, *op);
						//}
					} else {
						generation_error("Break/continue must be inside loop"a, *op);
					}
				} break;
				case TC_OP_CONTINUE: {
					if (lastLoopConstruct) {
						lastLoopConstruct->loopContinueCtx->add_context_to_phi_data(*this, currentBasicBlock->blockId);
						op_branch(procedureSpvCode, lastLoopConstruct->loopContinueCtx->blockId);
						currentBasicBlock->terminationInsn = TERMINATION_INSN_BRANCH;
						//if ((op + 1)->opcode != TC_OP_BLOCK_END && ((op + 1)->opcode != TC_OP_SCOPE_END || (op + 2)->opcode != TC_OP_BLOCK_END)) {
							//generation_error("Continue must be the last instruction in a block"a, *op);
						//}
					} else {
						generation_error("Break/continue must be inside loop"a, *op);
					}
				} break;
				case TC_OP_DEMOTE: {
					if (allModifierContexts.data[currentModifierContext].shaderExecutionModel != EXECUTION_MODEL_FRAGMENT) {
						generation_error("Demote is only valid in a fragment shader"a, *op);
					}
					enabledExtensions.demoteToHelperInvocation = true;
					op_demote_to_helper_invocation(procedureSpvCode);
				} break;
				case TC_OP_TERMINATE: {
					if (allModifierContexts.data[currentModifierContext].shaderExecutionModel != EXECUTION_MODEL_FRAGMENT) {
						generation_error("Terminate is only valid in a fragment shader"a, *op);
					}
					//if ((op + 1)->opcode != TC_OP_BLOCK_END && ((op + 1)->opcode != TC_OP_SCOPE_END || (op + 2)->opcode != TC_OP_BLOCK_END)) {
					//	generation_error("Terminate must be the last instruction in a block"a, *op);
					//}
					//TODO: SPIRV 1.6
					//op_terminate_invocation(procedureSpvCode);
					op_kill(procedureSpvCode);
					currentBasicBlock->terminationInsn = TERMINATION_INSN_TERMINATE_INVOCATION;
				} break;
				case TC_OP_CONDITIONAL_BRANCH: {
					// This must exactly match the phi generation code. It's a bit fragile, but I don't have a better idea.
					lastControlFlowConstruct = allControlFlowConstructs.data[controlFlowConstructIndex++];
					lastControlFlowConstruct->blockIndex = 0;
					lastControlFlowConstruct->mergeCtx->add_default_context_to_phi_data(*this);
					TCOp& condition = tcCode.data[op->operands[0]];
					Type* evaluatedType;
					SpvId evaluatedCondition = load_val(&evaluatedType, condition);
					if (evaluatedType) {
						evaluatedCondition = try_implicit_bool_cast(&evaluatedType, evaluatedCondition);
					}
					if (evaluatedType != defaultTypeBool) {
						generation_error("Branch condition must be a bool"a, condition);
					}
					op_selection_merge(procedureSpvCode, lastControlFlowConstruct->mergeCtx->blockId, allModifierContexts.data[currentModifierContext].selectionControl);
					currentBasicBlock->terminationInsn = TERMINATION_INSN_BRANCH_CONDITIONAL;
					op_branch_conditional(procedureSpvCode, evaluatedCondition, lastControlFlowConstruct->branchTrueCtx->blockId, lastControlFlowConstruct->branchFalseCtx->blockId);
					op_label(procedureSpvCode, lastControlFlowConstruct->branchTrueCtx->blockId);
					currentBasicBlock = lastControlFlowConstruct->branchTrueCtx;
				} break;
				case TC_OP_CONDITIONAL_LOOP: {
					lastLoopConstruct = lastControlFlowConstruct = allControlFlowConstructs.data[controlFlowConstructIndex++];
					lastControlFlowConstruct->blockIndex = 0;
					ModifierContext& mod = allModifierContexts.data[currentModifierContext];
					LoopControl loopCtrl = mod.loopControl;
					U32 loopControlArgs[NUM_LOOP_CONTROLS];
					U32 numLoopControlArgs = 0;
					if (loopCtrl & LOOP_CONTROL_DEPENDENCY_LENGTH) loopControlArgs[numLoopControlArgs++] = mod.loopControlArgs[31 - lzcnt32(LOOP_CONTROL_DEPENDENCY_LENGTH)];
					if (loopCtrl & LOOP_CONTROL_MIN_ITERATIONS) loopControlArgs[numLoopControlArgs++] = mod.loopControlArgs[31 - lzcnt32(LOOP_CONTROL_MIN_ITERATIONS)];
					if (loopCtrl & LOOP_CONTROL_MAX_ITERATIONS) loopControlArgs[numLoopControlArgs++] = mod.loopControlArgs[31 - lzcnt32(LOOP_CONTROL_MAX_ITERATIONS)];
					if (loopCtrl & LOOP_CONTROL_ITERATION_MULTIPLE) loopControlArgs[numLoopControlArgs++] = mod.loopControlArgs[31 - lzcnt32(LOOP_CONTROL_ITERATION_MULTIPLE)];
					if (loopCtrl & LOOP_CONTROL_PEEL_COUNT) loopControlArgs[numLoopControlArgs++] = mod.loopControlArgs[31 - lzcnt32(LOOP_CONTROL_PEEL_COUNT)];
					if (loopCtrl & LOOP_CONTROL_PARTIAL_COUNT) loopControlArgs[numLoopControlArgs++] = mod.loopControlArgs[31 - lzcnt32(LOOP_CONTROL_PARTIAL_COUNT)];
					
					currentBasicBlock->terminationInsn = TERMINATION_INSN_BRANCH;
					lastControlFlowConstruct->loopHeaderCtx->add_context_to_phi_data(*this, currentBasicBlock->blockId);
					op_branch(procedureSpvCode, lastLoopConstruct->loopHeaderCtx->blockId);
					op_label(procedureSpvCode, lastLoopConstruct->loopHeaderCtx->blockId);
					currentBasicBlock = lastLoopConstruct->loopHeaderCtx;
					lastLoopConstruct->loopHeaderCtx->generate_loop_header_phis(*this);
					op_loop_merge(procedureSpvCode, lastLoopConstruct->mergeCtx->blockId, lastLoopConstruct->loopContinueCtx->blockId, loopCtrl, loopControlArgs, numLoopControlArgs);
					SpvId loopStartId = nextSpvId++;
					op_branch(procedureSpvCode, loopStartId);
					op_label(procedureSpvCode, loopStartId);
				} break;
				case TC_OP_BLOCK_END: {
					if (lastControlFlowConstruct->is_loop()) {
						if (lastLoopConstruct->blockIndex == 0) { // End of condition logic
							TCOp& loopOp = *lastLoopConstruct->controlFlowHeader;
							TCOp& condition = tcCode.data[loopOp.operands[0]];
							Type* evaluatedType;
							SpvId evaluatedCondition = load_val(&evaluatedType, condition);
							if (evaluatedType) {
								evaluatedCondition = try_implicit_bool_cast(&evaluatedType, evaluatedCondition);
							}
							if (evaluatedType != defaultTypeBool) {
								generation_error("Loop condition must be a bool"a, condition);
							}
							currentBasicBlock->terminationInsn = TERMINATION_INSN_BRANCH_CONDITIONAL;
							op_branch_conditional(procedureSpvCode, evaluatedCondition, lastLoopConstruct->loopBodyCtx->blockId, lastLoopConstruct->mergeCtx->blockId);
							op_label(procedureSpvCode, lastLoopConstruct->loopBodyCtx->blockId);
							lastControlFlowConstruct->mergeCtx->add_context_to_phi_data(*this, currentBasicBlock->blockId);
							currentBasicBlock = lastLoopConstruct->loopBodyCtx;
							lastControlFlowConstruct->blockIndex++;
						} else if (lastLoopConstruct->blockIndex == 1) { // End of body logic
							if (currentBasicBlock->terminationInsn == TERMINATION_INSN_NONE) {
								lastLoopConstruct->loopContinueCtx->add_context_to_phi_data(*this, currentBasicBlock->blockId);
								op_branch(procedureSpvCode, lastLoopConstruct->loopContinueCtx->blockId);
							}
							op_label(procedureSpvCode, lastLoopConstruct->loopContinueCtx->blockId);
							currentBasicBlock = lastLoopConstruct->loopContinueCtx;
							lastControlFlowConstruct->blockIndex++;
						} else if (lastLoopConstruct->blockIndex == 2) { // End of continue logic
							lastControlFlowConstruct->loopContinueCtx->generate_phis(*this);
							lastControlFlowConstruct->loopHeaderCtx->add_context_to_phi_data(*this, currentBasicBlock->blockId);
							lastControlFlowConstruct->loopHeaderCtx->patch_loop_header_phis(*this);
							op_branch(procedureSpvCode, lastLoopConstruct->loopHeaderCtx->blockId);
							op_label(procedureSpvCode, lastLoopConstruct->mergeCtx->blockId);
							lastControlFlowConstruct->mergeCtx->generate_phis(*this);
							do {
								lastLoopConstruct = lastLoopConstruct->parent;
							} while (lastLoopConstruct && !lastLoopConstruct->is_loop());
							currentBasicBlock = lastControlFlowConstruct->mergeCtx;
							lastControlFlowConstruct = lastControlFlowConstruct->parent;
						}
					} else { // branch
						if (currentBasicBlock->terminationInsn == TERMINATION_INSN_NONE) {
							lastControlFlowConstruct->mergeCtx->add_context_to_phi_data(*this, currentBasicBlock->blockId);
							op_branch(procedureSpvCode, lastControlFlowConstruct->mergeCtx->blockId);
						}
						if (lastControlFlowConstruct->blockIndex == 0) { // End of true branch
							lastControlFlowConstruct->mergeCtx->rewind_first_branch_path(*this);
							op_label(procedureSpvCode, lastControlFlowConstruct->branchFalseCtx->blockId);
							currentBasicBlock = lastControlFlowConstruct->branchFalseCtx;
							lastControlFlowConstruct->blockIndex++;
						} else if (lastControlFlowConstruct->blockIndex == 1) { // End of false branch
							op_label(procedureSpvCode, lastControlFlowConstruct->mergeCtx->blockId);
							TCOp& ifOp = *lastControlFlowConstruct->controlFlowHeader;
							TCId trueVal = ifOp.operands[1];
							TCId falseVal = ifOp.operands[2];
							if (trueVal != TC_INVALID_ID && falseVal != TC_INVALID_ID) {
								Type* typeA;
								SpvId valA = load_val(&typeA, tcCode.data[trueVal]);
								Type* typeB;
								SpvId valB = load_val(&typeB, tcCode.data[falseVal]);
								if (typeA == nullptr || typeA != typeB) {
									generation_error("If statement branches yielded values with mismatched types"a, ifOp);
								} else {
									SpvId variablesAndParents[4]{ valA, lastControlFlowConstruct->branchTrueEndCtx->blockId, valB, currentBasicBlock->blockId };
									SpvId selectedResult = op_phi(procedureSpvCode, typeA->id, nextSpvId++, variablesAndParents, 2);
									ifOp.resultValueClass = VALUE_CLASS_RUNTIME_VALUE;
									ifOp.variable = arena->zalloc<Variable>(1);
									ifOp.variable->type = typeA;
									ifOp.variable->id = selectedResult;
								}
							}
							lastControlFlowConstruct->mergeCtx->generate_phis(*this);
							currentBasicBlock = lastControlFlowConstruct->mergeCtx;
							if (lastControlFlowConstruct->mergeCtx->numIncomingEdges == 0) {
								lastControlFlowConstruct->mergeCtx->terminationInsn = TERMINATION_INSN_UNREACHABLE;
								op_unreachable(procedureSpvCode);
								//if (op + 1 != opEnd && (op + 1)->opcode != TC_OP_BLOCK_END) {
								//	generation_error("Unreachable code"a, *(op + 1));
								//}
							}
							lastControlFlowConstruct = lastControlFlowConstruct->parent;
						}
					}
				} break;
				case TC_OP_SCOPE_BEGIN: currentScope = op->operands[0]; break;
				case TC_OP_SCOPE_END: currentScope = allScopes.data[currentScope].parentIdx;  break;
				case TC_OP_MODIFIER_SCOPE_BEGIN: currentModifierContext = op->operands[0]; break;
				case TC_OP_MODIFIER_SCOPE_END: currentModifierContext = allModifierContexts.data[currentModifierContext].parentIndex; break;
				case TC_OP_TC_JMP: op = &tcCode.data[op->operands[0]]; continue;
				default: break;
				}
				op++;
			}
			if (currentBasicBlock->terminationInsn == TERMINATION_INSN_NONE) {
				if (proc.signature.returnType == defaultTypeVoid) {
					op_return(procedureSpvCode);
				} else {
					generation_error("Return statement required"a, *op);
				}
			}
			op_function_end(procedureSpvCode);
			// Remove variables so any nested procedure definitions can't access them (not handling closures)
			for (U32 scopeId : scopesToRemoveDummyVariableFrom) {
				allScopes.data[scopeId].varNameToVar.clear();
			}
		}
	}

	SpvDword* compile(U32* numCompiledDwords) {
		register_default_types();
		generate_type_checking_bytecode();
		if (compilerErrored) {
			return nullptr;
		}
		type_checking_bytecode_to_spirv();
		if (compilerErrored) {
			return nullptr;
		}

		ArenaArrayList<SpvDword> finalCode{ arena };

		// SPIR-V header
		U32 spirvMagic = 0x07230203;
		finalCode.push_back(spirvMagic);
		// Version 1.5 (bytes are 0 | major version | minor version | 0)
		U32 spirvVersionNumber = 0x00010500;
		finalCode.push_back(spirvVersionNumber);
		// Not registered or anything (DUCK should be a large enough integer that no one ever uses it though)
		// If I ever release this game, I'll see if I can register a real number
		finalCode.push_back(bswap32('DUCK'));
		// Instruction id bound
		finalCode.push_back(nextSpvId);
		// Reserved for instruction schema
		finalCode.push_back(0);

		op_capability(finalCode, CAPABILITY_SHADER);
		op_capability(finalCode, CAPABILITY_VULKAN_MEMORY_MODEL);
		if (enabledExtensions.multiview)                           op_capability(finalCode, CAPABILITY_MULTIVIEW);
		if (enabledExtensions.physicalStorageBuffer)               op_capability(finalCode, CAPABILITY_PHYSICAL_STORAGE_BUFFER_ADDRESSES);
		if (enabledExtensions.runtimeDescriptorArray)              op_capability(finalCode, CAPABILITY_RUNTIME_DESCRIPTOR_ARRAY);
		if (enabledExtensions.shaderNonUniform)                    op_capability(finalCode, CAPABILITY_SHADER_NON_UNIFORM);
		if (enabledExtensions.sampledImageArrayNonUniformIndexing) op_capability(finalCode, CAPABILITY_SAMPLED_IMAGE_ARRAY_NON_UNIFORM_INDEXING);
		if (enabledExtensions.demoteToHelperInvocation)            op_capability(finalCode, CAPABILITY_DEMOTE_TO_HELPER_INVOCATION);

		if (enabledExtensions.multiview) op_extension(finalCode, "SPV_KHR_multiview"a);

		op_ext_inst_import(finalCode, glsl450InstructionSet, "GLSL.std.450"a);

		op_memory_model(finalCode, enabledExtensions.physicalStorageBuffer ? ADDRESSING_MODEL_PHYSICAL_STORAGE_BUFFER_64 : ADDRESSING_MODEL_LOGICAL, MEMORY_MODEL_VULKAN);

		for (ShaderEntrypoint& entry : entrypoints) {
			ShaderSection& shaderSection = shaderSections.data[entry.shaderSectionIdx];
			if (shaderSection.shaderType == SHADER_TYPE_INTER_SHADER_INTERFACE) {
				continue;
			}
			SpvId* globalVarIds = arena->alloc<U32>(shaderSection.globals.size);
			for (U32 i = 0; i < shaderSection.globals.size; i++) {
				globalVarIds[i] = shaderSection.globals.data[i].var->id;
			}
			op_entry_point(finalCode, entry.executionModel, entry.id, entry.name, globalVarIds, shaderSection.globals.size);
		}

		for (ShaderEntrypoint& entry : entrypoints) {
			if (entry.executionModel == EXECUTION_MODEL_FRAGMENT) {
				op_execution_mode(finalCode, entry.id, EXECUTION_MODE_ORIGIN_UPPER_LEFT, nullptr, 0);
			} else if (entry.executionModel == EXECUTION_MODEL_GL_COMPUTE) {
				SpvDword localSize[]{ SpvDword(entry.localSizeX), SpvDword(entry.localSizeY), SpvDword(entry.localSizeZ) };
				op_execution_mode(finalCode, entry.id, EXECUTION_MODE_LOCAL_SIZE, localSize, ARRAY_COUNT(localSize));
			}
		}

		finalCode.push_back_n(decorationSpvCode.data, decorationSpvCode.size);

		for (Type* type : allTypes) {
			if (type->typeClass == TYPE_CLASS_STRUCT) {
				StructType& strct = *reinterpret_cast<StructType*>(type);
				ModifierContext& mod = allModifierContexts.data[strct.modifierCtxIdx];
				for (StructType::Member& member : strct.members) {
					op_member_decorate(finalCode, strct.type.id, member.index, DECORATION_OFFSET, &member.byteOffset, 1);
				}
				for (Decoration decor : mod.decorations) {
					switch (decor) {
					case DECORATION_BLOCK: op_decorate(finalCode, strct.type.id, DECORATION_BLOCK, nullptr, 0); break;
					default: break;
					}
				}
			} else if (type->typeClass == TYPE_CLASS_ARRAY) {
				ArrayType& arr = *reinterpret_cast<ArrayType*>(type);
				if (arr.elementType->sizeBytes) {
					U32 stride = ALIGN_HIGH(arr.elementType->sizeBytes, arr.elementType->alignment);
					op_decorate(finalCode, arr.type.id, DECORATION_ARRAY_STRIDE, &stride, 1);
				}
			} else if (type->typeClass == TYPE_CLASS_POINTER) {
				PointerType& ptr = *((PointerType*)type);
				if (ptr.storageClass == STORAGE_CLASS_PHYSICAL_STORAGE_BUFFER) {
					U32 stride = ALIGN_HIGH(ptr.pointedAt->sizeBytes, ptr.pointedAt->alignment);
					op_decorate(finalCode, ptr.type.id, DECORATION_ARRAY_STRIDE, &stride, 1);
				}
			}
		}

		U32 interfaceLocationLookup[MAX_SHADER_INTERFACE_LOCATIONS]{};
		U32 currentInterfaceLocation = 1;

		// TODO handle decorations for all variables and not just globals (important for non uniform decoration)
		for (ShaderSection& shaderSection : shaderSections) {
			for (GlobalVariable& var : shaderSection.globals) {
				if (var.var->isFromInterfaceBlock && shaderSection.shaderType != SHADER_TYPE_INTER_SHADER_INTERFACE) {
					continue;
				}
				ModifierContext& mod = allModifierContexts.data[var.modifierCtxIdx];
				bool hasLocationDecoration = false;
				for (Decoration decor : mod.decorations) {
					switch (decor) {
					case DECORATION_LOCATION: {
						if (shaderSection.shaderType == SHADER_TYPE_INTER_SHADER_INTERFACE) {
							break;
						}
						U32 decoratedLocation = mod.location;
						if (interfaceLocationLookup[mod.location] == 0 &&
							(mod.shaderExecutionModel == EXECUTION_MODEL_VERTEX && mod.storageClass == STORAGE_CLASS_OUTPUT ||
							 mod.shaderExecutionModel == EXECUTION_MODEL_FRAGMENT && mod.storageClass == STORAGE_CLASS_INPUT)) {
							// If I support matrix types, they will have to use more locations than 1
							interfaceLocationLookup[mod.location] = currentInterfaceLocation++;
							decoratedLocation = interfaceLocationLookup[mod.location] - 1;
						}
						op_decorate(finalCode, var.var->id, DECORATION_LOCATION, &decoratedLocation, 1);
						hasLocationDecoration = true;
					} break;
					case DECORATION_BUILTIN: {
						U32 builtIn = mod.builtIn;
						op_decorate(finalCode, var.var->id, DECORATION_BUILTIN, &builtIn, 1);
					} break;
					case DECORATION_DESCRIPTOR_SET: op_decorate(finalCode, var.var->id, DECORATION_DESCRIPTOR_SET, &mod.descriptorSet, 1); break;
					case DECORATION_BINDING: op_decorate(finalCode, var.var->id, DECORATION_BINDING, &mod.binding, 1); break;
					case DECORATION_ALIGNMENT: {
						if (var.var->type->typeClass == TYPE_CLASS_POINTER) {
							op_decorate(finalCode, var.var->id, DECORATION_ALIGNMENT, &mod.alignmentDecoration, 1);
						}
					} break;
					case DECORATION_NON_UNIFORM:
					case DECORATION_RESTRICT:
					case DECORATION_NON_WRITABLE:
					case DECORATION_NON_READABLE:
					case DECORATION_FLAT: {
						op_decorate(finalCode, var.var->id, decor, nullptr, 0);
					} break;
					default: break;
					}
				}
			}
		}
		for (ArenaArrayList<Variable*>& group : shaderInterfaceLocationGroups) {
			U32 decoratedLocation = currentInterfaceLocation++ - 1;
			for (Variable* var : group) {
				op_decorate(finalCode, var->id, DECORATION_LOCATION, &decoratedLocation, 1);
			}
		}

		// Type declarations, constants, global variables here
		for (Type* type : allTypes) {
			emit_type_definition(finalCode, type);
		}
		SpvId addCarryTypeMembers[2]{ defaultTypeU32->id, defaultTypeU32->id };
		op_type_struct(finalCode, addCarryStructTypeId, addCarryTypeMembers, 2);
		for (U32 i = 0; i < procedureTypeToSpvId.capacity; i++) {
			ProcedureType& key = procedureTypeToSpvId.keys[i];
			if (key == procedureTypeToSpvId.emptyKey) {
				continue;
			}
			op_type_function(finalCode, procedureTypeToSpvId.values[i], key.returnType->id, key.identifier.argTypes, key.identifier.numArgs);
		}
		for (U32 i = 0; i < savedConstants.capacity; i++) {
			Token constant = savedConstants.keys[i];
			if (constant == savedConstants.emptyKey || constant.type == TOKEN_IDENTIFIER) {
				continue;
			}
			TCOp& constantOp = tcCode.data[savedConstants.values[i]];
			switch (constant.type) {
			case TOKEN_STRING: generation_error("String constants not yet supported"a, constantOp); break;
			case TOKEN_BOOL: constant.vBool ? op_constant_true(finalCode, defaultTypeBool->id, constantOp.variable->id) : op_constant_false(finalCode, defaultTypeBool->id, constantOp.variable->id); break;
			case TOKEN_I8: generation_error("I8 constants not yet supported"a, constantOp); break;
			case TOKEN_I16: generation_error("I16 constants not yet supported"a, constantOp); break;
			case TOKEN_I32: op_constant(finalCode, defaultTypeI32->id, constantOp.variable->id, U32(constant.vI32)); break;
			case TOKEN_I64: generation_error("I64 constants not yet supported"a, constantOp); break;
			case TOKEN_U8: generation_error("U8 constants not yet supported"a, constantOp); break;
			case TOKEN_U16: generation_error("U16 constants not yet supported"a, constantOp); break;
			case TOKEN_U32: op_constant(finalCode, defaultTypeU32->id, constantOp.variable->id, constant.vU32); break;
			case TOKEN_U64: generation_error("U64 constants not yet supported"a, constantOp); break;
			case TOKEN_F16: generation_error("F16 constants not yet supported"a, constantOp); break;
			case TOKEN_F32: op_constant(finalCode, defaultTypeF32->id, constantOp.variable->id, bitcast<U32>(constant.vF32)); break;
			case TOKEN_F64: generation_error("F64 constants not yet supported"a, constantOp); break;
			default: generation_error("Supposed literal did not have literal type"a, constantOp); break;
			}
		}
		for (ShaderSection& section : shaderSections) {
			if (section.shaderType == SHADER_TYPE_INTER_SHADER_INTERFACE) {
				continue;
			}
			for (GlobalVariable& var : section.globals) {
				if (var.initializer != SPV_NULL_ID) {
					op_variable(finalCode, var.var->type->id, var.var->id, reinterpret_cast<PointerType*>(var.var->type)->storageClass, var.initializer);
				} else {
					op_variable(finalCode, var.var->type->id, var.var->id, reinterpret_cast<PointerType*>(var.var->type)->storageClass);
				}
			}
		}

		if (compilerErrored) {
			return nullptr;
		}

		finalCode.push_back_n(procedureSpvCode.data, procedureSpvCode.size);
		*numCompiledDwords = finalCode.size;
		return finalCode.data;
	}
};


SpvDword* compile_dsl(U32* numCompiledDwords, const char* src, U32 srcSize) {
	MemoryArena& arena = get_scratch_arena();
	SpvDword* result = nullptr;
	MEMORY_ARENA_FRAME(arena) {
		DSLCompiler compiler{};
		compiler.init(arena, src, srcSize);
		result = compiler.compile(numCompiledDwords);
	}
	return result;
}
SpvDword* compile_dsl(U32* numCompiledDwords, StrA src) {
	return compile_dsl(numCompiledDwords, src.str, U32(src.length));
}

bool compile_dsl_from_file_to_file(StrA src, StrA dst) {
	MemoryArena& arena = get_scratch_arena();
	bool success = true;
	MEMORY_ARENA_FRAME(arena) {
		U32 srcSize;
		char* srcFile = read_full_file_to_arena<char>(&srcSize, arena, src);
		U32 compiledDwords;
		U32* compiled;
		if (!srcFile) {
			goto fail;
		}
		compiled = compile_dsl(&compiledDwords, srcFile, srcSize);
		if (!compiled) {
			goto fail;
		}
		if (!write_data_to_file(dst, compiled, compiledDwords * sizeof(SpvDword))) {
			goto fail;
		}
		goto success;
	fail:;
		success = false;
	success:;
	}
	return success;
}

}
