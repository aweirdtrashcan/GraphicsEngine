#pragma once

#define DEFINE_KEY(key, num) key## = num

#ifdef Escape
#undef Escape
#endif

enum class KeyCode : int
{
    // https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    DEFINE_KEY(Backspace, 0x08),
    DEFINE_KEY(Clear, 0x0C),
    DEFINE_KEY(Enter, 0x0D),
    DEFINE_KEY(Return, 0x0D),
    DEFINE_KEY(Shift, 0x10),
    DEFINE_KEY(Control, 0x11),
    DEFINE_KEY(Alt, 0x12),
    DEFINE_KEY(Pause, 0x13),

    DEFINE_KEY(CapsLock, 0x14),
    DEFINE_KEY(Space, 0x20),
    DEFINE_KEY(Escape, 0x1B),

    DEFINE_KEY(PageUp, 0x22),
    DEFINE_KEY(End, 0x23),
    DEFINE_KEY(Home, 0x24),

    DEFINE_KEY(ArrowLeft, 0x25),
    DEFINE_KEY(ArrowUp, 0x26),
    DEFINE_KEY(ArrowRight, 0x27),
    DEFINE_KEY(ArrowDown, 0x28),

    DEFINE_KEY(Select, 0x29),
    DEFINE_KEY(Print, 0x2A),
    DEFINE_KEY(Execute, 0x2B),
    DEFINE_KEY(PrintScreen, 0x2C),
    DEFINE_KEY(Delete, 0x2E),

    DEFINE_KEY(Key_0, 0x30),
    DEFINE_KEY(Key_1, 0x31),
    DEFINE_KEY(Key_2, 0x32),
    DEFINE_KEY(Key_3, 0x33),
    DEFINE_KEY(Key_4, 0x34),
    DEFINE_KEY(Key_5, 0x35),
    DEFINE_KEY(Key_6, 0x36),
    DEFINE_KEY(Key_7, 0x37),
    DEFINE_KEY(Key_8, 0x38),
    DEFINE_KEY(Key_9, 0x39),
    
    DEFINE_KEY(Key_A, 0x41),
    DEFINE_KEY(Key_B, 0x42),
    DEFINE_KEY(Key_C, 0x43),
    DEFINE_KEY(Key_D, 0x44),
    DEFINE_KEY(Key_E, 0x45),
    DEFINE_KEY(Key_F, 0x46),
    DEFINE_KEY(Key_G, 0x47),
    DEFINE_KEY(Key_H, 0x48),
    DEFINE_KEY(Key_I, 0x49),
    DEFINE_KEY(Key_J, 0x4A),
    DEFINE_KEY(Key_K, 0x4B),
    DEFINE_KEY(Key_L, 0x4C),
    DEFINE_KEY(Key_M, 0x4D),
    DEFINE_KEY(Key_N, 0x4E),
    DEFINE_KEY(Key_O, 0x4F),
    DEFINE_KEY(Key_P, 0x50),
    DEFINE_KEY(Key_Q, 0x51),
    DEFINE_KEY(Key_R, 0x52),
    DEFINE_KEY(Key_S, 0x53),
    DEFINE_KEY(Key_T, 0x54),
    DEFINE_KEY(Key_U, 0x55),
    DEFINE_KEY(Key_V, 0x56),
    DEFINE_KEY(Key_W, 0x57),
    DEFINE_KEY(Key_X, 0x58),
    DEFINE_KEY(Key_Y, 0x59),
    DEFINE_KEY(Key_Z, 0x5A),

    DEFINE_KEY(LeftWin, 0x5B),
    DEFINE_KEY(RightWin, 0x5C),
    
    DEFINE_KEY(NumPad_0, 0x60),
    DEFINE_KEY(NumPad_1, 0x61),
    DEFINE_KEY(NumPad_2, 0x62),
    DEFINE_KEY(NumPad_3, 0x63),
    DEFINE_KEY(NumPad_4, 0x64),
    DEFINE_KEY(NumPad_5, 0x65),
    DEFINE_KEY(NumPad_6, 0x66),
    DEFINE_KEY(NumPad_7, 0x67),
    DEFINE_KEY(NumPad_8, 0x68),
    DEFINE_KEY(NumPad_9, 0x69),
    
    DEFINE_KEY(Multiply, 0x6A),
    DEFINE_KEY(Add, 0x6B),
    DEFINE_KEY(Separator, 0x6C),
    DEFINE_KEY(Subtract, 0x6D),
    DEFINE_KEY(Decimal, 0x6E),
    DEFINE_KEY(Divide, 0x6F),
    
    DEFINE_KEY(F1, 0x70),
    DEFINE_KEY(F2, 0x71),
    DEFINE_KEY(F3, 0x72),
    DEFINE_KEY(F4, 0x73),
    DEFINE_KEY(F5, 0x74),
    DEFINE_KEY(F6, 0x75),
    DEFINE_KEY(F7, 0x76),
    DEFINE_KEY(F8, 0x77),
    DEFINE_KEY(F9, 0x78),
    DEFINE_KEY(F10, 0x79),
    DEFINE_KEY(F11, 0x7A),
    DEFINE_KEY(F12, 0x7B),
    DEFINE_KEY(F13, 0x7C),
    DEFINE_KEY(F14, 0x7D),
    DEFINE_KEY(F15, 0x7E),
    DEFINE_KEY(F16, 0x7F),
    DEFINE_KEY(F17, 0x80),
    DEFINE_KEY(F18, 0x81),
    DEFINE_KEY(F19, 0x82),
    DEFINE_KEY(F20, 0x83),
    DEFINE_KEY(F21, 0x84),
    DEFINE_KEY(F22, 0x85),
    DEFINE_KEY(F23, 0x86),
    DEFINE_KEY(F24, 0x87),
    
    DEFINE_KEY(NumLock, 0x90),
    DEFINE_KEY(ScrollLock, 0x91),
    
    DEFINE_KEY(LeftShift, 0xA0),
    DEFINE_KEY(RightShift, 0xA1),
    
    DEFINE_KEY(LeftControl, 0xA2),
    DEFINE_KEY(RightControl, 0xA3),

    DEFINE_KEY(LeftAlt, 0xA4),
    DEFINE_KEY(RightAlt, 0xA5),

    MAX
};