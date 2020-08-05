#include <iostream>

#include "csv.hpp"

int main() 
{    
    csv::default_dataframe df1(std::filesystem::current_path()/".."/"test.csv");
    
    std::cout << "df1:\n" << df1 << "\n";
    
    auto [col3, col4] = df1("col3","col4").cols_to_vectors<double, int>();
    
    auto df2 = df1("col2", "col4");
    
    std::cout << "df2:\n" << df2 << "\n";
    
    std::cout << df1.select_rows( df1("col2","col3") < std::tuple(50,50) ).to_eigen_array<double,2 ,4>() << "\n";
    
    return 0;
}
