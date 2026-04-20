#pragma once
#include "../pch.h"

enum class e_registry_type
{
	none = REG_NONE,
	sz = REG_SZ,
	expand_sz = REG_EXPAND_SZ,
	binary = REG_BINARY,
	dword = REG_DWORD,
	dword_little_endian = REG_DWORD_LITTLE_ENDIAN,
	dword_big_endian = REG_DWORD_BIG_ENDIAN,
	link = REG_LINK,
	multi_sz = REG_MULTI_SZ,
	resource_list = REG_RESOURCE_LIST,
	full_resource_descriptor = REG_FULL_RESOURCE_DESCRIPTOR,
	resource_requirements_list = REG_RESOURCE_REQUIREMENTS_LIST,
	qword = REG_QWORD,
	qword_little_endian = REG_QWORD_LITTLE_ENDIAN
};

class c_registry
{
private:
public:
	c_registry()
	{
	}

	~c_registry()
	{
	}

	std::string QueryValue(const char* path, e_registry_type type);
};
