// LeonardOS - Comando: help
// Exibe a lista de comandos organizada por categoria

#include "cmd_help.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"

static void help_cmd(const char *name, const char *desc) {
    vga_puts_color("  ", THEME_DEFAULT);
    vga_puts_color(name, THEME_INFO);
    int len = kstrlen(name);
    for (int i = len; i < 10; i++) vga_putchar(' ');
    vga_puts_color(desc, THEME_DIM);
    vga_putchar('\n');
}

static void help_section(const char *title) {
    vga_puts_color("\n ", THEME_DEFAULT);
    vga_puts_color(title, THEME_TITLE);
    vga_putchar('\n');
}

void cmd_help(const char *args) {
    (void)args;

    vga_puts_color("\n LeonardOS", THEME_TITLE);
    vga_puts_color(" v0.1\n", THEME_DIM);

    help_section("Sistema");
    help_cmd("help",    "lista de comandos");
    help_cmd("clear",   "limpa a tela");
    help_cmd("sysinfo", "informacoes do sistema");
    help_cmd("mem",     "uso de memoria");
    help_cmd("df",      "uso de disco");
    help_cmd("env",     "variaveis de ambiente");
    help_cmd("test",    "teste automatizado");
    help_cmd("reboot",  "reinicia o sistema");
    help_cmd("halt",    "desliga o kernel");

    help_section("Arquivos");
    help_cmd("ls",      "listar diretorio");
    help_cmd("cat",     "ver arquivo");
    help_cmd("echo",    "escrever texto / > / >>");
    help_cmd("touch",   "criar arquivo");
    help_cmd("mkdir",   "criar diretorio");
    help_cmd("rm",      "remover (-r recursivo)");
    help_cmd("cp",      "copiar arquivo");
    help_cmd("stat",    "info de arquivo");
    help_cmd("pwd",     "diretorio atual");
    help_cmd("cd",      "mudar diretorio");

    help_section("Busca");
    help_cmd("find",    "buscar por nome");
    help_cmd("grep",    "buscar texto");
    help_cmd("tree",    "arvore de diretorios");
    help_cmd("wc",      "contar linhas/palavras");
    help_cmd("head",    "primeiras N linhas");

    help_section("Rede");
    help_cmd("ifconfig","config de rede");
    help_cmd("netstat", "estatisticas da NIC");
    help_cmd("ping",    "testa conectividade");
    help_cmd("nslookup","resolve DNS");
    help_cmd("wget",    "download HTTP");

    help_section("Script");
    help_cmd("source",  "executar script .sh");
    help_cmd("keytest", "diagnostico de teclado");

    vga_putchar('\n');
}
