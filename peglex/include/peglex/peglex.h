// (c) James Gregson 2024, MIT license
#pragma once 

#include <algorithm>
#include <concepts>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>

namespace peglex {

    /**
     * @brief Top level pure virtual pattern class only defines match method
     */
    struct Pattern {
        /**
         * @brief Returns advanced source pointer on success or std::nullopt on failure
         * @param src Source pointer to parse
         * @return std::optional<const char*>  
         */
        virtual std::optional<const char*> match( const char* src ) const = 0;
    };

    /**
     * @brief Epsilon, always matches but does not advance source
     * Useful for building expressions from raw characters and strings with overloaded operators
     */
    struct Eps : public Pattern {
        Eps(){}
        std::optional<const char*> match( const char* src ) const override {
            return {src};
        }
    };
    inline Eps eps(){ return Eps(); }

    /**
     * @brief Any, matches any single character
     */
    struct Any : public Pattern {
        Any(){}
        std::optional<const char*> match( const char* src ) const override {
            if( src ){
                return {*src ? src+1 : src};
            }
            return std::nullopt;
        }
    };
    inline Any any(){ return Any(); }

    /**
     * @brief Char, matches a single character exactly
     */
    struct Char : public Pattern {
        Char( char c ) : _c{c} {}
        std::optional<const char*> match( const char* src ) const override {
            if( src && *src == _c ){
                return {_c ? src+1 : src};
            }
            return std::nullopt;
        }
        const char _c;
    };


    /**
     * @brief Range, matches a single character within a character range
     */
    struct Range : public Pattern {
        Range( char lo, char hi ) : _lo{lo}, _hi{hi} {}
        std::optional<const char*> match( const char* src ) const override {
            if( src && *src >= _lo && *src <= _hi ){
                return {*src ? src+1 : src};
            }
            return std::nullopt;
        }
        const char _lo, _hi;
    };
    inline Range range( char lo, char hi ){ return Range(lo,hi); }

    /**
     * @brief Str, matches an exact string character by character, excluding null terminator
     */
    struct Str : public Pattern {
        Str( const char* seq ) : _seq{seq} {}
        std::optional<const char*> match( const char* src ) const override {
            const char* sptr = _seq;
            while( src && *src && *sptr ){
                if( *sptr != *src ){
                    return std::nullopt;
                }
                ++sptr;
                ++src;
            }
            if( *sptr == '\0' ){
                return src;
            }
            return std::nullopt;
        }
        const char* _seq;
    };
    inline Str str( const char* seq ){ return Str(seq); }

    /**
     * @brief Check, matches the provided expression, rewinding input on success
     */
    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    struct Check : public Pattern {
        Check( const Expr& expr ) : _expr{expr} {}
        std::optional<const char*> match( const char* src ) const override {
            if( auto ret = _expr.match(src) ){
                return src;
            }
            return std::nullopt;
        }
        const Expr _expr;
    };
    template< typename Expr >
    Check<Expr> check( const Expr& expr ){ return Check<Expr>(expr); }
    inline Check<Char> check( const char c ){ return Check<Char>(c); }
    inline Check<Str> check( const char* s ){ return Check<Str>(s); }

    /**
     * @brief Ensures that the string does not matches the provided expression, rewinding input on success
     */
    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    struct Not : public Pattern {
        Not( const Expr& expr ) : _expr{expr} {}
        std::optional<const char*> match( const char* src ) const override {
            if( auto ret = _expr.match(src) ){
                return std::nullopt;
            }
            return {src};
        }
        const Expr _expr;
    };


    /**
     * @brief Zero-plus, matches provided expression zero or more times, greedily
     */
    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    struct ZeroPlus : public Pattern {
        ZeroPlus( const Expr& expr ) : _expr{expr} {}
        std::optional<const char*> match( const char* src ) const override {
            while( src ){
                if( auto tmp = _expr.match( src ) ){
                    src = *tmp;
                } else {
                    return {src};
                }
            }
            return src;
        }
        const Expr _expr;
    };
    template< typename Expr >
    ZeroPlus<Expr> star( const Expr& expr ){ return ZeroPlus<Expr>(expr); }
    inline ZeroPlus<Char> star( const char c ){ return ZeroPlus<Char>(c); }
    inline ZeroPlus<Str> star( const char* s ){ return ZeroPlus<Str>(s); }


    /**
     * @brief Consumes input until the expression would match, rewinding to start of match
     * Use carefully, will process ALL input if match fails.
     */
    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    struct Until : public Pattern {
        Until( const Expr& expr ) : _expr{expr} {}
        std::optional<const char*> match( const char* src ) const override {
            while( src && *src ){
                if( auto tmp = _expr.match(src) ){
                    return src;                               
                } else {
                    ++src;
                }
            }
            return std::nullopt;
        }
        const Expr _expr;
    };
    template< typename Expr >
    Until<Expr> until( const Expr& expr ){ return Until<Expr>(expr); }
    inline Until<Char> until( const char c ){ return Until<Char>(c); }
    inline Until<Str> until( const char* s ){ return Until<Str>(s); }

    /**
     * @brief Matches the left expression greedily, on failure rewinds and matches the right
     */
    template< typename Left, typename Right >
    requires std::derived_from<Left,Pattern> && std::derived_from<Right,Pattern>
    struct Or : public Pattern {
        Or( const Left& left, const Right& right ) : _left{left}, _right{right} {}
        std::optional<const char*> match( const char* src ) const override {
            if( auto res = _left.match(src) ){
                return res;
            }
            return _right.match(src);
        }
        const Left  _left;
        const Right _right;
    };

    /**
     * @brief And, matches the left expression and, on success, matches the right
     */
    template< typename Left, typename Right >
    requires std::derived_from<Left,Pattern> && std::derived_from<Right,Pattern>
    struct And : public Pattern {
        And( const Left& left, const Right& right ) : _left{left}, _right{right} {}
        std::optional<const char*> match( const char* src ) const override {
            if( auto tmp = _left.match(src) ){
                return _right.match(*tmp);
            }
            return std::nullopt;
        }
        const Left  _left;
        const Right _right;
    };


    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    Or<Expr,Eps> maybe( const Expr& expr ){
        // Eps must be last so that expr is tried
        return Or<Expr,Eps>(expr,Eps());
    }
    inline Or<Char,Eps> maybe( const char c ){ return Or<Char,Eps>(Char(c),eps()); }
    inline Or<Str,Eps> maybe( const char* s ){ return Or<Str,Eps>(Str(s),eps()); }

    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    auto plus( const Expr& expr ){
        return And<Expr,ZeroPlus<Expr>>(expr,ZeroPlus(expr));
    }
    inline auto plus( const char& c ){ return plus(Char(c)); }
    inline auto plus( const char* c ){ return plus(Str(c)); }

    // lexer/parser hooks
    // Wrapper for a user-defined parseing function, also used to
    // implement recursive grammars
    using UserFn = std::function<std::optional<const char*>(const char*)>;

    struct User : public Pattern {
        User( UserFn fn ) : _fn(fn) {}
        std::optional<const char*> match( const char* src ) const override {
            return _fn(src);
        }
        UserFn _fn;
    };

    inline User cb( UserFn fn ){
        return User(fn);
    }

    // called whenever a given token matches
    using ExistCallbackFn   = std::function<void()>;
    using MissingCallbackFn = std::function<void()>; 

    inline void defaultExistFn(){}
    inline void defaultMissingFn(){}

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

    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    ExistCallback<Expr> cb( const Expr& expr, ExistCallbackFn exist_fn, MissingCallbackFn missing_fn=defaultMissingFn ){
        return ExistCallback<Expr>( expr, exist_fn, missing_fn );
    }
    inline ExistCallback<Char> cb( const char c, ExistCallbackFn exist_fn, MissingCallbackFn missing_fn=defaultMissingFn ){
        return ExistCallback<Char>( Char(c), exist_fn, missing_fn );
    }
    inline ExistCallback<Str> cb( const char* c, ExistCallbackFn exist_fn, MissingCallbackFn missing_fn=defaultMissingFn ){
        return ExistCallback<Str>( Str(c), exist_fn, missing_fn );
    }

    // zero-allocation callback that also provides the matching string
    using RangeCallbackFn = std::function<void(const char*,const char*)>;

    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    struct RangeCallback : public Pattern {
        RangeCallback( const Expr& expr, RangeCallbackFn exist_fn, MissingCallbackFn missing_fn ) : _expr{expr}, _exist_fn{exist_fn}, _missing_fn{missing_fn} {}
        std::optional<const char*> match( const char* src ) const override {
            if( auto ret = _expr.match(src) ){
                _exist_fn(src,*ret+1);
                return ret;
            }
            _missing_fn();
            return std::nullopt;
        }
        const Expr        _expr;
        RangeCallbackFn   _exist_fn;
        MissingCallbackFn _missing_fn;
    };

    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    RangeCallback<Expr> cb( const Expr& expr, RangeCallbackFn exist_fn, MissingCallbackFn missing_fn=defaultMissingFn ){
        return RangeCallback( expr, exist_fn, missing_fn );
    }

    // callback that provides the matching string
    using StringCallbackFn = std::function<void(const std::string&)>;

    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    struct StringCallback : public Pattern {
        StringCallback( const Expr& expr, StringCallbackFn exist_fn, MissingCallbackFn missing_fn ) : _expr{expr}, _exist_fn{exist_fn}, _missing_fn{missing_fn} {}
        std::optional<const char*> match( const char* src ) const override {
            if( auto ret = _expr.match(src) ){
                std::string tmp;
                std::copy( src, *ret, std::back_inserter(tmp) );
                _exist_fn(tmp);
                return ret;
            }
            _missing_fn();
            return std::nullopt;
        }
        const Expr        _expr;
        StringCallbackFn  _exist_fn;
        MissingCallbackFn _missing_fn;
    };

    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    StringCallback<Expr> cb( const Expr& expr, StringCallbackFn exist_fn, MissingCallbackFn missing_fn=defaultMissingFn ){
        return StringCallback<Expr>( expr, exist_fn, missing_fn );
    }

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
    
    // Overloaded pattern building operators
    template< typename Expr >
    requires std::derived_from<Expr,Pattern>
    Not<Expr> operator!( const Expr& expr ){
        return Not<Expr>{expr};
    }

    // pattern | pattern
    template< typename Left, typename Right >
    requires std::derived_from<Left,Pattern> && std::derived_from<Right,Pattern>
    Or<Left,Right> operator|( const Left& L, const Right& R ){
        return Or<Left,Right>(L,R);
    }

    // '<char>' | pattern
    template< typename Right >
    requires std::derived_from<Right,Pattern>
    Or<Char,Right> operator|( const char L, const Right& R ){
        return Or<Char,Right>(Char(L),R);
    }

    // pattern | '<char>'
    template< typename Left >
    requires std::derived_from<Left,Pattern>
    Or<Left,Char> operator|( const Left& L, const char R ){
        return Or<Left,Char>(L,Char(R));
    }

    // "<string>" | pattern
    template< typename Right >
    requires std::derived_from<Right,Pattern>
    Or<Str,Right> operator|( const char *L, const Right& R ){
        return Or<Str,Right>(Str(L),R);
    }

    // pattern | "<string>"
    template< typename Left >
    requires std::derived_from<Left,Pattern>
    Or<Left,Str> operator|( const Left& L, const char* R ){
        return Or<Left,Str>(L,Str(R));
    }

    // pattern & pattern
    template< typename Left, typename Right >
    requires std::derived_from<Left,Pattern> && std::derived_from<Right,Pattern>
    And<Left,Right> operator&( const Left& L, const Right& R ){
        return And<Left,Right>(L,R);
    }

    // '<char>' & pattern
    template< typename Right >
    requires std::derived_from<Right,Pattern>
    And<Char,Right> operator&( const char L, const Right& R ){
        return And<Char,Right>(Char(L),R);
    }

    // pattern & '<char>'
    template< typename Left >
    requires std::derived_from<Left,Pattern>
    And<Left,Char> operator&( const Left& L, const char R ){
        return And<Left,Char>(L,Char(R));
    }

    // "<string>" & pattern
    template< typename Right >
    requires std::derived_from<Right,Pattern>
    And<Str,Right> operator&( const char *L, const Right& R ){
        return And<Str,Right>(Str(L),R);
    }

    // pattern & "<string>"
    template< typename Left >
    requires std::derived_from<Left,Pattern>
    And<Left,Str> operator&( const Left& L, const char* R ){
        return And<Left,Str>(L,Str(R));
    }

    // convenience definitions
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
};