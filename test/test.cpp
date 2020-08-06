#include <iostream>

#include <mcsv/mcsv.hpp>

int main() 
{    
    // import test
    auto df1 = mcsv::read_csv<4>(std::filesystem::current_path()/"test.csv");
    std::cout << df1 << "\n";
    
    // test vector export
    auto [col3, col4] = df1("col3","col4").cols_to_vectors<double, int>();
    
    // operator() test
    std::cout << "\nOPERATOR() TEST\n";
    std::cout << df1("col2", "col4") << "\n";
    
    // is_in() test
    std::cout << "\nIS_IN TEST\n";
    std::vector<int> vec = {1,2,3,4,5,6,7,8,9,10};
    std::cout << df1.select_rows( df1("col1").is_in(vec) ) << "\n";
    
    // logical test
    std::cout << "\nLOGICAL OPERATORS TEST\n";
    std::cout << df1.select_rows( df1("col2") < std::tuple(10) || df1("col3") > std::tuple(200) ) << "\n";
    
    return 0;
}
