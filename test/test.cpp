#include <iostream>

#include <mcsv/mcsv.hpp>

int main() 
{    
    mcsv::default_dataframe df1(std::filesystem::current_path()/"test.csv");
    
    std::cout << "df1:\n" << df1 << "\n";
    
    auto [col3, col4] = df1("col3","col4").cols_to_vectors<double, int>();
    
    auto df2 = df1("col2", "col4");
    
    std::cout << "df2:\n" << df2 << "\n";
    
    std::cout << df1.select_rows( df1("col2") < std::tuple(10) || df1("col3") > std::tuple(200) ) << "\n";
    
    return 0;
}
