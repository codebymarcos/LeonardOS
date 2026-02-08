// LeonardOS - Registry de Comandos
// Tabela central com todos os comandos disponíveis

#include "commands.h"
#include "cmd_help.h"
#include "cmd_clear.h"
#include "cmd_sysinfo.h"
#include "cmd_halt.h"
#include "cmd_test.h"
#include "cmd_mem.h"

// ============================================================
// Tabela de comandos
// Para adicionar um novo comando, basta incluir o header
// e adicionar uma entrada aqui.
// ============================================================
static const command_t command_table[] = {
    { "help",    "exibe a lista de comandos",       cmd_help    },
    { "clear",   "limpa a tela",                    cmd_clear   },
    { "sysinfo", "exibe informacoes do sistema",    cmd_sysinfo },
    { "halt",    "desliga o kernel",                cmd_halt    },
    { "test",    "teste automatizado do kernel",    cmd_test    },
    { "mem",     "exibe uso de memoria fisica",     cmd_mem     },
};

static const int command_count = sizeof(command_table) / sizeof(command_table[0]);

// Compara strings (utilidade interna)
static int cmd_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

// Retorna todos os comandos
const command_t *commands_get_all(void) {
    return command_table;
}

// Retorna quantidade de comandos
int commands_get_count(void) {
    return command_count;
}

// Busca comando pelo nome
const command_t *commands_find(const char *name) {
    for (int i = 0; i < command_count; i++) {
        if (cmd_strcmp(name, command_table[i].name) == 0) {
            return &command_table[i];
        }
    }
    return NULL;
}

// Executa uma linha de comando (separa nome dos argumentos)
int commands_execute(const char *input) {
    // Pula espaços iniciais
    while (*input == ' ') input++;
    if (*input == '\0') return 0;

    // Copia o nome do comando (até o primeiro espaço)
    static char name_buf[64];
    int i = 0;
    while (*input && *input != ' ' && i < 63) {
        name_buf[i++] = *input++;
    }
    name_buf[i] = '\0';

    // Pula espaços entre nome e argumentos
    while (*input == ' ') input++;

    // Busca e executa
    const command_t *cmd = commands_find(name_buf);
    if (cmd) {
        cmd->handler(input);
        return 1;
    }
    return 0;
}
