// LeonardOS - Comando: env
// Lista todas as variáveis de ambiente ou define uma
//
// Uso:
//   env           → lista todas as variáveis
//   env VAR=valor → define uma variável (atalho para VAR=valor)

#include "cmd_env.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../shell/shell.h"

// Acesso à tabela de env (definida em shell.c)
// Para listar, usamos shell_getenv iterando — mas precisamos
// de uma API extra. Vamos usar os arrays diretamente via extern.

// Declarados em shell.c
#define ENV_MAX      32
#define ENV_KEY_MAX  32
#define ENV_VAL_MAX  128

extern char env_keys[ENV_MAX][ENV_KEY_MAX];
extern char env_vals[ENV_MAX][ENV_VAL_MAX];
extern int  env_count;

void cmd_env(const char *args) {
    if (args && args[0] != '\0') {
        // Checa se é VAR=valor
        const char *eq = args;
        while (*eq && *eq != '=') eq++;
        if (*eq == '=') {
            // Extrai key e value
            char key[ENV_KEY_MAX];
            int ki = 0;
            const char *p = args;
            while (p < eq && ki < ENV_KEY_MAX - 1) {
                key[ki++] = *p++;
            }
            key[ki] = '\0';
            shell_setenv(key, eq + 1);
            return;
        }
        // Mostra uma variável específica
        const char *val = shell_getenv(args);
        if (val) {
            vga_puts_color(args, THEME_LABEL);
            vga_puts("=");
            vga_puts_color(val, THEME_VALUE);
            vga_putchar('\n');
        } else {
            vga_puts_color("env: variavel nao encontrada: ", THEME_ERROR);
            vga_puts_color(args, THEME_WARNING);
            vga_putchar('\n');
        }
        return;
    }

    // Lista todas
    if (env_count == 0) {
        vga_puts_color("Nenhuma variavel definida\n", THEME_DIM);
        return;
    }

    for (int i = 0; i < env_count; i++) {
        vga_puts_color(env_keys[i], THEME_LABEL);
        vga_puts("=");
        vga_puts_color(env_vals[i], THEME_VALUE);
        vga_putchar('\n');
    }
}
