#include <iostream>

int main(){
    int a = 10;
    int *p = &a;

    std::cout << *p << std::endl; //this should output 10, dereferencing
    std::cout << p << std::endl; // thiss should print the adress 
    
    return 0;
}