// LeonardOS - Tipos e constantes comuns

#ifndef __LEONARDOS_TYPES_H__
#define __LEONARDOS_TYPES_H__

// Inteiros sem sinal
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// Inteiros com sinal
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

// Ponteiros e tamanhos
typedef uint32_t uintptr_t;
typedef uint32_t size_t;

// Booleano
typedef int bool;
#define true 1
#define false 0

// NULL
#define NULL ((void *)0)

#endif
