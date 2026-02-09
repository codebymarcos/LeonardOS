// LeonardOS - Script Engine
// Execução de scripts (.sh) com suporte a:
// - if/else/fi
// - while/do/done
// - for/in/do/done
// - Funções: function name { ... }

#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include "../common/types.h"

// Executa um script a partir de um array de linhas
// Retorna 0 em sucesso, -1 em erro
int script_execute_lines(const char **lines, int num_lines);

// Executa um script a partir de um arquivo VFS
int script_execute_file(const char *path);

// Executa uma linha com suporte a controle de fluxo
// Chamada pelo shell para processar if/while/for interativos
// Retorna: 1 = linha processada como controle, 0 = linha normal
int script_try_control(const char *line);

// Avalia uma condição (para if/while)
// Suporta: [ -e file ], [ $VAR == value ], [ $? -eq 0 ], true, false
int script_eval_condition(const char *cond);

// ============================================================
// Funções de shell (function name { ... })
// ============================================================
#define FUNC_MAX       16
#define FUNC_NAME_MAX  32
#define FUNC_LINES_MAX 32
#define FUNC_LINE_MAX  128

// Define uma função
void script_define_function(const char *name, const char **body, int body_count);

// Chama uma função (retorna 1 se encontrada, 0 se não)
int script_call_function(const char *name, const char *args);

#endif
