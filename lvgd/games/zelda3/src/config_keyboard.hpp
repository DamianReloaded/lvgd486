#pragma once

#include "config.h"
#include "../vgd/Input/Keyboard.hpp"

enum
{
    KeyMod_Alt = 0x400,
    KeyMod_Shift = 0x800,
    KeyMod_Ctrl = 0x1000
};

int FindCmdForVgdKey(vgd::Keyboard::Key key, uint32 modifiers);