#include <string>
#include <functional>
#include <iostream>
#include <optional>
 
// optional can be used as the return type of a factory that may fail
std::optional<std::string> create(bool b) {
//char* create(bool b) {
    if (b)
        return "Godzilla";
    return {};
}
 
// std::nullopt can be used to create any (empty) std::optional
auto create2(bool b) {
    return b ? std::optional<std::string>{"Godzilla"} : std::nullopt;
}
 
// std::reference_wrapper may be used to return a reference
auto create_ref(bool b) {
    static std::string value = "Godzilla";
    return b ? std::optional<std::reference_wrapper<std::string>>{value}
             : std::nullopt;
}
 
int main()
{
    std::cout << "create(false) returned ";
    //          << create(false).value_or("empty") << '\n';
    auto beast = create(true);
    if (beast)
    {
        std::cout << "contextualy converted to bool and created correctly. I am "<< *beast << beast.value() << std::endl;

    }
    //
    //
    //BECAREFULL
    //
    //
    beast = create(false);
    if (!beast)
    {
        std::cout << "create(false) . I am "<< *beast << std::endl;
        try
        {
            /* code */
            std::cout << "wrongly created. I am "<< beast.value() << std::endl;

        }
        catch(const std::exception& e)
        {
            std::cout << std::endl << "++ a shot in the foot ++"<< std::endl;;
            std::cout << e.what() << '\n';
            std::cout << std::endl << "exception " << std::endl ;
        }
        

    }
    
    auto biggerBeast = create(false);
    if (!biggerBeast)
    {
        std::cout << "create bigger beast (false) . I am "<< *biggerBeast << std::endl;
        try
        {
            /* code */
            std::cout << "access value of wrongly created. I am "<< biggerBeast.value() << std::endl;

        }
        catch(const std::exception& e)
        {
            std::cout << std::endl << "++ a shot in the foot ++"<< std::endl;;
            std::cout << e.what() << '\n';
            std::cout << std::endl << "exception ";
        }
        

    }
 
    // optional-returning factory functions are usable as conditions of while and if
    if (auto str = create2(true)) {
        std::cout << "create2(true) returned " << *str << '\n';
    }
 
    if (auto str = create_ref(true)) {
        // using get() to access the reference_wrapper's value
        std::cout << "create_ref(true) returned " << str->get() << '\n';
        str->get() = "Mothra";
        std::cout << "modifying it changed it to " << str->get() << '\n';
    }
}
