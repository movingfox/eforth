#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <sstream>
#include <vector>           // vector
#include <functional>       // function
#include <exception>

#if _WIN32 || _WIN64
#define ENDL "\r\n"
#else
#define ENDL endl
#endif // _WIN32 || _WIN64

using namespace std;

template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern

    T& operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    T operator<<(T t)    { v.push_back(t); }

    T dec_i() { return v.back() -= 1; }     /// decrement stack top
    T pop()   {
        if (v.empty()) throw underflow_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t;
    }
    void push(T t)            { v.push_back(t); }
    void clear()              { v.clear(); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
    void erase(int i)         { v.erase(v.begin() + i, v.end()); }
};

class Code;                                 /// forward declaration
struct xtbase {                             /// alternate solution for function
	virtual void operator()(Code*) = 0;
};
template<typename F>
struct XT :xtbase {
	F fp;
	XT(F &f) : fp(f) {}
	virtual void operator()(Code *c) { fp(c); }
};
//using fop = function<void(Code*)>;        /// Forth operator
class Code {
public:
    static int fence;                       /// token incremental counter
    string name;                            /// name of word
    int    token = 0;                       /// dictionary order token
    bool   immd  = false;                   /// immediate flag
    int    stage = 0;                       /// branching stage
    xtbase *xt   = NULL;                    /// primitive function
    //fop  xt    = NULL;
    string literal;                         /// string literal

    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<int>   qf;

    template<typename F>
    Code(string n, F fn, bool im=false);	/// primitive
    //Code(string n, fop fn, bool im=false);  /// primitive
    Code(string n, bool f=false);           /// new colon word or temp
    Code(Code *c,  int d);                  /// dolit, dovar
    Code(Code *c,  string s=string());      /// dotstr

    Code *immediate();                      /// set immediate flag
    Code *addcode(Code *w);                 /// append colon word

    string to_s();                          /// debugging
    string see(int dp);
    void   exec();                          /// execute word
};
///
/// Forth virtual machine variables
///
class ForthVM {
public:
    istream          &cin;                  /// stream input
	ostream          &cout;					/// stream output

    ForthList<int>   rs;                    /// return stack
    ForthList<int>   ss;                    /// parameter stack
    ForthList<Code*> dict;                  /// dictionary

    bool  compile = false;                  /// compiling flag
    int   base    = 10;                     /// numeric radix
    int   top     = -1;                     /// cached top of stack
    int   WP      = 0;                      /// instruction and parameter pointers

    ForthVM(istream &in, ostream &out);

    void init();
    void outer();

private:
    int POP();
    int PUSH(int v);
    
    Code *find(string s);                   /// search dictionary reversely
    string next_idiom(char delim=0);
    void call(Code *c);                     /// execute a word
    
    void dot_r(int n, int v);
    void ss_dump();
    void words();
};
#endif // __EFORTH_SRC_CEFORTH_H
