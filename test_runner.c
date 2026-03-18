#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "minidb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char* name;
    char* input;
    char* expected_type;
    int expected_null;
    char* expected_table_name;
    int expected_column_count;
    int expected_value_count;
    int expected_row_number;
    int has_order_by;
    int order_ascending;
    char* expected_func;
    int has_condition;
    char* expected_index_name;
    char* expected_first_type;
} TestCase;

int parse_json_tests(const char* filename, TestCase** tests, int* count);
void free_test_cases(TestCase* tests, int count);
const char* node_type_to_string(NodeType type);
const char* column_type_to_string(ColumnType type);
const char* agg_func_to_string(AggFunction func);

static char* read_file(const char* filename, long* len_out) {
    FILE* f = fopen(filename, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* content = malloc(len + 1);
    fread(content, 1, len, f);
    content[len] = '\0';
    fclose(f);
    *len_out = len;
    return content;
}

static void skip_ws(const char** p) {
    while (**p && isspace(**p)) (*p)++;
}

static const char* find_key(const char* p, const char* key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    return strstr(p, search);
}

static char* extract_string(const char* p, const char* key) {
    const char* kp = find_key(p, key);
    if (!kp) return NULL;
    const char* colon = strstr(kp, ":");
    if (!colon) return NULL;
    colon++;
    skip_ws(&colon);
    if (*colon != '"') return NULL;
    colon++;
    const char* end = strchr(colon, '"');
    if (!end) return NULL;
    int len = end - colon;
    char* result = malloc(len + 1);
    strncpy(result, colon, len);
    result[len] = '\0';
    return result;
}

static int extract_bool(const char* p, const char* key) {
    const char* kp = find_key(p, key);
    if (!kp) return 0;
    const char* colon = strstr(kp, ":");
    if (!colon) return 0;
    colon++;
    skip_ws(&colon);
    if (strncmp(colon, "true", 4) == 0) return 1;
    if (strncmp(colon, "false", 5) == 0) return 0;
    return 0;
}

static int extract_int(const char* p, const char* key) {
    const char* kp = find_key(p, key);
    if (!kp) return -1;
    const char* colon = strstr(kp, ":");
    if (!colon) return -1;
    colon++;
    skip_ws(&colon);
    return (int)strtol(colon, NULL, 10);
}

int parse_json_tests(const char* filename, TestCase** tests, int* count) {
    long len;
    char* content = read_file(filename, &len);
    if (!content) return 0;
    
    const char* p = content;
    const char* tests_start = strstr(p, "\"tests\"");
    if (!tests_start) { free(content); return 0; }
    
    const char* arr_start = strstr(tests_start, "[");
    if (!arr_start) { free(content); return 0; }
    arr_start++;
    
    int capacity = 100;
    *tests = malloc(capacity * sizeof(TestCase));
    *count = 0;
    
    while (*arr_start && *arr_start != ']') {
        skip_ws(&arr_start);
        if (*arr_start != '{') {
            if (*arr_start == ',') arr_start++;
            continue;
        }
        
        const char* obj_start = arr_start;
        const char* obj_end = strchr(obj_start, '}');
        if (!obj_end) break;
        
        char* obj_copy = malloc(obj_end - obj_start + 1);
        strncpy(obj_copy, obj_start, obj_end - obj_start);
        obj_copy[obj_end - obj_start] = '\0';
        
        TestCase tc = { .expected_row_number = -1, .expected_column_count = -1, .expected_value_count = -1, .order_ascending = -1 };
        tc.name = extract_string(obj_copy, "name");
        tc.input = extract_string(obj_copy, "input");
        tc.expected_type = extract_string(obj_copy, "expected_type");
        tc.expected_null = extract_bool(obj_copy, "expected_null");
        tc.expected_table_name = extract_string(obj_copy, "expected_table_name");
        tc.expected_column_count = extract_int(obj_copy, "expected_column_count");
        tc.expected_value_count = extract_int(obj_copy, "expected_value_count");
        tc.expected_row_number = extract_int(obj_copy, "expected_row_number");
        tc.has_order_by = extract_bool(obj_copy, "has_order_by");
        tc.order_ascending = extract_int(obj_copy, "order_ascending");
        tc.expected_func = extract_string(obj_copy, "expected_func");
        tc.has_condition = extract_bool(obj_copy, "has_condition");
        tc.expected_index_name = extract_string(obj_copy, "expected_index_name");
        tc.expected_first_type = extract_string(obj_copy, "expected_first_type");
        
        free(obj_copy);
        
        if (tc.name && tc.input) {
            if (*count >= capacity) {
                capacity *= 2;
                *tests = realloc(*tests, capacity * sizeof(TestCase));
            }
            (*tests)[*count] = tc;
            (*count)++;
        }
        
        arr_start = obj_end + 1;
    }
    
    free(content);
    return 1;
}

void free_test_cases(TestCase* tests, int count) {
    for (int i = 0; i < count; i++) {
        free(tests[i].name);
        free(tests[i].input);
        free(tests[i].expected_type);
        free(tests[i].expected_table_name);
        free(tests[i].expected_func);
        free(tests[i].expected_index_name);
        free(tests[i].expected_first_type);
    }
    free(tests);
}

const char* node_type_to_string(NodeType type) {
    switch (type) {
        case NODE_CREATE_TABLE: return "NODE_CREATE_TABLE";
        case NODE_CREATE_INDEX: return "NODE_CREATE_INDEX";
        case NODE_INSERT: return "NODE_INSERT";
        case NODE_SELECT_ALL: return "NODE_SELECT_ALL";
        case NODE_SELECT_WHERE: return "NODE_SELECT_WHERE";
        case NODE_SELECT_ROW: return "NODE_SELECT_ROW";
        case NODE_SELECT_AGGREGATE: return "NODE_SELECT_AGGREGATE";
        case NODE_UPDATE_ROW: return "NODE_UPDATE_ROW";
        case NODE_UPDATE_CONDITION: return "NODE_UPDATE_CONDITION";
        case NODE_DELETE_ROW: return "NODE_DELETE_ROW";
        case NODE_DELETE_CONDITION: return "NODE_DELETE_CONDITION";
        case NODE_CONDITION_EXPR: return "NODE_CONDITION_EXPR";
        case NODE_LOGICAL_EXPR: return "NODE_LOGICAL_EXPR";
        case NODE_ASSIGNMENT: return "NODE_ASSIGNMENT";
        case NODE_ORDER_BY: return "NODE_ORDER_BY";
        case NODE_EXPORT_CSV: return "NODE_EXPORT_CSV";
        case NODE_IMPORT_CSV: return "NODE_IMPORT_CSV";
        default: return "UNKNOWN";
    }
}

const char* column_type_to_string(ColumnType type) {
    switch (type) {
        case TYPE_INT: return "TYPE_INT";
        case TYPE_FLOAT: return "TYPE_FLOAT";
        case TYPE_STRING: return "TYPE_STRING";
        case TYPE_BOOL: return "TYPE_BOOL";
        default: return "UNKNOWN";
    }
}

const char* agg_func_to_string(AggFunction func) {
    switch (func) {
        case AGG_COUNT: return "AGG_COUNT";
        case AGG_SUM: return "AGG_SUM";
        case AGG_AVG: return "AGG_AVG";
        case AGG_MIN: return "AGG_MIN";
        case AGG_MAX: return "AGG_MAX";
        default: return "UNKNOWN";
    }
}

int main() {
    TestCase* tests = NULL;
    int test_count = 0;
    
    printf("=== Small-DBMS JSON Test Runner ===\n\n");
    
    if (!parse_json_tests("tests.json", &tests, &test_count)) {
        printf("Failed to load tests from JSON\n");
        return 1;
    }
    
    printf("Loaded %d tests from tests.json\n\n", test_count);
    
    int passed = 0;
    int failed = 0;
    
    for (int i = 0; i < test_count; i++) {
        TestCase* tc = &tests[i];
        
        ASTNode* ast = parse_command(tc->input);
        
        int test_passed = 1;
        char error_msg[256] = "";
        
        if (tc->expected_null) {
            if (ast != NULL) {
                test_passed = 0;
                snprintf(error_msg, sizeof(error_msg), "Expected NULL but got %s", node_type_to_string(ast->type));
            }
        } else {
            if (ast == NULL) {
                test_passed = 0;
                snprintf(error_msg, sizeof(error_msg), "Expected %s but got NULL", tc->expected_type ? tc->expected_type : "valid AST");
            } else {
                if (tc->expected_type) {
                    const char* actual_type = node_type_to_string(ast->type);
                    if (strcmp(tc->expected_type, actual_type) != 0) {
                        test_passed = 0;
                        snprintf(error_msg, sizeof(error_msg), "Expected type %s but got %s", tc->expected_type, actual_type);
                    }
                }
                
                if (test_passed && tc->expected_table_name) {
                    const char* actual_name = ast->table_name ? ast->table_name : 
                                              (ast->type == NODE_CREATE_INDEX ? ast->data.create_index.table_name : "");
                    if (strcmp(tc->expected_table_name, actual_name) != 0) {
                        test_passed = 0;
                        snprintf(error_msg, sizeof(error_msg), "Expected table_name %s but got %s", 
                                tc->expected_table_name, actual_name);
                    }
                }
                
                if (test_passed && tc->expected_column_count >= 0) {
                    int actual_count = 0;
                    if (ast->type == NODE_CREATE_TABLE) {
                        actual_count = ast->data.create.column_count;
                    } else if (ast->type == NODE_INSERT) {
                        actual_count = ast->data.insert.value_count;
                    }
                    if (actual_count != tc->expected_column_count) {
                        test_passed = 0;
                        snprintf(error_msg, sizeof(error_msg), "Expected column count %d but got %d", 
                                tc->expected_column_count, actual_count);
                    }
                }
                
                if (test_passed && tc->expected_value_count >= 0) {
                    if (ast->type == NODE_INSERT) {
                        if (ast->data.insert.value_count != tc->expected_value_count) {
                            test_passed = 0;
                            snprintf(error_msg, sizeof(error_msg), "Expected value count %d but got %d", 
                                    tc->expected_value_count, ast->data.insert.value_count);
                        }
                    }
                }
                
                if (test_passed && tc->expected_row_number >= 0) {
                    int actual_row = -1;
                    if (ast->type == NODE_SELECT_ROW || ast->type == NODE_DELETE_ROW) {
                        actual_row = ast->data.delete_node.row_number;
                    } else if (ast->type == NODE_UPDATE_ROW) {
                        actual_row = ast->data.update.row_number;
                    }
                    if (actual_row != tc->expected_row_number) {
                        test_passed = 0;
                        snprintf(error_msg, sizeof(error_msg), "Expected row number %d but got %d", 
                                tc->expected_row_number, actual_row);
                    }
                }
                
                if (test_passed && tc->has_order_by) {
                    if (ast->type != NODE_SELECT_WHERE || ast->data.query.order_by == NULL) {
                        test_passed = 0;
                        snprintf(error_msg, sizeof(error_msg), "Expected ORDER BY but got none");
                    } else if (tc->order_ascending >= 0) {
                        int actual_asc = ast->data.query.order_by->data.order_by.ascending;
                        if (actual_asc != tc->order_ascending) {
                            test_passed = 0;
                            snprintf(error_msg, sizeof(error_msg), "Expected ascending=%d but got %d", 
                                    tc->order_ascending, actual_asc);
                        }
                    }
                }
                
                if (test_passed && tc->expected_func) {
                    if (ast->type == NODE_SELECT_AGGREGATE) {
                        const char* actual_func = agg_func_to_string(ast->data.aggregate.func);
                        if (strcmp(tc->expected_func, actual_func) != 0) {
                            test_passed = 0;
                            snprintf(error_msg, sizeof(error_msg), "Expected func %s but got %s", 
                                    tc->expected_func, actual_func);
                        }
                    }
                }
                
                if (test_passed && tc->has_condition) {
                    if (ast->type == NODE_SELECT_AGGREGATE) {
                        if (ast->data.aggregate.condition == NULL) {
                            test_passed = 0;
                            snprintf(error_msg, sizeof(error_msg), "Expected WHERE condition but got none");
                        }
                    }
                }
                
                if (test_passed && tc->expected_index_name) {
                    if (ast->type == NODE_CREATE_INDEX) {
                        if (strcmp(tc->expected_index_name, ast->data.create_index.index_name) != 0) {
                            test_passed = 0;
                            snprintf(error_msg, sizeof(error_msg), "Expected index name %s but got %s", 
                                    tc->expected_index_name, ast->data.create_index.index_name);
                        }
                    }
                }
                
                if (test_passed && tc->expected_first_type) {
                    if (ast->type == NODE_CREATE_TABLE && ast->data.create.column_count > 0) {
                        const char* actual_type = column_type_to_string(ast->data.create.types[0]);
                        if (strcmp(tc->expected_first_type, actual_type) != 0) {
                            test_passed = 0;
                            snprintf(error_msg, sizeof(error_msg), "Expected first type %s but got %s", 
                                    tc->expected_first_type, actual_type);
                        }
                    }
                }
            }
        }
        
        if (ast) {
            free_ast_node(ast);
        }
        
        if (test_passed) {
            printf("[PASS] %s\n", tc->name);
            passed++;
        } else {
            printf("[FAIL] %s: %s\n", tc->name, error_msg);
            failed++;
        }
    }
    
    free_test_cases(tests, test_count);
    
    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Total:  %d\n", test_count);
    
    return failed > 0 ? 1 : 0;
}
