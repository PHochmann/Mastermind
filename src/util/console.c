#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

void clear_input()
{
    printf("\x1b[1F"); // Move to beginning of previous line
    printf("\x1b[2K"); // Clear entire line
}

void clear_screen()
{
    printf("\033[1;1H\033[2J");
}

void any_key()
{
    printf("Press enter to continue...\n");
    getchar();
}

char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c - 'A' + 'a';
    }
    else if (c >= 'a' && c <= 'z')
    {
        return c;
    }
    else
    {
        return '~';
    }
}

char to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return c - 'a' + 'A';
    }
    else if (c >= 'A' && c <= 'Z')
    {
        return c;
    }
    else
    {
        return '~';
    }
}

uint8_t digit_to_uint(char c)
{
    return c - '0';
}
