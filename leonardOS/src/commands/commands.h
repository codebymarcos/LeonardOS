// LeonardOS - Sistema de Comandos
// Registry central para todos os comandos do shell
//
// Para adicionar um novo comando:
// 1. Crie um arquivo em src/commands/ (ex: cmd_meucomando.c)
// 2. Inclua "commands.h"
// 3. Implemente a função: void cmd_meucomando(void);
// 4. Em commands.c, adicione no array: {"meucomando", "descrição", cmd_meucomando}
// 5. Adicione o .o no Makefile

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "../common/types.h"

// Tipo de função de comando (recebe os argumentos como string)
typedef void (*cmd_func_t)(const char *args);

// Estrutura que define um comando
typedef struct {
    const char *name;        // Nome do comando (ex: "help")
    const char *description; // Descrição curta (ex: "exibe ajuda")
    cmd_func_t  handler;     // Função que executa o comando
} command_t;

// Retorna a lista de todos os comandos registrados
const command_t *commands_get_all(void);

// Retorna o número de comandos registrados
int commands_get_count(void);

// Busca um comando pelo nome. Retorna NULL se não encontrado.
const command_t *commands_find(const char *name);

// Executa um comando a partir da linha digitada (nome + args)
// Retorna 1 se o comando foi encontrado, 0 se não
int commands_execute(const char *input);

#endif
