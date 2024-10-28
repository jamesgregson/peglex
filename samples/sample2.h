#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

struct StatementVM {
    struct Instr {
        enum class Type : uint16_t {
            LOADV,  // push( _heap[inst.addr] ) 
            LOADA,  // push( inst.addr )
            LOADC,  // push( _constant[inst.addr] )
            STORE,  // t = pop(); _heap[pop()] = t
            ADD,    // push(pop()+pop())
            SUB,    // t=pop(); push(pop()-t)
            MUL,    // push(pop()*pop())
            DIV,    // t=pop(); push(t/pop())
            PRINT,  // prints value on top of the stack
            LINE,   // debugging instruction
        };

        Type    inst;
        int16_t addr;
    };

    void emit_loadv( const std::string& sym ){
        if( auto it = _symbol_index.find(sym) ; it != _symbol_index.end() ){
            _code.push_back({.inst=Instr::Type::LOADV,.addr=it->second});
            return;
        }
        throw std::runtime_error("Tried to reference missing symbol.");
    }

    void emit_loada( const std::string& sym ){
        int16_t addr = -1;
        if( auto it = _symbol_index.find(sym) ; it != _symbol_index.end() ){
            addr = it->second;
        } else {
            addr = static_cast<int16_t>(_heap.size());
            _heap.push_back(0.0);
            _symbol_index[sym] = addr;
        }
        _code.push_back({.inst=Instr::Type::LOADA,.addr=addr});
    }

    void emit_loadc( const std::string& str ){
        int16_t addr = static_cast<int16_t>(_constant.size());
        _constant.push_back( std::stod(str) );
        _code.push_back({.inst=Instr::Type::LOADC,.addr=addr});
    }

    void emit_store(){
        _code.push_back({.inst=Instr::Type::STORE,.addr=0});
    }

    void emit_add(){ 
        _code.push_back({.inst=Instr::Type::ADD,.addr=0}); 
    }

    void emit_sub(){ 
        _code.push_back({.inst=Instr::Type::SUB,.addr=0}); 
    }
    
    void emit_mul(){ 
        _code.push_back({.inst=Instr::Type::MUL,.addr=0}); 
    }
    
    void emit_div(){ 
        _code.push_back({.inst=Instr::Type::DIV,.addr=0}); 
    }

    void emit_print(){ 
        _code.push_back({.inst=Instr::Type::PRINT,.addr=0}); 
    }

    void emit_line( int16_t line ){
        _code.push_back({.inst=Instr::Type::LINE,.addr=line});
    }

    void run() {
        auto pop = [this](){ double tmp=this->_stack.back(); this->_stack.pop_back(); return tmp; };

        for( const auto& op : _code ){
            switch( op.inst ){
                // load/store data
                case Instr::Type::LOADA: _stack.push_back(double(op.addr)+0.5); break;
                case Instr::Type::LOADV: _stack.push_back(     _heap[op.addr]); break;
                case Instr::Type::LOADC: _stack.push_back( _constant[op.addr]); break;
                case Instr::Type::STORE: { double tmp = pop(); _heap[static_cast<int>(pop())] = tmp; } break;

                // arithmetic ops
                case Instr::Type::ADD: _stack.push_back(pop()+pop());                       break;
                case Instr::Type::SUB: { double tmp = pop(); _stack.push_back(pop()-tmp); } break;
                case Instr::Type::MUL: _stack.push_back(pop()*pop());                       break;
                case Instr::Type::DIV: { double tmp = pop(); _stack.push_back(pop()/tmp); } break;

                // print/debugging ops
                case Instr::Type::PRINT: std::cout << pop() << std::endl; break;
                case Instr::Type::LINE:  break;
            }
        }
    }

    std::string decompile() const {
        std::ostringstream oss;
        oss << ".symbols" << std::endl;
        for( auto [sym,id] : _symbol_index ){
            oss << std::setw(8) << id << ": " << sym << std::endl;
        }
        oss << ".constants" << std::endl;
        for( size_t i=0; i<_constant.size(); ++i ){
            oss << std::setw(8) << i << ": " << _constant[i] << std::endl;
        }
        oss << ".instructions" << std::endl;
        for( size_t i=0; i<_code.size(); ++i ){
            const Instr& op = _code[i];
            oss << std::setw(8) << i << ": ";
            switch( op.inst ){
                // load/store from memory
                case Instr::Type::LOADA: oss << "LOADA, " << op.addr; break;
                case Instr::Type::LOADV: oss << "LOADV, " << op.addr; break;
                case Instr::Type::LOADC: oss << "LOADC, " << op.addr; break;
                case Instr::Type::STORE: oss << "STORE";              break;

                // Arithmetic ops
                case Instr::Type::ADD:   oss << "ADD"; break;
                case Instr::Type::SUB:   oss << "SUB"; break;
                case Instr::Type::MUL:   oss << "MUL"; break;
                case Instr::Type::DIV:   oss << "DIV"; break;

                // Printing & debugging
                case Instr::Type::PRINT: oss << "PRINT";    break;
                case Instr::Type::LINE:  oss << "NOP        ; Line: " << op.addr; break;
            default:
                // all other instruction types handled before switch
                continue;
            }
            if( i != _code.size()-1 ){
                oss << std::endl;
            }
        }
        return oss.str();
    }

    using SymbolTable = std::map<std::string,int16_t>;
    SymbolTable _symbol_index;

    std::vector<double> _constant;
    std::vector<double> _stack;
    std::vector<double> _heap;
    std::vector<Instr>  _code;
};