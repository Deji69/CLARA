#pragma once
#include <vector>
#include <memory>
#include <fstream>
#include "CLARA.h"
#include "Assembly.h"
#include "Parser.h"

CLARA_NAMESPACE_BEGIN

class Compiler {
	enum { phase1, phase2, phase3 };
	int m_phase = phase1;

	std::vector<Instruction> m_instructions;
	std::vector<Instruction> m_heldInstructions;
	std::vector<std::vector<Operand>> m_lines;

	inline void CompileInstruction(std::ofstream& file, std::shared_ptr<Ins> instr, std::vector<Operand>::iterator& end, std::vector<Operand>::iterator& cur) {
		CLARA_INSTRUCTION insn = INSN_INVALID;
		std::vector<CLARA_INSTRUCTION> insns = GetMnemonicInstruction(instr->GetMnemonic())->second;
		std::vector<CLARA_INSTRUCTION> considered;
		//bool noparams = cur == line.end() || (cur + 1) == line.end();

		std::vector<Operand> params;
		for (; cur != end; ++cur) {
			params.push_back(*cur);
		}

		auto mnemonic = instr->GetMnemonic();
		if (instr->GetInstruction() != INSN_INVALID)
			insn = instr->GetInstruction();
		else {
			for (auto& r : insns) {
				auto& instr = g_Instructions[r];
				if (instr.minParams > params.size())
					continue;
				else if (instr.params.size() < params.size()) {
					auto it = g_Friends.find(r);
					if (it == g_Friends.end()) continue;

					cur -= params.size();
					for (size_t i = params.size(); i; --i) {
						auto ins = std::make_shared<Ins>(it->second);
						CompileInstruction(file, ins, cur + 1, cur);
					}
					insn = r;
					params.clear();
				}
				else if (instr.params.size() > params.size()) {
					if (instr.defaults.size() == (instr.params.size() - params.size())) {
						for (auto& def : instr.defaults) {
							Parser parser(m_lines);
							params.emplace_back(parser.ParseOperand(def, true));
						}
					}
				}

				bool nogood = false;
				size_t i = 0;
				auto& args = instr.params;
				for (auto arg : args) {
					switch (*arg) {
					case Imm8:
						nogood = params[i].GetBase()->GetSize() > 1;
						break;
					case Imm16:
						nogood = params[i].GetBase()->GetSize() > 2;
						break;
					case Imm32:
						nogood = params[i].GetBase()->GetSize() > 4;
						break;
					case String32:
						nogood = params[i].GetBase()->GetSize() > 4;
						break;
					case Local8:
						nogood = params[i].GetBase()->GetSize() > 1;
						break;
					case Local16:
						nogood = params[i].GetBase()->GetSize() > 2;
						break;
					case Local32:
						nogood = params[i].GetBase()->GetSize() > 4;
						break;
					case Global16:
						nogood = params[i].GetBase()->GetSize() > 2;
						break;
					case Global32:
						nogood = params[i].GetBase()->GetSize() > 4;
						break;
					}

					if (nogood) break;

					++i;
				}

				if (!nogood) considered.emplace_back(r);
			}

			if (considered.size())
				insn = considered.front();
		}

		assert(insn != INSN_INVALID);

		file.write(reinterpret_cast<const char*>(&insn), sizeof(insn));
		for (auto param : params) {
			auto size = param.GetBase()->GetSize();
			auto type = param.GetBase()->GetType();
			switch (type) {
			case OP_IMMEDIATE:
				{
					if (size <= 1) {
						auto v = param.Get<Imm<uint8_t>>()->GetValue<uint8_t>();
						file.write(reinterpret_cast<const char*>(&v), 1);
					}
					else if (size <= 2) {
						auto v = param.Get<Imm<uint16_t>>()->GetValue<uint16_t>();
						file.write(reinterpret_cast<const char*>(&v), 2);
					}
					else {
						auto v = param.Get<Imm<uint32_t>>()->GetValue<uint32_t>();
						file.write(reinterpret_cast<const char*>(&v), 4);
					}
				}
				break;
			case OP_VARIABLE:
				break;
			case OP_INSTRUCTION:
			case OP_INVALID:
			default:
				BREAK();
			}
		}
	}

public:
	Compiler() = default;

	bool Digest();
	bool Digest(std::string);

	void Parse(std::string code) {
		Parser parser(code, m_lines);
	}
	void Compile(std::ofstream& file) {
		size_t i = 0;
		for (auto& ln : m_lines) {
			auto it = ln.begin();
			auto op = *it;

			switch (op.GetBase()->GetType()) {
			case OP_INSTRUCTION:
				CompileInstruction(file, op.Get<Ins>(), ln.end(), ++it);
				break;
			}

			++i;
		}
		i = 0;
	}
};

CLARA_NAMESPACE_END