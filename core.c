// core.c
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "minidb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAGIC "TBL1"
#define MAGIC_SIZE 4

// Create new table
Table* create_table(const char* name, Column* columns, int column_count) {
    Table* table = (Table*)malloc(sizeof(Table));
    if (!table) return NULL;
    
    memset(table, 0, sizeof(Table));
    
    strncpy(table->table_name, name, 63);
    table->table_name[63] = '\0';
    
    table->column_count = column_count;
    table->columns = (Column*)malloc(column_count * sizeof(Column));
    if (!table->columns) {
        free(table);
        return NULL;
    }
    memcpy(table->columns, columns, column_count * sizeof(Column));
    
    table->row_capacity = 10;
    table->row_count = 0;
    table->rows = (void***)calloc(table->row_capacity, sizeof(void**));
    
    // Initialize indexes
    table->indexes = NULL;
    table->index_count = 0;
    
    return table;
}

// Free table memory
void free_table(Table* table) {
    if (!table) return;
    
    for (int i = 0; i < table->row_count; i++) {
        for (int j = 0; j < table->column_count; j++) {
            if (table->rows[i][j]) {
                free(table->rows[i][j]);
            }
        }
        free(table->rows[i]);
    }
    
    free(table->rows);
    free(table->columns);
    
    for (int i = 0; i < table->index_count; i++) {
        if (table->indexes[i]) {
            for (int j = 0; j < table->indexes[i]->bucket_count; j++) {
                IndexEntry* entry = table->indexes[i]->buckets[j];
                while (entry) {
                    IndexEntry* next = entry->next;
                    free(entry->key);
                    free(entry->row_numbers);
                    free(entry);
                    entry = next;
                }
            }
            free(table->indexes[i]->buckets);
            free(table->indexes[i]);
        }
    }
    free(table->indexes);
    
    free(table);
}

// Add row to table
int add_row(Table* table, void** row_data) {
    if (!table || !row_data) return -1;
    
    if (table->row_count >= table->row_capacity) {
        table->row_capacity *= 2;
        table->rows = (void***)realloc(table->rows, 
                                        table->row_capacity * sizeof(void**));
    }
    
    table->rows[table->row_count] = (void**)malloc(table->column_count * sizeof(void*));
    
    for (int i = 0; i < table->column_count; i++) {
        if (row_data[i]) {
            switch (table->columns[i].type) {
                case TYPE_INT:
                case TYPE_BOOL: {
                    int* val = (int*)malloc(sizeof(int));
                    *val = *(int*)row_data[i];
                    table->rows[table->row_count][i] = val;
                    break;
                }
                case TYPE_FLOAT: {
                    float* val = (float*)malloc(sizeof(float));
                    *val = *(float*)row_data[i];
                    table->rows[table->row_count][i] = val;
                    break;
                }
                case TYPE_STRING: {
                    char* str = (char*)row_data[i];
                    table->rows[table->row_count][i] = strdup(str);
                    break;
                }
            }
        } else {
            table->rows[table->row_count][i] = NULL;
        }
    }
    
    return table->row_count++;
}

// Get cell value
void* get_cell_value(Table* table, int row, int col) {
    if (!table || row < 0 || row >= table->row_count || 
        col < 0 || col >= table->column_count) {
        return NULL;
    }
    
    return table->rows[row][col];
}

// Convert Value to table data
void* value_to_data(Value* val, ColumnType type) {
    if (!val || val->type == VAL_NULL) return NULL;
    
    switch (type) {
        case TYPE_INT:
            if (val->type == VAL_INT) {
                int* data = (int*)malloc(sizeof(int));
                *data = val->data.i;
                return data;
            } else if (val->type == VAL_STRING) {
                int* data = (int*)malloc(sizeof(int));
                *data = atoi(val->data.s);
                return data;
            } else if (val->type == VAL_FLOAT) {
                int* data = (int*)malloc(sizeof(int));
                *data = (int)val->data.f;
                return data;
            }
            break;
            
        case TYPE_FLOAT:
            if (val->type == VAL_FLOAT) {
                float* data = (float*)malloc(sizeof(float));
                *data = val->data.f;
                return data;
            } else if (val->type == VAL_INT) {
                float* data = (float*)malloc(sizeof(float));
                *data = (float)val->data.i;
                return data;
            } else if (val->type == VAL_STRING) {
                float* data = (float*)malloc(sizeof(float));
                *data = atof(val->data.s);
                return data;
            }
            break;
            
        case TYPE_STRING:
            if (val->type == VAL_STRING) {
                return strdup(val->data.s);
            } else if (val->type == VAL_INT) {
                char buffer[32];
                sprintf(buffer, "%d", val->data.i);
                return strdup(buffer);
            } else if (val->type == VAL_FLOAT) {
                char buffer[32];
                sprintf(buffer, "%g", val->data.f);
                return strdup(buffer);
            }
            break;
            
        case TYPE_BOOL:
            int* data = (int*)malloc(sizeof(int));
            if (val->type == VAL_INT) {
                *data = (val->data.i != 0) ? 1 : 0;
            } else if (val->type == VAL_STRING) {
                if (strcmp(val->data.s, "true") == 0 || 
                    strcmp(val->data.s, "True") == 0 ||
                    strcmp(val->data.s, "1") == 0) {
                    *data = 1;
                } else {
                    *data = 0;
                }
            } else if (val->type == VAL_FLOAT) {
                *data = (val->data.f != 0.0) ? 1 : 0;
            } else {
                *data = 0;
            }
            return data;
    }
    
    return NULL;
}

// Update row by number
int update_row(Table* table, int row_num, ASTNode* assignments) {
    if (!table || row_num < 0 || row_num >= table->row_count || !assignments) {
        return -1;
    }
    
    ASTNode* current = assignments;
    while (current) {
        if (current->type == NODE_ASSIGNMENT) {
            int col_index = find_column_index(table, current->data.assignment.column);
            
            if (col_index >= 0) {
                if (table->rows[row_num][col_index]) {
                    free(table->rows[row_num][col_index]);
                }
                
                table->rows[row_num][col_index] = 
                    value_to_data(current->data.assignment.value, 
                                 table->columns[col_index].type);
            }
        }
        current = current->next;
    }
    
    return row_num;
}

// Update rows by condition
int update_rows_where(Table* table, ASTNode* condition, ASTNode* assignments) {
    if (!table || !assignments) return 0;
    
    int updated = 0;
    for (int i = 0; i < table->row_count; i++) {
        if (!condition || evaluate_condition_on_row(condition, table, i)) {
            update_row(table, i, assignments);
            updated++;
        }
    }
    
    return updated;
}

// Delete row by number
int delete_row(Table* table, int row_num) {
    if (!table || row_num < 0 || row_num >= table->row_count) {
        return -1;
    }
    
    for (int j = 0; j < table->column_count; j++) {
        if (table->rows[row_num][j]) {
            free(table->rows[row_num][j]);
        }
    }
    free(table->rows[row_num]);
    
    for (int i = row_num; i < table->row_count - 1; i++) {
        table->rows[i] = table->rows[i + 1];
    }
    
    table->row_count--;
    
    return 0;
}

// Delete rows by condition
int delete_rows_where(Table* table, ASTNode* condition) {
    if (!table) return 0;
    
    int deleted = 0;
    for (int i = table->row_count - 1; i >= 0; i--) {
        if (!condition || evaluate_condition_on_row(condition, table, i)) {
            delete_row(table, i);
            deleted++;
        }
    }
    
    return deleted;
}

// Print table with word wrapping for long strings
void print_table(Table* table) {
    if (!table) {
        printf("Table does not exist\n");
        return;
    }
    
    if (table->row_count == 0) {
        printf("\nTable: %s (rows: 0)\n", table->table_name);
        return;
    }
    
    // Calculate max width for each column
    int* col_widths = malloc(table->column_count * sizeof(int));
    for (int j = 0; j < table->column_count; j++) {
        col_widths[j] = strlen(table->columns[j].name);
    }
    
    // Check actual data widths
    for (int i = 0; i < table->row_count; i++) {
        for (int j = 0; j < table->column_count; j++) {
            char buf[256];
            format_cell_value(buf, sizeof(buf), table->rows[i][j], table->columns[j].type);
            int len = strlen(buf);
            if (len > col_widths[j]) col_widths[j] = len;
        }
    }
    
    // Limit max width to 80 chars per cell to avoid crazy wide tables
    for (int j = 0; j < table->column_count; j++) {
        if (col_widths[j] > 80) col_widths[j] = 80;
    }
    
    // Print header
    printf("\nTable: %s (rows: %d)\n", table->table_name, table->row_count);
    
    // Top border
    for (int j = 0; j < table->column_count; j++) {
        printf("+");
        for (int k = 0; k < col_widths[j] + 2; k++) printf("-");
    }
    printf("+\n|");
    
    for (int j = 0; j < table->column_count; j++) {
        printf(" %-*s |", col_widths[j], table->columns[j].name);
    }
    printf("\n");
    
    // Header border
    for (int j = 0; j < table->column_count; j++) {
        printf("+");
        for (int k = 0; k < col_widths[j] + 2; k++) printf("-");
    }
    printf("+\n");
    
    // Print rows with wrapping
    for (int i = 0; i < table->row_count; i++) {
        // Get all cell values as strings
        char** cell_strs = malloc(table->column_count * sizeof(char*));
        int* cell_lens = malloc(table->column_count * sizeof(int));
        
        for (int j = 0; j < table->column_count; j++) {
            cell_strs[j] = malloc(1024);
            format_cell_value(cell_strs[j], 1024, table->rows[i][j], table->columns[j].type);
            cell_lens[j] = strlen(cell_strs[j]);
        }
        
        // Calculate max lines needed
        int max_lines = 1;
        for (int j = 0; j < table->column_count; j++) {
            int lines = (cell_lens[j] + col_widths[j] - 1) / col_widths[j];
            if (lines > max_lines) max_lines = lines;
        }
        
        // Print each line
        int* pos = malloc(table->column_count * sizeof(int));
        for (int j = 0; j < table->column_count; j++) {
            pos[j] = 0;
        }
        
        int line = 0;
        while (1) {
            printf("|");
            
            for (int j = 0; j < table->column_count; j++) {
                int start = pos[j];
                int len = col_widths[j];
                
                if (start >= cell_lens[j]) {
                    printf(" %-*s |", col_widths[j], "");
                } else {
                    if (start + len > cell_lens[j]) {
                        len = cell_lens[j] - start;
                    }
                    
                    // Copy chunk
                    char* chunk = malloc(len + 1);
                    strncpy(chunk, cell_strs[j] + start, len);
                    chunk[len] = '\0';
                    
                    // Check if there's more content after this chunk
                    int remaining = cell_lens[j] - (start + len);
                    if (remaining > 0 && len > 0 && chunk[len-1] != ' ') {
                        // We're in the middle of a word
                        char* last_space = strrchr(chunk, ' ');
                        if (last_space) {
                            // Break at the last space
                            int break_pos = last_space - chunk;
                            chunk[break_pos] = '\0';
                            // Update position for next line: skip to after the space
                            pos[j] = start + break_pos + 1;
                        } else {
                            // No space - word is longer than column width
                            // Truncate to column width
                            chunk[col_widths[j]] = '\0';
                            pos[j] = start + col_widths[j];
                        }
                    } else {
                        // Either no more content or ended on space
                        pos[j] = start + len;
                    }
                    
                    // Print with proper padding to column width
                    printf(" %-*s |", col_widths[j], chunk);
                    free(chunk);
                }
            }
            printf("\n");
            line++;
            
            // Check if any column has more content
            int all_done = 1;
            for (int j = 0; j < table->column_count; j++) {
                if (pos[j] < cell_lens[j]) {
                    all_done = 0;
                    break;
                }
            }
            if (all_done) break;
        }
        
        free(pos);
        
        // Free cell strings
        for (int j = 0; j < table->column_count; j++) {
            free(cell_strs[j]);
        }
        free(cell_strs);
        free(cell_lens);
    }
    
    // Bottom border
    for (int j = 0; j < table->column_count; j++) {
        printf("+");
        for (int k = 0; k < col_widths[j] + 2; k++) printf("-");
    }
    printf("+\n");
    
    free(col_widths);
}

// Format cell value to string (without truncation)
void format_cell_value(char* buf, size_t bufsize, void* data, ColumnType type) {
    if (!data) {
        snprintf(buf, bufsize, "NULL");
        return;
    }
    
    switch (type) {
        case TYPE_INT:
            snprintf(buf, bufsize, "%d", *(int*)data);
            break;
        case TYPE_FLOAT:
            snprintf(buf, bufsize, "%g", *(float*)data);
            break;
        case TYPE_STRING:
            snprintf(buf, bufsize, "%s", (char*)data);
            break;
        case TYPE_BOOL:
            snprintf(buf, bufsize, "%s", (*(int*)data) ? "true" : "false");
            break;
    }
}

// Print cell value (for compatibility)
void print_cell_value(void* data, ColumnType type) {
    char buf[256];
    format_cell_value(buf, sizeof(buf), data, type);
    printf("%s", buf);
}

// Compare values
int compare_values(void* data, Value* val, ColumnType type) {
    if (!data && !val) return 1;
    if (!data || !val) return 0;
    
    switch (type) {
        case TYPE_INT: {
            int int_val = *(int*)data;
            if (val->type == VAL_INT) {
                return int_val - val->data.i;
            } else if (val->type == VAL_FLOAT) {
                float diff = int_val - val->data.f;
                return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
            } else if (val->type == VAL_STRING) {
                return int_val - atoi(val->data.s);
            }
            break;
        }
        
        case TYPE_FLOAT: {
            float float_val = *(float*)data;
            if (val->type == VAL_FLOAT) {
                float diff = float_val - val->data.f;
                return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
            } else if (val->type == VAL_INT) {
                float diff = float_val - val->data.i;
                return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
            } else if (val->type == VAL_STRING) {
                return (float_val > atof(val->data.s)) ? 1 : 
                       (float_val < atof(val->data.s)) ? -1 : 0;
            }
            break;
        }
        
        case TYPE_STRING: {
            char* str_val = (char*)data;
            if (val->type == VAL_STRING) {
                return strcmp(str_val, val->data.s);
            } else if (val->type == VAL_INT) {
                char buffer[32];
                sprintf(buffer, "%d", val->data.i);
                return strcmp(str_val, buffer);
            } else if (val->type == VAL_FLOAT) {
                char buffer[32];
                sprintf(buffer, "%g", val->data.f);
                return strcmp(str_val, buffer);
            }
            break;
        }
        
        case TYPE_BOOL: {
            int bool_val = *(int*)data;
            int val_bool = 0;
            if (val->type == VAL_INT) {
                val_bool = (val->data.i != 0) ? 1 : 0;
            } else if (val->type == VAL_STRING) {
                val_bool = (strcmp(val->data.s, "true") == 0 ||
                           strcmp(val->data.s, "1") == 0) ? 1 : 0;
            } else if (val->type == VAL_FLOAT) {
                val_bool = (val->data.f != 0.0) ? 1 : 0;
            }
            return bool_val - val_bool;
        }
    }
    
    return -1;
}

// Evaluate condition for row
int evaluate_condition_on_row(ASTNode* cond, Table* table, int row) {
    if (!cond) return 1;
    
    if (cond->type == NODE_CONDITION_EXPR) {
        if (strcmp(cond->data.condition.column, "__TRUE__") == 0) {
            return cond->data.condition.value && 
                   cond->data.condition.value->type == VAL_INT && 
                   cond->data.condition.value->data.i != 0;
        }
        if (strcmp(cond->data.condition.column, "__FALSE__") == 0) {
            return 0;
        }
        
        void* cell_data = get_cell_value(table, row, 
            find_column_index(table, cond->data.condition.column));
        
        int cmp = compare_values(cell_data, cond->data.condition.value,
            get_column_type(table, cond->data.condition.column));
        
        switch (cond->data.condition.op) {
            case OP_EQ: return cmp == 0;
            case OP_NE: return cmp != 0;
            case OP_GT: return cmp > 0;
            case OP_LT: return cmp < 0;
            case OP_GTE: return cmp >= 0;
            case OP_LTE: return cmp <= 0;
            default: return 0;
        }
    }
    else if (cond->type == NODE_LOGICAL_EXPR) {
        int left = evaluate_condition_on_row(cond->data.logical.left, table, row);
        int right = evaluate_condition_on_row(cond->data.logical.right, table, row);
        
        if (cond->data.logical.op == LOGICAL_AND) {
            return left && right;
        } else {
            return left || right;
        }
    }
    
    return 0;
}

// Find column index
int find_column_index(Table* table, const char* column_name) {
    for (int i = 0; i < table->column_count; i++) {
        if (strcmp(table->columns[i].name, column_name) == 0) {
            return i;
        }
    }
    return -1;
}

// Get column type
ColumnType get_column_type(Table* table, const char* column_name) {
    int idx = find_column_index(table, column_name);
    if (idx >= 0) {
        return table->columns[idx].type;
    }
    return TYPE_STRING;
}

// Save table
int save_table(Table* table, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return -1;
    }
    
    FileHeader header;
    memcpy(header.magic, MAGIC, MAGIC_SIZE);
    strncpy(header.table_name, table->table_name, 63);
    header.table_name[63] = '\0';
    header.column_count = table->column_count;
    header.row_count = table->row_count;
    
    fwrite(&header, sizeof(FileHeader), 1, file);
    fwrite(table->columns, sizeof(Column), table->column_count, file);
    
    for (int i = 0; i < table->row_count; i++) {
        for (int j = 0; j < table->column_count; j++) {
            void* cell = table->rows[i][j];
            uint8_t has_data = (cell != NULL) ? 1 : 0;
            fwrite(&has_data, sizeof(uint8_t), 1, file);
            
            if (has_data) {
                switch (table->columns[j].type) {
                    case TYPE_INT:
                    case TYPE_BOOL:
                        fwrite(cell, sizeof(int), 1, file);
                        break;
                    case TYPE_FLOAT:
                        fwrite(cell, sizeof(float), 1, file);
                        break;
                    case TYPE_STRING: {
                        uint16_t len = strlen((char*)cell) + 1;
                        fwrite(&len, sizeof(uint16_t), 1, file);
                        fwrite(cell, len, 1, file);
                        break;
                    }
                }
            }
        }
    }
    
    fclose(file);
    return 0;
}

// Load table
Table* load_table(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return NULL;
    }
    
    FileHeader header;
    if (fread(&header, sizeof(FileHeader), 1, file) != 1) {
        fclose(file);
        return NULL;
    }
    
    if (memcmp(header.magic, MAGIC, MAGIC_SIZE) != 0) {
        fclose(file);
        return NULL;
    }
    
    Table* table = (Table*)malloc(sizeof(Table));
    strncpy(table->table_name, header.table_name, 63);
    table->table_name[63] = '\0';
    table->column_count = header.column_count;
    table->row_count = header.row_count;
    table->row_capacity = header.row_count + 10;
    table->indexes = NULL;
    table->index_count = 0;
    
    table->columns = (Column*)malloc(table->column_count * sizeof(Column));
    fread(table->columns, sizeof(Column), table->column_count, file);
    
    table->rows = (void***)malloc(table->row_capacity * sizeof(void**));
    
    for (int i = 0; i < table->row_count; i++) {
        table->rows[i] = (void**)malloc(table->column_count * sizeof(void*));
        
        for (int j = 0; j < table->column_count; j++) {
            uint8_t has_data;
            fread(&has_data, sizeof(uint8_t), 1, file);
            
            if (has_data) {
                switch (table->columns[j].type) {
                    case TYPE_INT:
                    case TYPE_BOOL: {
                        int* val = (int*)malloc(sizeof(int));
                        fread(val, sizeof(int), 1, file);
                        table->rows[i][j] = val;
                        break;
                    }
                    case TYPE_FLOAT: {
                        float* val = (float*)malloc(sizeof(float));
                        fread(val, sizeof(float), 1, file);
                        table->rows[i][j] = val;
                        break;
                    }
                    case TYPE_STRING: {
                        uint16_t len;
                        fread(&len, sizeof(uint16_t), 1, file);
                        char* str = (char*)malloc(len);
                        fread(str, len, 1, file);
                        table->rows[i][j] = str;
                        break;
                    }
                }
            } else {
                table->rows[i][j] = NULL;
            }
        }
    }
    
    fclose(file);
    return table;
}

// Save all tables
void save_all_tables(Table** tables, int table_count) {
    for (int i = 0; i < table_count; i++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s.tbl", tables[i]->table_name);
        save_table(tables[i], filename);
    }
}

// Load all tables
int load_all_tables(Table** tables, int max_tables) {
    (void)tables;
    (void)max_tables;
    return 0;
}

// Find table
Table* find_table(Table** tables, int table_count, const char* name) {
    for (int i = 0; i < table_count; i++) {
        if (strcmp(tables[i]->table_name, name) == 0) {
            return tables[i];
        }
    }
    return NULL;
}

// === INDEX FUNCTIONS ===

// Hash function for strings
unsigned int hash_string(const char* str, int bucket_count) {
    if (!str) return 0;
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % bucket_count;
}

// Hash function for integers
unsigned int hash_int(int value, int bucket_count) {
    return (unsigned int)value % bucket_count;
}

// Hash function for floats
unsigned int hash_float(float value, int bucket_count) {
    return (unsigned int)(*(int*)&value) % bucket_count;
}

// Create index
// Maximum safe version of create_index
Index* create_index(Table* table, const char* index_name, const char* column_name) {
    // Check input parameters
    if (!table) {
        fprintf(stderr, "ERROR: table is NULL\n");
        return NULL;
    }
    
    printf("DEBUG: create_index started\n");
    fflush(stdout);
    
    printf("DEBUG: table name=%s, rows=%d\n", table->table_name, table->row_count);
    fflush(stdout);
    
    // Check rows array
    if (!table->rows) {
        fprintf(stderr, "ERROR: table->rows is NULL\n");
        return NULL;
    }
    
    // Find column index
    int col_idx = -1;
    for (int i = 0; i < table->column_count; i++) {
        if (strcmp(table->columns[i].name, column_name) == 0) {
            col_idx = i;
            break;
        }
    }
    
    if (col_idx == -1) {
        fprintf(stderr, "ERROR: column '%s' not found\n", column_name);
        return NULL;
    }
    
    printf("DEBUG: column index = %d, type = %d\n", col_idx, table->columns[col_idx].type);
    fflush(stdout);
    
    // Allocate memory for index
    Index* idx = (Index*)calloc(1, sizeof(Index));
    if (!idx) {
        fprintf(stderr, "ERROR: failed to allocate Index\n");
        return NULL;
    }
    
    // Copy name
    strncpy(idx->name, index_name, 63);
    idx->name[63] = '\0';
    strncpy(idx->column_name, column_name, 63);
    idx->column_name[63] = '\0';
    
    // Create buckets
    idx->bucket_count = 100;
    idx->buckets = (IndexEntry**)calloc(idx->bucket_count, sizeof(IndexEntry*));
    if (!idx->buckets) {
        fprintf(stderr, "ERROR: failed to allocate buckets\n");
        free(idx);
        return NULL;
    }
    
    printf("DEBUG: buckets allocated, count=%d\n", idx->bucket_count);
    fflush(stdout);
    
    // Index rows
    for (int i = 0; i < table->row_count; i++) {
        // Check row
        if (!table->rows[i]) {
            printf("DEBUG: row %d is NULL, skipping\n", i);
            fflush(stdout);
            continue;
        }
        
        // Check value in column
        if (i >= table->row_count || !table->rows[i]) {
            printf("DEBUG: row %d is invalid, skipping\n", i);
            fflush(stdout);
            continue;
        }
        
        void* key_data = table->rows[i][col_idx];
        if (!key_data) {
            printf("DEBUG: row %d key is NULL, skipping\n", i);
            fflush(stdout);
            continue;
        }
        
        // Compute hash
        unsigned int hash = 0;
        switch (table->columns[col_idx].type) {
            case TYPE_INT:
            case TYPE_BOOL:
                hash = (*(int*)key_data) % idx->bucket_count;
                printf("DEBUG: row %d int=%d, hash=%u\n", i, *(int*)key_data, hash);
                break;
            case TYPE_FLOAT: {
                float f = *(float*)key_data;
                hash = ((unsigned int)(f * 1000)) % idx->bucket_count;
                printf("DEBUG: row %d float=%f, hash=%u\n", i, f, hash);
                break;
            }
            case TYPE_STRING: {
                char* s = (char*)key_data;
                hash = 0;
                for (int j = 0; s[j] && j < 10; j++) {
                    hash = hash * 31 + s[j];
                }
                hash = hash % idx->bucket_count;
                printf("DEBUG: row %d string=%s, hash=%u\n", i, s, hash);
                break;
            }
            default:
                printf("DEBUG: unknown type %d, skipping\n", table->columns[col_idx].type);
                continue;
        }
        fflush(stdout);
        
        // Find or create entry in buckets
        IndexEntry* entry = idx->buckets[hash];
        int found = 0;
        
        while (entry) {
            // Compare keys
            int keys_equal = 0;
            switch (table->columns[col_idx].type) {
                case TYPE_INT:
                case TYPE_BOOL:
                    if (entry->key && *(int*)entry->key == *(int*)key_data) {
                        keys_equal = 1;
                    }
                    break;
                case TYPE_FLOAT:
                    if (entry->key && *(float*)entry->key == *(float*)key_data) {
                        keys_equal = 1;
                    }
                    break;
                case TYPE_STRING:
                    if (entry->key && strcmp((char*)entry->key, (char*)key_data) == 0) {
                        keys_equal = 1;
                    }
                    break;
            }
            
            if (keys_equal) {
                // Add row number to existing entry
                if (entry->count >= entry->capacity) {
                    int new_cap = entry->capacity == 0 ? 4 : entry->capacity * 2;
                    int* new_rows = (int*)realloc(entry->row_numbers, new_cap * sizeof(int));
                    if (new_rows) {
                        entry->row_numbers = new_rows;
                        entry->capacity = new_cap;
                    } else {
                        fprintf(stderr, "ERROR: failed to realloc row_numbers\n");
                        break;
                    }
                }
                entry->row_numbers[entry->count] = i;
                entry->count++;
                found = 1;
                printf("DEBUG: added row %d to existing entry, count=%d\n", i, entry->count);
                fflush(stdout);
                break;
            }
            entry = entry->next;
        }
        
        if (!found) {
            // Create new entry
            printf("DEBUG: creating new entry for hash %u\n", hash);
            fflush(stdout);
            
            IndexEntry* new_entry = (IndexEntry*)calloc(1, sizeof(IndexEntry));
            if (!new_entry) {
                fprintf(stderr, "ERROR: failed to allocate new entry\n");
                continue;
            }
            
            // Copy key
            switch (table->columns[col_idx].type) {
                case TYPE_INT:
                case TYPE_BOOL: {
                    int* key_copy = (int*)malloc(sizeof(int));
                    if (key_copy) {
                        *key_copy = *(int*)key_data;
                        new_entry->key = key_copy;
                    }
                    break;
                }
                case TYPE_FLOAT: {
                    float* key_copy = (float*)malloc(sizeof(float));
                    if (key_copy) {
                        *key_copy = *(float*)key_data;
                        new_entry->key = key_copy;
                    }
                    break;
                }
                case TYPE_STRING:
                    new_entry->key = strdup((char*)key_data);
                    break;
            }
            
            if (!new_entry->key) {
                fprintf(stderr, "ERROR: failed to copy key\n");
                free(new_entry);
                continue;
            }
            
            // Create row number array
            new_entry->row_numbers = (int*)malloc(4 * sizeof(int));
            if (!new_entry->row_numbers) {
                fprintf(stderr, "ERROR: failed to allocate row_numbers\n");
                free(new_entry->key);
                free(new_entry);
                continue;
            }
            
            new_entry->row_numbers[0] = i;
            new_entry->count = 1;
            new_entry->capacity = 4;
            
            // Insert at beginning of list
            new_entry->next = idx->buckets[hash];
            idx->buckets[hash] = new_entry;
            
            printf("DEBUG: new entry created with row %d\n", i);
            fflush(stdout);
        }
    }
    
    printf("DEBUG: index creation completed successfully\n");
    fflush(stdout);
    
    return idx;
}

// Search by index
int* index_lookup(Index* idx, Table* table, Value* val, int* result_count) {
    if (!idx || !val) return NULL;
    
    int col_idx = find_column_index(table, idx->column_name);
    if (col_idx == -1) return NULL;
    
    unsigned int hash;
    switch (table->columns[col_idx].type) {
        case TYPE_INT:
        case TYPE_BOOL:
            if (val->type != VAL_INT) return NULL;
            hash = hash_int(val->data.i, idx->bucket_count);
            break;
        case TYPE_FLOAT:
            if (val->type != VAL_FLOAT) return NULL;
            hash = hash_float(val->data.f, idx->bucket_count);
            break;
        case TYPE_STRING:
            if (val->type != VAL_STRING) return NULL;
            hash = hash_string(val->data.s, idx->bucket_count);
            break;
        default:
            return NULL;
    }
    
    IndexEntry* entry = idx->buckets[hash];
    while (entry) {
        int key_equal = 0;
        switch (table->columns[col_idx].type) {
            case TYPE_INT:
            case TYPE_BOOL:
                if (*(int*)entry->key == val->data.i) key_equal = 1;
                break;
            case TYPE_FLOAT:
                if (*(float*)entry->key == val->data.f) key_equal = 1;
                break;
            case TYPE_STRING:
                if (strcmp((char*)entry->key, val->data.s) == 0) key_equal = 1;
                break;
        }
        
        if (key_equal) {
            *result_count = entry->count;
            return entry->row_numbers;
        }
        entry = entry->next;
    }
    
    *result_count = 0;
    return NULL;
}

// Find rows by index
int* find_rows_by_index(Table* table, ASTNode* cond, int* count) {
    if (!table || !cond || cond->type != NODE_CONDITION_EXPR) {
        return NULL;
    }
    
    Index* idx = NULL;
    for (int i = 0; i < table->index_count; i++) {
        if (strcmp(table->indexes[i]->column_name, cond->data.condition.column) == 0) {
            idx = table->indexes[i];
            break;
        }
    }
    
    if (!idx) return NULL;
    if (cond->data.condition.op != OP_EQ) return NULL;
    
    return index_lookup(idx, table, cond->data.condition.value, count);
}

// === SORTING FUNCTIONS ===

typedef struct {
    Table* table;
    int col_idx;
    int ascending;
} SortContext;

int compare_rows(const void* a, const void* b, void* context) {
    SortContext* ctx = (SortContext*)context;
    int row_a = *(int*)a;
    int row_b = *(int*)b;
    
    void* val_a = ctx->table->rows[row_a][ctx->col_idx];
    void* val_b = ctx->table->rows[row_b][ctx->col_idx];
    
    if (!val_a && !val_b) return 0;
    if (!val_a) return 1;
    if (!val_b) return -1;
    
    ColumnType type = ctx->table->columns[ctx->col_idx].type;
    int result = 0;
    
    switch (type) {
        case TYPE_INT:
        case TYPE_BOOL:
            result = (*(int*)val_a - *(int*)val_b);
            break;
        case TYPE_FLOAT: {
            float diff = *(float*)val_a - *(float*)val_b;
            result = (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
            break;
        }
        case TYPE_STRING:
            result = strcmp((char*)val_a, (char*)val_b);
            break;
    }
    
    return ctx->ascending ? result : -result;
}

int* sort_rows(Table* table, const char* column, int ascending, int* sorted_count) {
    int col_idx = find_column_index(table, column);
    if (col_idx == -1) return NULL;
    
    int* rows = (int*)malloc(table->row_count * sizeof(int));
    for (int i = 0; i < table->row_count; i++) {
        rows[i] = i;
    }
    
    SortContext ctx = {
        .table = table,
        .col_idx = col_idx,
        .ascending = ascending
    };
    
    qsort_r(rows, table->row_count, sizeof(int), compare_rows, &ctx);
    
    *sorted_count = table->row_count;
    return rows;
}

// === AGGREGATE FUNCTIONS ===

long long aggregate_count(Table* table, ASTNode* condition, const char* column) {
    long long count = 0;
    
    for (int i = 0; i < table->row_count; i++) {
        if (!condition || evaluate_condition_on_row(condition, table, i)) {
            if (strcmp(column, "*") == 0) {
                count++;
            } else {
                int col_idx = find_column_index(table, column);
                if (col_idx >= 0 && table->rows[i][col_idx] != NULL) {
                    count++;
                }
            }
        }
    }
    
    return count;
}

double aggregate_sum(Table* table, ASTNode* condition, const char* column) {
    double sum = 0;
    int col_idx = find_column_index(table, column);
    if (col_idx == -1) return 0;
    
    ColumnType type = table->columns[col_idx].type;
    
    for (int i = 0; i < table->row_count; i++) {
        if (!condition || evaluate_condition_on_row(condition, table, i)) {
            void* data = table->rows[i][col_idx];
            if (data) {
                switch (type) {
                    case TYPE_INT:
                    case TYPE_BOOL:
                        sum += *(int*)data;
                        break;
                    case TYPE_FLOAT:
                        sum += *(float*)data;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
    return sum;
}

double aggregate_avg(Table* table, ASTNode* condition, const char* column) {
    double sum = 0;
    int count = 0;
    int col_idx = find_column_index(table, column);
    if (col_idx == -1) return 0;
    
    ColumnType type = table->columns[col_idx].type;
    
    for (int i = 0; i < table->row_count; i++) {
        if (!condition || evaluate_condition_on_row(condition, table, i)) {
            void* data = table->rows[i][col_idx];
            if (data) {
                switch (type) {
                    case TYPE_INT:
                    case TYPE_BOOL:
                        sum += *(int*)data;
                        count++;
                        break;
                    case TYPE_FLOAT:
                        sum += *(float*)data;
                        count++;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
    return (count > 0) ? sum / count : 0;
}

double aggregate_min(Table* table, ASTNode* condition, const char* column) {
    int col_idx = find_column_index(table, column);
    if (col_idx == -1) return 0;
    
    ColumnType type = table->columns[col_idx].type;
    double min_val = 0;
    int first = 1;
    
    for (int i = 0; i < table->row_count; i++) {
        if (!condition || evaluate_condition_on_row(condition, table, i)) {
            void* data = table->rows[i][col_idx];
            if (data) {
                double val = 0;
                switch (type) {
                    case TYPE_INT:
                    case TYPE_BOOL:
                        val = *(int*)data;
                        break;
                    case TYPE_FLOAT:
                        val = *(float*)data;
                        break;
                    default:
                        continue;
                }
                
                if (first || val < min_val) {
                    min_val = val;
                    first = 0;
                }
            }
        }
    }
    
    return min_val;
}

double aggregate_max(Table* table, ASTNode* condition, const char* column) {
    int col_idx = find_column_index(table, column);
    if (col_idx == -1) return 0;
    
    ColumnType type = table->columns[col_idx].type;
    double max_val = 0;
    int first = 1;
    
    for (int i = 0; i < table->row_count; i++) {
        if (!condition || evaluate_condition_on_row(condition, table, i)) {
            void* data = table->rows[i][col_idx];
            if (data) {
                double val = 0;
                switch (type) {
                    case TYPE_INT:
                    case TYPE_BOOL:
                        val = *(int*)data;
                        break;
                    case TYPE_FLOAT:
                        val = *(float*)data;
                        break;
                    default:
                        continue;
                }
                
                if (first || val > max_val) {
                    max_val = val;
                    first = 0;
                }
            }
        }
    }
    
    return max_val;
}

void execute_aggregate(Table* table, ASTNode* ast) {
    if (!table) return;
    
    double result = 0;
    long long count_result = 0;
    int is_count = 0;
    
    switch (ast->data.aggregate.func) {
        case AGG_COUNT:
            count_result = aggregate_count(table, ast->data.aggregate.condition, 
                                          ast->data.aggregate.column);
            is_count = 1;
            break;
        case AGG_SUM:
            result = aggregate_sum(table, ast->data.aggregate.condition, 
                                  ast->data.aggregate.column);
            break;
        case AGG_AVG:
            result = aggregate_avg(table, ast->data.aggregate.condition, 
                                  ast->data.aggregate.column);
            break;
        case AGG_MIN:
            result = aggregate_min(table, ast->data.aggregate.condition, 
                                  ast->data.aggregate.column);
            break;
        case AGG_MAX:
            result = aggregate_max(table, ast->data.aggregate.condition, 
                                  ast->data.aggregate.column);
            break;
    }
    
    printf("\n");
    printf("+---------------------+\n");
    printf("| result              |\n");
    printf("+---------------------+\n");
    printf("| ");
    
    if (is_count) {
        printf("%-19lld |\n", count_result);
    } else {
        printf("%-19g |\n", result);
    }
    
    printf("+---------------------+\n");
}

// === CSV FUNCTIONS ===

int export_to_csv(Table* table, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error: could not create file %s\n", filename);
        return -1;
    }
    
    for (int i = 0; i < table->column_count; i++) {
        fprintf(file, "%s", table->columns[i].name);
        if (i < table->column_count - 1) {
            fprintf(file, ",");
        }
    }
    fprintf(file, "\n");
    
    for (int i = 0; i < table->row_count; i++) {
        for (int j = 0; j < table->column_count; j++) {
            void* data = table->rows[i][j];
            
            if (!data) {
                fprintf(file, "NULL");
            } else {
                switch (table->columns[j].type) {
                    case TYPE_INT:
                        fprintf(file, "%d", *(int*)data);
                        break;
                    case TYPE_FLOAT:
                        fprintf(file, "%g", *(float*)data);
                        break;
                    case TYPE_STRING: {
                        char* str = (char*)data;
                        int need_quotes = (strchr(str, ',') != NULL || 
                                          strchr(str, '"') != NULL ||
                                          strchr(str, '\n') != NULL);
                        
                        if (need_quotes) {
                            fprintf(file, "\"");
                            for (char* c = str; *c; c++) {
                                if (*c == '"') {
                                    fprintf(file, "\"\"");
                                } else {
                                    fprintf(file, "%c", *c);
                                }
                            }
                            fprintf(file, "\"");
                        } else {
                            fprintf(file, "%s", str);
                        }
                        break;
                    }
                    case TYPE_BOOL:
                        fprintf(file, "%s", (*(int*)data) ? "true" : "false");
                        break;
                }
            }
            
            if (j < table->column_count - 1) {
                fprintf(file, ",");
            }
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("Table '%s' exported to %s (%d rows)\n", 
           table->table_name, filename, table->row_count);
    return 0;
}

int import_from_csv(Table* table, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: could not open file %s\n", filename);
        return -1;
    }
    
    char line[4096];
    int line_num = 0;
    int imported = 0;
    
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return -1;
    }
    line_num++;
    line[strcspn(line, "\n")] = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        line[strcspn(line, "\n")] = 0;
        
        if (strlen(line) == 0) continue;
        
        void** row_data = malloc(table->column_count * sizeof(void*));
        for (int i = 0; i < table->column_count; i++) {
            row_data[i] = NULL;
        }
        
        char* p = line;
        int col = 0;
        
        while (*p && col < table->column_count) {
            while (*p == ' ') p++;
            
            if (*p == '"') {
                p++;
                char buffer[1024];
                int i = 0;
                
                while (*p && !(*p == '"' && *(p+1) != '"')) {
                    if (*p == '"' && *(p+1) == '"') {
                        buffer[i++] = '"';
                        p += 2;
                    } else {
                        buffer[i++] = *p++;
                    }
                }
                buffer[i] = '\0';
                
                if (*p == '"') p++;
                
                Value val;
                val.type = VAL_STRING;
                val.data.s = buffer;
                row_data[col] = value_to_data(&val, table->columns[col].type);
            }
            else {
                char buffer[256];
                int i = 0;
                
                while (*p && *p != ',' && i < 255) {
                    buffer[i++] = *p++;
                }
                buffer[i] = '\0';
                
                if (strcmp(buffer, "NULL") == 0) {
                    row_data[col] = NULL;
                } else {
                    Value val;
                    char* endptr;
                    long int_val = strtol(buffer, &endptr, 10);
                    
                    if (*endptr == '\0') {
                        val.type = VAL_INT;
                        val.data.i = int_val;
                    } else {
                        float float_val = strtof(buffer, &endptr);
                        if (*endptr == '\0') {
                            val.type = VAL_FLOAT;
                            val.data.f = float_val;
                        } else {
                            val.type = VAL_STRING;
                            val.data.s = buffer;
                        }
                    }
                    
                    row_data[col] = value_to_data(&val, table->columns[col].type);
                }
            }
            
            if (*p == ',') {
                p++;
            }
            col++;
        }
        
        add_row(table, row_data);
        imported++;
        free(row_data);
    }
    
    fclose(file);
    printf("Imported %d rows from %s\n", imported, filename);
    return imported;
}