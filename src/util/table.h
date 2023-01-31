#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

// Can be changed without further modifications
#define TABLE_MAX_COLS 11

typedef enum
{
    TBL_BORDER_NONE,
    TBL_BORDER_SINGLE,
    TBL_BORDER_DOUBLE
} TableBorderStyle;

typedef enum
{
    TBL_H_ALIGN_LEFT,
    TBL_H_ALIGN_RIGHT,
    TBL_H_ALIGN_CENTER // Rounded to the left
} TableHAlign;

typedef enum
{
    TBL_V_ALIGN_TOP,
    TBL_V_ALIGN_BOTTOM,
    TBL_V_ALIGN_CENTER // Rounded to the top
} TableVAlign;

typedef struct Table Table;

// Data and printing
Table *tbl_get_new();
void tbl_set_alternative_style(Table *table);
void tbl_print(Table *table);
void tbl_fprint(Table *table, FILE *stream);
void tbl_free(Table *table);

// Control
void tbl_set_position(Table *table, size_t x, size_t y);
void tbl_next_row(Table *table);

// Cell insertion
void tbl_add_empty_cell(Table *table);
void tbl_add_cell(Table *table, const char *text);
void tbl_add_cells(Table *table, size_t num_cells, ...);
void tbl_add_cell_gc(Table *table, char *text);
void tbl_add_cell_fmt(Table *table, const char *fmt, ...);
void tbl_add_cell_vfmt(Table *table, const char *fmt, va_list args);
void tbl_add_cells_from_array(Table *table, size_t width, size_t height, const char **array);

// Settings
void tbl_set_default_alignments(Table *table, size_t num_alignments, const TableHAlign *hor_aligns, const TableVAlign *vert_aligns);
void tbl_override_vertical_alignment(Table *table, TableVAlign align);
void tbl_override_horizontal_alignment(Table *table, TableHAlign align);
void tbl_override_vertical_alignment_of_row(Table *table, TableVAlign align);
void tbl_override_horizontal_alignment_of_row(Table *table, TableHAlign align);
void tbl_set_hline(Table *table, TableBorderStyle style);
void tbl_set_vline(Table *table, size_t index, TableBorderStyle style);
void tbl_make_boxed(Table *table, TableBorderStyle style);
void tbl_set_all_vlines(Table *table, TableBorderStyle style);
void tbl_override_left_border(Table *table, TableBorderStyle style);
void tbl_override_above_border(Table *table, TableBorderStyle style);
void tbl_set_span(Table *table, size_t span_x, size_t span_y);
