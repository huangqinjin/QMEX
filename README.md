# QMEX - Query & Map & Evaluation & eXecution for Tabular Data

[QMEX](https://github.com/huangqinjin/QMEX) is a small library that can query a table with a set of criteria and
retrieve corresponding data, and additionally execute [lua](https://www.lua.org/) expressions and functions to
get the final data if needed.

## Quick Start

Given a QMEX [table](test/demo.ini),

```ini
0.|      Grade.EQ  Subject.MH  Score.GE  Score.LT  =  Class  Average  Weight
1.|        1        Math         60        inf     =  PASS    85.5    {0.6;print(Class,Average)}
2.|        1        Math        -inf       60      =  FAIL    55      {0.4}
3.|        2        Math|Art     90        inf     =  A       95      [math.random]
4.|        2        Math|Art     60        90      =  B       80      {math.sqrt(Average/100)}
5.|        2        Math|Art    -inf       60      =  C       55      [CalcWeight]
```

you can query the table and retrieve data in just a few lines of code:

```c++
std::vector<char> buf;
// fill buf with table
Table table;
table.parse(buf, buf.size());

KeyValue criteria[] = {
  {"Grade", 2},
  {"Subject", "Math"},
  {"Score", 80},
};

KeyValue data[] = {
  {"Class", ""},  // require a STRING value
  {"Average", 0}, // require a NUMBER value
  {"Weight", 0},  // require a NUMBER value
};

int row = table.query(criteria, sizeof(criteria)/sizeof(criteria[0])); // return 4
if (row <= 0) {
  // no matched row
} else {
  table.retrieve(row, data, sizeof(data)/sizeof(data[0]));
  // Class = B, Average = 80, Weight = 0.894
}

```

## Table Format
The first row (row index `0`) is the header of a QMEX table, which contains names of all columns. Other rows (row index
starting from `1`) make up the body.

There are three kinds of columns.
- **Separator**. The first equal sign (`=`) occurred at each row.
- **Criteria**. Columns occur to the left of the separator.
- **Data**. Columns occur to the right of the separator.

Cells of the table body by default are of value type `STRING`, but will be converted to fixed-point number type `NUMBER`
if needed. QMEX implements `NUMBER` type as a 32-bit signed integer that is scaled by the factor 1000. Some special values
of `NUMBER` are listed as below.

| Value  | Hexadecimal   | Integer Number   | Real Number      |
|--------| --------------|------------------|------------------|
| `inf`  | `7f ff ff ff` | `2,147,483,647`  | `INFINITY`       |
| `max`  | `7f ff ff fe` | `2,147,483,646`  | `2,147,483.646`  |
| `min`  | `80 00 00 01` | `-2,147,483,647` | `-2,147,483.647` |
| `-inf` | `80 00 00 00` | `-2,147,483,648` | `-INFINITY`      |


## Criteria Operators
The criteria operators indicate how the query value `Q` to be compared with the criteria value `C` at each row of the
table, and also how to calcuate the distance between them. QMEX query will return the row with minimum distance.

| Operator | Definition          | Value Type | Distance Formula                |
|----------|---------------------|------------|---------------------------------|
| **MH**   | MatcH               | STRING     | `(Q match C) ? 0 : INFINITY`    |
| **EQ**   | EQual               | NUMBER     | `(Q == C) ? 0 : INFINITY`       |
| **LT**   | Less Than           | NUMBER     | `(Q < C) ? (C - Q) : INFINITY`  |
| **LE**   | Less Equal          | NUMBER     | `(Q <= C) ? (C - Q) : INFINITY` |
| **GT**   | Greater Than        | NUMBER     | `(Q > C) ? (Q - C) : INFINITY`  |
| **GE**   | Greater Equal       | NUMBER     | `(Q >= C) ? (Q - C) : INFINITY` |
| **AE**   | Approximately Equal | NUMBER     | `abs(Q - C)`                    |

The pattern used in a **MH** operation is basically [glob](https://en.wikipedia.org/wiki/Glob_(programming)) pattern
with multiple patterns extension.

| Character | Description                                         |
|-----------|-----------------------------------------------------|
| `*`       | matches any number of any characters including none |
| `?`       | matches any single character                        |
| `\|`      | matches one of the patterns separated by `\|`       |


## Lua Evaluation
For data columns, if the cell string is enclosed in square brackets `[]`, QMEX treats it as a zero-argument-single-return
lua callable and looks up it or compile it Just-In-Time. If the cell string is enclosed in braces `{}`, QMEX treats it as
a lua table and evaluates it and then uses `[1]` as the cell value.

When evaluating the lua expressions, lua can access the data columns before current column of the same row as global
variables. To access any other data, e.g. criteria columns, you can use `Table::context(KeyValue[], size_t)` to set
corresponding lua global variables before retrieving data.


## Command Line Tool
`qmex-cli` can be used to query a table. By passing the table file path to it on command line, you can input queries
line by line. The format of query string line is:

```
Criteria1:Value1  Criteria2:Value2  ...  Data1  Data2  Data3:Required3 ...
```

By specifying required values on data columns, `qmex-cli` will check whether the retrieved data equal to required values,
and output an error message if they don't.

`qmex-cli` works as lua interpreter if the file path passed to it ends with `.lua`. The whole command line is

```shell
qmex-cli script [arg...]
```
