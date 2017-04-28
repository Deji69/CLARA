#pragma once
#include <vector>
#include <sstream>
#include <memory>
#include "CLARA.h"
#include "Types.h"

CLARA_NAMESPACE_BEGIN

struct InstructionType {
	const char * name;
	unsigned minParams = 0;
	std::vector<const ImmediateType*> params;
	std::vector<std::string> defaults;

	InstructionType(const char* nm) : name(nm) { }
	InstructionType(const char* nm, unsigned mp) : name(nm), minParams(mp) { }
	InstructionType(const char* nm, unsigned mp, std::vector<const ImmediateType*> vec) : name(nm), minParams(mp), params(vec) { }
	InstructionType(const char* nm, unsigned mp, std::vector<const ImmediateType*> vec, std::vector<std::string> defs) : name(nm), minParams(mp), params(vec), defaults(defs) { }
};

typedef std::pair<CLARA_MNEMONIC, std::vector<CLARA_INSTRUCTION>> MnemonicInstruction;

extern const InstructionType g_Instructions[MAX_INSN];
extern const std::map<std::string, CLARA_MNEMONIC> g_Mnemonics;
extern const std::vector<MnemonicInstruction> g_MnemonicVec;
extern const std::vector<MnemonicInstruction> g_MnemonicVec2;
extern const std::map<CLARA_INSTRUCTION, CLARA_MNEMONIC> g_Friends;

const MnemonicInstruction* GetMnemonicInstruction(std::string);
const MnemonicInstruction* GetMnemonicInstruction(CLARA_MNEMONIC);
size_t GetIntNumBytes(int32_t);
size_t GetUIntNumBytes(uint32_t);

enum ParamType {
	PT_NULL,
	PT_DWORD, PT_WORD, PT_BYTE,
	PT_INT, PT_SHORT, PT_CHAR,
	PT_FLOAT, PT_STRING
};
enum OperandType {
	OP_INVALID, OP_IMMEDIATE, OP_INSTRUCTION,
	OP_VARIABLE,
};

class Mnemonic {
	CLARA_MNEMONIC m_mnemonic;

public:
	Mnemonic(std::string mnemonic) {
		auto it = g_Mnemonics.find(mnemonic);
		m_mnemonic = it == g_Mnemonics.end() ? CLARA_BAD_MNEMONIC : it->second;
	}
	Mnemonic(CLARA_MNEMONIC mn) : m_mnemonic(mn) { }

	inline operator CLARA_MNEMONIC() const { return m_mnemonic; }
};

class Parameter {
	std::string m_str;
	ParamType m_type;
	union {
		uint32_t m_dwValue;
		uint16_t m_wValue;
		uint8_t m_bValue;
		int32_t m_nValue;
		int16_t m_sValue;
		int8_t m_cValue;
		float m_fValue;
	};

	void DetermineValue() {
		if (!m_str.empty()) {
			if (m_str.find('.') != m_str.npos) {
				m_fValue = std::stof(m_str);
				m_type = PT_FLOAT;
			}
			else {
				bool sign = m_str.find('-') != m_str.npos;
				if (sign)
					m_nValue = std::stol(m_str, 0, 0);
				else
					m_dwValue = std::stoul(m_str, 0, 0);

				if (sign) {
					if (m_cValue == m_nValue) m_type = PT_CHAR;
					else if (m_sValue == m_nValue) m_type = PT_SHORT;
					else m_type = PT_INT;
				}
				else {
					if (m_cValue == m_dwValue) m_type = PT_BYTE;
					else if (m_cValue == m_dwValue) m_type = PT_WORD;
					else m_type = PT_DWORD;
				}
			}
		}
	}

public:
	Parameter(std::string str) : m_str(str) {
		DetermineValue();
	}
	template<typename T>
	Parameter(T str) : m_str(std::to_string(str)) {
		DetermineValue();
	}
	inline bool operator==(const ValueType& vt) {
		switch (vt.GetImmType()) {
		case Imm8: return m_type == PT_BYTE || m_type == PT_CHAR;
		case Imm16: return m_type == PT_WORD || m_type == PT_SHORT;
		case Imm32: return m_type == PT_DWORD || m_type == PT_INT || m_type == PT_FLOAT;
		case Local8: return m_type == PT_BYTE;
		case Local16: return m_type == PT_WORD;
		case Local32: return m_type == PT_DWORD;
		case Global16: return m_type == PT_WORD;
		case Global32: return m_type == PT_DWORD;
		case String32: return m_type == PT_STRING;
		}
		return false;
	}
	inline bool operator!=(const ValueType& vt) {
		return !(*this == vt);
	}
};

class Instruction {
	const MnemonicInstruction* m_mninsn = nullptr;
	CLARA_INSTRUCTION m_insn = INSN_INVALID;
	std::vector<Parameter> m_params;

	void Init() {
		if (m_mninsn && m_mninsn->second.size() == 1)
			m_insn = m_mninsn->second.front();
	}

public:
	Instruction(std::string mn) : m_mninsn(GetMnemonicInstruction(mn)), m_insn(INSN_INVALID) {
		Init();
	}
	Instruction(CLARA_INSTRUCTION insn) : m_mninsn(&g_MnemonicVec[insn]), m_insn(insn) {
		Init();
	}
	Instruction(CLARA_MNEMONIC mn) : m_mninsn(&g_MnemonicVec2[mn]), m_insn(INSN_INVALID) {
		Init();
	}
	Instruction(const Instruction& instr) : m_mninsn(instr.m_mninsn), m_insn(instr.m_insn), m_params(instr.m_params) {
		Init();
	}

	void AddParam(const Parameter& param) {
		m_params.emplace_back(param);
		Finalize();
	}

	void Reset() {
		m_insn = INSN_INVALID;
		m_params.clear();
	}

	CLARA_MNEMONIC GetFriendInstruction() {
		if (m_insn == INSN_INVALID) return CLARA_BAD_MNEMONIC;
		auto it = g_Friends.find(m_insn);
		return it != g_Friends.end() ? it->second : CLARA_BAD_MNEMONIC;
	}

	bool Finalize() {
		if (m_insn != -1)
			return true;

		if (GetNumParams() > GetMaxNumParams())
			return false;

		std::vector<CLARA_INSTRUCTION> insns;
		for (auto& r : m_mninsn->second) {
			auto& instr = g_Instructions[r];
			if (instr.minParams > GetNumParams())
				continue;
			else if (instr.params.size() < GetNumParams())
				continue;
			else if (instr.params.size() > GetNumParams()) {
				if (instr.defaults.size() == (instr.params.size() - GetNumParams())) {
					for (auto& def : instr.defaults) {
						Parameter param(def);
						m_params.emplace_back(param);
					}
				}
			}

			bool nogood = false;
			size_t i = 0;
			auto& args = instr.params;
			for (auto& param : m_params) {
				/*if (param != *args[i]) {
				nogood = true;
				break;
				}*/

				++i;
			}

			if (!nogood) insns.emplace_back(r);
		}

		if (!insns.empty()) {
			if (insns.size() > 1)
				return false;

			m_insn = insns.front();
			return true;
		}
		return false;
	}

	bool OK() const {
		return m_mninsn != nullptr;
	}
	bool IsComplete() const {
		return m_insn != INSN_INVALID && m_params.size() >= g_Instructions[m_insn].minParams;
	}
	size_t GetNumParams() const {
		return m_params.size();
	}
	size_t GetMaxNumParams() const {
		size_t max = 0;
		for (auto& r : m_mninsn->second) {
			if (g_Instructions[r].params.size() > max)
				max = g_Instructions[r].params.size();
		}
		return max;
	}
};

class BaseOperand {
	OperandType m_opType;

public:
	BaseOperand(OperandType type) : m_opType(type) { }
	virtual ~BaseOperand() { }

	inline OperandType GetType() const {
		return m_opType;
	}

	virtual size_t GetSize() const = 0;
};

template<typename T>
class Imm : public BaseOperand {
	T m_value;

public:
	template<typename TV>
	Imm(TV val) : BaseOperand(OP_IMMEDIATE), m_value(static_cast<T>(val)) { }

	template<typename Ty>
	inline bool IsType() const {
		return typeid(Ty) == typeid(T);
	}
	template<typename Ty>
	inline Ty GetValue() const {
		assert(IsType<Ty>());
		return m_value;
	}

	virtual size_t GetSize() const override {
		return sizeof(T);
	}
};

class Ins : public BaseOperand {
	CLARA_INSTRUCTION m_instruction = INSN_INVALID;
	CLARA_MNEMONIC m_mnemonic = CLARA_BAD_MNEMONIC;

public:
	Ins() : BaseOperand(OP_INSTRUCTION) { }
	Ins(CLARA_MNEMONIC mn) : BaseOperand(OP_INSTRUCTION), m_mnemonic(mn) { }

	inline CLARA_MNEMONIC GetMnemonic() const { return m_mnemonic; }
	inline CLARA_INSTRUCTION GetInstruction() const { return m_instruction; }

	virtual size_t GetSize() const override {
		return sizeof(m_instruction);
	}
};

class Operand {
	std::shared_ptr<BaseOperand> m_op;

public:
	Operand() { }
	Operand(BaseOperand* op) : m_op(op) { }
	template<typename T>
	Operand(std::shared_ptr<T> op) : m_op(op) { }

	template<typename T>
	Operand& operator=(std::shared_ptr<T> op) {
		m_op = op;
		return *this;
	}

	template<typename Ty>
	std::shared_ptr<Ty> Get() const {
		return std::static_pointer_cast<Ty>(m_op);
	}
	std::shared_ptr<BaseOperand> GetBase() const {
		return m_op;
	}

	inline bool Empty() const { return m_op.get() == nullptr; }
	inline operator bool() const { return !Empty(); }
};

CLARA_NAMESPACE_END