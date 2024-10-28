#include <peglex/peglex.h>

#include <tuple>
#include <iostream>

/**
 * @brief Creates a parser that matches hex numbers, integers, reals and quoted strings
 * 
 * Parsers are built as trees of peglex::Pattern subclasses, nodes in the tree are very lightweight 
 * and are stored by value using templates. This makes knowing the explicit type of a parser very 
 * difficult since it is often very complex depending on the grammar. It is also very awkward to 
 * reference specific sub-components of the grammar from the parser once constructed.
 * 
 * Stateful parsers are implemented using callback functions. If data-structures outside the parser
 * need to be manipulated, these callback functions can either be passed in or the callbacks can
 * reference the state/data-structures via reference/pointer or global functions.
 * 
 * When only the parser itself needs state, e.g. for higher-level validity checking, the entire 
 * parser can be wrapped in a lambda that accepts an input string and returns user-defined results. 
 * This is convenient, but rebuilds the parser on every invocation.
 *  
 * @return std::function<std::tuple<const char*,std::string>(const char*)>
 */
auto literal_parser(){
    // lambda wrapper for state
    return []( const char* src ){
        using namespace peglex;
        std::string result = "Not Found.";
        auto hex_cb  = [&result]( const std::string& s ){ result = "Hex: "  + s; };
        auto real_cb = [&result]( const std::string& s ){ result = "Real: " + s; };
        auto int_cb  = [&result]( const std::string& s ){ result = "Int: "  + s; };
        auto str_cb  = [&result]( const std::string& s ){ result = "Str: "  + s; };

        // Check requires a delimiter, but will rewind input
        auto delim = Check( whitespace() | eof() );

        // order can matter here, '|' works left to right in PEGs and keeps the first match:
        //  - integers match the 0 in hex numbers
        //  - the integral part of reals match integers
        // requiring delimiters using Check helps avoid this 
        auto literal = cb( "0x" & plus( hex() & hex() ) & delim, hex_cb )
                     | cb(    real() & delim, real_cb ) 
                     | cb( integer() & delim,  int_cb )
                     | ('"' & cb( Until(Check(Char('"'))), str_cb ) & '"');
        
        // match returns an optional with the advanced input pointer when successful
        if( auto ret = literal.match( src ) ){
            return std::tuple{*ret,result};
        }
        return std::tuple{static_cast<const char*>(nullptr),result};
    };
}

int main( int argc, char** argv ){

    auto parser = literal_parser();

    auto [rest,token] = parser("\"What's up?\" and some more stuff");
    std::cout << "Result: " << token << std::endl;
    std::cout << "Remaining: " << (rest ? rest : "invalid") << std::endl;

    return 0;
}