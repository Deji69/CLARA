#pragma once
#include <map>
#include <vector>
#include "CLARA.h"

CLARA_NAMESPACE_BEGIN

enum BasicType {
	Null, Integer, Float, String, Local, Global
};
enum ImmediateType {
	ImmNone, Imm8, Imm16, Imm32,
	Local8, Local16, Local32,
	Global16, Global32,
	String32,
};

class ValueType {
	BasicType m_type;

public:
	ValueType(BasicType type) : m_type(type) { }
	virtual ~ValueType() { }

	virtual ImmediateType GetImmType() const = 0;
};

template<ImmediateType TImm>
class ImmValue : public ValueType {
	inline static BasicType GetBasicType(ImmediateType type) {
		switch (type) {
		case ImmNone: return Null;
		case Imm8: case Imm16: case Imm32:
			return Integer;
		case Local8: case Local16: case Local32:
			return Local;
		case Global16: case Global32:
			return Global;
		case String32:
			return String;
		}
		return Null;
	}

public:
	ImmValue() : ValueType(GetBasicType(TImm)) { }

	virtual ImmediateType GetImmType() const override {
		return TImm;
	}
};

CLARA_NAMESPACE_END