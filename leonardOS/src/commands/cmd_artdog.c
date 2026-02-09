// LeonardOS - artdog command (ASCII dog art)
// Prints a cute ASCII dog

#include "cmd_artdog.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

void cmd_artdog(const char *args) {
    (void)args; // Unused

    const char *dog =
        "       _,=.=,_\n"
        "      ,'=.     `\\___,\n"
        "     /    \\  (0     |        __ _\n"
        "    /      \\     ___/       /| | ''--.._\n"
        "    |      |     \\)         || |    ===|\\\n"
        "    ',   _/    .--'          || |   ====| |\n"
        "      `\"`; (                || |    ===|/\n"
        "         [[[[]]_..,_        \\|_|_..--;\"`\n"
        "         /  .--\"\"``\\\\          __)__|\n"
        "       .'       .\\,,||___     |        |\n"
        " (   .'     -\"\"`| `\"\";___)---'|________|__\n"
        " |\\ /         __|   [_____________________]\n"
        "  \\|       .-'  `\\        |.----------.|\n"
        "   \\  _           |       ||          ||\n"
        "mgg  (          .-' )      ||          ||\n"
        "      `\"\"\"\"\"\"\"\"\"\"\"\"\"` \"\"\"`         \"\"\"\n";

    vga_puts_color(dog, THEME_DEFAULT);
}
