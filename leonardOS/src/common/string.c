// LeonardOS - Funções de string (freestanding)
// Implementação de utilitários de string e memória

#include "string.h"

// ============================================================
// kstrlen — comprimento da string
// ============================================================
int kstrlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// ============================================================
// kstrcmp — compara duas strings
// ============================================================
int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

// ============================================================
// kstrncmp — compara no máximo n caracteres
// ============================================================
int kstrncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

// ============================================================
// kstrcpy — copia string (safe, sempre termina com '\0')
// ============================================================
void kstrcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

// ============================================================
// kstrcat — concatena src ao final de dst
// ============================================================
void kstrcat(char *dst, const char *src, int max) {
    int len = kstrlen(dst);
    int i = 0;
    while (src[i] && len + i < max - 1) {
        dst[len + i] = src[i];
        i++;
    }
    dst[len + i] = '\0';
}

// ============================================================
// kstrstr — busca substring
// ============================================================
const char *kstrstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (needle[0] == '\0') return haystack;

    int nlen = kstrlen(needle);
    int hlen = kstrlen(haystack);
    if (nlen > hlen) return NULL;

    for (int i = 0; i <= hlen - nlen; i++) {
        int match = 1;
        for (int j = 0; j < nlen; j++) {
            if (haystack[i + j] != needle[j]) {
                match = 0;
                break;
            }
        }
        if (match) return &haystack[i];
    }
    return NULL;
}

// ============================================================
// ktolower — converte para lowercase
// ============================================================
char ktolower(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

// ============================================================
// kmemcpy — copia n bytes
// ============================================================
void kmemcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

// ============================================================
// kmemset — preenche n bytes com val
// ============================================================
void kmemset(void *dst, uint8_t val, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < n; i++) d[i] = val;
}
