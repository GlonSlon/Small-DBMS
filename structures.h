#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define _POSIX_C_SOURCE 200809L

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL
} ColumnType;

typedef struct {
    char name[64];
    ColumnType type;
} Column;

typedef struct Cell {
    void* data;
    size_t size;
    ColumnType type;
    int is_null;
} Cell;

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

typedef struct {
    char magic[4];
    char table_name[64];
    int column_count;
    int row_count;
} FileHeader;

typedef struct {
    char magic[8];
    char table_name[64];
    int column_count;
    int row_count;
    int compressed;
    size_t original_size;
    size_t compressed_size;
    int version;
} CompressedHeader;

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_NULL
} ValueType;

typedef struct Value {
    ValueType type;
    union {
        int i;
        float f;
        char* s;
    } data;
} Value;

typedef enum {
    OP_EQ, OP_NE, OP_GT, OP_LT, OP_GTE, OP_LTE
} ConditionOp;

typedef enum {
    LOGICAL_AND, LOGICAL_OR
} LogicalOp;

typedef enum {
    AGG_COUNT, AGG_SUM, AGG_AVG, AGG_MIN, AGG_MAX
} AggFunction;

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

#endif