// LeonardOS - Shell
// Interface interativa com sistema modular de comandos
// Suporta: histórico, pipe (|), ponto-e-vírgula (;), variáveis $VAR

#include "shell.h"
#include "script.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../commands/commands.h"

// ============================================================
// Estado global do shell
// ============================================================
vfs_node_t *current_dir = NULL;
char current_path[256] = "/";

// ============================================================
// Variáveis de ambiente ($VAR)
// ============================================================
#define ENV_MAX      32
#define ENV_KEY_MAX  32
#define ENV_VAL_MAX  128

char env_keys[ENV_MAX][ENV_KEY_MAX];
char env_vals[ENV_MAX][ENV_VAL_MAX];
int  env_count = 0;

// Define ou atualiza uma variável
void shell_setenv(const char *key, const char *value) {
    // Procura existente
    for (int i = 0; i < env_count; i++) {
        if (kstrcmp(env_keys[i], key) == 0) {
            kstrcpy(env_vals[i], value, ENV_VAL_MAX);
            return;
        }
    }
    if (env_count < ENV_MAX) {
        kstrcpy(env_keys[env_count], key, ENV_KEY_MAX);
        kstrcpy(env_vals[env_count], value, ENV_VAL_MAX);
        env_count++;
    }
}

// Busca uma variável (retorna NULL se não encontrada)
const char *shell_getenv(const char *key) {
    for (int i = 0; i < env_count; i++) {
        if (kstrcmp(env_keys[i], key) == 0) {
            return env_vals[i];
        }
    }
    return NULL;
}

// ============================================================
// Expansão de variáveis ($VAR e $?)
// ============================================================
int last_exit_code = 0;

static void expand_vars(const char *input, char *output, int max) {
    int o = 0;
    const char *p = input;
    while (*p && o < max - 1) {
        if (*p == '$') {
            p++; // pula '$'
            if (*p == '?') {
                // $? = último exit code
                char num[12];
                int val = last_exit_code;
                int ni = 0;
                if (val == 0) {
                    num[ni++] = '0';
                } else {
                    char tmp[12];
                    int ti = 0;
                    while (val > 0) { tmp[ti++] = '0' + (val % 10); val /= 10; }
                    while (ti > 0) num[ni++] = tmp[--ti];
                }
                num[ni] = '\0';
                for (int i = 0; num[i] && o < max - 1; i++) output[o++] = num[i];
                p++;
            } else {
                // Nome de variável: alfanuméricos + _
                char varname[ENV_KEY_MAX];
                int vi = 0;
                while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                       (*p >= '0' && *p <= '9') || *p == '_') {
                    if (vi < ENV_KEY_MAX - 1) varname[vi++] = *p;
                    p++;
                }
                varname[vi] = '\0';
                const char *val = shell_getenv(varname);
                if (val) {
                    while (*val && o < max - 1) output[o++] = *val++;
                }
            }
        } else {
            output[o++] = *p++;
        }
    }
    output[o] = '\0';
}

// ============================================================
// Glob matching (*, ?)
// ============================================================
static int glob_match(const char *pattern, const char *text) {
    while (*pattern) {
        if (*pattern == '*') {
            pattern++;
            // '*' no final = aceita tudo
            if (*pattern == '\0') return 1;
            // Tenta match a partir de cada posição do texto
            while (*text) {
                if (glob_match(pattern, text)) return 1;
                text++;
            }
            return glob_match(pattern, text);
        } else if (*pattern == '?') {
            if (*text == '\0') return 0;
            pattern++;
            text++;
        } else {
            if (*pattern != *text) return 0;
            pattern++;
            text++;
        }
    }
    return (*text == '\0');
}

// Checa se um token contém caracteres glob
static int has_glob(const char *s) {
    while (*s) {
        if (*s == '*' || *s == '?') return 1;
        s++;
    }
    return 0;
}

// Expande globs no input. Se o token não tem *, ? → copia literal.
// Caso contrário, faz readdir e expande matches.
static void expand_globs(const char *input, char *output, int max) {
    int o = 0;
    const char *p = input;

    while (*p && o < max - 1) {
        // Pula espaços
        while (*p == ' ' && o < max - 1) {
            output[o++] = *p++;
        }
        if (*p == '\0') break;

        // Extrai token
        char token[256];
        int ti = 0;
        int in_quotes = 0;
        while (*p && (*p != ' ' || in_quotes) && ti < 255) {
            if (*p == '"') in_quotes = !in_quotes;
            token[ti++] = *p++;
        }
        token[ti] = '\0';

        if (has_glob(token) && !in_quotes) {
            // Faz readdir no diretório atual e match
            int found = 0;
            uint32_t idx = 0;
            vfs_node_t *entry;
            while ((entry = vfs_readdir(current_dir, idx)) != NULL) {
                if (glob_match(token, entry->name)) {
                    if (found > 0 && o < max - 1) output[o++] = ' ';
                    int ni = 0;
                    while (entry->name[ni] && o < max - 1) {
                        output[o++] = entry->name[ni++];
                    }
                    found++;
                }
                idx++;
            }
            // Se nenhum match, copia literal
            if (found == 0) {
                for (int i = 0; token[i] && o < max - 1; i++) {
                    output[o++] = token[i];
                }
            }
        } else {
            // Token sem glob, copia literal
            for (int i = 0; token[i] && o < max - 1; i++) {
                output[o++] = token[i];
            }
        }
    }
    output[o] = '\0';
}

// ============================================================
// Pipeline: executa um segmento de pipe
// Captura saída do lado esquerdo, passa como stdin (args) ao direito
// ============================================================
#define PIPE_BUF_SIZE 4096

static char pipe_buf[PIPE_BUF_SIZE];

static int execute_single(const char *input) {
    return commands_execute(input);
}

static int execute_pipeline(const char *input) {
    // Copia input para poder modificar
    char line[512];
    kstrcpy(line, input, 512);

    // Procura '|' fora de aspas
    char *segments[8];  // Máximo 8 pipes
    int seg_count = 0;
    segments[seg_count++] = line;

    char *p = line;
    int in_quotes = 0;
    while (*p) {
        if (*p == '"') in_quotes = !in_quotes;
        else if (*p == '|' && !in_quotes) {
            *p = '\0';
            if (seg_count < 8) {
                segments[seg_count++] = p + 1;
            }
        }
        p++;
    }

    if (seg_count == 1) {
        // Sem pipe, executa direto
        return execute_single(line);
    }

    // Executa pipeline: captura saída de cada comando e passa ao próximo
    pipe_buf[0] = '\0';
    int result = 0;

    for (int i = 0; i < seg_count; i++) {
        // Pula espaços
        char *cmd = segments[i];
        while (*cmd == ' ') cmd++;

        // Remove espaços finais
        int len = kstrlen(cmd);
        while (len > 0 && cmd[len - 1] == ' ') len--;
        cmd[len] = '\0';

        if (cmd[0] == '\0') continue;

        if (i < seg_count - 1) {
            // Não é o último: captura saída
            char capture[PIPE_BUF_SIZE];
            vga_capture_start(capture, PIPE_BUF_SIZE);

            if (i == 0) {
                // Primeiro segmento: executa normalmente
                result = execute_single(cmd);
            } else {
                // Segmentos intermediários: passa pipe_buf como args
                char combined[512];
                // Constrói "comando pipe_buf_content"
                kstrcpy(combined, cmd, 512);
                kstrcat(combined, " ", 512);
                kstrcat(combined, pipe_buf, 512);
                result = execute_single(combined);
            }

            vga_capture_stop();
            kstrcpy(pipe_buf, capture, PIPE_BUF_SIZE);
        } else {
            // Último segmento: saída normal (para tela)
            if (pipe_buf[0] != '\0') {
                char combined[512];
                kstrcpy(combined, cmd, 512);
                kstrcat(combined, " ", 512);
                kstrcat(combined, pipe_buf, 512);
                result = execute_single(combined);
            } else {
                result = execute_single(cmd);
            }
        }
    }

    return result;
}

// ============================================================
// Separa por ponto-e-vírgula (;) e executa cada parte
// ============================================================
void execute_line(const char *input) {
    char line[512];
    kstrcpy(line, input, 512);

    // Split por ';' fora de aspas
    char *segments[16];
    int seg_count = 0;
    segments[seg_count++] = line;

    char *p = line;
    int in_quotes = 0;
    while (*p) {
        if (*p == '"') in_quotes = !in_quotes;
        else if (*p == ';' && !in_quotes) {
            *p = '\0';
            if (seg_count < 16) {
                segments[seg_count++] = p + 1;
            }
        }
        p++;
    }

    for (int s = 0; s < seg_count; s++) {
        char *cmd = segments[s];
        // Pula espaços
        while (*cmd == ' ') cmd++;
        int len = kstrlen(cmd);
        while (len > 0 && cmd[len - 1] == ' ') len--;
        cmd[len] = '\0';

        if (cmd[0] == '\0') continue;

        // Checa atribuição de variável: VAR=value
        char *eq = NULL;
        char *c = cmd;
        int valid_var = 1;
        while (*c && *c != '=' && *c != ' ') {
            if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') ||
                  (*c >= '0' && *c <= '9') || *c == '_')) {
                valid_var = 0;
            }
            c++;
        }
        if (*c == '=' && valid_var && c > cmd) {
            eq = c;
        }

        if (eq) {
            // Atribuição: KEY=VALUE
            *eq = '\0';
            const char *val = eq + 1;
            // Remove aspas ao redor do valor
            char clean_val[ENV_VAL_MAX];
            int vi = 0;
            while (*val == ' ') val++;
            if (*val == '"') {
                val++;
                while (*val && *val != '"' && vi < ENV_VAL_MAX - 1) {
                    clean_val[vi++] = *val++;
                }
            } else {
                while (*val && *val != ' ' && vi < ENV_VAL_MAX - 1) {
                    clean_val[vi++] = *val++;
                }
            }
            clean_val[vi] = '\0';
            shell_setenv(cmd, clean_val);
            continue;
        }

        // Expande variáveis
        char expanded[512];
        expand_vars(cmd, expanded, 512);

        // Expande globs (*, ?)
        char globbed[512];
        expand_globs(expanded, globbed, 512);

        // Executa (com suporte a pipe)
        int found = execute_pipeline(globbed);

        // Se não encontrou como comando, tenta como função do script
        if (!found) {
            // Extrai nome da função e args
            const char *fp = globbed;
            while (*fp == ' ') fp++;
            char fname[64];
            int fi = 0;
            while (*fp && *fp != ' ' && fi < 63) {
                fname[fi++] = *fp++;
            }
            fname[fi] = '\0';
            while (*fp == ' ') fp++;
            found = script_call_function(fname, fp);
        }

        last_exit_code = found ? 0 : 1;

        if (!found) {
            const char *ep = globbed;
            while (*ep == ' ') ep++;
            vga_puts_color("Comando desconhecido: ", THEME_ERROR);
            vga_puts_color(ep, THEME_WARNING);
            vga_puts("\n");
        }
    }
}

// ============================================================
// Loop principal do shell
// ============================================================
void shell_loop(void) {
    static char cmd_buf[256];

    // Inicializa diretório atual como raiz
    current_dir = vfs_root;

    // Variáveis de ambiente padrão
    shell_setenv("HOME", "/");
    shell_setenv("SHELL", "LeonardOS");
    shell_setenv("VERSION", "1.0.0");

    vga_set_color(THEME_DEFAULT);
    vga_puts_color("LeonardOS v1.0.0", THEME_TITLE);
    vga_puts(" - Digite '");
    vga_puts_color("help", THEME_INFO);
    vga_puts("' para ajuda\n\n");

    while (1) {
        // Linha 1: ┌─ LeonardOS ── leonardo@kernel
        vga_puts_color("\u250C\u2500", THEME_DIM);           // ┌─
        vga_puts_color(" LeonardOS ", THEME_TITLE);           // LeonardOS
        vga_puts_color("\u2500\u2500 ", THEME_DIM);           // ──
        vga_puts_color("leonardo", THEME_PROMPT);             // leonardo
        vga_puts_color("@", THEME_DIM);                       // @
        vga_puts_color("kernel", THEME_PROMPT);               // kernel
        vga_putchar('\n');

        // Linha 2: └─[/path] >
        vga_puts_color("\u2514\u2500", THEME_DIM);            // └─
        vga_puts_color("[", THEME_DIM);                       // [
        vga_puts_color(current_path, THEME_INFO);             // /path
        vga_puts_color("] ", THEME_DIM);                      // ]
        vga_puts_color("> ", THEME_PROMPT);                   // >
        vga_set_color(THEME_DEFAULT);
        kbd_read_line(cmd_buf, sizeof(cmd_buf));
        vga_putchar('\n');

        if (cmd_buf[0] != 0) {
            execute_line(cmd_buf);
        }
    }
}

