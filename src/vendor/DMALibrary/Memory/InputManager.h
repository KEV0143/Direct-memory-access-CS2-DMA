#pragma once
#include <array>
#include "../pch.h"

class c_keys
{
private:
	std::array<bool, 256> state { };
	std::array<bool, 256> prevState { };

public:
	c_keys() = default;

	~c_keys() = default;

	bool InitKeyboard();

	void UpdateKeys();
	bool IsKeyDown(uint32_t virtual_key_code);
	bool WasKeyPressed(uint32_t virtual_key_code);
};
