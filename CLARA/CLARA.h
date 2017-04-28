#pragma once
#include "API.h"
#ifdef _MSC_VER
	#define BREAK __debugbreak
#else
	#define BREAK
#endif
#ifdef __cplusplus
	#ifndef CLARA_NAMESPACE_BEGIN
		#define CLARA_NAMESPACE_BEGIN namespace CLARA{
		#define CLARA_NAMESPACE_END }
	#endif
#endif