#ifndef CSV_HPP
#define CSV_HPP

#include <filesystem>
#include <algorithm>
#include <vector>
#include <fstream>
#include <ostream>
#include <memory>
#include <map>

#if __has_include(<Eigen/Dense>)
#define MCSV_EIGEN_SUPPORT
#include <Eigen/Dense>
#endif

#define FMT_HEADER_ONLY
#include <fmt/format.h>

namespace mcsv {

/// @brief Underlying data storage class. At the start loads the whole data into memory
/// Maybe in future a kind of 'lazy-loader' could be useful
class default_loader
{
    std::vector<std::vector<std::string>> m_data;
    std::vector<std::string> m_header;

    std::map<std::string, std::size_t> m_header_map;

    ///@brief turns a string into a std::vector of strings, based of a delimiter (, at the moment)
    static auto extract_line(const std::string &line, std::optional<std::size_t> expected_size = std::nullopt)
    {
        std::istringstream linestream(line);
        std::vector<std::string> cells;

        if( expected_size )
            cells.reserve(*expected_size);

        // leading whitespaces are erased by std::ws in loop header
        for(std::string cell; std::getline(linestream >> std::ws, cell, ','); )
        {
            // trailing whitespaces are erased here
            cell.erase(std::find_if(cell.rbegin(), cell.rend(), [](auto c) {
                return !std::isspace(c);
            }).base(), cell.end());
            cells.push_back(cell);
        }

        if( expected_size )
            cells.resize(*expected_size);

        return cells;
    }

    /// @brief ensures, that the header does not contain duplicates
    static void throw_if_duplicates(std::vector<std::string> ref_header)
    {
        std::sort(ref_header.begin(), ref_header.end());

        auto test_header = ref_header;
        std::unique(test_header.begin(), test_header.end());

        if( test_header != ref_header )
            throw std::runtime_error("csv-file contains multiple columns with the same name!");
    }

public:
    /// @brief constructs the loader, and loads all data to memory
    default_loader(std::filesystem::path path)
    {
        if( !std::filesystem::exists(path) )
            throw std::runtime_error("path '" + path.string() + "' does not exist!");

        std::ifstream file(path);

        // header
        std::string header_str;
        std::getline(file, header_str, '\n');

        m_header = extract_line(header_str);
        throw_if_duplicates(m_header);
        for(std::size_t i=0ul; i<m_header.size(); ++i)
            m_header_map[ m_header[i] ] = i;

        // body
        for( std::string line; std::getline(file, line, '\n'); )
            m_data.push_back( extract_line(line, m_header.size()) );

    }

    /// @brief getter for the body of the csv-file
    const auto &data() const
    {
        return m_data;
    }

    /// @brief getter for the header of the csv-file
    const auto &header() const
    {
        return m_header;
    }

    /// @brief getter for a map, which relates indices and column-headers
    const auto &header_map() const
    {
        return m_header_map;
    }

    /// @brief access a specific cell in the csv file
    const auto &at(std::size_t row, std::size_t col) const
    {
        if( m_data.size() < row )
            throw std::runtime_error(fmt::format(
                                         "csv file has only {} rows, but row {} has been requested",
                                         m_data.size(), row ));

        if( m_header.size() < col )
            throw std::runtime_error(fmt::format(
                                         "csv file has only {} cols, but col {} has been requested",
                                         m_header.size(), col ));

        return m_data[row][col];
    }
};

/// @brief Dataframe class, which allows easy manipulation of rows and columns.
/// When a manipulating operation is used, a new object is created with updated
/// column- and row mask. No data are copied, since they are stored in a shared pointer.
/// @tparam loader_t implementation of the data storage
/// @tparam C non-type-template-parameter which stores the column-count or -1

template<typename loader_t, int C = -1>
class dataframe
{
    std::shared_ptr<loader_t> m_loader;

    const std::vector<bool> m_row_mask;
    const std::vector<bool> m_col_mask;

public:
    /// @brief no default constructor
    dataframe() = delete;

    dataframe(std::filesystem::path path) :
        m_loader(std::make_shared<loader_t>(path)),
        m_row_mask(m_loader->data().size(), true),
        m_col_mask(m_loader->header().size(), true)
    {
        if constexpr( C != -1 )
            if( cols() != C )
                throw std::runtime_error("internal error: column mismatch in constructor");
    }

private:
    template<typename floader_t, int FC>
    friend class dataframe;

    /// @brief private constructor, used dataframe-manipulation
    dataframe(std::shared_ptr<loader_t> loader,
              const std::vector<bool> &row_mask,
              const std::vector<bool> &col_mask) :
        m_loader(loader),
        m_row_mask(row_mask),
        m_col_mask(col_mask)
    {
        if constexpr( C != -1 )
            if( cols() != C )
                throw std::runtime_error("internal error: column mismatch in constructor");
    }

    /// @brief wrapper-iterator, which holds a base iterator and a reference
    /// to a mask, so it can iterate while respecting the mask. Therefore, each
    /// iterator needs to know its end-iterator.
    template<class base_iterator_t>
    struct masked_iterator
    {
        base_iterator_t iter;
        const base_iterator_t end_iter;
        std::vector<bool>::const_iterator mask_it;

        bool operator != (const masked_iterator & other) const {
            return iter != other.iter;
        }
        void operator ++ () {
            do {
                ++mask_it;
                ++iter;
            }
            while( iter != end_iter && !*mask_it );
        }
        auto operator *  () const {
            return *iter;
        }
    };

    /// @brief helper-type, which enables range-based for-loops for masked iterators
    template<class iterator_t>
    struct iterable_wrapper
    {
        const iterator_t begin_it, end_it;

        auto begin() {
            return begin_it;
        }
        auto end() {
            return end_it;
        }
    };

    /// @brief Static, private function which creates a masked-iterable
    /// @param c Container over which will be iterated
    /// @param mask Mask for the container. Must have the same size as the container
    template<class container_t>
    static auto masked_iterable(container_t &c,
                                const std::vector<bool> &mask)
    {
        using base_it_t = decltype(std::begin(std::declval<container_t>()));

        if( c.size() != mask.size() )
            throw std::runtime_error("tried to create masked iterable with wrong-sized mask");

        // prepare iterators to have correct start-point
        auto base_it = c.begin();
        auto mask_it = mask.begin();

        while( mask_it != mask.end() && !*mask_it )
        {
            ++base_it;
            ++mask_it;
        }

        // initialize masked iterators
        masked_iterator<base_it_t> begin_it{ base_it, c.end(), mask_it };
        masked_iterator<base_it_t> end_it{ c.end(), c.end(), mask_it };

        return iterable_wrapper<decltype(begin_it)> { begin_it, end_it };
    }

    /// @brief generic convert function from string. special handling for empty strings.
    /// @tparam T type to which the string is converted
    /// @param str string which will be converted
    template<class T>
    static auto convert(const std::string &str)
    {
        std::remove_const_t<std::remove_reference_t<T>> val{};

        if( std::is_arithmetic_v<T> && str.empty() )
        {
            val = 0;
        }
        else
        {
            std::stringstream sstr(str);
            sstr >> val;
        }
        return val;
    }

    /// @brief convert helper function for vectors
    template<class T>
    static auto convert(const std::vector<std::string> &str_vec)
    {
        std::vector<T> val_vec;
        val_vec.reserve(str_vec.size());

        for(auto &str : str_vec)
            val_vec.push_back( convert<T>(str) );

        return val_vec;
    }

    /// @brief helper function, which converts a std::tuple,
    /// which is only allowed to contain std::strings, to a
    /// std::array of std::strings
    template<class tuple_t, std::size_t... idx>
    static auto string_tuple_to_array(const tuple_t &tuple,
                                      std::index_sequence<idx...>)
    {
        static_assert( std::tuple_size_v<tuple_t> == sizeof...(idx), "tuple size mismatches index_sequence size");

        std::array<std::string, sizeof...(idx)> array;
        ((std::get<idx>(array) = std::get<idx>(tuple)), ...);
        return array;
    }

    /// @brief helper-function, which appends an array of strings to
    /// a tuple of vectors
    /// @param vector_tuple std::tuple of std::vectors with different types
    /// @param array std::array of std::strings, which will be converted
    template<class tuple_t, std::size_t... idx>
    static void push_str_array_to_vector_tuple(tuple_t &vector_tuple,
            const std::array<std::string, sizeof...(idx)> &array,
            std::index_sequence<idx...>)
    {
        static_assert( std::tuple_size_v<tuple_t> == sizeof...(idx), "tuple size mismatches index_sequence size");

        (std::get<idx>(vector_tuple).push_back(
             convert<typename std::remove_reference_t<decltype(std::get<idx>(vector_tuple))>::value_type>( std::get<idx>(array) )
         ), ...);
    }

    /// @brief static, private helber-function to implement the comparison operators.
    /// @tparam tuple_t instance of std::tuple. The strings are converted in the contained types.
    /// @param pred predicate, which is used to compare (std::less, std::equal_to, ...)
    /// @param tuple tuple which will be compared to array
    /// @param array array which will be compared to tuple after conversion to the specific type
    template<class pred_t, class tuple_t, std::size_t... idx>
    static bool compare_tuple_and_string_array(const pred_t &pred, const tuple_t &tuple,
            const std::array<std::string, std::tuple_size_v<tuple_t>> &array,
            std::index_sequence<idx...>)
    {
        static_assert( std::tuple_size_v<tuple_t> == sizeof...(idx),
                       "tuple and index_sequence have different size" );

        //  convert(array[0]) == tuple[0]  &&  convert(array[1]) == tuple[1]  &&  ...
        return (( pred( convert<decltype(std::get<idx>(tuple))>(std::get<idx>(array)), std::get<idx>(tuple) ) ) && ...);
    }

    /// @brief helper-function to implement the comparison operators.
    /// @param pred predicate, which is used to compare (std::less, std::equal_to, ...)
    /// @param tuple each row is compared to the tuple
    template<class pred_t, class tuple_t>
    auto row_wise_comparison(const pred_t &pred, const tuple_t &tuple) const
    {
        constexpr std::size_t N = std::tuple_size_v<tuple_t>;

        static_assert( C == -1 || C == static_cast<int>(N), "row-wise comparison only possible if tuple size matches column number" );

        if( std::count(m_col_mask.begin(),m_col_mask.end(),true) != N )
            throw std::runtime_error("row-wise comparison only possible if tuple size matches column number");

        auto new_row_mask = m_row_mask;

        // cannot use row_iterable, because we need to stay in sync with the mask
        for(std::size_t i=0ul; i<new_row_mask.size(); ++i)
        {
            if( new_row_mask[i] == true )
            {
                auto array = row_as_str_array<N>(m_loader->data()[i]);

                if( !compare_tuple_and_string_array(pred, tuple, array, std::make_index_sequence<N> {}) )
                    new_row_mask[i] = false;
            }
        }

        return dataframe<loader_t, static_cast<int>(N)>(m_loader, new_row_mask, m_col_mask);
    }

    /// @brief helper-function, which extracts a column as a std::vector of strings
    auto col_as_str_vector(std::size_t idx)
    {
        if( idx > m_col_mask.size() || m_col_mask[idx] == false )
            throw std::runtime_error("col_as_vector: requested invalid column");

        std::vector<std::string> col;
        col.reserve(rows());

        for(const auto &row : row_iterable())
            col.push_back(row[idx]);

        return col;
    }

    /// @brief helper-function, which converts a std::vector of strings to a std::array
    template<std::size_t N>
    auto row_as_str_array(const std::vector<std::string> &row) const
    {
        std::array<std::string, N> array;
        auto array_it = array.begin();

        for(const auto &cell : col_iterable(row) )
        {
            *array_it = cell;
            ++array_it;
        }

        return array;
    }

    /// @brief helper-function, which "extracts" a row as a std::vector of strings
    auto row_as_str_vector(std::size_t idx)
    {
        if( idx > m_row_mask.size() || m_row_mask[idx] == false )
            throw std::runtime_error("row_as_vector: requested invalid column");

        return m_loader->data()[idx];
    }

public:
    /// @brief returns the header of the csv-file
    const auto &header() const
    {
        return m_loader->header();
    }

    /// @brief returns the number of active rows
    auto rows() const
    {
        return std::count(m_row_mask.begin(), m_row_mask.end(), true);
    }

    /// @brief returns the number of active columns
    auto cols() const
    {
        return std::count(m_col_mask.begin(), m_col_mask.end(), true);
    }

    /// @brief returns a row-iterable, which can be used in a range-based for-loop
    auto row_iterable() const
    {
        return masked_iterable(m_loader->data(), m_row_mask );
    }

    /// @brief returns a column-iterable, which can be used in a range-based for-loop
    auto col_iterable(const std::vector<std::string> &row) const
    {
        return masked_iterable(row, m_col_mask);
    }

    /// @brief compares (==) a dataframe with a tuple.
    /// @return a dataframe, which contains only matching rows
    template<class tuple_t>
    auto operator==(const tuple_t &tuple) const
    {
        return row_wise_comparison(std::equal_to{}, tuple);
    }

    /// @brief compares (!=) a dataframe with a tuple.
    /// @return a dataframe, which contains only matching rows
    template<class tuple_t>
    auto operator!=(const tuple_t &tuple) const
    {
        return !( *this == tuple );
    }

    /// @brief compares (<) a dataframe with a tuple.
    /// @return a dataframe, which contains only matching rows
    template<class tuple_t>
    auto operator<(const tuple_t &tuple) const
    {
        return row_wise_comparison(std::less{}, tuple);
    }

    /// @brief compares (<=) a dataframe with a tuple.
    /// @return a dataframe, which contains only matching rows
    template<class tuple_t>
    auto operator<=(const tuple_t &tuple) const
    {
        return row_wise_comparison(std::less_equal{}, tuple);
    }

    /// @brief compares (>) a dataframe with a tuple.
    /// @return a dataframe, which contains only matching rows
    template<class tuple_t>
    auto operator>(const tuple_t &tuple) const
    {
        return row_wise_comparison(std::greater{}, tuple);
    }

    /// @brief compares (>=) a dataframe with a tuple.
    /// @return a dataframe, which contains only matching rows
    template<class tuple_t>
    auto operator>=(const tuple_t &tuple) const
    {
        return row_wise_comparison(std::greater_equal{}, tuple);
    }

    /// @brief logical AND operator, only affects the row mask
    template<int OC>
    auto operator&&(const dataframe<loader_t, OC> &df)
    {
        static_assert( C == -1 || C == OC, "number of columns does not match");

        if( df.m_loader != m_loader )
            throw std::runtime_error("cannot logically combine dataframes of differen csv-files");

        std::vector<bool> new_row_mask(m_row_mask.size());

        for(std::size_t i=0ul; i<m_row_mask.size(); ++i)
            new_row_mask[i] = m_row_mask[i] && df.m_row_mask[i];

        return dataframe<loader_t, C>(m_loader, new_row_mask, m_col_mask);
    }

    /// @brief logical OR operator, only affects the row mask
    template<int OC>
    auto operator||(const dataframe<loader_t, OC> &df)
    {
        static_assert( C == -1 || C == OC, "number of columns does not match");

        if( df.m_loader != m_loader )
            throw std::runtime_error("cannot logically combine dataframes of differen csv-files");

        std::vector<bool> new_row_mask(m_row_mask.size());

        for(std::size_t i=0ul; i<m_row_mask.size(); ++i)
            new_row_mask[i] = m_row_mask[i] || df.m_row_mask[i];

        return dataframe<loader_t, C>(m_loader, new_row_mask, m_col_mask);
    }

    /// @brief filters the dataframe with respect to column names
    /// @param args parameter-pack, which only is allowed to be of string-like types
    template< class... Ts, typename = std::enable_if_t<std::conjunction_v< std::is_convertible<Ts,std::string>... >> >
    auto operator()(Ts... args) const
    {
        std::tuple<Ts...> arg_tuple(args...);
        constexpr std::size_t N = std::tuple_size_v<decltype(arg_tuple)>;

        auto cols = string_tuple_to_array(arg_tuple, std::make_index_sequence<N> {});

        std::vector<bool> new_col_mask(m_col_mask.size(), false);
        for(const auto &col : cols)
        {
            new_col_mask[ m_loader->header_map().at(col) ] = true;
        }

        return dataframe<loader_t, static_cast<int>(N)>(m_loader, m_row_mask, new_col_mask);
    }

    /// @brief filter the rows of a dataframe with help of another dataframe
    /// @param df input-dataframe, from which the rows are used
    /// @return dataframe, which has the rows of the input dataframe, but the original columns
    template<int OC>
    auto select_rows(const dataframe<loader_t, OC> &df)
    {
        static_assert( C == -1 || C == OC, "col count mismatch" );

        if( df.m_loader != m_loader )
            throw std::runtime_error("cannot select rows based on a different csv-file");

        return dataframe<loader_t, C>(m_loader, df.m_row_mask, m_col_mask);
    }

    /// @brief filter the columns of a dataframe with help of another dataframe
    /// @param df input-dataframe, from which the collumns are used
    /// @return dataframe, which has the columns of the input dataframe, but the original rows
    template<int OC>
    auto select_cols(const dataframe<loader_t, OC> &df)
    {
        if( df.m_loader != m_loader )
            throw std::runtime_error("cannot select ros based on a different csv-file");

        return dataframe<loader_t, C>(m_loader, m_row_mask, df.m_col_mask);
    }

    /// @brief extracts one ore more columns as std::vectors
    /// @tparam Ts the types in which the columns can be converted
    /// @return a std::vector, if Ts is just one type, otherwise a std::tuple of std::vectors
    template<typename... Ts>
    auto cols_to_vectors()
    {
        constexpr std::size_t N = std::tuple_size_v<std::tuple<Ts...>>;

        static_assert( C == -1 || C == static_cast<int>(N), "number of template parameters does not match number of cols");

        if( N != cols() )
            throw std::runtime_error("number of template parameters does not match number of cols");

        std::tuple< std::vector<Ts>... > result_tuple;

        for(const auto &row : row_iterable())
        {
            auto array = row_as_str_array<N>(row);
            push_str_array_to_vector_tuple(result_tuple, array, std::make_index_sequence<N> {});
        }

        if constexpr( N == 1ul )
            return std::get<0>(result_tuple);
        else
            return result_tuple;
    }

    /// @brief extracts all rows as std::vectors of a specific type
    /// @tparam T the type in which the rows are converted
    /// @return a std::vector of std::vector of T
    template<typename T>
    auto rows_to_vectors()
    {
        std::vector<std::vector<T>> vecs;

        for(const auto &row : row_iterable())
            vecs.push_back( convert<T>(row) );

        return vecs;
    }

    /// TODO
    template<typename... Ts>
    auto rows_to_tuples()
    {
        // should return std::vector of std::tuple<Ts...>
    }

#ifdef MCSV_EIGEN_SUPPORT

    /// @brief converts a csv-file in an Eigen::Array.
    /// @tparam T the type of the Eigen::Array.
    /// @tparam OR the rows of the Eigen::Array (-1 for Eigen::Dynamic)
    /// @tparam OC the columns of the Eigen::Array (-1 for Eigen::Dynamic)
    /// @return A fixed- or dynamic sized Eigen::Array
    template<typename T, int OR = -1, int OC = -1>
    auto to_eigen_array()
    {
        static_assert( OC == -1 || C == -1 || OC == C, "column number mismatch for fixed size eigen export");

        if( OR != -1 && rows() != OR )
            throw std::runtime_error("row number mismatch for fixed size eigen export");

        if( OC != -1 && cols() != OC )
            throw std::runtime_error("column number mismatch for fixed size eigen export");

        Eigen::Array<T, OR, OC> array;
        array.resize(rows(),cols());

        Eigen::Index r = 0;
        for(const auto &row : row_iterable())
        {
            Eigen::Index c = 0;
            for(const auto &cell : col_iterable(row))
            {
                array(r,c) = convert<T>(cell);
                ++c;
            }
            ++r;
        }

        return array;
    }

    /// @brief converts a csv-file in an Eigen::Matrix.
    /// @tparam T the type of the Eigen::Matrix.
    /// @tparam OR the rows of the Eigen::Matrix (-1 for Eigen::Dynamic)
    /// @tparam OC the columns of the Eigen::Matrix (-1 for Eigen::Dynamic)
    /// @return A fixed- or dynamic sized Eigen::Matrix
    template<typename T, int OR = -1, int OC = -1>
    auto to_eigen_matrix()
    {
        return to_eigen_array<T,OR,OC>().matrix();
    }

#endif // CSV_EIGEN_SUPPORT
};

using default_dataframe = dataframe<default_loader>;

/// @brief not very sophisticated print method
template<class loader_t, int C>
auto &operator<<(std::ostream &os, const dataframe<loader_t, C> &df)
{
    for( const auto &cell : df.col_iterable(df.header()) )
        os << cell << '\t';
    os << '\n';

    for( const auto &row : df.row_iterable() )
    {
        for( const auto &cell : df.col_iterable(row) )
            os << cell << '\t';
        os << '\n';
    }

    return os;
}

/// @brief utility function to read csv-file
auto read_csv(std::filesystem::path path)
{
    return default_dataframe(path);
}

} // namespace csv

#undef CSV_EIGEN_SUPPORT

#endif
