# Peglex

Peglex is an implementation of [Parsing Expression Grammars](https://en.wikipedia.org/wiki/Parsing_expression_grammar) (PEGs) as a compact single-header C++20 library. It was written originally as test/learning code but turns out to be quite useful for a variety of string parsing tasks. It has no dependencies other than the C++ standard library (although the unit-test suite uses [Catch2](https://github.com/catchorg/Catch2)).

Grammars are constructed in C++ code directly using the templated nodes types provided in [peglex.h](./peglex/include/peglex/peglex.h), which is the only file needed to use the library. These nodes build grammars as a compile-time tree where child nodes are stored by value. Overloaded operators make the definition of complex grammars relatively natural. With the basic types, strings can be validated as being valid/invalid with respect to the grammar. With the more advanced node types, quite complex tasks can be addressed.

Since grammars are built as compile-time trees with nodes stored by value, parsers require no dynamic memory allocation in most cases. Furthermore, since the nodes are arranged in depth-first order, this should also provide relatively good cache-coherence. That said, I don't know if it's particularly fast. Probably not since the 'and' and 'or' operators are binary...

> **Editorial:** Overall, whether you use it or not, I hope that it's clear that this sub-500 line (including whitespace and comments) library does **a lot** and highlights how powerful PEGs are. The concepts were introduced in the '70s and formalized in 2004 but are far too powerful to stay as obscure as they have been, in my opinion. This README, without line-wrapping, is ~2/3 as long as the entire library....

## Introductions, Basics & Helpers

Grammars are defined by `peglex::Pattern` subclasses. These provide a `std::optional<const char*> PatternSubType::match(const char*) const` method that returns an optional which contains the advanced input pointer on success. That is the entire interface to the core of the library.

A number of convenience definitions are provided to easily construct more complex grammars and highlight how this simple idea can be built up. Names with leading upper-case letters denote `peglex::Pattern` subclasses and while lower-case are free functions:

```cpp
inline Char  eof(){ return Char('\0'); }
inline Char  space(){ return Char(' '); }
inline Char  tab(){ return Char('\t'); }
inline Char  carriage_return(){ return Char('\r'); }
inline Char  newline(){ return Char('\n'); }
inline auto  whitespace(){ return space() | tab() | carriage_return() | newline(); }  
inline Range digit(){ return {'0','9'}; }
inline auto  hex(){ return Range{'0','9'} | Range('a','f') | Range('A','F'); }
inline Range lower(){ return {'a','z'}; }
inline Range upper(){ return {'A','Z'}; }
inline auto  alpha(){ return lower() | upper(); }
inline auto  alphanum(){ return alpha() | digit(); }
inline auto  digits(){ return plus( digit() ); }
inline auto  pm(){ return Char('+') | Char('-'); }
inline auto  integer(){ return maybe(pm()) & digits(); }
inline auto  real(){ return maybe(pm()) & digits() & Char('.') & maybe(digits()) & maybe( (Char('e')|Char('E')) & maybe(pm()) & digits() ); }
```
As evidenced by the rampant use of `auto`, knowing the concrete type of complex expressions is often quite challenging. Embrace `auto`: it's been 13 years, it's old enough to roll it's eyes when you complain and it's here to stay. 

The final expression, `real()`, returns a parser for real numbers that match `1.`, `1.234343`, `-1.`, `+1.3344`, `+1.354e4`, `-3454.345E11`, `22.E-23` & `+143.34e+4` along with analogous strings. It, along with all Peglex grammars, can be used like this:

```cpp
auto parser = peglex::real();
auto res = parser.match(some_const_char_ptr); // res is std::optional<const char*>
if( res ){
    std::cout << "remaining string is: " << *res << std::endl;
} else {
    std::cout << "match failed." << std::endl;
}
```

The `&` and `|` operators build grammar elements whenever at least one of the left/right operands is a grammar element and the other type, if present, is `char` or `const char*`. The `!(expr)` operator negates a match for `expr`, leaving the input pointer unchanged when successful (i.e. when `expr` does **not** match). `(expr)?` is implemented as `maybe(expr)`, `(expr)*` is implemented as `star(expr)` and `(expr)+` is implemented as `plus(expr)` since the association/(un|bin|trin)aryness of the corresponding C++ operators do not match conventional grammars.

> **Note:** People new to PEGs should note that **unlike [Regular Expressions](https://en.wikipedia.org/wiki/Regular_expression)**, the `+` and `*` operators are **[greedy](https://en.wikipedia.org/wiki/Greedy_algorithm)**. Consequently grammars like `(ab)*ab` (or `star("ab")&"ab"` in Peglex) will **never match successfully** since the `*` operator will consume all the "ab" substrings and fail to match the final "ab". In other words, the `*` and `+` operators do not [backtrack](https://en.wikipedia.org/wiki/Backtracking). 

PEGs are very nice since they provide **arbitrary lookahead**. This is hugely useful for resolving nearly ambiguous grammars. Peglex exposes this with the `check(expr)` function which verifies that `expr` matches and then rewinds to the beginning of the match. Since `expr` can be any Peglex grammar, `check(expr)` allows lookahead at the character, string or grammar level: you could require that field values in a JSON document are valid XML strings, for example. This comes at a cost though: [PEGs have worst-case exponential processing time](https://en.wikipedia.org/wiki/Parsing_expression_grammar#Implementing_parsers_from_parsing_expression_grammars) due to their unbounded lookahead. That said, you control the heat: the grammar that you define in turn defines the worst-case processing cost.

## Callbacks & User Defined Nodes

By including callback nodes in the tree, a variety of other useful tasks can be unlocked. Callbacks are exposed as overloads of the `peglex::cb( ... )` function and can be triggered when grammar elements match or fail to match. The latter is useful for error reporting. Callbacks can also return the matched text either as pointers into the original data or as new strings (this triggers dynamic allocations). Finally, entirely new behavior can be implemented with user-provided callbacks that mirror the interface to `peglex::Pattern::match`. 

This simple example uses a callback to reference an 'external' grammar element (which could be dynamic and passed in from outside the grammar's scope) but the possibilities get much more powerful:

```cpp
TEST_CASE("User_works","[Basic Tests]"){
    const char *example = "abcdef";

    // it's PEG parsers all the way down...
    auto fn = [](const char *s){ 
        return str("bc").match(s); 
    };
    REQUIRE( **('a'&cb(fn)&'d').match(example) == 'e' );
    REQUIRE( !(cb(fn)&'d').match(example).has_value() );
}
```

## Lookahead & Conditional Matching

PEGs provide theoretically arbitrary lookahead and this is exposed via the `check(expr)` function. `check(expr)` matches the provided expression and rewinds the input when successful. Combined with callbacks, this allows extracting substrings from relatively complex inputs. The `until(expr)` function sweeps the input until a match for the provided expression is found and then rewinds to the start of the match. This allows searching for matches within strings to be implemented (but be aware that it can entirely consume the input and lead to exponential processing time):

```cpp
// conditional matching with check()
TEST_CASE( "Check_works", "[Basic Tests]"){
    REQUIRE( check(eps()&'a'&'b').match("abcde").has_value() );
    REQUIRE( check("ab").match("abcde").has_value() );
    REQUIRE( check("abcd").match("abcde").has_value() );
    REQUIRE( !check("abcd").match("abc").has_value() );

    // verify that check properly rewinds after testing
    const char* example = "abcde";
    REQUIRE( *check("abcd").match(example) == example );
}

// sweeping through input until a match succeeds
TEST_CASE("Until_works", "[Basic Tests]"){
    const char *example = "abababcdef";
    REQUIRE( **until('f').match(example) == 'f' );
    REQUIRE( **until("ef").match(example) == 'e' );

    REQUIRE( !until("fg").match(example).has_value() );
}
```

## Stateful Callbacks & Encapsulation

The compile-time nature of Peglex makes knowing the specific type of a parser a complex affair. Similarly, state handling can be tricky since C++ lambdas returned from functions cannot bind locals by reference, at least not without segfaulting. Thanks Bjarne Stroustrup!

Still, the whole show can simply be wrapped in it's own lambda and returned to be used easily, like in this example that matches hexadecimal numbers, real numbers, integers and double-quoted strings:

```cpp
#include <peglex/peglex.h>
using namespace peglex;

#include <iostream>

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
```

which prints:

```bash
Result: Str: What's up?
Remaining:  and some more stuff
```

Note the double space following `Remaining:`. It illustrates that `Check( whitespace() | eof() )` both matched the delimiter (since the result was printed) and rewound it (since there are two spaces).

> **Note:** This approach rebuilds the parse tree on every invocation of `parser(const char*)` so it actually pays off to make parsers more complex to amortize this cost if you opt to follow this pattern.

While the above example only manipulated & encapsulated local parser state, more complex behavior that interacts with the broader program can also be handled. To do this, the necessary state that the parser interacts with must be either:

- free-functions or global variables (yuck!) accessible from the parser construction function, or
- captured rvalues (i.e. not dynamic), or
- lvalues passed in to the parser construction function **that will outlive any use of the returned parser**.

## Recursive Grammars

PEGs have no theoretical difficulty with recursive grammars but the compile-time approach to defining grammars in Peglex does. This is because inner/leaf nodes of the grammar tree cannot refer to their ancestors (which are necessarily defined afterwards). 

However, by including state and a level of indirection, recursive grammars can be implemented using dynamic binding. Using this approach leaf/inner grammar elements reference callbacks that are triggered when ancestors in the grammar tree match. These callbacks are resolved at runtime. The bindings are necessarily defined after the leaf/inner elements due to the compile-time expression definition approach. `Peglex` exposes this with a `UserFnRegistry<KeyType>` class that simplifies dynamic binding from a usage point of view.

Since most grammars don't have that many recursive elements, this neatly sidesteps the issues of cycles in a static grammar tree. In turn it allows self-referential grammars like expressions with parenthesized sub-expressions to be realized, in this case in just 5 lines of code:

```cpp
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
```

> **Editorial:** Getting recursive grammars to actually work can be **exceptionally** irritating...

The provided implementation of this, using `UserFnRegistry<KeyType>`, requires dynamic allocations for the `std::map`. However, you could avoid this fairly easily if it's a problem since the implementation is quite simple (although arguably the most complex part of the library):

```cpp
    template< typename Key >
    struct UserFnRegistry {
        UserFnRegistry(){}

        template< typename Expr >
        requires std::derived_from<Expr,Pattern>
        void bind( const Key& key, Expr& expr ){
            set( key, std::bind(&Expr::match,expr,std::placeholders::_1) );
        }

        UserFn cb( const Key& key ) const {
            return std::bind(&UserFnRegistry<Key>::match,this,key,std::placeholders::_1);
        }

        std::optional<const char*> match( const Key& key, const char* src ) const {
            return get(key)(src);
        }

        void set( const Key& key, UserFn fn ){
            if( _registry.find(key) != _registry.end() ){
                throw std::runtime_error("Error: tried to add duplicate UserFn for key.");
            }
            _registry[key] = fn;
        }

        UserFn get( const Key& key ) const {
            if( auto it = _registry.find(key) ; it != _registry.end() ){
                return it->second;
            }
            throw std::runtime_error("Error: UserFn not registered.");
            return {};
        }

        std::map<Key,UserFn> _registry;
    };
```

## Advanced Usage

Things are getting real but lets go further. By using stateful callbacks, a variety of relatively complex tasks can be handled. Scope level can be enumerated and imbalanced tag opening/closing for HTML/XML can be handled, as illustrated in this test case that (again) uses only 5 lines of code to define the parser itself:

```cpp
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

    ... more test cases ...

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
```

## User Defined Extensions

While you can always extend the library functionality at run-ish-time by using the `User` node type and providing a callback function, you can also create additional nodes that *should* interoperate with the broader library at compile-time. This is due to the use of C++ concepts (thanks Bjarne Stroustrup!). Just inherit from `peglex::Pattern` and implement `std::optional<const char*> YourNewNodeType::match( const char* ) const override`, similar to the following library example:

```cpp
    // called whenever a given token matches
    using ExistCallbackFn   = std::function<void()>;
    using MissingCallbackFn = std::function<void()>; 

    inline void defaultExistFn(){}
    inline void defaultMissingFn(){}

    // You need to do something like the following...maybe without the callbacks.
    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    struct ExistCallback : public Pattern {
        ExistCallback( const Expr& expr, ExistCallbackFn exist_fn, MissingCallbackFn missing_fn ) : _expr{expr}, _exist_fn{exist_fn}, _missing_fn{missing_fn} {}
        std::optional<const char*> match( const char* src ) const override {
            if( auto ret = _expr.match(src) ){
                _exist_fn();
                return ret;
            }
            _missing_fn();
            return std::nullopt;
        }
        const Expr          _expr;
        ExistCallbackFn     _exist_fn;
        MissingCallbackFn   _missing_fn;
    };
```

Just make sure any child nodes are stored by value or you will have a rough time.

This is the **only** reason why C++20 is required; you could pretty easily strip all the `requires` lines from `./peglex/include/peglex/peglex.h` and get a C++11/14/17 (probably!?!?) library. Then you'd have to just deal with the million lines of inscrutable error messages when you make a minor mistake. Or embrace that it's 2024 and we can have nice things, unless you're in industry.