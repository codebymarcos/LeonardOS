// LeonardOS - Registry de Comandos
// Tabela central com todos os comandos disponíveis

#include "commands.h"
#include "cmd_help.h"
#include "cmd_clear.h"
#include "cmd_sysinfo.h"
#include "cmd_halt.h"
#include "cmd_test.h"
#include "cmd_mem.h"
#include "cmd_df.h"
#include "cmd_ls.h"
#include "cmd_cat.h"
#include "cmd_echo.h"
#include "cmd_pwd.h"
#include "cmd_cd.h"
#include "cmd_mkdir.h"
#include "cmd_touch.h"
#include "cmd_rm.h"
#include "cmd_cp.h"
#include "cmd_reboot.h"
#include "cmd_stat.h"
#include "cmd_tree.h"
#include "cmd_find.h"
#include "cmd_grep.h"
#include "cmd_env.h"
#include "cmd_wc.h"
#include "cmd_head.h"
#include "cmd_source.h"
#include "cmd_keytest.h"
#include "cmd_ifconfig.h"
#include "cmd_netstat.h"
#include "cmd_ping.h"

// ============================================================
// Tabela de comandos
// Para adicionar um novo comando, basta incluir o header
// e adicionar uma entrada aqui.
// ============================================================
static const command_t command_table[] = {
    { "help",     "lista de comandos",              cmd_help     },
    { "clear",    "limpa a tela",                   cmd_clear    },
    { "sysinfo",  "informacoes do sistema",         cmd_sysinfo  },
    { "halt",     "desliga o kernel",               cmd_halt     },
    { "reboot",   "reinicia o sistema",             cmd_reboot   },
    { "test",     "teste automatizado",             cmd_test     },
    { "mem",      "uso de memoria fisica",          cmd_mem      },
    { "df",       "uso de disco",                   cmd_df       },
    { "ls",       "lista diretorio",                cmd_ls       },
    { "cat",      "exibe arquivo",                  cmd_cat      },
    { "echo",     "escreve texto",                  cmd_echo     },
    { "pwd",      "diretorio atual",                cmd_pwd      },
    { "cd",       "muda diretorio",                 cmd_cd       },
    { "mkdir",    "cria diretorio",                 cmd_mkdir    },
    { "touch",    "cria arquivo vazio",             cmd_touch    },
    { "rm",       "remove arquivo/diretorio",       cmd_rm       },
    { "cp",       "copia arquivo",                  cmd_cp       },
    { "stat",     "info de arquivo",                cmd_stat     },
    { "tree",     "arvore de diretorios",           cmd_tree     },
    { "find",     "busca por nome",                 cmd_find     },
    { "grep",     "busca texto em arquivo",         cmd_grep     },
    { "wc",       "conta linhas/palavras/bytes",    cmd_wc       },
    { "head",     "primeiras N linhas",             cmd_head     },
    { "env",      "variaveis de ambiente",          cmd_env      },
    { "source",   "executa script .sh",             cmd_source   },
    { "keytest",  "diagnostico scancodes",          cmd_keytest  },
    { "ifconfig", "configuracao de rede",           cmd_ifconfig },
    { "netstat",  "estatisticas de rede",           cmd_netstat  },
    { "ping",     "testa conectividade (ICMP)",     cmd_ping     },
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
