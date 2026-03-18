// Small-DBMS v1.0
// Author: Freud(GlonSlon)
#ifndef MINIDB_H
#define MINIDB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define _POSIX_C_SOURCE 200809L

// === COLUMN TYPES ===
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL
} ColumnType;

// === COLUMN STRUCTURE ===
typedef struct {
    char name[64];
    ColumnType type;
} Column;

// === DYNAMICALLY SIZED CELL ===
typedef struct Cell {
    void* data;
    size_t size;
    ColumnType type;
    int is_null;
} Cell;

// === TABLE ===
typedef struct Table {
    char table_name[64];
    Column* columns;
    int column_count;
    void*** rows;
    int row_count;
    int row_capacity;
    struct Index** indexes;
    int index_count;
} Table;

// === FILE HEADER ===
typedef struct {
    char magic[4];
    char table_name[64];
    int column_count;
    int row_count;
} FileHeader;

// === COMPRESSED HEADER ===
typedef struct {
    char magic[8];
    char table_name[64];
    int column_count;
    int row_count;
    int compressed;
    size_t original_size;
    size_t compressed_size;
    int version;  // v1.0
} CompressedHeader;

// === VALUE TYPES ===
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_NULL
} ValueType;

// === VALUE ===
typedef struct Value {
    ValueType type;
    union {
        int i;
        float f;
        char* s;
    } data;
} Value;

// === OPERATORS ===
typedef enum {
    OP_EQ, OP_NE, OP_GT, OP_LT, OP_GTE, OP_LTE
} ConditionOp;

typedef enum {
    LOGICAL_AND, LOGICAL_OR
} LogicalOp;

typedef enum {
    AGG_COUNT, AGG_SUM, AGG_AVG, AGG_MIN, AGG_MAX
} AggFunction;

// === AST NODE TYPES ===
typedef enum {
    NODE_CREATE_TABLE,
    NODE_CREATE_INDEX,
    NODE_INSERT,
    NODE_SELECT_ALL,
    NODE_SELECT_WHERE,
    NODE_SELECT_ROW,
    NODE_SELECT_AGGREGATE,
    NODE_UPDATE_ROW,
    NODE_UPDATE_CONDITION,
    NODE_DELETE_ROW,
    NODE_DELETE_CONDITION,
    NODE_CONDITION_EXPR,
    NODE_LOGICAL_EXPR,
    NODE_ASSIGNMENT,
    NODE_ORDER_BY,
    NODE_EXPORT_CSV,
    NODE_IMPORT_CSV,
    NODE_COMPARE_TABLES
} NodeType;

// === AST NODE ===
struct ASTNode;
typedef struct ASTNode ASTNode;

typedef struct ASTNode {
    NodeType type;
    char* table_name;
    
    union {
        struct { char** columns; ColumnType* types; int column_count; } create;
        struct { char* index_name; char* table_name; char* column_name; } create_index;
        struct { Value** values; int value_count; } insert;
        struct { char** selected; int count; ASTNode* condition; ASTNode* order_by; } query;
        struct { AggFunction func; char* column; ASTNode* condition; } aggregate;
        struct { int row_number; ASTNode* assignments; } update;
        struct { ASTNode* condition; ASTNode* assignments; } update_cond;
        struct { int row_number; ASTNode* condition; } delete_node;
        struct { char* column; ConditionOp op; Value* value; ASTNode* left; ASTNode* right; } condition;
        struct { LogicalOp op; ASTNode* left; ASTNode* right; } logical;
        struct { char* column; Value* value; } assignment;
        struct { char* column; int ascending; } order_by;
        struct { char* table_a; char* table_b; char** columns; int count; } compare;
        struct { char* filename; } file_op;
    } data;
    
    struct ASTNode* next;
} ASTNode;

// === INDEX ===
typedef struct IndexEntry {
    void* key;
    int* row_numbers;
    int count;
    int capacity;
    struct IndexEntry* next;
} IndexEntry;

typedef struct Index {
    char name[64];
    char column_name[64];
    IndexEntry** buckets;
    int bucket_count;
} Index;

// === PROTOTYPES ===
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
