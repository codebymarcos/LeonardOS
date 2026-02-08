// Keyboard Driver - Interface pública

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

char kbd_getchar(void);
int kbd_has_char(void);
void kbd_read_line(char *buf, int maxlen);

#endif
