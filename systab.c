// systab.c
#define _POSIX_C_SOURCE 200809L
#include "minidb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Create AST node
ASTNode* create_node(NodeType type) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = type;
    node->table_name = NULL;
    node->next = NULL;
    memset(&node->data, 0, sizeof(node->data));
    
    return node;
}

// Free value
void free_value(Value* val) {
    if (!val) return;
    if (val->type == VAL_STRING && val->data.s) {
        free(val->data.s);
    }
    free(val);
}

// Skip whitespace
void skip_whitespace(char** input) {
    while (**input && isspace(**input)) {
        (*input)++;
    }
}

// Check identifier
int is_valid_identifier(char c) {
    return isalnum(c) || c == '_';
}

// Parsing identifier
char* parse_identifier(char** input) {
    char buffer[256];
    int i = 0;
    
    while (**input && is_valid_identifier(**input) && i < 255) {
        buffer[i++] = *(*input)++;
    }
    buffer[i] = '\0';
    
    if (i == 0) return NULL;
    return strdup(buffer);
}

// Parsing quoted string
char* parse_quoted_string(char** input) {
    if (**input != '"' && **input != '\'') {
        return NULL;
    }
    
    char quote_char = **input;
    (*input)++;
    
    char buffer[1024];
    int i = 0;
    
    while (**input && **input != quote_char && i < 1023) {
        if (**input == '\\') {
            (*input)++;
            switch (**input) {
                case 'n': buffer[i++] = '\n'; break;
                case 't': buffer[i++] = '\t'; break;
                case '"': buffer[i++] = '"'; break;
                case '\\': buffer[i++] = '\\'; break;
                default: buffer[i++] = **input; break;
            }
        } else {
            buffer[i++] = **input;
        }
        (*input)++;
    }
    buffer[i] = '\0';
    
    if (**input == quote_char) {
        (*input)++;
    }
    
    return strdup(buffer);
}

// Parsing value
Value* parse_value(char** input) {
    skip_whitespace(input);
    
    if (!**input) return NULL;
    
    Value* val = (Value*)malloc(sizeof(Value));
    
    if (**input == '"' || **input == '\'') {
        val->type = VAL_STRING;
        val->data.s = parse_quoted_string(input);
        return val;
    }
    
    char* endptr;
    long int_val = strtol(*input, &endptr, 10);
    
    if (endptr != *input) {
        if (*endptr == '.') {
            val->type = VAL_FLOAT;
            val->data.f = strtof(*input, &endptr);
            *input = endptr;
        } else {
            val->type = VAL_INT;
            val->data.i = (int)int_val;
            *input = endptr;
        }
        return val;
    }
    
    if (is_valid_identifier(**input)) {
        char* ident = parse_identifier(input);
        if (strcmp(ident, "true") == 0 || strcmp(ident, "false") == 0) {
            val->type = VAL_INT;
            val->data.i = (strcmp(ident, "true") == 0) ? 1 : 0;
            free(ident);
            return val;
        }
        if (strcmp(ident, "null") == 0) {
            val->type = VAL_NULL;
            free(ident);
            return val;
        }
        val->type = VAL_STRING;
        val->data.s = ident;
        return val;
    }
    
    free(val);
    return NULL;
}

// Create condition node
ASTNode* create_condition_node(const char* column, ConditionOp op, Value* val) {
    ASTNode* node = create_node(NODE_CONDITION_EXPR);
    if (!node) return NULL;
    
    node->data.condition.column = strdup(column);
    node->data.condition.op = op;
    node->data.condition.value = val;
    node->data.condition.left = NULL;
    node->data.condition.right = NULL;
    
    return node;
}

// Parsing condition
ASTNode* parse_condition(char** input) {
    skip_whitespace(input);
    
    if (!**input || **input == ']') {
        return NULL;
    }
    
    char* column = parse_identifier(input);
    if (!column) {
        printf("Error: need column name\n");
        return NULL;
    }
    
    skip_whitespace(input);
    
    if (**input == ']') {
        if (strcmp(column, "true") == 0 || strcmp(column, "false") == 0) {
            Value* val = (Value*)malloc(sizeof(Value));
            val->type = VAL_INT;
            val->data.i = (strcmp(column, "true") == 0) ? 1 : 0;
            free(column);
            
            ASTNode* node = create_node(NODE_CONDITION_EXPR);
            node->data.condition.column = strdup("__TRUE__");
            node->data.condition.op = OP_EQ;
            node->data.condition.value = val;
            return node;
        }
        printf("Error: need column name\n");
        free(column);
        return NULL;
    }
    
    skip_whitespace(input);
    
    ConditionOp op;
    if (strncmp(*input, ">=", 2) == 0) {
        op = OP_GTE;
        *input += 2;
    }
    else if (strncmp(*input, "<=", 2) == 0) {
        op = OP_LTE;
        *input += 2;
    }
    else if (strncmp(*input, "!=", 2) == 0) {
        op = OP_NE;
        *input += 2;
    }
    else if (strncmp(*input, "==", 2) == 0) {
        op = OP_EQ;
        *input += 2;
    }
    else if (**input == '>') {
        op = OP_GT;
        (*input)++;
    }
    else if (**input == '<') {
        op = OP_LT;
        (*input)++;
    }
    else if (**input == '=') {
        op = OP_EQ;
        (*input)++;
    }
    else {
        printf("Error: comparison operator expected, but '%c'\n", **input);
        free(column);
        return NULL;
    }
    
    skip_whitespace(input);
    
    Value* val = parse_value(input);
    if (!val) {
        printf("Error: Expected a value after operator\n");
        free(column);
        return NULL;
    }
    
    ASTNode* cond = create_condition_node(column, op, val);
    free(column);
    
    skip_whitespace(input);
    
    if (**input == '&' && *(*input + 1) == '&') {
        *input += 2;
        skip_whitespace(input);
        
        ASTNode* right = parse_condition(input);
        if (!right) {
            free_ast_node(cond);
            return NULL;
        }
        
        ASTNode* logical = create_node(NODE_LOGICAL_EXPR);
        logical->data.logical.op = LOGICAL_AND;
        logical->data.logical.left = cond;
        logical->data.logical.right = right;
        return logical;
    }
    else if (**input == '|' && *(*input + 1) == '|') {
        *input += 2;
        skip_whitespace(input);
        
        ASTNode* right = parse_condition(input);
        if (!right) {
            free_ast_node(cond);
            return NULL;
        }
        
        ASTNode* logical = create_node(NODE_LOGICAL_EXPR);
        logical->data.logical.op = LOGICAL_OR;
        logical->data.logical.left = cond;
        logical->data.logical.right = right;
        return logical;
    }
    
    return cond;
}

// Create assignments from values
ASTNode* create_assignments_from_values(Value** values, int count) {
    ASTNode* head = NULL;
    ASTNode* current = NULL;
    
    for (int i = 0; i < count; i++) {
        char col_name[32];
        snprintf(col_name, sizeof(col_name), "col%d", i);
        
        ASTNode* assign = create_node(NODE_ASSIGNMENT);
        assign->data.assignment.column = strdup(col_name);
        assign->data.assignment.value = values[i];
        assign->next = NULL;
        
        if (!head) {
            head = assign;
            current = assign;
        } else {
            current->next = assign;
            current = assign;
        }
    }
    
    return head;
}

// Parsing values for INSERT
ASTNode* parse_insert_values(char** input, char* table_name) {
    ASTNode* node = create_node(NODE_INSERT);
    node->table_name = strdup(table_name);
    node->data.insert.values = NULL;
    node->data.insert.value_count = 0;
    
    skip_whitespace(input);
    
    if (**input != '[') {
        free_ast_node(node);
        return NULL;
    }
    (*input)++;
    
    skip_whitespace(input);
    
    Value** values = NULL;
    int count = 0;
    int capacity = 10;
    
    values = (Value**)malloc(capacity * sizeof(Value*));
    
    while (**input && **input != ']') {
        skip_whitespace(input);
        
        Value* val = parse_value(input);
        if (!val) break;
        
        if (count >= capacity) {
            capacity *= 2;
            values = (Value**)realloc(values, capacity * sizeof(Value*));
        }
        
        values[count] = val;
        count++;
        
        skip_whitespace(input);
        
        if (**input == ',') {
            (*input)++;
            skip_whitespace(input);
        }
        else if (**input != ']') {
            printf("Error: expected ',' or ']'\n");
            for (int i = 0; i < count; i++) {
                free_value(values[i]);
            }
            free(values);
            free_ast_node(node);
            return NULL;
        }
    }
    
    if (**input == ']') {
        (*input)++;
    }
    
    node->data.insert.values = values;
    node->data.insert.value_count = count;
    
    return node;
}

// Parsing assignments
ASTNode* parse_assignments(char** input) {
    ASTNode* head = NULL;
    ASTNode* current = NULL;
    
    while (**input && **input != ']') {
        skip_whitespace(input);
        
        char* column = parse_identifier(input);
        if (!column) break;
        
        skip_whitespace(input);
        
        if (**input != '=') {
            free(column);
            break;
        }
        (*input)++;
        
        skip_whitespace(input);
        
        Value* val = parse_value(input);
        if (!val) {
            free(column);
            break;
        }
        
        ASTNode* assign = create_node(NODE_ASSIGNMENT);
        assign->data.assignment.column = column;
        assign->data.assignment.value = val;
        assign->next = NULL;
        
        if (!head) {
            head = assign;
            current = assign;
        } else {
            current->next = assign;
            current = assign;
        }
        
        skip_whitespace(input);
        
        if (**input == ',') {
            (*input)++;
        }
    }
    
    return head;
}

// Parsing row operation
ASTNode* parse_row_operation(char** input, char* table_name) {
    skip_whitespace(input);
    
    char* endptr;
    long row_num = strtol(*input, &endptr, 10);
    if (endptr == *input) {
        printf("Error: Expected line number after '#'\n");
        return NULL;
    }
    *input = endptr;
    
    skip_whitespace(input);
    
    if (**input == '\0') {
        ASTNode* node = create_node(NODE_SELECT_ROW);
        node->table_name = strdup(table_name);
        node->data.delete_node.row_number = (int)row_num;
        return node;
    }
    
    if (**input == '-') {
        ASTNode* node = create_node(NODE_DELETE_ROW);
        node->table_name = strdup(table_name);
        node->data.delete_node.row_number = (int)row_num;
        node->data.delete_node.condition = NULL;
        (*input)++;
        return node;
    }
    else if (strncmp(*input, "SET", 3) == 0 || strncmp(*input, "set", 3) == 0) {
        *input += 3;
        skip_whitespace(input);
        
        ASTNode* node = create_node(NODE_UPDATE_ROW);
        node->table_name = strdup(table_name);
        node->data.update.row_number = (int)row_num;
        
        if (**input == '[') {
            (*input)++;
            node->data.update.assignments = parse_assignments(input);
            if (**input == ']') {
                (*input)++;
            }
        }
        
        return node;
    }
    else if (**input == '=') {
        (*input)++;
        skip_whitespace(input);
        
        if (**input == '[') {
            (*input)++;
            
            ASTNode* node = create_node(NODE_UPDATE_ROW);
            node->table_name = strdup(table_name);
            node->data.update.row_number = (int)row_num;
            
            Value** values = NULL;
            int count = 0;
            
            while (**input && **input != ']') {
                skip_whitespace(input);
                Value* val = parse_value(input);
                if (!val) break;
                
                values = (Value**)realloc(values, (count + 1) * sizeof(Value*));
                values[count] = val;
                count++;
                
                skip_whitespace(input);
                if (**input == ',') {
                    (*input)++;
                }
            }
            
            if (**input == ']') {
                (*input)++;
            }
            
            node->data.update.assignments = create_assignments_from_values(values, count);
            free(values);
            
            return node;
        }
    }
    
    printf("Error: Unknown operation after row number\n");
    return NULL;
}

// Parsing ORDER BY
ASTNode* parse_order_by(char** input) {
    skip_whitespace(input);
    
    if (strncmp(*input, "ORDER", 5) != 0 && strncmp(*input, "order", 5) != 0) {
        return NULL;
    }
    *input += 5;
    
    skip_whitespace(input);
    
    if (strncmp(*input, "BY", 2) != 0 && strncmp(*input, "by", 2) != 0) {
        return NULL;
    }
    *input += 2;
    
    skip_whitespace(input);
    
    char* column = parse_identifier(input);
    if (!column) return NULL;
    
    int ascending = 1;
    
    skip_whitespace(input);
    
    if (strncmp(*input, "DESC", 4) == 0 || strncmp(*input, "desc", 4) == 0) {
        ascending = 0;
        *input += 4;
    }
    else if (strncmp(*input, "ASC", 3) == 0 || strncmp(*input, "asc", 3) == 0) {
        ascending = 1;
        *input += 3;
    }
    
    ASTNode* node = create_node(NODE_ORDER_BY);
    node->data.order_by.column = column;
    node->data.order_by.ascending = ascending;
    
    return node;
}

// Parsing condition operation
ASTNode* parse_condition_operation(char** input, char* table_name) {
    if (**input != '[') return NULL;
    (*input)++;
    
    ASTNode* condition = parse_condition(input);
    
    if (**input != ']') {
        printf("Error: expected ']' after condition\n");
        free_ast_node(condition);
        return NULL;
    }
    (*input)++;
    
    skip_whitespace(input);
    
    if (**input == '-') {
        ASTNode* node = create_node(NODE_DELETE_CONDITION);
        node->table_name = strdup(table_name);
        node->data.delete_node.condition = condition;
        (*input)++;
        return node;
    }
    else if (strncmp(*input, "SET", 3) == 0 || strncmp(*input, "set", 3) == 0) {
        *input += 3;
        skip_whitespace(input);
        
        ASTNode* node = create_node(NODE_UPDATE_CONDITION);
        node->table_name = strdup(table_name);
        node->data.update_cond.condition = condition;
        
        if (**input == '[') {
            (*input)++;
            node->data.update_cond.assignments = parse_assignments(input);
            if (**input == ']') {
                (*input)++;
            }
        }
        
        return node;
    }
    else {
        ASTNode* node = create_node(NODE_SELECT_WHERE);
        node->table_name = strdup(table_name);
        node->data.query.condition = condition;
        node->data.query.selected = NULL;
        node->data.query.count = 0;
        node->data.query.order_by = NULL;
        
        skip_whitespace(input);
        
        // Check for ORDER BY
        ASTNode* order_by = parse_order_by(input);
        if (order_by) {
            node->data.query.order_by = order_by;
        }
        
        return node;
    }
}

// Parsing CREATE
ASTNode* parse_create(char** input) {
    skip_whitespace(input);
    
    char* table_name = parse_identifier(input);
    if (!table_name) return NULL;
    
    skip_whitespace(input);
    
    if (**input != '[') {
        printf("Error: expected '[' after table name\n");
        free(table_name);
        return NULL;
    }
    (*input)++;
    
    char** columns = NULL;
    ColumnType* types = NULL;
    int count = 0;
    int capacity = 10;
    
    columns = (char**)malloc(capacity * sizeof(char*));
    types = (ColumnType*)malloc(capacity * sizeof(ColumnType));
    
    while (**input && **input != ']') {
        skip_whitespace(input);
        
        char* col_name = parse_identifier(input);
        if (!col_name) break;
        
        ColumnType col_type = TYPE_STRING;
        
        skip_whitespace(input);
        if (**input == ':') {
            (*input)++;
            skip_whitespace(input);
            
            char* type_name = parse_identifier(input);
            if (type_name) {
                if (strcmp(type_name, "int") == 0 || strcmp(type_name, "INT") == 0) {
                    col_type = TYPE_INT;
                } else if (strcmp(type_name, "float") == 0 || strcmp(type_name, "FLOAT") == 0) {
                    col_type = TYPE_FLOAT;
                } else if (strcmp(type_name, "string") == 0 || strcmp(type_name, "STRING") == 0) {
                    col_type = TYPE_STRING;
                } else if (strcmp(type_name, "bool") == 0 || strcmp(type_name, "BOOL") == 0) {
                    col_type = TYPE_BOOL;
                }
                free(type_name);
            }
        }
        
        if (count >= capacity) {
            capacity *= 2;
            columns = (char**)realloc(columns, capacity * sizeof(char*));
            types = (ColumnType*)realloc(types, capacity * sizeof(ColumnType));
        }
        
        columns[count] = col_name;
        types[count] = col_type;
        count++;
        
        skip_whitespace(input);
        
        if (**input == ',') {
            (*input)++;
        }
        else if (**input != ']') {
            for (int i = 0; i < count; i++) free(columns[i]);
            free(columns);
            free(types);
            free(table_name);
            return NULL;
        }
    }
    
    if (**input == ']') {
        (*input)++;
    }
    
    ASTNode* node = create_node(NODE_CREATE_TABLE);
    node->table_name = table_name;
    node->data.create.columns = columns;
    node->data.create.types = types;
    node->data.create.column_count = count;
    
    return node;
}

// Parsing CREATE INDEX
ASTNode* parse_create_index(char** input) {
    skip_whitespace(input);
    
    char* index_name = parse_identifier(input);
    if (!index_name) return NULL;
    
    skip_whitespace(input);
    
    if (strncmp(*input, "ON", 2) != 0 && strncmp(*input, "on", 2) != 0) {
        free(index_name);
        return NULL;
    }
    *input += 2;
    
    skip_whitespace(input);
    
    char* table_name = parse_identifier(input);
    if (!table_name) {
        free(index_name);
        return NULL;
    }
    
    skip_whitespace(input);
    
    if (**input != '(') {
        free(index_name);
        free(table_name);
        return NULL;
    }
    (*input)++;
    
    skip_whitespace(input);
    
    char* column_name = parse_identifier(input);
    if (!column_name) {
        free(index_name);
        free(table_name);
        return NULL;
    }
    
    skip_whitespace(input);
    
    if (**input != ')') {
        free(index_name);
        free(table_name);
        free(column_name);
        return NULL;
    }
    (*input)++;
    
    ASTNode* node = create_node(NODE_CREATE_INDEX);
    node->data.create_index.index_name = index_name;
    node->data.create_index.table_name = table_name;
    node->data.create_index.column_name = column_name;
    
    return node;
}

// Parsing aggregate functions
ASTNode* parse_aggregate(char** input) {
    skip_whitespace(input);
    
    if (strncmp(*input, "SELECT", 6) != 0 && strncmp(*input, "select", 6) != 0) {
        return NULL;
    }
    *input += 6;
    
    skip_whitespace(input);
    
    char* func_name = parse_identifier(input);
    if (!func_name) return NULL;
    
    AggFunction func;
    if (strcmp(func_name, "COUNT") == 0 || strcmp(func_name, "count") == 0) {
        func = AGG_COUNT;
    } else if (strcmp(func_name, "SUM") == 0 || strcmp(func_name, "sum") == 0) {
        func = AGG_SUM;
    } else if (strcmp(func_name, "AVG") == 0 || strcmp(func_name, "avg") == 0) {
        func = AGG_AVG;
    } else if (strcmp(func_name, "MIN") == 0 || strcmp(func_name, "min") == 0) {
        func = AGG_MIN;
    } else if (strcmp(func_name, "MAX") == 0 || strcmp(func_name, "max") == 0) {
        func = AGG_MAX;
    } else {
        free(func_name);
        return NULL;
    }
    free(func_name);
    
    skip_whitespace(input);
    
    if (**input != '(') {
        return NULL;
    }
    (*input)++;
    
    skip_whitespace(input);
    
    char* column;
    if (**input == '*') {
        column = strdup("*");
        (*input)++;
    } else {
        column = parse_identifier(input);
        if (!column) return NULL;
    }
    
    skip_whitespace(input);
    
    if (**input != ')') {
        free(column);
        return NULL;
    }
    (*input)++;
    
    skip_whitespace(input);
    
    if (strncmp(*input, "FROM", 4) != 0 && strncmp(*input, "from", 4) != 0) {
        free(column);
        return NULL;
    }
    *input += 4;
    
    skip_whitespace(input);
    
    char* table_name = parse_identifier(input);
    if (!table_name) {
        free(column);
        return NULL;
    }
    
    skip_whitespace(input);
    
    ASTNode* condition = NULL;
    if (strncmp(*input, "WHERE", 5) == 0 || strncmp(*input, "where", 5) == 0) {
        *input += 5;
        skip_whitespace(input);
        
        if (**input == '[') {
            (*input)++;
            condition = parse_condition(input);
            if (**input == ']') {
                (*input)++;
            }
        }
    }
    
    ASTNode* node = create_node(NODE_SELECT_AGGREGATE);
    node->table_name = table_name;
    node->data.aggregate.func = func;
    node->data.aggregate.column = column;
    node->data.aggregate.condition = condition;
    
    return node;
}

// Parsing EXPORT/IMPORT
ASTNode* parse_export_import(char** input) {
    int is_export = 0;
    
    if (strncmp(*input, "EXPORT", 6) == 0 || strncmp(*input, "export", 6) == 0) {
        is_export = 1;
        *input += 6;
    }
    else if (strncmp(*input, "IMPORT", 6) == 0 || strncmp(*input, "import", 6) == 0) {
        is_export = 0;
        *input += 6;
    }
    else {
        return NULL;
    }
    
    skip_whitespace(input);
    
    char* table_name = parse_identifier(input);
    if (!table_name) return NULL;
    
    skip_whitespace(input);
    
    if (strncmp(*input, "TO", 2) != 0 && strncmp(*input, "to", 2) != 0 &&
        strncmp(*input, "FROM", 4) != 0 && strncmp(*input, "from", 4) != 0) {
        free(table_name);
        return NULL;
    }
    *input += (strncmp(*input, "FROM", 4) == 0 || strncmp(*input, "from", 4) == 0) ? 4 : 2;
    
    skip_whitespace(input);
    
    char* filename = parse_quoted_string(input);
    if (!filename) {
        free(table_name);
        return NULL;
    }
    
    ASTNode* node = create_node(is_export ? NODE_EXPORT_CSV : NODE_IMPORT_CSV);
    node->table_name = table_name;
    node->data.file_op.filename = filename;
    
    return node;
}

// Main parsing function
ASTNode* parse_command(char* input) {
    char* p = input;
    skip_whitespace(&p);
    
    if (!*p) return NULL;
    
    // Check for aggregate functions
    if (strncmp(p, "SELECT", 6) == 0 || strncmp(p, "select", 6) == 0) {
        return parse_aggregate(&p);
    }
    
    // Check for EXPORT/IMPORT
    if (strncmp(p, "EXPORT", 6) == 0 || strncmp(p, "export", 6) == 0 ||
        strncmp(p, "IMPORT", 6) == 0 || strncmp(p, "import", 6) == 0) {
        return parse_export_import(&p);
    }
    
    if (strncmp(p, "CREATE", 6) == 0 || strncmp(p, "create", 6) == 0) {
        p += 6;
        skip_whitespace(&p);
        
        if (strncmp(p, "INDEX", 5) == 0 || strncmp(p, "index", 5) == 0) {
            p += 5;
            return parse_create_index(&p);
        }
        return parse_create(&p);
    }
    
    char* table_name = parse_identifier(&p);
    if (!table_name) return NULL;
    
    skip_whitespace(&p);
    
    ASTNode* result = NULL;
    
    if (*p == '\0') {
        result = create_node(NODE_SELECT_ALL);
        result->table_name = table_name;
    }
    else if (*p == '+') {
        p++;
        result = parse_insert_values(&p, table_name);
    }
    else if (*p == '#') {
        p++;
        result = parse_row_operation(&p, table_name);
    }
    else if (*p == '[') {
        result = parse_condition_operation(&p, table_name);
    }
    else {
        free(table_name);
        return NULL;
    }
    
    if (!result && table_name) {
        free(table_name);
    }
    
    return result;
}

// Free AST node
void free_ast_node(ASTNode* node) {
    if (!node) return;
    
    if (node->table_name) {
        free(node->table_name);
    }
    
    switch (node->type) {
        case NODE_CONDITION_EXPR:
            free(node->data.condition.column);
            free_value(node->data.condition.value);
            break;
            
        case NODE_LOGICAL_EXPR:
            free_ast_node(node->data.logical.left);
            free_ast_node(node->data.logical.right);
            break;
            
        case NODE_ASSIGNMENT:
            free(node->data.assignment.column);
            free_value(node->data.assignment.value);
            if (node->next) {
                free_ast_node(node->next);
            }
            break;
            
        case NODE_INSERT:
            if (node->data.insert.values) {
                for (int i = 0; i < node->data.insert.value_count; i++) {
                    free_value(node->data.insert.values[i]);
                }
                free(node->data.insert.values);
            }
            break;
            
        case NODE_CREATE_TABLE:
            if (node->data.create.columns) {
                for (int i = 0; i < node->data.create.column_count; i++) {
                    free(node->data.create.columns[i]);
                }
                free(node->data.create.columns);
            }
            if (node->data.create.types) {
                free(node->data.create.types);
            }
            break;
            
        case NODE_CREATE_INDEX:
            free(node->data.create_index.index_name);
            free(node->data.create_index.table_name);
            free(node->data.create_index.column_name);
            break;
            
        case NODE_SELECT_WHERE:
            if (node->data.query.selected) {
                for (int i = 0; i < node->data.query.count; i++) {
                    free(node->data.query.selected[i]);
                }
                free(node->data.query.selected);
            }
            free_ast_node(node->data.query.condition);
            free_ast_node(node->data.query.order_by);
            break;
            
        case NODE_SELECT_AGGREGATE:
            free(node->data.aggregate.column);
            free_ast_node(node->data.aggregate.condition);
            break;
            
        case NODE_ORDER_BY:
            free(node->data.order_by.column);
            break;
            
        case NODE_EXPORT_CSV:
        case NODE_IMPORT_CSV:
            free(node->data.file_op.filename);
            break;
            
        case NODE_UPDATE_ROW:
            free_ast_node(node->data.update.assignments);
            break;
            
        case NODE_UPDATE_CONDITION:
            free_ast_node(node->data.update_cond.assignments);
            free_ast_node(node->data.update_cond.condition);
            break;
            
        case NODE_DELETE_ROW:
        case NODE_SELECT_ROW:
            break;
            
        case NODE_DELETE_CONDITION:
            free_ast_node(node->data.delete_node.condition);
            break;
            
        default:
            break;
    }
    
    free(node);
}