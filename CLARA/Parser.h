#pragma once
#include <assert.h>
#include <vector>
#include <algorithm>
#include <cctype>
#include "Assembly.h"
#include "Types.h"

CLARA_NAMESPACE_BEGIN

class Parser {
	bool m_acceptRepeatInstr = false;

	std::vector<Operand> m_operands;
	std::vector<std::vector<Operand>>& m_lines;

public:
	Parser(std::vector<std::vector<Operand>>& lines) : m_lines(lines) { }
	Parser(std::string line, std::vector<std::vector<Operand>>& lines) : m_lines(lines) {
		Parse(line);
	}

	void Parse(std::string line) {
		m_operands.clear();
		line = line.substr(line.find_first_not_of(" \t"), line.find_first_of(';'));

		// remove duplicate whitespace
		auto end = std::unique(line.begin(), line.end(), [](char l, char r) { return (l == ' ' || l == '\t') && (r == ' ' || r == '\t'); });
		line.erase(end, line.end());
		if (line.empty()) return;

		std::transform(line.begin(), line.end(), line.begin(), [](int c) { return std::tolower(c); });

		for (size_t i = line.find_first_not_of(" \t"), j; (j = line.find_first_of(" \t,", i)) != line.npos || i != line.npos; i = line.find_first_not_of(" \t", j)) {
			if (line[i] == '.')
				break;
			if (line[i] == ',') {
				auto op = ParseOperand(",");
				if (op) m_operands.emplace_back(op);
				++j;
			}
			else {
				auto ln = line.substr(i, j - i);
				auto op = ParseOperand(line.substr(i, j - i));
				if (op) m_operands.emplace_back(op);
			}
		}

		if (!m_operands.empty()) {
			m_lines.emplace_back(m_operands);
			m_operands.clear();
		}
	}

	Operand ParseOperand(std::string op, bool noComma = false) {
		Operand operand;

		if (!op.empty()) {
			if (std::isdigit(op[0]) || op[0] == '-') {
				if (m_acceptRepeatInstr & !noComma) {
					assert(!m_lines.empty());

					auto tmp = m_lines.back().front();
					m_operands.emplace_back(tmp);
					m_acceptRepeatInstr = false;
				}

				assert(!m_operands.empty() || noComma);

				bool b = true;
				if (b) {
					try {
						auto v = std::stol(op, 0, 0);
						size_t s = GetIntNumBytes(v);
						if (s <= 1) operand = std::make_shared<Imm<int8_t>>(v);
						else if (s <= 2) operand = std::make_shared<Imm<int16_t>>(v);
						else operand = std::make_shared<Imm<int32_t>>(v);
						b = false;
					}
					catch (...) { BREAK(); }
				}
				if (b) {
					try {
						auto v = std::stoul(op, 0, 0);
						operand = std::make_shared<Imm<uint32_t>>(v);
						size_t s = GetUIntNumBytes(v);
						if (s <= 1) operand = std::make_shared<Imm<uint8_t>>(v);
						else if (s <= 2) operand = std::make_shared<Imm<uint16_t>>(v);
						else operand = std::make_shared<Imm<uint32_t>>(v);
						b = false;
					}
					catch (...) { BREAK(); }
				}
				if (b) {
					try {
						auto v = std::stof(op);
						operand = std::make_shared<Imm<float>>(v);
						b = false;
					}
					catch (...) { BREAK(); }
				}
				assert(!b);
			}
			else if (op == ",") {
				if (m_operands.size()) {
					m_lines.emplace_back(m_operands);
					m_operands.clear();
					m_acceptRepeatInstr = true;
				}
			}
			else {
				// identifier operand, check for mnemonic
				auto it = g_Mnemonics.find(op);
				if (it != g_Mnemonics.end()) {
					m_acceptRepeatInstr = false;
					operand = std::make_shared<Ins>(it->second);
				}
				else {
					// none found, check for variable
					BREAK();
				}
			}
		}
		return operand;
	}
};

CLARA_NAMESPACE_END