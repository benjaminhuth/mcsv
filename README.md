# MCSV
CSV-library in C++17 which is not a Microsoft compiler. Highlights:
* Pandas-like manipulation
* Extraction as `Eigen::Array` or `Eigen::Matrix`
* header-only

## Usage
A CSV-library, which is meant allow easy dataframe-manipulation inspired by the python-library [pandas](https://pandas.pydata.org/docs/index.html).

### Loading and viewing the data
Simply load a csv-file by passing its path as `std::string` or `std::filesystem::path` to the utility-function `mcsv::read_csv`. The csv-file can then be printed e.g. with `std::cout`.

```c++
auto df1 = mcsv::read_csv("test.csv");

std::cout << df1 << std::endl;
```
Output:

```
col1 col2 col3 col4
1    2    3    4
10   20   30   40
100  200  300  400
```

If the number of columns of the csv-file is known at compile-time, we can pass it as a template-parameter to enable additional compile-time checks. The constructor of the dataframe checks if the number is correct and throws an exception otherwise.

```c++
auto df1 = mcsv::read_csv<4>("test.csv");
```

Some notes on the csv-file import:

* All column headers must be unique
* The number of columns is determined by the first row (the 'header')
* If a row has less entries then the header, the row is filled with empty strings
* If a row has more entries then the header, the row is simply cut at the end

### Filtering the data
There exist several possibilities to filter rows and columns of the csv-file. The basic principle is the following: Each filter-operation returns a new `dataframe`-object. This works without copying the data, all dataframes originating in a certain file hold one `std::shared_ptr` to the actual data. The only things that are changed by these operations are the information, which columns or rows are active.

* Filter columns with `operator()`: 

```c++
auto df2 = df1("col2","col4");

std::cout << df2 << std::endl;
```
Output:

```
col2 col4
2    4
20   40
200  400
```

* Filter rows with comparison-operators:

```c++
auto df3 = (df2 > std::tuple(10,10))

std::cout << df3 << std::endl;
```
Output

```
col2 col4
20   40
200  400
```

* Combined filtering

```c++
auto df4 = df3.select_rows( df3("col2") == std::tuple(20) );

std::cout << df4 << std::endl;
```
Output:

```
col2 col4
20   40
```

* Filter with STL-containers

Note: At the moment, this works only, if the dataframe has exactly one row.

```c++
std::vector<int> vec = { 1,2,3,4,5,6,7,8,9,10 };
auto df5 = df1.select_rows( df1("col1").is_in(vec) )

std::cout << df5 << std::endl;
```
Output:

```
col1 col2 col3 col4
1    2    3    4
10   20   30   40
```

* Use logical operators

Note: So fare, the logical operators only change the rows. The columns stay untouched.

```c++
auto df6 = df1.select_rows( df1("col2") < std::tuple(10) || df1("col3") > std::tuple(200) )

std::cout << df6 << std::endl;
```
Output:

```
col1 col2 col3 col4
1    2    3    4
100  200  300  400
```

### Iterating through the data
The `dataframe` class provides an easy-to-use itable for range-based for-loops:

```c++
for( const auto &row : df1.row_iterable() )
    for( const auto &cell : df1.col_iterable(row) )
        do_something_with(cell);
```

### Extracting data

* Extract columns as `std::vector`. The result is given as a `std::tuple< std::vector<T1>, ... >`:

```c++
auto [col2, col4] = df1("col2","col4").cols_to_vectors<double, int>();
```

* Extract rows as `std::vector`. The result is given as a `std::vector< std::vector<T> >`:

```c++
auto rows = df1( df1("col1") < std::tuple(100) ).rows_to_vectors<double>();
```

### Extracting data to Eigen-Objects

* If the header `<Eigen/Dense>` is found, export to `Eigen::Matrix` and `Eigen::Array` is enabled:

```c++
auto mat = df1.to_eigen_matrix<double>();
auto arr = df1.to_eigen_array<double>();
```

Extraction as fixed-size arrays is possible (`df1.to_eigen_matrix<T,Row,Col>()`), but then the number of rows and columns must be known at compile time.


## Error handling

As mutch checks as possible are done at compile time. When filtering out columns e.g. with `df1("col2","col3")`, the number of columns is stored as a integer template parameter, which can be used du ensure the validity of subsequent operations. What remains is handled by throwing exceptions at runtime.
