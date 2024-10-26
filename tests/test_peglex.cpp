#include <peglex/peglex.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;

using namespace peglex;

TEST_CASE( "Eps_works", "[Basic Tests]"){
    auto parser = Eps();
    REQUIRE( parser.match("").has_value() );
    REQUIRE( parser.match("a").has_value()  );
}

TEST_CASE( "Any_works", "[Basic Tests]"){
    // Any() matches any character but does 
    // not advance past '\0'
    auto parser = Any();
    REQUIRE( Any().match("a").has_value() );
    REQUIRE( Any().match("").has_value() );

    // check '\0' behavior
    const char *empty = "";
    REQUIRE( *(Any()&Any()).match(empty) == empty );
}

TEST_CASE( "Char_works", "[Basic Tests]"){
    auto parser = Char('a');
    REQUIRE(  parser.match("a").has_value() );
    REQUIRE( !parser.match("b").has_value() );

    // check empty string behavior
    const char *empty = "";
    REQUIRE( !Char('a').match(empty).has_value() );
    REQUIRE(  Char('\0').match(empty).has_value() );
    REQUIRE( *Char('\0').match(empty) == empty );
}

TEST_CASE( "Range_works", "[Basic Tests]"){
    REQUIRE( !Range('1','8').match("0").has_value() );
    REQUIRE(  Range('1','8').match("1").has_value() );
    REQUIRE(  Range('1','8').match("2").has_value() );
    REQUIRE(  Range('1','8').match("3").has_value() );
    REQUIRE(  Range('1','8').match("4").has_value() );
    REQUIRE(  Range('1','8').match("5").has_value() );
    REQUIRE(  Range('1','8').match("6").has_value() );
    REQUIRE(  Range('1','8').match("7").has_value() );
    REQUIRE(  Range('1','8').match("8").has_value() );
    REQUIRE( !Range('1','8').match("9").has_value() );

    // check empty string behavior
    const char *empty = "";
    REQUIRE( *Range('\0', 'z').match(empty) == empty );
}

TEST_CASE( "Str_works", "[Basic Tests]"){
    REQUIRE(  Str("abcd").match("abcdefg").has_value() );
    REQUIRE( !Str("abcd").match("ab").has_value() );

}

TEST_CASE( "Check_works", "[Basic Tests]"){
    REQUIRE( check(eps()&'a'&'b').match("abcde").has_value() );
    REQUIRE( check("ab").match("abcde").has_value() );
    REQUIRE( check("abcd").match("abcde").has_value() );
    REQUIRE( !check("abcd").match("abc").has_value() );

    // verify that check properly rewinds after testing
    const char* example = "abcde";
    REQUIRE( *check("abcd").match(example) == example );
}

TEST_CASE( "Not_works", "[Basic Tests]"){
    const char* example = "abcd";
    // Not should not advance the input on return
    REQUIRE( *(!str("ba")).match(example) == example );
    REQUIRE( !(!str("ab")).match(example).has_value() );
}

TEST_CASE( "ZeroPlus_works", "[Basic Tests]"){
    const char *example = "abababcdef";
    // check that we correctly pick up the 'c' after the 'ab' matches
    REQUIRE( **star(eps()&'a'&'b').match(example) == 'c' );
    // the following is successfully matched zero times, so it should return the original string
    REQUIRE( *star(eps()&'a'&'b'&'c').match(example) == example );

    // note that * is greedy and does not backtrack, so this fails
    // since the star operator consumes "ab" three times leaving none
    // for the and-ed "ab"
    REQUIRE( !(star(eps()&"ab")&"ab").match(example).has_value() );
}

TEST_CASE( "OnePlus_works", "[Basic Tests]"){
    const char *example = "abababcdef";
    // check that we correctly pick up the 'c' after the 'ab' matches
    REQUIRE( **plus(eps()&'a'&'b').match(example) == 'c' );
    // the following is successfully matched zero times, so it should return the original string
    REQUIRE( !plus(eps()&'a'&'b'&'c').match(example).has_value() );

    // note that + is greedy and does not backtrack, so this fails
    // since the star operator consumes "ab" three times leaving none
    // for the and-ed "ab"
    REQUIRE( !(plus(eps()&"ab")&"ab").match(example).has_value() );
}

TEST_CASE("Until_works", "[Basic Tests]"){
    const char *example = "abababcdef";
    REQUIRE( **until('f').match(example) == 'f' );
    REQUIRE( **until("ef").match(example) == 'e' );

    REQUIRE( !until("fg").match(example).has_value() );
}

TEST_CASE("Or_works", "[Basic Tests]"){
    const char *example = "abababcdef";
    // Or greedily matches left, then right, put the hardest patterns on the left
    REQUIRE( **star(str("abc")|str("ab")).match(example) == 'd' );
    REQUIRE( **star(str("ab")|str("abc")).match(example) == 'c' );

    REQUIRE( !(str("ba")|str("bab")).match(example).has_value() );
}

TEST_CASE("Maybe_works","[Basic Tests]"){
    const char *example = "abcdefg";
    REQUIRE( **maybe('a').match(example) == 'b' );
    REQUIRE( **maybe("ab").match(example) == 'c' );
    REQUIRE( **maybe("ba").match(example) == 'a' );
}

TEST_CASE("User_works","[Basic Tests]"){
    const char *example = "abcdef";

    // it's PEG parsers all the way down...
    auto fn = [](const char *s){ 
        return str("bc").match(s); 
    };
    REQUIRE( **('a'&cb(fn)&'d').match(example) == 'e' );
    REQUIRE( !(cb(fn)&'d').match(example).has_value() );
}

TEST_CASE("ExistsCallback_works","[Basic Tests]"){
    const char *example = "abcdefg";

    // State is where things get really funky...

    {
        // This is parsing example for 'a' & 'c', but using a callbacks that
        // trigger when 'a' is matched and when 'c' is not matched which 
        // toggle the states of the bool variables below. The overall expression
        // is not successully matched, but the callbacks are triggered regardless.
        bool a_found  = false, c_found = true;
        auto a_cb = [&a_found]{ a_found = true; };
        auto c_cb = [&c_found]{ c_found = false; };
        REQUIRE( !( 
            cb( 'a', a_cb ) & 
            cb( 'c', defaultExistFn, c_cb ) 
        ).match(example).has_value() ); 
        REQUIRE(  a_found );
        REQUIRE( !c_found );
    }

    {
        // increment scope for each of 'a', 'b' and 'c', log the scope
        // when encountering 'd' and decrement scope for 'e', 'f' and 'g'.
        // capture semantics can get tricky, capturing scope by value
        // will cause d_cb to not see changes from inc_scope and dec_scope
        int d_scope=-1, scope = 0;
        auto inc_scope = [&scope]{ ++scope; };
        auto dec_scope = [&scope]{ --scope; };
        auto d_cb = [&d_scope,&scope]{ d_scope = scope; };
        REQUIRE( 
            ( 
                star(cb( Char('a')|'b'|'c',inc_scope)) & 
                cb('d',d_cb) & 
                star(cb( Char('e')|'f'|'g',dec_scope))
            ).match(example).has_value()
        );
        REQUIRE( scope == 0 );
        REQUIRE( d_scope == 3 );
    }
}

TEST_CASE("Recursion_works", "[Basic Tests]"){
    // define a user function registry, the template parameter is 
    // a type usable as key in std::map (int,const char*,std::string,...),
    // this serves as the grammar state.
    UserFnRegistry<int> user_fns;

    // define a terminal matching a parenthesized expression 
    // that matches a callback registered to index 0
    auto paren = '(' & cb( user_fns.cb(0) ) & ')';

    // a term is just an 'a' or a parenthesized expression
    auto term = 'a' | paren;

    // an expression is just a non-empty sequence of terms
    auto expr = plus(term);

    // bind function 0 to the top-level expression
    // to link expressions back to parens
    user_fns.bind(0,expr);

    //parse a nasty example....
    const char* sample = "(a)((a))a(a)(((a))(a))b";
    REQUIRE( **expr.match(sample) == 'b' );
}


TEST_CASE("StringCallback_works","[Basic Tests]"){

    int max_depth = 0;
    std::vector< std::string > tag_stack;
    bool error_stack_underflow  = false;
    bool error_closed_wrong_tag = false;

    auto reset = [&](){
        error_stack_underflow = false;
        error_closed_wrong_tag = false;
        tag_stack.clear();
        max_depth = 0;
    };

    auto push = [&]( const std::string& t ){ 
        tag_stack.push_back(t); 
        max_depth=std::max(max_depth,int(tag_stack.size())); 
    };

    auto pop = [&]( const std::string& t ){
        if( tag_stack.empty() ){ 
            error_stack_underflow = true;
            error_closed_wrong_tag = true; 
        } else if( tag_stack.back() != t ){ 
            error_closed_wrong_tag = true; 
        } else { 
            tag_stack.pop_back();
        }
    };

    // define the parser itself
    auto tag_parser = plus(
        // self-closed tags
        ( Char('<')&plus(alphanum())&"/>" ) |   
        // opening tags, plus lookahead test
        ( Char('<')&cb(plus(alphanum())&check('>'),push)&'>' ) |
        // closing tags, plug lookahead test
        ( str("</")&cb(plus(alphanum())&check('>'),pop)&'>')
    );

    {
        // should succeed without errors
        const char *xml = "<tag1><tag2><tag3/><tag4/></tag2></tag1>";
        REQUIRE( **tag_parser.match(xml) == '\0' );
        REQUIRE( !error_stack_underflow );
        REQUIRE( !error_closed_wrong_tag );
        REQUIRE( tag_stack.empty() );
        REQUIRE( max_depth == 2 );
    }
    
    reset();

    {
        // should complete but have mismatched tags, stack will not
        // be empty due to push/pop semantics
        const char *xml = "<tag1><tag2><tag3/><tag4/></tag1></tag2>";
        REQUIRE( **tag_parser.match(xml) == '\0' );
        REQUIRE( !error_stack_underflow );
        REQUIRE( error_closed_wrong_tag );
        REQUIRE( !tag_stack.empty() );
        REQUIRE( max_depth == 2 );
    }

    reset();

    {
        // should complete but stack will not be empty
        const char *xml = "<tag1><tag2><tag3/><tag4/></tag2>";
        REQUIRE( **tag_parser.match(xml) == '\0' );
        REQUIRE( !error_stack_underflow );
        REQUIRE( !error_closed_wrong_tag );
        REQUIRE( !tag_stack.empty() );
        REQUIRE( max_depth == 2 );
    }

    reset();

    {
        // should complete but will trigger underflow error and mismatched tag error
        const char *xml = "<tag1><tag2><tag3/><tag4/></tag2></tag1></tag0>";
        REQUIRE( **tag_parser.match(xml) == '\0' );
        REQUIRE( error_stack_underflow );
        REQUIRE( error_closed_wrong_tag );
        REQUIRE( tag_stack.empty() );
        REQUIRE( max_depth == 2 );
    }


}