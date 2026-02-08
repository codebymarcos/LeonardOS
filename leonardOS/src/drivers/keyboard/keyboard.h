// Keyboard Driver - Interface pública

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

// Códigos especiais de teclado (fora do range ASCII)
#define KEY_PAGE_UP   0x80
#define KEY_PAGE_DOWN 0x81
#define KEY_ARROW_UP  0x82
#define KEY_ARROW_DOWN 0x83
#define KEY_HOME      0x84
#define KEY_END       0x85

// Inicializa o driver (registra IRQ1)
void kbd_init(void);

// Lê caractere do buffer (bloqueia)
char kbd_getchar(void);

// Verifica se há caractere disponível
int kbd_has_char(void);

// Lê linha com echo
void kbd_read_line(char *buf, int maxlen);

#endif
