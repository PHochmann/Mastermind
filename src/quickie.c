#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quickie.h"
#include "mastermind.h"
#include "util/console.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

int fb_scores[MM_MAX_NUM_FEEDBACKS] = {
    5, 0, 2, 7, 12, 3, 1, 6, 11, 4, 8, 10, 9, 13
};

void calculate_fb_scores(MM_Context *ctx)
{
    FeedbackSize_t num_feedbacks = mm_get_num_feedbacks(ctx);
    CodeSize_t num_codes         = mm_get_num_codes(ctx);

    for (Feedback_t fb = 0; fb < num_feedbacks; fb++)
    {
        fb_scores[fb] = 0;
        for (Code_t i = 0; i < num_codes; i++)
        {
            for (Code_t j = 0; j < num_codes; j++)
            {
                if (mm_get_feedback(ctx, i, j) == fb)
                {
                    fb_scores[fb]++;
                }
            }
        }
    }

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
            if (fb_scores[j] > fb_scores[max] || max == -1)
            {
                max = j;
            }
        }
        sorted[i]            = max;
        fb_scores[sorted[i]] = i;
    }

#ifdef DEBUG
    printf("Feedback scores: ");
    for (int i = 0; i < num_feedbacks; i++)
    {
        printf("[%d]=%d, ", i, fb_scores[i]);
    }
    printf("\n");
#endif
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

static Code_t change_guess_and_solution(MM_Match *match, CodeSize_t num_candidates, Code_t *candidates, int min_score, int max_score, Code_t *solution)
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
            if (mm_is_in_solution(match, j) && (score <= max_score && score >= min_score))
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
                if (mm_is_in_solution(match, j) && (score <= max_score && score >= min_score))
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
        printf("Next...\n");
#endif
    }
    printf("Error\n");
    return candidates[0];
}

void quickie(MM_Context *ctx)
{
    const int num_difficulties = 4;
    int difficulty;
    if (!readline_int("Difficulty", num_difficulties / 2, 1, num_difficulties, &difficulty))
    {
        return;
    }

    int max_fb    = mm_get_num_feedbacks(ctx) - 2;
    int score_min = (num_difficulties - difficulty) * (max_fb / num_difficulties);
    int score_max = score_min + (max_fb / num_difficulties);

    mm_init_feedback_lookup(ctx);
    Code_t solution = rand() % mm_get_num_codes(ctx);
    MM_Match *match = mm_new_match(ctx, true);

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
#ifdef DEBUG
            printf("Recommender returned %d codes\n", num_candidates);
#endif
        }

        Code_t guess = change_guess_and_solution(match, num_candidates, candidates, score_min, score_max, &solution);
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
