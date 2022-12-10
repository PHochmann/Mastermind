#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <readline/readline.h>

#include "mastermind.h"
#include "table.h"
#include "console.h"
#include "string_util.h"
#include "singleplayer.h" 
#include "multiplayer/server.h"
#include "multiplayer/client.h"

#define RED "\033[38:2:255:000:000m"
#define GRN "\033[38:2:000:255:000m"
#define YEL "\033[38:2:250:237:000m"
#define BLU "\033[38:2:000:000:255m"
#define CYN "\033[38:2:065:253:254m"
#define ORN "\033[38:2:255:165:000m"
#define PIN "\033[38:2:219:112:147m"
#define DRG "\033[38:2:085:107:047m"
#define RST "\033[0m"

void multiplayer(MM_Context *ctx, const char * const * colors)
{
    char *input = NULL;
    input = readline("(c)lient or (s)erver? ");
    clear_input();
    switch (to_lower(input[0]))
    {
        case 'c':
            play_multiplayer("127.0.0.1", 8080, colors);
            break;
        case 's':
            start_server(ctx, 2, 1, 8080);
            break;
        default:
            break;
    }
    free(input);
}


int main()
{
    srand(time(NULL));

    const char * const colors[] = {
        ORN "Orange" RST,
        RED " Red  " RST,
        YEL "Yellow" RST,
        BLU " Blue " RST,
        CYN " Cyan " RST,
        GRN "Green " RST,
        DRG "DGreen" RST,
        PIN "Pink " RST
    };

    MM_Context *very_easy_ctx = mm_new_ctx(4, 3, 3, colors);
    MM_Context *easy_ctx = mm_new_ctx(10, 4, 6, colors);
    MM_Context *hard_ctx = mm_new_ctx(12, 5, 8, colors);

    while (true)
    {
        char *input = readline("(s)ingle player, local (d)uel, (m)ultiplayer, (r)ecommender or (e)xit? ");
        clear_input();
        bool exit = false;

        switch (to_lower(input[0]))
        {
            case 's':
                play_singleplayer(very_easy_ctx);
                break;
            case 'd':
                play_duel(very_easy_ctx, 2);
                break;
            case 'r':
                play_recommender(very_easy_ctx, MM_STRAT_AVERAGE);
                break;
            case 'm':
                multiplayer(easy_ctx, colors);
                break;
            case 'e':
                exit = true;
                break;
        }
        free(input);
        if (exit)
        {
            break;
        }
    }

    mm_free_ctx(very_easy_ctx);
    mm_free_ctx(easy_ctx);
    mm_free_ctx(hard_ctx);
    return 0;
}
