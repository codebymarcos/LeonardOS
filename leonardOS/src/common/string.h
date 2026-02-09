// LeonardOS - Funções de string (freestanding)
// Utilitários compartilhados — elimina duplicação entre módulos
//
// Uso: #include "common/string.h" (do kernel)
//      #include "../common/string.h" (de commands/)

#ifndef __LEONARDOS_STRING_H__
#define __LEONARDOS_STRING_H__

#include "types.h"

// ============================================================
// Funções básicas de string
// ============================================================

// Comprimento da string (sem '\0')
int kstrlen(const char *s);

// Compara duas strings. Retorna 0 se iguais.
int kstrcmp(const char *a, const char *b);

// Compara no máximo n caracteres
int kstrncmp(const char *a, const char *b, int n);

// Copia src para dst (até max-1 chars, sempre termina com '\0')
void kstrcpy(char *dst, const char *src, int max);

// Concatena src ao final de dst (respeitando max total)
void kstrcat(char *dst, const char *src, int max);

// ============================================================
// Funções de memória
// ============================================================

void kmemcpy(void *dst, const void *src, uint32_t n);
void kmemset(void *dst, uint8_t val, uint32_t n);

#endif
