#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "quickie.h"
#include "mastermind.h"
#include "util/console.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Mapping from feedback to rank
double fb_scores[MM_MAX_NUM_FEEDBACKS] = {
    5, 0, 2, 7, 12, 3, 1, 6, 11, 4, 8, 10, 9, 13
};
int num_fbs = 14;

#define NUM_DIFFICULTIES 3
static const char *difficulty_labels[NUM_DIFFICULTIES] = { "Easy", "Medium", "Hard" };

void calculate_fb_scores(MM_Context *ctx)
{
    FeedbackSize_t num_feedbacks = mm_get_num_feedbacks(ctx);
    CodeSize_t num_codes         = mm_get_num_codes(ctx);

    num_fbs = 0;
    for (Feedback_t fb = 0; fb < num_feedbacks; fb++)
    {
        fb_scores[fb] = 0;
        int hits      = 0;
        for (Code_t i = 0; i < num_codes; i++)
        {
            bool hit = false;
            for (Code_t j = 0; j < num_codes; j++)
            {
                if (mm_get_feedback(ctx, i, j) == fb)
                {
                    hit = true;
                    fb_scores[fb]++;
                }
            }
            if (hit)
            {
                hits++;
            }
        }
        if (hits != 0)
        {
            fb_scores[fb] = (double)fb_scores[fb] / hits;
            num_fbs++;
        }
    }

#ifdef DEBUG
    printf("num codes: %d, ", num_codes);
    printf("num_fbs / reachable: %d / %d\n", mm_get_num_feedbacks(ctx), num_fbs);
#endif

    Feedback_t sorted[MM_MAX_NUM_FEEDBACKS];
    for (int i = 0; i < num_feedbacks; i++)
    {
        int max = -1;
        for (int j = 0; j < num_feedbacks; j++)
        {
            bool f = false;
            for (int k = 0; k < i; k++)
            {
                if (sorted[k] == j)
                {
                    f = true;
                    break;
                }
            }
            if (f)
            {
                continue;
            }
            if ((fb_scores[j] > fb_scores[max]) || (max == -1))
            {
                max = j;
            }
        }
        sorted[i] = max;
#ifdef DEBUG
        print_feedback(ctx, sorted[i]);
        printf("score: %f, %d\n", fb_scores[sorted[i]], i);
#endif
        fb_scores[sorted[i]] = i;
    }
}

static CodeSize_t recommend_guess(MM_Match *match, Code_t **candidates)
{
    MM_Context *ctx      = mm_get_context(match);
    CodeSize_t num_codes = mm_get_num_codes(ctx);
    long *aggregations   = malloc(num_codes * sizeof(long));

    if (mm_get_remaining_solutions(match) == 1)
    {
        for (Code_t j = 0; j < num_codes; j++)
        {
            if (mm_is_in_solution(match, j))
            {
                *candidates      = malloc(sizeof(Code_t) * 1);
                (*candidates)[0] = j;
                return 1;
            }
        }
    }

    for (Code_t i = 0; i < num_codes; i++)
    {
        aggregations[i] = 0;
        for (Feedback_t fb = 0; fb < mm_get_num_feedbacks(ctx); fb++)
        {
            CodeSize_t num_solutions = 0;
            for (Code_t j = 0; j < num_codes; j++)
            {
                if (mm_is_in_solution(match, j))
                {
                    if (mm_get_feedback(ctx, j, i) == fb)
                    {
                        num_solutions++;
                    }
                }
            }
            aggregations[i] = MAX(aggregations[i], num_solutions);
        }
    }

    int min            = INT32_MAX;
    int num_candidates = 0;
    for (Code_t i = 0; i < num_codes; i++)
    {
        if (aggregations[i] < min)
        {
            min            = aggregations[i];
            num_candidates = 0;
        }
        if (aggregations[i] == min)
        {
            num_candidates++;
        }
    }

    *candidates = malloc(sizeof(Code_t) * num_candidates);
    int counter = 0;
    for (Code_t i = 0; i < num_codes; i++)
    {
        if (aggregations[i] == min)
        {
            (*candidates)[counter++] = i;
        }
    }

    free(aggregations);
    return num_candidates;
}

static Code_t get_guess_and_solution(MM_Match *match, CodeSize_t num_candidates, Code_t *candidates, int min_score, int max_score, Code_t *solution)
{
    if (mm_get_remaining_solutions(match) == 1)
    {
        return candidates[0];
    }

    // Shuffle candidates
    for (CodeSize_t i = 0; i < num_candidates; i++)
    {
        int j         = rand() % num_candidates;
        Code_t temp   = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = temp;
    }

    MM_Context *ctx = mm_get_context(match);

    for (CodeSize_t i = 0; i < num_candidates; i++)
    {
        Code_t candidate = candidates[i];
        int viable       = 0;
        for (Code_t j = 0; j < mm_get_num_codes(ctx); j++)
        {
            int score = fb_scores[mm_get_feedback(ctx, candidate, j)];
            if (mm_is_in_solution(match, j) && (score < max_score && score >= min_score))
            {
                viable++;
            }
        }

        if (viable != 0)
        {
            int sol = rand() % viable;
            for (Code_t j = 0; j < mm_get_num_codes(ctx); j++)
            {
                int score = fb_scores[mm_get_feedback(ctx, candidate, j)];
                if (mm_is_in_solution(match, j) && (score < max_score && score >= min_score))
                {
                    if (sol == 0)
                    {
                        *solution = j;
                        return candidate;
                    }
                    else
                    {
                        sol--;
                    }
                }
            }
        }
#ifdef DEBUG
        printf("Skipping code...\n");
#endif
    }

#ifdef DEBUG
    printf("Ran out of guess/solution-pairs\n");
#endif

    for (Code_t code = 0; code < mm_get_num_codes(ctx); code++)
    {
        if (mm_is_in_solution(match, code))
        {
            *solution = code;
        }
    }

    return candidates[0];
}

void quickie(MM_Context *ctx)
{
    int difficulty;
    if (!readline_int("Difficulty", NUM_DIFFICULTIES / 2, 1, NUM_DIFFICULTIES, &difficulty))
    {
        return;
    }

    int score_min = (NUM_DIFFICULTIES - difficulty) * (num_fbs / NUM_DIFFICULTIES);
    int score_max = score_min + (num_fbs / NUM_DIFFICULTIES);

    if ((difficulty == 1) && (score_max != num_fbs - 1))
    {
        score_max = num_fbs - 1;
    }

#ifdef DEBUG
    printf("Min score (incl.): %d, Max score (excl.): %d, #fb: %d\n", score_min, score_max, num_fbs);
#endif

    mm_init_feedback_lookup(ctx);
    Code_t solution = rand() % mm_get_num_codes(ctx);
    MM_Match *match = mm_new_match(ctx, true);

    printf("~ ~ %s ~ ~\n", difficulty_labels[difficulty - 1]);

    while (mm_get_remaining_solutions(match) > 1)
    {
        Code_t *candidates;
        CodeSize_t num_candidates;
        if (mm_get_turns(match) == 0)
        {
            candidates     = malloc(sizeof(Code_t) * mm_get_num_codes(ctx));
            num_candidates = mm_get_num_codes(ctx);
            for (Code_t i = 0; i < mm_get_num_codes(ctx); i++)
            {
                candidates[i] = i;
            }
        }
        else
        {
            num_candidates = recommend_guess(match, &candidates);
        }

        Code_t guess = get_guess_and_solution(match, num_candidates, candidates, score_min, score_max, &solution);
        mm_constrain(match, guess, mm_get_feedback(ctx, guess, solution));
        print_guess(mm_get_turns(match) - 1, match, true);
        printf("\n");
        free(candidates);
    }

    Code_t input;
    if (read_colors(ctx, -1, &input))
    {
        mm_constrain(match, input, mm_get_feedback(ctx, input, solution));
        print_guess(mm_get_turns(match) - 1, match, true);
        printf("\n");
        print_match_end_message(match, solution, false);
    }
    else
    {
        printf("\n");
    }

    mm_free_match(match);
}
