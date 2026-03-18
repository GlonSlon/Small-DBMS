#define _POSIX_C_SOURCE 200809L
#include "minidb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define TEST_INIT() \
    do { \
        printf("=== %s ===\n", __func__); \
    } while(0)

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s\n", msg); \
            failed++; \
        } else { \
            printf("PASS: %s\n", msg); \
        } \
    } while(0)

extern ASTNode* parse_command(char* input);
extern void free_ast_node(ASTNode* node);
extern Table* create_table(const char* name, Column* columns, int column_count);
extern void free_table(Table* table);
extern int add_row(Table* table, void** row_data);
extern int save_table(Table* table, const char* filename);
extern Table* load_table(const char* filename);

static int failed = 0;

void test_parse_create_table_valid() {
    TEST_INIT();
    ASTNode* ast = parse_command("CREATE users [id:int, name:string, age:int]");
    TEST_ASSERT(ast != NULL, "Parse CREATE TABLE with valid syntax");
    TEST_ASSERT(ast->type == NODE_CREATE_TABLE, "Node type is CREATE_TABLE");
    TEST_ASSERT(strcmp(ast->table_name, "users") == 0, "Table name is 'users'");
    TEST_ASSERT(ast->data.create.column_count == 3, "Column count is 3");
    free_ast_node(ast);
}

void test_parse_create_table_no_brackets() {
    TEST_INIT();
    ASTNode* ast = parse_command("CREATE users id:int, name:string");
    TEST_ASSERT(ast == NULL, "CREATE without brackets should fail");
}

void test_parse_create_table_missing_type() {
    TEST_INIT();
    ASTNode* ast = parse_command("CREATE users [id, name:string]");
    TEST_ASSERT(ast != NULL, "CREATE with missing type uses default TYPE_STRING");
    if (ast) {
        TEST_ASSERT(ast->data.create.types[0] == TYPE_STRING, "First column defaults to STRING");
        free_ast_node(ast);
    }
}

void test_parse_insert_valid() {
    TEST_INIT();
    ASTNode* ast = parse_command("users+ [1, \"John\", 25]");
    TEST_ASSERT(ast != NULL, "Parse INSERT with valid syntax");
    TEST_ASSERT(ast->type == NODE_INSERT, "Node type is INSERT");
    TEST_ASSERT(ast->data.insert.value_count == 3, "Value count is 3");
    free_ast_node(ast);
}

void test_parse_insert_empty() {
    TEST_INIT();
    ASTNode* ast = parse_command("users+ []");
    TEST_ASSERT(ast != NULL, "Empty INSERT should parse (returns NULL values)");
    free_ast_node(ast);
}

void test_parse_insert_unquoted_string() {
    TEST_INIT();
    ASTNode* ast = parse_command("users+ [1, John]");
    TEST_ASSERT(ast != NULL, "Unquoted string parses but treated as identifier");
    free_ast_node(ast);
}

void test_parse_insert_too_many_values() {
    TEST_INIT();
    ASTNode* ast = parse_command("users+ [1, \"John\", 25, \"extra\"]");
    TEST_ASSERT(ast != NULL, "Too many values parses (silent ignore)");
    TEST_ASSERT(ast->data.insert.value_count == 4, "All values stored");
    free_ast_node(ast);
}

void test_parse_select_all() {
    TEST_INIT();
    ASTNode* ast = parse_command("users");
    TEST_ASSERT(ast != NULL, "Parse SELECT ALL");
    TEST_ASSERT(ast->type == NODE_SELECT_ALL, "Node type is SELECT_ALL");
    free_ast_node(ast);
}

void test_parse_select_row() {
    TEST_INIT();
    ASTNode* ast = parse_command("users#0");
    TEST_ASSERT(ast != NULL, "Parse SELECT ROW");
    TEST_ASSERT(ast->type == NODE_SELECT_ROW, "Node type is SELECT_ROW");
    TEST_ASSERT(ast->data.delete_node.row_number == 0, "Row number is 0");
    free_ast_node(ast);
}

void test_parse_select_condition() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[id = 1]");
    TEST_ASSERT(ast != NULL, "Parse SELECT with condition");
    TEST_ASSERT(ast->type == NODE_SELECT_WHERE, "Node type is SELECT_WHERE");
    TEST_ASSERT(ast->data.query.condition != NULL, "Condition is set");
    free_ast_node(ast);
}

void test_parse_condition_with_spaces() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[ id = 1 ]");
    TEST_ASSERT(ast != NULL, "Condition with spaces");
    free_ast_node(ast);
}

void test_parse_condition_and() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[id = 1 && age = 20]");
    TEST_ASSERT(ast != NULL, "Condition with AND");
    free_ast_node(ast);
}

void test_parse_condition_or() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[id = 1 || age = 20]");
    TEST_ASSERT(ast != NULL, "Condition with OR");
    free_ast_node(ast);
}

void test_parse_condition_no_spaces_fails() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[id=1&age=20]");
    TEST_ASSERT(ast == NULL, "Condition without spaces fails");
}

void test_parse_update_row() {
    TEST_INIT();
    ASTNode* ast = parse_command("users#0 SET [name=\"Jane\"]");
    TEST_ASSERT(ast != NULL, "Parse UPDATE ROW");
    TEST_ASSERT(ast->type == NODE_UPDATE_ROW, "Node type is UPDATE_ROW");
    free_ast_node(ast);
}

void test_parse_delete_row() {
    TEST_INIT();
    ASTNode* ast = parse_command("users#0 -");
    TEST_ASSERT(ast != NULL, "Parse DELETE ROW");
    TEST_ASSERT(ast->type == NODE_DELETE_ROW, "Node type is DELETE_ROW");
    free_ast_node(ast);
}

void test_parse_delete_condition() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[id > 10] -");
    TEST_ASSERT(ast != NULL, "Parse DELETE with condition");
    TEST_ASSERT(ast->type == NODE_DELETE_CONDITION, "Node type is DELETE_CONDITION");
    free_ast_node(ast);
}

void test_parse_update_condition() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[id > 10] SET [age=30]");
    TEST_ASSERT(ast != NULL, "Parse UPDATE with condition");
    TEST_ASSERT(ast->type == NODE_UPDATE_CONDITION, "Node type is UPDATE_CONDITION");
    free_ast_node(ast);
}

void test_parse_order_by_asc() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[age > 18] ORDER BY id ASC");
    TEST_ASSERT(ast != NULL, "Parse ORDER BY ASC");
    TEST_ASSERT(ast->type == NODE_SELECT_WHERE, "Node type is SELECT_WHERE");
    TEST_ASSERT(ast->data.query.order_by != NULL, "Order by is set");
    TEST_ASSERT(ast->data.query.order_by->data.order_by.ascending == 1, "Ascending is 1");
    free_ast_node(ast);
}

void test_parse_order_by_desc() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[age > 18] ORDER BY id DESC");
    TEST_ASSERT(ast != NULL, "Parse ORDER BY DESC");
    TEST_ASSERT(ast->data.query.order_by->data.order_by.ascending == 0, "Ascending is 0");
    free_ast_node(ast);
}

void test_parse_aggregate_count() {
    TEST_INIT();
    ASTNode* ast = parse_command("SELECT COUNT(*) FROM users");
    TEST_ASSERT(ast != NULL, "Parse SELECT COUNT");
    TEST_ASSERT(ast->type == NODE_SELECT_AGGREGATE, "Node type is AGGREGATE");
    TEST_ASSERT(ast->data.aggregate.func == AGG_COUNT, "Function is COUNT");
    free_ast_node(ast);
}

void test_parse_aggregate_sum() {
    TEST_INIT();
    ASTNode* ast = parse_command("SELECT SUM(age) FROM users");
    TEST_ASSERT(ast != NULL, "Parse SELECT SUM");
    TEST_ASSERT(ast->data.aggregate.func == AGG_SUM, "Function is SUM");
    free_ast_node(ast);
}

void test_parse_aggregate_avg() {
    TEST_INIT();
    ASTNode* ast = parse_command("SELECT AVG(score) FROM users");
    TEST_ASSERT(ast != NULL, "Parse SELECT AVG");
    TEST_ASSERT(ast->data.aggregate.func == AGG_AVG, "Function is AVG");
    free_ast_node(ast);
}

void test_parse_aggregate_min() {
    TEST_INIT();
    ASTNode* ast = parse_command("SELECT MIN(age) FROM users");
    TEST_ASSERT(ast != NULL, "Parse SELECT MIN");
    TEST_ASSERT(ast->data.aggregate.func == AGG_MIN, "Function is MIN");
    free_ast_node(ast);
}

void test_parse_aggregate_max() {
    TEST_INIT();
    ASTNode* ast = parse_command("SELECT MAX(age) FROM users");
    TEST_ASSERT(ast != NULL, "Parse SELECT MAX");
    TEST_ASSERT(ast->data.aggregate.func == AGG_MAX, "Function is MAX");
    free_ast_node(ast);
}

void test_parse_aggregate_with_where() {
    TEST_INIT();
    ASTNode* ast = parse_command("SELECT COUNT(*) FROM users WHERE [age > 18]");
    TEST_ASSERT(ast != NULL, "Parse AGGREGATE with WHERE");
    TEST_ASSERT(ast->data.aggregate.condition != NULL, "Condition is set");
    free_ast_node(ast);
}

void test_parse_aggregate_without_brackets() {
    TEST_INIT();
    ASTNode* ast = parse_command("SELECT COUNT(*) FROM users WHERE age > 18");
    TEST_ASSERT(ast != NULL, "Parse AGGREGATE without brackets (parses but no condition)");
    free_ast_node(ast);
}

void test_parse_create_index() {
    TEST_INIT();
    ASTNode* ast = parse_command("CREATE INDEX idx ON users (id)");
    TEST_ASSERT(ast != NULL, "Parse CREATE INDEX");
    TEST_ASSERT(ast->type == NODE_CREATE_INDEX, "Node type is CREATE_INDEX");
    TEST_ASSERT(strcmp(ast->data.create_index.index_name, "idx") == 0, "Index name is 'idx'");
    TEST_ASSERT(strcmp(ast->data.create_index.table_name, "users") == 0, "Table name is 'users'");
    TEST_ASSERT(strcmp(ast->data.create_index.column_name, "id") == 0, "Column name is 'id'");
    free_ast_node(ast);
}

void test_parse_create_index_invalid() {
    TEST_INIT();
    ASTNode* ast = parse_command("CREATE INDEX idx users id");
    TEST_ASSERT(ast == NULL, "CREATE INDEX without parentheses fails");
}

void test_parse_export() {
    TEST_INIT();
    ASTNode* ast = parse_command("EXPORT users TO \"data.csv\"");
    TEST_ASSERT(ast != NULL, "Parse EXPORT");
    TEST_ASSERT(ast->type == NODE_EXPORT_CSV, "Node type is EXPORT_CSV");
    free_ast_node(ast);
}

void test_parse_import() {
    TEST_INIT();
    ASTNode* ast = parse_command("IMPORT users FROM \"data.csv\"");
    TEST_ASSERT(ast != NULL, "Parse IMPORT");
    TEST_ASSERT(ast->type == NODE_IMPORT_CSV, "Node type is IMPORT_CSV");
    free_ast_node(ast);
}

void test_parse_invalid_command() {
    TEST_INIT();
    ASTNode* ast = parse_command("INVALID COMMAND");
    TEST_ASSERT(ast == NULL, "Invalid command returns NULL");
}

void test_parse_empty_input() {
    TEST_INIT();
    ASTNode* ast = parse_command("");
    TEST_ASSERT(ast == NULL, "Empty input returns NULL");
}

void test_parse_whitespace_only() {
    TEST_INIT();
    ASTNode* ast = parse_command("   ");
    TEST_ASSERT(ast == NULL, "Whitespace only returns NULL");
}

void test_parse_comparison_operators() {
    TEST_INIT();
    
    ASTNode* eq = parse_command("users[id = 1]");
    TEST_ASSERT(eq != NULL, "Equal operator (=)");
    free_ast_node(eq);
    
    ASTNode* eq2 = parse_command("users[id == 1]");
    TEST_ASSERT(eq2 != NULL, "Equal operator (==)");
    free_ast_node(eq2);
    
    ASTNode* ne = parse_command("users[id != 1]");
    TEST_ASSERT(ne != NULL, "Not equal operator (!=)");
    free_ast_node(ne);
    
    ASTNode* gt = parse_command("users[id > 1]");
    TEST_ASSERT(gt != NULL, "Greater than (>)");
    free_ast_node(gt);
    
    ASTNode* lt = parse_command("users[id < 1]");
    TEST_ASSERT(lt != NULL, "Less than (<)");
    free_ast_node(lt);
    
    ASTNode* gte = parse_command("users[id >= 1]");
    TEST_ASSERT(gte != NULL, "Greater than or equal (>=)");
    free_ast_node(gte);
    
    ASTNode* lte = parse_command("users[id <= 1]");
    TEST_ASSERT(lte != NULL, "Less than or equal (<=)");
    free_ast_node(lte);
}

void test_parse_bool_values() {
    TEST_INIT();
    
    ASTNode* t = parse_command("users[active = true]");
    TEST_ASSERT(t != NULL, "Boolean true");
    free_ast_node(t);
    
    ASTNode* f = parse_command("users[active = false]");
    TEST_ASSERT(f != NULL, "Boolean false");
    free_ast_node(f);
}

void test_parse_null_value() {
    TEST_INIT();
    ASTNode* ast = parse_command("users[name = null]");
    TEST_ASSERT(ast != NULL, "NULL value in condition");
    free_ast_node(ast);
}

void test_parse_float_values() {
    TEST_INIT();
    ASTNode* ast = parse_command("products+ [1, 99.99, \"Test\"]");
    TEST_ASSERT(ast != NULL, "Float values in INSERT");
    free_ast_node(ast);
}

void test_parse_quoted_strings() {
    TEST_INIT();
    
    ASTNode* dq = parse_command("users+ [1, \"Hello World\"]");
    TEST_ASSERT(dq != NULL, "Double quoted string");
    free_ast_node(dq);
    
    ASTNode* sq = parse_command("users+ [1, 'Hello World']");
    TEST_ASSERT(sq != NULL, "Single quoted string");
    free_ast_node(sq);
}

void test_core_create_table() {
    TEST_INIT();
    Column cols[] = {
        {.name = "id", .type = TYPE_INT},
        {.name = "name", .type = TYPE_STRING}
    };
    Table* table = create_table("test", cols, 2);
    TEST_ASSERT(table != NULL, "create_table returns non-NULL");
    TEST_ASSERT(table->column_count == 2, "Column count is 2");
    TEST_ASSERT(strcmp(table->table_name, "test") == 0, "Table name is 'test'");
    TEST_ASSERT(table->row_count == 0, "Row count is 0 initially");
    free_table(table);
}

void test_core_add_row() {
    TEST_INIT();
    Column cols[] = {
        {.name = "id", .type = TYPE_INT},
        {.name = "name", .type = TYPE_STRING}
    };
    Table* table = create_table("test", cols, 2);
    
    int id = 1;
    char* name = strdup("John");
    void* row[] = {&id, name};
    
    int row_num = add_row(table, row);
    TEST_ASSERT(row_num == 0, "First row added at index 0");
    TEST_ASSERT(table->row_count == 1, "Row count is 1");
    
    free(name);
    free_table(table);
}

void test_core_save_load_table() {
    TEST_INIT();
    Column cols[] = {
        {.name = "id", .type = TYPE_INT},
        {.name = "name", .type = TYPE_STRING}
    };
    Table* table = create_table("test", cols, 2);
    
    int id1 = 1;
    char* name1 = strdup("John");
    add_row(table, (void*[]){&id1, name1});
    
    int id2 = 2;
    char* name2 = strdup("Jane");
    add_row(table, (void*[]){&id2, name2});
    
    int result = save_table(table, "test_save.tbl");
    TEST_ASSERT(result == 0, "save_table returns 0");
    
    Table* loaded = load_table("test_save.tbl");
    TEST_ASSERT(loaded != NULL, "load_table returns non-NULL");
    TEST_ASSERT(loaded->row_count == 2, "Loaded table has 2 rows");
    
    free(name1);
    free(name2);
    free_table(table);
    free_table(loaded);
    
    remove("test_save.tbl");
}

int main() {
    printf("=== Small-DBMS Parser and Core Tests ===\n\n");
    
    printf("--- CREATE TABLE Tests ---\n");
    test_parse_create_table_valid();
    test_parse_create_table_no_brackets();
    test_parse_create_table_missing_type();
    
    printf("\n--- INSERT Tests ---\n");
    test_parse_insert_valid();
    test_parse_insert_empty();
    test_parse_insert_unquoted_string();
    test_parse_insert_too_many_values();
    
    printf("\n--- SELECT Tests ---\n");
    test_parse_select_all();
    test_parse_select_row();
    test_parse_select_condition();
    test_parse_condition_with_spaces();
    
    printf("\n--- Condition Tests ---\n");
    test_parse_condition_and();
    test_parse_condition_or();
    test_parse_condition_no_spaces_fails();
    test_parse_comparison_operators();
    test_parse_bool_values();
    test_parse_null_value();
    
    printf("\n--- UPDATE/DELETE Tests ---\n");
    test_parse_update_row();
    test_parse_delete_row();
    test_parse_delete_condition();
    test_parse_update_condition();
    
    printf("\n--- ORDER BY Tests ---\n");
    test_parse_order_by_asc();
    test_parse_order_by_desc();
    
    printf("\n--- Aggregate Tests ---\n");
    test_parse_aggregate_count();
    test_parse_aggregate_sum();
    test_parse_aggregate_avg();
    test_parse_aggregate_min();
    test_parse_aggregate_max();
    test_parse_aggregate_with_where();
    test_parse_aggregate_without_brackets();
    
    printf("\n--- Index Tests ---\n");
    test_parse_create_index();
    test_parse_create_index_invalid();
    
    printf("\n--- CSV Tests ---\n");
    test_parse_export();
    test_parse_import();
    
    printf("\n--- Error Handling Tests ---\n");
    test_parse_invalid_command();
    test_parse_empty_input();
    test_parse_whitespace_only();
    
    printf("\n--- Value Parsing Tests ---\n");
    test_parse_float_values();
    test_parse_quoted_strings();
    
    printf("\n--- Core Function Tests ---\n");
    test_core_create_table();
    test_core_add_row();
    test_core_save_load_table();
    
    printf("\n=== Summary ===\n");
    if (failed > 0) {
        printf("FAILED: %d test(s)\n", failed);
        return 1;
    } else {
        printf("All tests passed!\n");
        return 0;
    }
}
