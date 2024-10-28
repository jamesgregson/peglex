#include "sample2.h"

#include <peglex/peglex.h>

#include <cstdint>
#include <iostream>

auto get_compiler(){
    return []( StatementVM& vm, int16_t& line, const char* input ) {
        namespace pl = peglex;

        pl::UserFnRegistry<int> user_fn;

        // whitespace
        auto wschar  = pl::space() | pl::tab() | pl::carriage_return();
        auto ws      = pl::star(wschar);

        // identifiers and statement delimiter
        auto ident   = pl::alpha() & pl::star( pl::alphanum() );
        auto real    = pl::cb( pl::real(), [&]( auto s ){ vm.emit_loadc(s); }) & ws;
        auto rvalue  = pl::cb( ident,      [&]( auto s ){ vm.emit_loadv(s); }) & ws;
        auto lvalue  = pl::cb( ident,      [&]( auto s ){ vm.emit_loada(s); }) & ws;

        auto factor = rvalue | real | ( '(' & ws & pl::cb( user_fn.cb(0) ) & ')' & ws );
        auto term = factor & star( 
            pl::cb( pl::Char('*') & ws & factor, [&](){ vm.emit_mul(); } ) |
            pl::cb( pl::Char('/') & ws & factor, [&](){ vm.emit_div(); } )
        );
        auto expr = term & star( 
            pl::cb( pl::Char('+') &ws & term, [&](){ vm.emit_add(); } ) |
            pl::cb( pl::Char('-') &ws & term, [&](){ vm.emit_sub(); } )
        );

        // bind exprs back to inside parenthesized expressions
        user_fn.bind( 0, expr );

        auto print = pl::cb( pl::Str("print") & ws & '(' & ws & expr & ws & ')' & ws, [&](){ 
            vm.emit_print(); }
        );

        auto stmt = print
                  | pl::cb(lvalue & '=' & ws & expr & ws, [&](){ vm.emit_store(); });
        
        auto parser = pl::cb( pl::eps(), [&](){ vm.emit_line(line); } ) & stmt & ws & pl::eof();

        if( !parser.match( input ) ){
            std::cerr << "Compile error on line: " << line << std::endl;
            throw std::runtime_error("Compilation failed.");
        }
    };
};

int main( int argc, char **argv ){
    auto compiler = get_compiler();

    int16_t line = 0;
    StatementVM vm;
    compiler(vm, ++line, "a = 2.0" );
    compiler(vm, ++line, "b = (5.0*(1.0 + 2.0*(3.0+a)) )" );
    compiler(vm, ++line, "print( b-a )" );

    std::cout << "Decompiled" << std::endl;
    std::cout << "==========" << std::endl; 
    std::cout << vm.decompile() << std::endl << std::endl;

    std::cout << "Running Program" << std::endl;
    std::cout << "===============" << std::endl;
    vm.run();

    return 0;
}