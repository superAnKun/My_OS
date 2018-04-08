#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#define esc '\033'
#define backspace '\b'
#define tab  '\t'
#define enter '\r'
#define delete_  '\177'

//将控制字符的ASCLL码设置为0
#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible


static char keymap[][2] = {
	{0, 0},
	{esc, esc},
	{'1', '!'},
	{'2', '@'},
	{'3', '#'},
	{'4', '$'},
	{'5', '%'},
	{'6', '^'},
	{'7', '&'},
	{'8', '*'},
	{'9', '('},
	{'0', ')'},
	{'-', '_'},
	{'=', '+'},
	{backspace, backspace},
	{tab, tab},
	{'q', 'Q'},
	{'w', 'W'},
	{'e', 'E'},
	{'r', 'R'},
	{'t', 'T'},
	{'y', 'Y'},
	{'u', 'U'},
	{'i', 'I'},
	{'o', 'O'},
	{'p', 'P'},
	{'[', '{'},
	{']', '}'},
	{enter, enter},
	{ctrl_l_char, ctrl_l_char},
	{'a', 'A'},
	{'s', 'S'},
	{'d', 'D'},
	{'f', 'F'},
	{'g', 'G'},
	{'h', 'H'},
	{'j', 'J'},
	{'k', 'K'},
	{'l', 'L'},
	{';', ':'},
	{'\'', '"'},
	{'`', '~'},
	{shift_l_char, shift_l_char},
	{'\\', '|'},
	{'z', 'Z'},
	{'x', 'X'},
	{'c', 'C'},
	{'v', 'V'},
	{'b', 'B'},
	{'n', 'B'},
	{'m', 'M'},
	{',', '<'},
	{'.', '>'},
	{'/', '?'},
	{shift_r_char, shift_r_char},
	{'*', '*'},
	{alt_l_char, alt_l_char},
	{' ', ' '},
	{caps_lock_char, caps_lock_char}
};

static void intr_keyboard_handler();
void keyboard_init();
#endif
