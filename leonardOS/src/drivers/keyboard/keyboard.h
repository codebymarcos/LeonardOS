// Keyboard Driver - Interface pública

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

// Inicializa o driver (registra IRQ1)
void kbd_init(void);

// Lê caractere do buffer (bloqueia)
char kbd_getchar(void);

// Verifica se há caractere disponível
int kbd_has_char(void);

// Lê linha com echo
void kbd_read_line(char *buf, int maxlen);

#endif
