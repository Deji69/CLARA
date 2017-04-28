#include "stdafx.h"
#include <cctype>
#include <vector>
#include <fstream>
#include <algorithm>
#include <functional>
#include "API.h"
#include "CLARA.h"
#include "Compiler.h"
#include "Types.h"

#define MAX_BUFFER 64

CLARA_NAMESPACE_BEGIN

std::ifstream g_hInput;
std::ifstream g_hOutput;
int g_nNumInstructionsRead;
int g_nNumOpcodesWritten;
char g_Buffer[MAX_BUFFER];
char* g_BufferCurrent = g_Buffer;
const char* g_BufferEnd = &g_Buffer[MAX_BUFFER];

const ImmediateType gImm8 = Imm8;
const ImmediateType gImm16 = Imm16;
const ImmediateType gImm32 = Imm32;
const ImmediateType gLocal8 = Local8;
const ImmediateType gLocal16 = Local16;
const ImmediateType gLocal32 = Local32;
const ImmediateType gGlobal16 = Global16;
const ImmediateType gGlobal32 = Global32;
const ImmediateType gString32 = String32;

// Information on instructions including their parameter types and default values
// {name, minNumberOfParams, {[paramType1, paramType2...]}, {[defaultValue1, defaultValue2...]}}
const InstructionType g_Instructions[] = {
	{"nop"},
	{"break"},
	{"throw", 1,{&gImm8}},
	{"pushn"},
	{"pushb", 1,{&gImm8}},
	{"pushw", 1,{&gImm16}},
	{"pushd", 1,{&gImm32}},
	{"pushf", 1,{&gImm32}},
	{"pushab"},{"pushaw"},{"pushad"},{"pushaf"},
	{"pushs", 1,{&gImm32}},
	{"pop", 0,{&gImm8},{"1"}},
	{"popln", 1,{&gLocal8}},
	{"popl", 1,{&gGlobal16}},
	{"pople", 1,{&gGlobal32}},
	{"popv", 1,{&gGlobal16}},
	{"popve", 1,{&gGlobal32}},
	{"swap"},{"dup"},
	{"dupe", 1,{&gImm8}},
	{"local"},{"global"},
	{"array", 1,{&gImm8}},
	{"exf"},{"inc"},{"dec"},
	{"add"},{"sub"},{"mul"},{"div"},
	{"mod"},{"and"},{"or"},{"xor"},
	{"shl"},{"shr"},{"neg"},{"not"},
	{"toi"},{"tof"},
	{"cmpnn"},{"cmpe"},{"cmpne"},
	{"cmpge"},{"cmple"},{"cmpg"},{"cmpl"},
	{"if"},
	{"eval", 1,{&gImm8}},
	{"jt", 1,{&gImm32}},
	{"jnt", 1,{&gImm32}},
	{"jmp"},
	{"jmpa", 1,{&gImm32}},
	{"sw", 2,{&gImm16, &gImm32}},
	{"rsw", 2},
	{"call"},
	{"calla", 1,{&gImm32}},
	{"enter", 1,{&gImm8}},
	{"ret"}
};

// Map of the instruction mnemonic IDs
const std::map<std::string, CLARA_MNEMONIC> g_Mnemonics = {
	{"nop", CLARA_NOP},
	{"break", CLARA_BREAK},
	{"throw", CLARA_THROW},
	{"push", CLARA_PUSH},
	{"pushab", CLARA_PUSHAB},
	{"pushaw", CLARA_PUSHAW},
	{"pushad", CLARA_PUSHAD},
	{"pushaf", CLARA_PUSHAF},
	{"pop", CLARA_POP},
	{"swap", CLARA_SWAP},
	{"dup", CLARA_DUP},

	{"loc", CLARA_LOCAL},
	{"local", CLARA_LOCAL},
	{"glob", CLARA_GLOBAL},
	{"global", CLARA_GLOBAL},
	{"arr", CLARA_ARRAY},
	{"array", CLARA_ARRAY},

	{"exf", CLARA_EXF},
	{"inc", CLARA_INC},
	{"dec", CLARA_DEC},
	{"add", CLARA_ADD},
	{"sub", CLARA_SUB},
	{"mul", CLARA_MUL},
	{"div", CLARA_DIV},
	{"mod", CLARA_MOD},
	{"and", CLARA_AND},
	{"or", CLARA_OR},
	{"xor", CLARA_XOR},
	{"shl", CLARA_SHL},
	{"shr", CLARA_SHR},
	{"neg", CLARA_NEG},
	{"not", CLARA_NOT},
	{"toi", CLARA_TOI},
	{"tof", CLARA_TOF},

	{"cmpnn", CLARA_CMPNN},
	{"cmpe", CLARA_CMPE},
	{"cmpne", CLARA_CMPNE},
	{"cmpge", CLARA_CMPGE},
	{"cmple", CLARA_CMPLE},
	{"cmpg", CLARA_CMPG},
	{"cmpl", CLARA_CMPL},

	{"if", CLARA_IF},
	{"eval", CLARA_EVAL},
	{"jt", CLARA_JT},
	{"jnz", CLARA_JT},
	{"jnt", CLARA_JNT},
	{"jz", CLARA_JNT},
	{"jmp", CLARA_JMP},
	{"jump", CLARA_JMP},
	{"goto", CLARA_JMP},
	{"switch", CLARA_SWITCH},
	{"sw", CLARA_SWITCH},
	{"rswitch", CLARA_RSWITCH},
	{"rsw", CLARA_RSWITCH},

	{"call", CLARA_CALL},
	{"enter", CLARA_ENTER},
	{"ret", CLARA_RET},
	{"return", CLARA_RET}
};

// Mnemonics that can be used in the process of evaluating other instructions
const std::map<CLARA_INSTRUCTION, CLARA_MNEMONIC> g_Friends = {
	{INSN_PUSHAB, CLARA_PUSH},
	{INSN_PUSHAW, CLARA_PUSH},
	{INSN_PUSHAD, CLARA_PUSH},
	{INSN_PUSHAF, CLARA_PUSH},
	{INSN_SWAP, CLARA_PUSH},
	{INSN_LOCAL, CLARA_PUSH},
	{INSN_GLOBAL, CLARA_PUSH},
	{INSN_ARRAY, CLARA_PUSH},
	{INSN_INC, CLARA_PUSH},
	{INSN_DEC, CLARA_PUSH},
	{INSN_ADD, CLARA_PUSH},
	{INSN_SUB, CLARA_PUSH},
	{INSN_MUL, CLARA_PUSH},
	{INSN_DIV, CLARA_PUSH},
	{INSN_MOD, CLARA_PUSH},
	{INSN_AND, CLARA_PUSH},
	{INSN_OR, CLARA_PUSH},
	{INSN_XOR, CLARA_PUSH},
	{INSN_SHL, CLARA_PUSH},
	{INSN_SHR, CLARA_PUSH},
	{INSN_NEG, CLARA_PUSH},
	{INSN_NOT, CLARA_PUSH},
	{INSN_TOI, CLARA_PUSH},
	{INSN_OR, CLARA_PUSH},
	{INSN_CMPNN, CLARA_PUSH},
	{INSN_CMPE, CLARA_PUSH},
	{INSN_CMPNE, CLARA_PUSH},
	{INSN_CMPGE, CLARA_PUSH},
	{INSN_CMPLE, CLARA_PUSH},
	{INSN_CMPG, CLARA_PUSH},
	{INSN_CMPL, CLARA_PUSH},
	{INSN_JMP, CLARA_PUSH},
	{INSN_CALL, CLARA_PUSH},
};

// A mnemonic vector
const std::vector<MnemonicInstruction> g_MnemonicVec = {
	{CLARA_NOP,{INSN_NOP}},
	{CLARA_BREAK,{INSN_BREAK}},
	{CLARA_THROW,{INSN_THROW}},
	{CLARA_PUSH,{INSN_PUSHN, INSN_PUSHB, INSN_PUSHW, INSN_PUSHD, INSN_PUSHF, INSN_PUSHS}},
	{CLARA_PUSH,{INSN_PUSHN, INSN_PUSHB, INSN_PUSHW, INSN_PUSHD, INSN_PUSHF, INSN_PUSHS}},
	{CLARA_PUSH,{INSN_PUSHN, INSN_PUSHB, INSN_PUSHW, INSN_PUSHD, INSN_PUSHF, INSN_PUSHS}},
	{CLARA_PUSH,{INSN_PUSHN, INSN_PUSHB, INSN_PUSHW, INSN_PUSHD, INSN_PUSHF, INSN_PUSHS}},
	{CLARA_PUSH,{INSN_PUSHN, INSN_PUSHB, INSN_PUSHW, INSN_PUSHD, INSN_PUSHF, INSN_PUSHS}},
	{CLARA_PUSH,{INSN_PUSHN, INSN_PUSHB, INSN_PUSHW, INSN_PUSHD, INSN_PUSHF, INSN_PUSHS}},
	{CLARA_PUSHAB,{INSN_PUSHAB}},
	{CLARA_PUSHAW,{INSN_PUSHAW}},
	{CLARA_PUSHAD,{INSN_PUSHAD}},
	{CLARA_PUSHAF,{INSN_PUSHAF}},
	{CLARA_POP,{INSN_POP, INSN_POPL, INSN_POPLE, INSN_POPV, INSN_POPVE}},
	{CLARA_POP,{INSN_POP, INSN_POPL, INSN_POPLE, INSN_POPV, INSN_POPVE}},
	{CLARA_POP,{INSN_POP, INSN_POPL, INSN_POPLE, INSN_POPV, INSN_POPVE}},
	{CLARA_POP,{INSN_POP, INSN_POPL, INSN_POPLE, INSN_POPV, INSN_POPVE}},
	{CLARA_POP,{INSN_POP, INSN_POPL, INSN_POPLE, INSN_POPV, INSN_POPVE}},
	{CLARA_SWAP,{INSN_SWAP}},
	{CLARA_DUP,{INSN_DUP, INSN_DUPE}},
	{CLARA_DUP,{INSN_DUP, INSN_DUPE}},

	{CLARA_LOCAL,{INSN_LOCAL}},
	{CLARA_GLOBAL,{INSN_GLOBAL}},
	{CLARA_ARRAY,{INSN_ARRAY}},

	{CLARA_EXF,{INSN_EXF}},
	{CLARA_INC,{INSN_INC}},
	{CLARA_DEC,{INSN_DEC}},
	{CLARA_ADD,{INSN_ADD}},
	{CLARA_SUB,{INSN_SUB}},
	{CLARA_MUL,{INSN_MUL}},
	{CLARA_DIV,{INSN_DIV}},
	{CLARA_MOD,{INSN_MOD}},
	{CLARA_AND,{INSN_AND}},
	{CLARA_OR,{INSN_OR}},
	{CLARA_XOR,{INSN_XOR}},
	{CLARA_SHL,{INSN_SHL}},
	{CLARA_SHR,{INSN_SHR}},
	{CLARA_NEG,{INSN_NEG}},
	{CLARA_NOT,{INSN_NOT}},
	{CLARA_TOI,{INSN_TOI}},
	{CLARA_TOF,{INSN_TOF}},

	{CLARA_CMPNN,{INSN_CMPNN}},
	{CLARA_CMPE,{INSN_CMPE}},
	{CLARA_CMPNE,{INSN_CMPNE}},
	{CLARA_CMPGE,{INSN_CMPGE}},
	{CLARA_CMPLE,{INSN_CMPLE}},
	{CLARA_CMPG,{INSN_CMPG}},
	{CLARA_CMPL,{INSN_CMPL}},

	{CLARA_IF,{INSN_IF}},
	{CLARA_EVAL,{INSN_EVAL}},
	{CLARA_JT,{INSN_JT}},
	{CLARA_JNT,{INSN_JNT}},
	{CLARA_JMP,{INSN_JMP, INSN_JMPA}},
	{CLARA_JMP,{INSN_JMP, INSN_JMPA}},
	{CLARA_SWITCH,{INSN_SWITCH}},
	{CLARA_RSWITCH,{INSN_RSWITCH}},

	{CLARA_CALL,{INSN_CALL, INSN_CALLA}},
	{CLARA_CALL,{INSN_CALL, INSN_CALLA}},
	{CLARA_ENTER,{INSN_ENTER}},
	{CLARA_RET,{INSN_RET}}
};

// Another mnemonic vector
const std::vector<MnemonicInstruction> g_MnemonicVec2 = {
	{CLARA_NOP,{INSN_NOP}},
	{CLARA_BREAK,{INSN_BREAK}},
	{CLARA_THROW,{INSN_THROW}},
	{CLARA_PUSH,{INSN_PUSHN, INSN_PUSHB, INSN_PUSHW, INSN_PUSHD, INSN_PUSHF, INSN_PUSHS}},
	{CLARA_PUSHAB,{INSN_PUSHAB}},
	{CLARA_PUSHAW,{INSN_PUSHAW}},
	{CLARA_PUSHAD,{INSN_PUSHAD}},
	{CLARA_PUSHAF,{INSN_PUSHAF}},
	{CLARA_POP,{INSN_POP, INSN_POPL, INSN_POPLE, INSN_POPV, INSN_POPVE}},
	{CLARA_SWAP,{INSN_SWAP}},
	{CLARA_DUP,{INSN_DUP, INSN_DUPE}},

	{CLARA_LOCAL,{INSN_LOCAL}},
	{CLARA_GLOBAL,{INSN_GLOBAL}},
	{CLARA_ARRAY,{INSN_ARRAY}},

	{CLARA_EXF,{INSN_EXF}},
	{CLARA_INC,{INSN_INC}},
	{CLARA_DEC,{INSN_DEC}},
	{CLARA_ADD,{INSN_ADD}},
	{CLARA_SUB,{INSN_SUB}},
	{CLARA_MUL,{INSN_MUL}},
	{CLARA_DIV,{INSN_DIV}},
	{CLARA_MOD,{INSN_MOD}},
	{CLARA_AND,{INSN_AND}},
	{CLARA_OR,{INSN_OR}},
	{CLARA_XOR,{INSN_XOR}},
	{CLARA_SHL,{INSN_SHL}},
	{CLARA_SHR,{INSN_SHR}},
	{CLARA_NEG,{INSN_NEG}},
	{CLARA_NOT,{INSN_NOT}},
	{CLARA_TOI,{INSN_TOI}},
	{CLARA_TOF,{INSN_TOF}},

	{CLARA_CMPNN,{INSN_CMPNN}},
	{CLARA_CMPE,{INSN_CMPE}},
	{CLARA_CMPNE,{INSN_CMPNE}},
	{CLARA_CMPGE,{INSN_CMPGE}},
	{CLARA_CMPLE,{INSN_CMPLE}},
	{CLARA_CMPG,{INSN_CMPG}},
	{CLARA_CMPL,{INSN_CMPL}},

	{CLARA_IF,{INSN_IF}},
	{CLARA_EVAL,{INSN_EVAL}},
	{CLARA_JT,{INSN_JT}},
	{CLARA_JNT,{INSN_JNT}},
	{CLARA_JMP,{INSN_JMP, INSN_JMPA}},
	{CLARA_SWITCH,{INSN_SWITCH}},
	{CLARA_RSWITCH,{INSN_RSWITCH}},

	{CLARA_CALL,{INSN_CALL, INSN_CALLA}},
	{CLARA_ENTER,{INSN_ENTER}},
	{CLARA_RET,{INSN_RET}}
};


bool(*g_Output)(const char*) = [](const char * msg) {
	return true;
};
bool(*g_Error)(CLARA_ERROR, const char*) = [](CLARA_ERROR, const char * msg) {
	return true;
};

bool Error(CLARA_ERROR err, std::string msg) {
	return g_Error(err, msg.c_str());
}
bool Error(CLARA_ERROR err, const std::vector<std::string>& args) {
	switch (err) {
	case CLARA_ERROR_OPEN_FILE:
		Error(err, "failed to open file '" + args[0] + "'");
		break;
	case CLARA_ERROR_INVALID_DIRECTIVE:
		Error(err, "invalid directive '" + args[0] + "'");
		break;
	}

	return Error(err, "unknown");
}
bool Output(std::string msg) {
	return g_Output(msg.c_str());
}

template<typename TArg>
inline bool SendError(CLARA_ERROR err, std::vector<std::string>& vec, TArg arg) {
	vec.emplace_back(arg);
	return Error(err, vec);
}
template<typename TArg, typename... TArgs>
inline bool SendError(CLARA_ERROR err, std::vector<std::string>& vec, TArg arg, TArgs&&... args) {
	vec.emplace_back(arg);
	return SendError(err, vec, args...);
}
template<typename... TArgs>
inline bool SendError(CLARA_ERROR err, TArgs&&... args) {
	std::vector<std::string> vec;
	return SendError(err, vec, args...);
}

CLARA_INSTRUCTION ReadInstruction() {
	CLARA_INSTRUCTION op = CLARA_INSTRUCTION::INSN_ADD;
	//g_hInput.r >> op;
	return op;
}

const MnemonicInstruction* GetMnemonicInstruction(CLARA_MNEMONIC mn) {
	return &g_MnemonicVec2[mn];
}
const MnemonicInstruction* GetMnemonicInstruction(std::string mn) {
	auto it = g_Mnemonics.find(mn);
	return it != g_Mnemonics.end() ? &g_MnemonicVec2[it->second] : nullptr;
}

void WriteOpcode(CLARA_OPCODE opcode) {
	//fwrite(&opcode, 1, 1, g_hOutput);
	++g_nNumOpcodesWritten;
}

bool Compiler::Digest(std::string code) {
	if (code.empty()) return false;

	if (code[0] == ',') {
		if (m_phase == phase3) {
			Instruction insn = m_instructions.back();
			insn.Reset();
			m_instructions.emplace_back(insn);
			m_phase = phase2;
		}
		else if (m_phase == phase1)
			m_phase = phase3;
		else BREAK();
		code = code.substr(1);
	}
	else {
		size_t commaPos = code.find_first_of(',');
		if (commaPos != code.npos) {
			if (Digest(code.substr(0, commaPos))) {
				Digest();
			}
			auto str = code.substr(commaPos + 1, code.npos);
			if (!str.empty())
				return Digest(code.substr(commaPos, code.npos));
			return m_phase == phase2;
		}
	}

	if (code.size()) {
		if (m_phase == phase2) {
			Instruction insn(code);
			if (insn.OK()) {
				m_instructions.back().Finalize();
				m_phase = phase1;
			}
		}
		if (m_phase == phase1 || m_phase == phase3) {
			Instruction insn(code);
			if (!insn.OK()) {
				if (m_phase == phase3) {
					auto instr = m_instructions.back();
					m_instructions.emplace_back(instr);
					m_instructions.back().Reset();
					m_phase = phase2;
				}
				else SendError(CLARA_ERROR_INVALID_MNEMONIC, code);
			}
			else {
				m_instructions.emplace_back(insn);
				auto& insn = m_instructions.back();
				if (!insn.IsComplete()) {
					m_phase = phase2;
					return true;
				}
				else {
					if (!insn.Finalize()) BREAK();
				}
			}
		}
		if (m_phase == phase2) {
			Instruction* insn = &m_instructions.back();
			if (!insn->IsComplete()) {
				Parameter param(code);
				if (insn->GetMaxNumParams() <= insn->GetNumParams()) {
					if (insn->Finalize()) {
						Instruction instr(insn->GetFriendInstruction());
						m_instructions.erase(m_instructions.end() - 1);
						m_heldInstructions.emplace_back(*insn);
						m_instructions.emplace_back(instr);
						insn = &m_instructions.back();
					}
				}

				insn->AddParam(param);
				if (insn->IsComplete()) {
					if (insn->GetMaxNumParams() > insn->GetNumParams())
						m_phase = phase3;
					else {
						if (!insn->Finalize()) BREAK();
						m_phase = phase3;
					}
				}
			}
			else BREAK();
		}
	}
	return m_phase == phase2;
}
bool Compiler::Digest() {
	if (m_phase == phase2) {
		Instruction& insn = m_instructions.back();
		if (!insn.Finalize())
			m_phase = phase2;
		else
			m_phase = phase3;
	}
	return m_phase == phase2;
}

size_t GetIntNumBytes(int32_t nVal) {
	int v = abs(nVal);
	if (v < 0x80) return 1;
	if (v < 0x8000) return 2;
	return 4;
}
size_t GetUIntNumBytes(uint32_t dwVal) {
	if (dwVal <= 0xFF) return 1;
	if (dwVal <= 0xFFFF) return 2;
	return 4;
}

#ifdef __cplusplus
extern "C" {
#endif
	CLARA_ERROR SetOutputHandler(bool(*func)(const char*)) {
		g_Output = func;
		return CLARA_ERROR_NONE;
	}
	CLARA_ERROR SetErrorHandler(bool(*func)(CLARA_ERROR, const char*)) {
		g_Error = func;
		return CLARA_ERROR_NONE;
	}
	CLARA_ERROR Compile(const char * path_in, const char * path_out) {
		if (!Output((std::string("Opening file ") + path_in).c_str()))
			return CLARA_ERROR_INTERRUPTED;

		std::ifstream in(path_in, std::ifstream::in);
		if (!in.is_open()) {
			SendError(CLARA_ERROR_OPEN_FILE, path_in);
		}
		else {
			std::ofstream out(path_out, std::ofstream::out | std::ofstream::binary);
			if (out.is_open()) {
				Compiler compiler;

				g_nNumOpcodesWritten = 0;
				g_nNumInstructionsRead = 0;

				int lineNum = 0;
				for (std::string line; std::getline(in, line); ++lineNum) {
					line = line.substr(0, line.find_first_of(';'));
					if (line.empty()) continue;
					compiler.Parse(line);
				}

				compiler.Compile(out);
			}
		}

		return CLARA_ERROR_NONE;
	}
#ifdef __cplusplus
}
#endif

CLARA_NAMESPACE_END