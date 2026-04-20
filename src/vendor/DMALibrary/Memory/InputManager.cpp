#include "pch.h"
#include "InputManager.h"

bool c_keys::InitKeyboard()
{
	UpdateKeys();
	prevState = state;
	return true;
}

void c_keys::UpdateKeys()
{
	prevState = state;

	for (int vk = 0; vk < 256; ++vk)
		state[static_cast<size_t>(vk)] = (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool c_keys::IsKeyDown(uint32_t virtual_key_code)
{
	if (virtual_key_code >= state.size())
		return false;

	return (GetAsyncKeyState(static_cast<int>(virtual_key_code)) & 0x8000) != 0;
}

bool c_keys::WasKeyPressed(uint32_t virtual_key_code)
{
	if (virtual_key_code >= state.size())
		return false;

	return state[virtual_key_code] && !prevState[virtual_key_code];
}
