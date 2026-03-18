#include "minidb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    Table** tables = NULL;
    int table_count = 0;
    int table_capacity = 0;
    
    char input[4096];
    
    printf("Small-DBMS v1.0\n");
    printf("Commands:\n");
    printf("  CREATE table [col:type, ...] - create table with types\n");
    printf("  CREATE INDEX name ON table (col) - create index\n");
    printf("  table+ [val1, val2, ...] - insert row\n");
    printf("  table#N - select row N\n");
    printf("  table#N SET [col=val, ...] - update row\n");
    printf("  table#N - - delete row\n");
    printf("  table[condition] - select with condition\n");
    printf("  table[condition] ORDER BY col [ASC|DESC] - sort results\n");
    printf("  table[condition] SET [col=val, ...] - update by condition\n");
    printf("  table[condition] - - delete by condition\n");
    printf("  table - show all data\n");
    printf("  SELECT COUNT|SUM|AVG|MIN|MAX (col) FROM table [WHERE condition] - aggregation\n");
    printf("  EXPORT table TO \"file.csv\" - export to CSV\n");
    printf("  IMPORT table FROM \"file.csv\" - import from CSV\n");
    printf("  exit - quit\n\n");
    
    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }
        
        if (strlen(input) == 0) continue;
        
        input[strcspn(input, ";")] = 0;
        
        ASTNode* ast = parse_command(input);
        if (!ast) {
            printf("Syntax error\n");
            continue;
        }
        
        Table* current_table = NULL;
        if (ast->type == NODE_CREATE_INDEX) {
            current_table = find_table(tables, table_count, ast->data.create_index.table_name);
        } else {
            current_table = find_table(tables, table_count, ast->table_name);
        }
        
        switch (ast->type) {
            case NODE_CREATE_TABLE: {
                if (current_table) {
                    printf("Table '%s' already exists\n", ast->table_name);
                    break;
                }
                
                Column* cols = malloc(ast->data.create.column_count * sizeof(Column));
                for (int i = 0; i < ast->data.create.column_count; i++) {
                    strncpy(cols[i].name, ast->data.create.columns[i], 63);
                    cols[i].name[63] = '\0';
                    cols[i].type = ast->data.create.types[i];
                }
                
                Table* new_table = create_table(ast->table_name, cols, 
                                                ast->data.create.column_count);
                
                if (table_count >= table_capacity) {
                    table_capacity = table_capacity == 0 ? 10 : table_capacity * 2;
                    Table** new_tables = realloc(tables, table_capacity * sizeof(Table*));
                    if (!new_tables) {
                        printf("Error: failed to allocate memory\n");
                        free_table(new_table);
                        free(cols);
                        break;
                    }
                    tables = new_tables;
                }
                tables[table_count] = new_table;
                table_count++;
                
                free(cols);
                printf("Table '%s' created\n", ast->table_name);
                break;
            }
            
            case NODE_CREATE_INDEX: {
                Table* target_table = find_table(tables, table_count, 
                                                 ast->data.create_index.table_name);
                if (!target_table) {
                    printf("Table '%s' not found\n", ast->data.create_index.table_name);
                    break;
                }
                
                int index_exists = 0;
                for (int i = 0; i < target_table->index_count; i++) {
                    if (strcmp(target_table->indexes[i]->name, ast->data.create_index.index_name) == 0) {
                        printf("Index '%s' already exists\n", ast->data.create_index.index_name);
                        index_exists = 1;
                        break;
                    }
                }
                
                if (!index_exists) {
                    Index* idx = create_index(target_table, ast->data.create_index.index_name,
                                             ast->data.create_index.column_name);
                    if (idx) {
                        target_table->indexes = realloc(target_table->indexes, 
                                                       (target_table->index_count + 1) * sizeof(Index*));
                        if (target_table->indexes) {
                            target_table->indexes[target_table->index_count] = idx;
                            target_table->index_count++;
                            printf("Index '%s' created\n", ast->data.create_index.index_name);
                        }
                    }
                }
                break;
            }
            
            case NODE_INSERT: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                
                void** row_data = malloc(current_table->column_count * sizeof(void*));
                for (int i = 0; i < current_table->column_count; i++) {
                    row_data[i] = NULL;
                }
                
                for (int i = 0; i < ast->data.insert.value_count && 
                                    i < current_table->column_count; i++) {
                    if (ast->data.insert.values[i]) {
                        Value* val = ast->data.insert.values[i];
                        switch (current_table->columns[i].type) {
                            case TYPE_INT:
                            case TYPE_BOOL: {
                                int* ip = malloc(sizeof(int));
                                *ip = val->data.i;
                                row_data[i] = ip;
                                break;
                            }
                            case TYPE_FLOAT: {
                                float* fp = malloc(sizeof(float));
                                *fp = val->data.f;
                                row_data[i] = fp;
                                break;
                            }
                            case TYPE_STRING: {
                                row_data[i] = strdup(val->data.s);
                                break;
                            }
                        }
                    }
                }
                
                int row_num = add_row(current_table, row_data);
                printf("Row %d inserted\n", row_num);
                
                free(row_data);
                break;
            }
            
            case NODE_SELECT_ALL: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                print_table(current_table);
                break;
            }
            
            case NODE_SELECT_ROW: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                
                int row = ast->data.delete_node.row_number;
                if (row < 0 || row >= current_table->row_count) {
                    printf("Row %d does not exist (total rows: %d)\n", 
                           row, current_table->row_count);
                    break;
                }
                
                Table* temp = create_table("temp", current_table->columns, 
                                          current_table->column_count);
                add_row(temp, current_table->rows[row]);
                print_table(temp);
                free_table(temp);
                break;
            }
            
            case NODE_SELECT_WHERE: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                
                int* result_rows = NULL;
                int result_count = 0;
                
                if (ast->data.query.condition && 
                    ast->data.query.condition->type == NODE_CONDITION_EXPR) {
                    result_rows = find_rows_by_index(current_table, 
                                                     ast->data.query.condition, 
                                                     &result_count);
                }
                
                if (!result_rows) {
                    result_rows = (int*)malloc(current_table->row_count * sizeof(int));
                    result_count = 0;
                    
                    for (int i = 0; i < current_table->row_count; i++) {
                        if (evaluate_condition_on_row(ast->data.query.condition, 
                                                      current_table, i)) {
                            result_rows[result_count++] = i;
                        }
                    }
                }
                
                if (ast->data.query.order_by) {
                    Table* temp = create_table("temp", current_table->columns, 
                                              current_table->column_count);
                    
                    for (int i = 0; i < result_count; i++) {
                        add_row(temp, current_table->rows[result_rows[i]]);
                    }
                    
                    ASTNode* order_by_node = ast->data.query.order_by;
                    int* sorted = sort_rows(temp, order_by_node->data.order_by.column,
                                           order_by_node->data.order_by.ascending, 
                                           &result_count);
                    
                    printf("\n");
                    for (int i = 0; i < temp->column_count; i++) {
                        printf("+---------------");
                    }
                    printf("+\n|");
                    
                    for (int i = 0; i < temp->column_count; i++) {
                        printf(" %-13s |", temp->columns[i].name);
                    }
                    printf("\n");
                    
                    for (int i = 0; i < temp->column_count; i++) {
                        printf("+---------------");
                    }
                    printf("+\n");
                    
                    for (int i = 0; i < result_count; i++) {
                        printf("|");
                        for (int j = 0; j < temp->column_count; j++) {
                            printf(" ");
                            void* cell_data = temp->rows[sorted[i]][j];
                            print_cell_value(cell_data, 
                                           temp->columns[j].type);
                            printf(" |");
                        }
                        printf("\n");
                    }
                    
                    for (int i = 0; i < temp->column_count; i++) {
                        printf("+---------------");
                    }
                    printf("+\n");
                    
                    free(sorted);
                    free_table(temp);
                } else {
                    Table* temp = create_table("temp", current_table->columns, 
                                              current_table->column_count);
                    
                    for (int i = 0; i < result_count; i++) {
                        add_row(temp, current_table->rows[result_rows[i]]);
                    }
                    
                    print_table(temp);
                    free_table(temp);
                }
                
                free(result_rows);
                break;
            }
            
            case NODE_SELECT_AGGREGATE: {
                Table* target_table = find_table(tables, table_count, ast->table_name);
                if (!target_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                execute_aggregate(target_table, ast);
                break;
            }
            
            case NODE_UPDATE_ROW: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                
                update_row(current_table, ast->data.update.row_number, 
                          ast->data.update.assignments);
                printf("Row %d updated\n", ast->data.update.row_number);
                break;
            }
            
            case NODE_UPDATE_CONDITION: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                
                int updated = update_rows_where(current_table, 
                                               ast->data.update_cond.condition,
                                               ast->data.update_cond.assignments);
                printf("Rows updated: %d\n", updated);
                break;
            }
            
            case NODE_DELETE_ROW: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                
                delete_row(current_table, ast->data.delete_node.row_number);
                printf("Row %d deleted\n", ast->data.delete_node.row_number);
                break;
            }
            
            case NODE_DELETE_CONDITION: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                
                int deleted = delete_rows_where(current_table, 
                                               ast->data.delete_node.condition);
                printf("Rows deleted: %d\n", deleted);
                break;
            }
            
            case NODE_EXPORT_CSV: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                export_to_csv(current_table, ast->data.file_op.filename);
                break;
            }
            
            case NODE_IMPORT_CSV: {
                if (!current_table) {
                    printf("Table '%s' not found\n", ast->table_name);
                    break;
                }
                import_from_csv(current_table, ast->data.file_op.filename);
                break;
            }
            
            default:
                printf("Unknown command\n");
        }
        
        free_ast_node(ast);
    }
    
    save_all_tables(tables, table_count);
    
    for (int i = 0; i < table_count; i++) {
        free_table(tables[i]);
    }
    free(tables);

    return 0;
}
