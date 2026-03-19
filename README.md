# Small-DBMS

A lightweight embedded database implemented in C, inspired by simple file-based databases. Small-DBMS provides basic SQL-like operations including create, insert, select, update, delete with support for indexes, ordering, and aggregate functions.

**Version:** 1.0
**Author:** Freud(GlonSlon)

## Building

```bash
make
```

This produces the `dbms-linux-bin` executable.

## Running

```bash
./dbms-linux-bin
```

## Commands

### Creating Tables

```sql
CREATE table_name [column:TYPE, ...]
```

Supported types: `INT`, `FLOAT`, `STRING`, `BOOL`

Example:
```
CREATE users [id:INT, name:STRING, email:STRING, age:INT]
```

### Inserting Data

```
table+ [value1, value2, ...]
```

Example:
```
users+ [1, "John Doe", "john@example.com", 30]
```

### Selecting Data

Show all rows:
```
table
```

Select specific row by number:
```
table#N
```

Select with condition:
```
table[column = value]
table[column > value]
table[column < value]
table[column >= value]
table[column <= value]
table[column != value]
```

Combine conditions:
```
table[age > 18 AND name = "John"]
table[status = "active" OR role = "admin"]
```

Ordering:
```
table[condition] ORDER BY column ASC
table[condition] ORDER BY column DESC
```

### Updating Data

Update specific row:
```
table#N SET [column=value, ...]
```

Update by condition:
```
table[condition] SET [column=value, ...]
```

Example:
```
users#0 SET [age=31]
users[age < 18] SET [status="inactive"]
```

### Deleting Data

Delete specific row:
```
table#N - -
```

Delete by condition:
```
table[condition] - -
```

### Aggregate Functions

```
SELECT COUNT(column) FROM table
SELECT COUNT(column) FROM table [WHERE condition]

SELECT SUM(column) FROM table [WHERE condition]
SELECT AVG(column) FROM table [WHERE condition]
SELECT MIN(column) FROM table [WHERE condition]
SELECT MAX(column) FROM table [WHERE condition]
```

### Indexes

Create index on a column:
```
CREATE INDEX index_name ON table (column)
```

Indexes improve query performance for filtered searches.

### CSV Import/Export

Export table to CSV:
```
EXPORT table TO "filename.csv"
```

Import from CSV:
```
IMPORT table FROM "filename.csv"
```

### Exiting

```
exit
```

## Data Types

- **INT**: Integer values
- **FLOAT**: Floating point numbers
- **STRING**: Text values (stored as strings)
- **BOOL**: Boolean values (0 or 1)

## File Storage

Tables are automatically saved to `.tbl` files when exiting the program. On startup, all existing tables are loaded from these files.

## Architecture

The database consists of three main components:

- **main.c**: Entry point and command dispatch
- **core.c**: Core database operations (table management, I/O, indexing)
- **systab.c**: Parser for the query language

## Testing

Run the test suite:
```bash
gcc test_runner.c core.c systab.c -o test_runner
./test_runner
```

The test runner validates the parser and core functionality against test cases defined in `tests.json`.

## Limitations

- Single-user, embedded database
- No transaction support
- Basic query optimizer
- In-memory indexes (rebuilt on load)
- No foreign key or join support in current version
