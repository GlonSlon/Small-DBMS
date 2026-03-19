#ifndef PREFUNC_H
#define PREFUNC_H

#include "structures.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define _POSIX_C_SOURCE 200809L

Table* create_table(const char* name, Column* columns, int column_count);
void free_table(Table* table);
int add_row(Table* table, void** row_data);
Cell* get_cell(Table* table, int row, int col);
void* get_cell_value(Table* table, int row, int col);
Cell* value_to_cell(Value* val);
void* cell_to_data(Cell* cell);
int update_row(Table* table, int row_num, ASTNode* assignments);
int update_rows_where(Table* table, ASTNode* condition, ASTNode* assignments);
int delete_row(Table* table, int row_num);
int delete_rows_where(Table* table, ASTNode* condition);
void print_table(Table* table);
void print_cell_value(void* data, ColumnType type);
void format_cell_value(char* buf, size_t bufsize, void* data, ColumnType type);
int evaluate_condition_on_row(ASTNode* cond, Table* table, int row);
int compare_values(void* data, Value* val, ColumnType type);
int find_column_index(Table* table, const char* column_name);
ColumnType get_column_type(Table* table, const char* column_name);

// Save with compression
int save_table(Table* table, const char* filename);
int save_table_compressed(Table* table, const char* filename);
Table* load_table(const char* filename);
Table* load_table_compressed(const char* filename);
void save_all_tables(Table** tables, int table_count);
int load_all_tables(Table** tables, int max_tables);
Table* find_table(Table** tables, int table_count, const char* name);

// Indexes
Index* create_index(Table* table, const char* index_name, const char* column_name);
int* find_rows_by_index(Table* table, ASTNode* cond, int* count);

// Sorting
int* sort_rows(Table* table, const char* column, int ascending, int* sorted_count);

// Aggregates
long long aggregate_count(Table* table, ASTNode* condition, const char* column);
double aggregate_sum(Table* table, ASTNode* condition, const char* column);
double aggregate_avg(Table* table, ASTNode* condition, const char* column);
double aggregate_min(Table* table, ASTNode* condition, const char* column);
double aggregate_max(Table* table, ASTNode* condition, const char* column);
void execute_aggregate(Table* table, ASTNode* ast);

// CSV
int export_to_csv(Table* table, const char* filename);
int import_from_csv(Table* table, const char* filename);

// Parser
ASTNode* parse_command(char* input);
void free_ast_node(ASTNode* node);
char* parse_identifier(char** input);
char* parse_quoted_string(char** input);
Value* parse_value(char** input);
ASTNode* parse_condition(char** input);
ASTNode* parse_insert_values(char** input, char* table_name);
ASTNode* parse_row_operation(char** input, char* table_name);
ASTNode* parse_assignments(char** input);
ASTNode* parse_condition_operation(char** input, char* table_name);
ASTNode* parse_create(char** input);
ASTNode* parse_create_index(char** input);
ASTNode* parse_order_by(char** input);
ASTNode* parse_aggregate(char** input);
ASTNode* parse_export_import(char** input);
ASTNode* create_node(NodeType type);
ASTNode* create_condition_node(const char* column, ConditionOp op, Value* val);
void free_value(Value* val);

#endif
