// LeonardOS - Script Engine
// Motor de scripting com controle de fluxo, funções e execução de .sh
//
// Sintaxe suportada:
//   if [ condição ]; then ... elif ... else ... fi
//   while [ condição ]; do ... done
//   for VAR in item1 item2 ...; do ... done
//   function nome { ... }
//   source script.sh

#include "script.h"
#include "shell.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../common/types.h"
#include "../commands/commands.h"
#include "../fs/vfs.h"

// ============================================================
// Forward: execute_line do shell.c
// ============================================================
extern void execute_line(const char *input);
extern int last_exit_code;

// ============================================================
// Funções registradas
// ============================================================
static char func_names[FUNC_MAX][FUNC_NAME_MAX];
static char func_bodies[FUNC_MAX][FUNC_LINES_MAX][FUNC_LINE_MAX];
static int  func_line_counts[FUNC_MAX];
static int  func_count = 0;

void script_define_function(const char *name, const char **body, int body_count) {
    // Procura existente
    int slot = -1;
    for (int i = 0; i < func_count; i++) {
        if (kstrcmp(func_names[i], name) == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (func_count >= FUNC_MAX) {
            vga_puts_color("script: maximo de funcoes atingido\n", THEME_ERROR);
            return;
        }
        slot = func_count++;
    }

    kstrcpy(func_names[slot], name, FUNC_NAME_MAX);
    int count = body_count < FUNC_LINES_MAX ? body_count : FUNC_LINES_MAX;
    for (int i = 0; i < count; i++) {
        kstrcpy(func_bodies[slot][i], body[i], FUNC_LINE_MAX);
    }
    func_line_counts[slot] = count;
}

int script_call_function(const char *name, const char *args) {
    for (int i = 0; i < func_count; i++) {
        if (kstrcmp(func_names[i], name) == 0) {
            // Define $1, $2, ... como variáveis temporárias
            if (args && args[0]) {
                shell_setenv("1", args);
            }
            // Executa corpo
            for (int l = 0; l < func_line_counts[i]; l++) {
                execute_line(func_bodies[i][l]);
            }
            return 1;
        }
    }
    return 0;
}

// ============================================================
// Avaliação de condições
// ============================================================
int script_eval_condition(const char *cond) {
    // Pula espaços
    while (*cond == ' ') cond++;

    // Pula '[' se presente
    if (*cond == '[') {
        cond++;
        while (*cond == ' ') cond++;
    }

    // "true" / "false"
    if (kstrncmp(cond, "true", 4) == 0) return 1;
    if (kstrncmp(cond, "false", 5) == 0) return 0;

    // "-e file" (existe?)
    if (kstrncmp(cond, "-e ", 3) == 0) {
        const char *path = cond + 3;
        while (*path == ' ') path++;
        // Remove ']' final
        char clean[128];
        int ci = 0;
        while (path[ci] && path[ci] != ']' && ci < 127) {
            clean[ci] = path[ci];
            ci++;
        }
        while (ci > 0 && clean[ci-1] == ' ') ci--;
        clean[ci] = '\0';
        vfs_node_t *node = vfs_resolve(clean, current_dir, NULL, 0);
        return (node != NULL) ? 1 : 0;
    }

    // "-d dir" (é diretório?)
    if (kstrncmp(cond, "-d ", 3) == 0) {
        const char *path = cond + 3;
        while (*path == ' ') path++;
        char clean[128];
        int ci = 0;
        while (path[ci] && path[ci] != ']' && ci < 127) {
            clean[ci] = path[ci];
            ci++;
        }
        while (ci > 0 && clean[ci-1] == ' ') ci--;
        clean[ci] = '\0';
        vfs_node_t *node = vfs_resolve(clean, current_dir, NULL, 0);
        return (node && (node->type & VFS_DIRECTORY)) ? 1 : 0;
    }

    // "-f file" (é arquivo?)
    if (kstrncmp(cond, "-f ", 3) == 0) {
        const char *path = cond + 3;
        while (*path == ' ') path++;
        char clean[128];
        int ci = 0;
        while (path[ci] && path[ci] != ']' && ci < 127) {
            clean[ci] = path[ci];
            ci++;
        }
        while (ci > 0 && clean[ci-1] == ' ') ci--;
        clean[ci] = '\0';
        vfs_node_t *node = vfs_resolve(clean, current_dir, NULL, 0);
        return (node && (node->type & VFS_FILE)) ? 1 : 0;
    }

    // "str1 == str2" ou "str1 != str2"
    {
        // Busca operador
        const char *p = cond;
        char left[64] = {0};
        char right[64] = {0};
        int li = 0;
        int op = 0; // 1 = ==, 2 = !=, 3 = -eq, 4 = -ne, 5 = -gt, 6 = -lt

        // Lê lado esquerdo
        while (*p && *p != ' ' && *p != '=' && *p != '!' && li < 63) {
            left[li++] = *p++;
        }
        left[li] = '\0';
        while (*p == ' ') p++;

        // Operador
        if (*p == '=' && *(p+1) == '=') { op = 1; p += 2; }
        else if (*p == '!' && *(p+1) == '=') { op = 2; p += 2; }
        else if (kstrncmp(p, "-eq", 3) == 0) { op = 3; p += 3; }
        else if (kstrncmp(p, "-ne", 3) == 0) { op = 4; p += 3; }
        else if (kstrncmp(p, "-gt", 3) == 0) { op = 5; p += 3; }
        else if (kstrncmp(p, "-lt", 3) == 0) { op = 6; p += 3; }

        while (*p == ' ') p++;

        // Lê lado direito
        int ri = 0;
        while (*p && *p != ' ' && *p != ']' && ri < 63) {
            right[ri++] = *p++;
        }
        right[ri] = '\0';

        if (op > 0 && left[0] && right[0]) {
            int cmp = kstrcmp(left, right);
            switch (op) {
                case 1: return (cmp == 0) ? 1 : 0;
                case 2: return (cmp != 0) ? 1 : 0;
                case 3: // -eq (numérico)
                case 4: // -ne
                case 5: // -gt
                case 6: // -lt
                {
                    // Parse numérico simples
                    int lv = 0, rv = 0;
                    int neg_l = 0, neg_r = 0;
                    const char *s = left;
                    if (*s == '-') { neg_l = 1; s++; }
                    while (*s >= '0' && *s <= '9') { lv = lv * 10 + (*s - '0'); s++; }
                    if (neg_l) lv = -lv;
                    s = right;
                    if (*s == '-') { neg_r = 1; s++; }
                    while (*s >= '0' && *s <= '9') { rv = rv * 10 + (*s - '0'); s++; }
                    if (neg_r) rv = -rv;
                    (void)neg_l; (void)neg_r;
                    switch (op) {
                        case 3: return (lv == rv) ? 1 : 0;
                        case 4: return (lv != rv) ? 1 : 0;
                        case 5: return (lv > rv)  ? 1 : 0;
                        case 6: return (lv < rv)  ? 1 : 0;
                    }
                }
            }
        }
    }

    return 0;
}

// ============================================================
// Executor de linhas (scripts)
// ============================================================
int script_execute_lines(const char **lines, int num_lines) {
    int i = 0;
    while (i < num_lines) {
        const char *line = lines[i];
        while (*line == ' ') line++;

        // Pula linhas vazias e comentários
        if (*line == '\0' || *line == '#') { i++; continue; }

        // --- if / elif / else / fi ---
        if (kstrncmp(line, "if ", 3) == 0 || kstrncmp(line, "if[", 3) == 0) {
            // Encontra condição (entre "if" e "; then" ou "then")
            const char *cond_start = line + 2;
            while (*cond_start == ' ') cond_start++;

            // Remove "; then" do final
            char cond_buf[128];
            kstrcpy(cond_buf, cond_start, 128);
            // Remove "then" e ";" do final
            {
                int len = kstrlen(cond_buf);
                // Remove trailing "then"
                if (len >= 4 && kstrcmp(cond_buf + len - 4, "then") == 0) {
                    cond_buf[len - 4] = '\0';
                    len -= 4;
                }
                // Remove trailing ";"
                while (len > 0 && (cond_buf[len-1] == ';' || cond_buf[len-1] == ' ')) {
                    cond_buf[--len] = '\0';
                }
            }

            // Expande variáveis na condição
            char cond_expanded[128];
            // Usamos expansão manual simples aqui
            kstrcpy(cond_expanded, cond_buf, 128);

            int cond_result = script_eval_condition(cond_expanded);
            int executed = cond_result;

            // Coleta linhas do bloco if
            i++;
            int depth = 1;
            int in_else = 0;
            int in_elif = 0;
            (void)in_elif;
            (void)in_else;

            while (i < num_lines && depth > 0) {
                const char *l = lines[i];
                while (*l == ' ') l++;

                if (kstrncmp(l, "if ", 3) == 0) {
                    depth++;
                    if (depth == 1) { i++; continue; }
                } else if (kstrcmp(l, "fi") == 0 || kstrncmp(l, "fi ", 2) == 0) {
                    depth--;
                    if (depth == 0) { i++; break; }
                } else if (kstrncmp(l, "elif ", 5) == 0 && depth == 1) {
                    if (!executed) {
                        // Avalia condição do elif
                        const char *ec = l + 5;
                        while (*ec == ' ') ec++;
                        char elif_cond[128];
                        kstrcpy(elif_cond, ec, 128);
                        int elen = kstrlen(elif_cond);
                        if (elen >= 4 && kstrcmp(elif_cond + elen - 4, "then") == 0) {
                            elif_cond[elen - 4] = '\0';
                            elen -= 4;
                        }
                        while (elen > 0 && (elif_cond[elen-1] == ';' || elif_cond[elen-1] == ' ')) {
                            elif_cond[--elen] = '\0';
                        }
                        cond_result = script_eval_condition(elif_cond);
                        if (cond_result) executed = 1;
                        in_else = 0;
                    } else {
                        cond_result = 0;
                    }
                    i++;
                    continue;
                } else if (kstrcmp(l, "else") == 0 && depth == 1) {
                    if (!executed) {
                        cond_result = 1;
                        in_else = 1;
                    } else {
                        cond_result = 0;
                    }
                    i++;
                    continue;
                }

                if (depth == 1 && cond_result) {
                    execute_line(l);
                }
                i++;
            }
            continue;
        }

        // --- while / do / done ---
        if (kstrncmp(line, "while ", 6) == 0) {
            const char *cond_start = line + 6;
            while (*cond_start == ' ') cond_start++;

            char cond_buf[128];
            kstrcpy(cond_buf, cond_start, 128);
            {
                int len = kstrlen(cond_buf);
                if (len >= 2 && kstrcmp(cond_buf + len - 2, "do") == 0) {
                    cond_buf[len - 2] = '\0';
                    len -= 2;
                }
                while (len > 0 && (cond_buf[len-1] == ';' || cond_buf[len-1] == ' ')) {
                    cond_buf[--len] = '\0';
                }
            }

            // Coleta linhas do corpo
            int body_start = i + 1;
            int body_end = body_start;
            int depth = 1;
            int j = body_start;
            while (j < num_lines && depth > 0) {
                const char *l = lines[j];
                while (*l == ' ') l++;
                if (kstrncmp(l, "while ", 6) == 0 || kstrncmp(l, "for ", 4) == 0) depth++;
                else if (kstrcmp(l, "done") == 0) {
                    depth--;
                    if (depth == 0) { body_end = j; break; }
                }
                j++;
            }

            // Loop (máximo 1000 iterações para segurança)
            int iterations = 0;
            while (script_eval_condition(cond_buf) && iterations < 1000) {
                for (int b = body_start; b < body_end; b++) {
                    execute_line(lines[b]);
                }
                iterations++;
            }

            i = body_end + 1;
            continue;
        }

        // --- for VAR in item1 item2 ...; do ... done ---
        if (kstrncmp(line, "for ", 4) == 0) {
            const char *p = line + 4;
            while (*p == ' ') p++;

            // Extrai nome da variável
            char var_name[32];
            int vi = 0;
            while (*p && *p != ' ' && vi < 31) {
                var_name[vi++] = *p++;
            }
            var_name[vi] = '\0';
            while (*p == ' ') p++;

            // Espera "in"
            if (kstrncmp(p, "in ", 3) != 0) {
                vga_puts_color("script: erro de sintaxe em 'for' (esperado 'in')\n", THEME_ERROR);
                i++;
                continue;
            }
            p += 3;
            while (*p == ' ') p++;

            // Coleta itens até ";" ou "do"
            char items_str[256];
            int ii = 0;
            while (*p && *p != ';' && ii < 255) {
                if (kstrncmp(p, " do", 3) == 0) break;
                items_str[ii++] = *p++;
            }
            while (ii > 0 && items_str[ii-1] == ' ') ii--;
            items_str[ii] = '\0';

            // Coleta linhas do corpo
            int body_start = i + 1;
            int body_end = body_start;
            int depth = 1;
            int j = body_start;
            while (j < num_lines && depth > 0) {
                const char *l = lines[j];
                while (*l == ' ') l++;
                if (kstrncmp(l, "while ", 6) == 0 || kstrncmp(l, "for ", 4) == 0) depth++;
                else if (kstrcmp(l, "done") == 0) {
                    depth--;
                    if (depth == 0) { body_end = j; break; }
                }
                j++;
            }

            // Itera sobre cada item
            const char *ip = items_str;
            while (*ip) {
                while (*ip == ' ') ip++;
                if (*ip == '\0') break;

                char item[64];
                int iti = 0;
                while (*ip && *ip != ' ' && iti < 63) {
                    item[iti++] = *ip++;
                }
                item[iti] = '\0';

                shell_setenv(var_name, item);

                for (int b = body_start; b < body_end; b++) {
                    execute_line(lines[b]);
                }
            }

            i = body_end + 1;
            continue;
        }

        // --- function name { ... } ---
        if (kstrncmp(line, "function ", 9) == 0) {
            const char *p = line + 9;
            while (*p == ' ') p++;

            char fname[FUNC_NAME_MAX];
            int fi = 0;
            while (*p && *p != ' ' && *p != '{' && fi < FUNC_NAME_MAX - 1) {
                fname[fi++] = *p++;
            }
            fname[fi] = '\0';

            // Coleta linhas até '}'
            i++;
            const char *body[FUNC_LINES_MAX];
            int body_count = 0;
            while (i < num_lines) {
                const char *l = lines[i];
                while (*l == ' ') l++;
                if (*l == '{') { i++; continue; }  // pula '{'
                if (*l == '}') { i++; break; }
                if (body_count < FUNC_LINES_MAX) {
                    body[body_count++] = lines[i];
                }
                i++;
            }
            script_define_function(fname, body, body_count);
            continue;
        }

        // Linha normal — executa via shell
        execute_line(line);
        i++;
    }

    return 0;
}

// ============================================================
// Executor de arquivo .sh
// ============================================================
int script_execute_file(const char *path) {
    vfs_node_t *file = vfs_resolve(path, current_dir, NULL, 0);
    if (!file) {
        vga_puts_color("script: arquivo nao encontrado: ", THEME_ERROR);
        vga_puts_color(path, THEME_WARNING);
        vga_putchar('\n');
        return -1;
    }

    if (file->type & VFS_DIRECTORY) {
        vga_puts_color("script: nao e um arquivo: ", THEME_ERROR);
        vga_puts_color(path, THEME_WARNING);
        vga_putchar('\n');
        return -1;
    }

    if (file->size == 0) return 0;

    // Lê o arquivo
    static uint8_t script_buf[4096];
    uint32_t size = file->size < 4095 ? file->size : 4095;
    uint32_t bytes = vfs_read(file, 0, size, script_buf);
    script_buf[bytes] = '\0';

    // Separa em linhas
    const char *lines[128];
    int num_lines = 0;
    char *s = (char *)script_buf;

    while (*s && num_lines < 128) {
        // Pula linhas em branco
        while (*s == '\r') s++;
        if (*s == '\0') break;

        lines[num_lines++] = s;

        // Avança até \n
        while (*s && *s != '\n') s++;
        if (*s == '\n') {
            *s = '\0';
            s++;
        }
    }

    // Pula shebang (#!)
    int start = 0;
    if (num_lines > 0 && lines[0][0] == '#' && lines[0][1] == '!') {
        start = 1;
    }

    return script_execute_lines(lines + start, num_lines - start);
}

// ============================================================
// Tenta processar controle de fluxo interativo (stub)
// Para uso futuro — o shell interativo poderia acumular linhas
// ============================================================
int script_try_control(const char *line) {
    (void)line;
    return 0;  // Não implementado para modo interativo
}
