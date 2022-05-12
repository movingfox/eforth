///
/// ceForth8 - A token indirect threading Forth implemented in C++
///
/// Note: the threading method is very inefficient (slow) because
///       callframe needs to be setup/teardown with multiple lookups, and
///       every call will miss a branch prediction. Bad stuffs!
///       However, the goal of eForth is to show how a Forth can be
///       easily understood and constructed.
///
#include <stdint.h>     // uintxx_t
#include <stdlib.h>     // strtol
#include <string.h>     // strcmp
#include <exception>    // try...catch, throw
using namespace std;
///
/// conditional compililation options
///
#define LAMBDA_CAP      1
#define RANGE_CHECK     1
///
/// conditional compilation for different platforms
///
#if _WIN32 || _WIN64
#define ENDL "\r\n"
#else
#define ENDL endl; fout_cb(fout.str().length(), fout.str().c_str()); fout.str("")
#endif // _WIN32 || _WIN64

#if ARDUINO
#include <Arduino.h>
#if ESP32
#define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
#endif // ESP32
#else
#include <chrono>
#include <thread>
#define millis()        chrono::duration_cast<chrono::milliseconds>( \
                            chrono::steady_clock::now().time_since_epoch()).count()
#define delay(ms)       this_thread::sleep_for(chrono::milliseconds(ms))
#define yield()         this_thread::yield()
#define PROGMEM
#endif // ARDUINO
///
/// logical units (instead of physical) for type check and portability
///
typedef uint16_t IU;    // instruction pointer unit
typedef int32_t  DU;    // data unit
typedef uint16_t U16;   // unsigned 16-bit integer
typedef uint8_t  U8;    // byte, unsigned character
///
/// alignment macros
///
#define ALIGN(sz)       ((sz) + (-(sz) & 0x1))
#define ALIGN16(sz)     ((sz) + (-(sz) & 0xf))
#define ALIGN32(sz)     ((sz) + (-(sz) & 0x1f))
///
/// array class template (so we don't have dependency on C++ STL)
/// Note:
///   * using decorator pattern
///   * this is similar to vector class but much simplified
///
template<class T, int N>
struct List {
    T   *v;             /// fixed-size array storage
    int idx = 0;        /// current index of array
    int max = 0;        /// high watermark for debugging

    List()  { v = new T[N]; }      /// dynamically allocate array storage
    ~List() { delete[] v;   }      /// free memory
    T& operator[](int i)   { return i < 0 ? v[idx + i] : v[i]; }
#if RANGE_CHECK
    T pop() {
        if (idx>0) return v[--idx];
        throw "ERR: List empty";
    }
    T push(T t) {
        if (idx<N) return v[max=idx++] = t;
        throw "ERR: List full";
    }
#else
    T pop()     { return v[--idx]; }
    T push(T t) { return v[max=idx++] = t; }
#endif // RANGE_CHECK
    void push(T *a, int n)  { for (int i=0; i<n; i++) push(*(a+i)); }
    void merge(List& a)     { for (int i=0; i<a.idx; i++) push(a[i]);}
    void clear(int i=0)     { idx=i; }
};
///
/// functor implementation - for lambda support (without STL)
///
#if LAMBDA_CAP
struct fop { virtual void operator()(IU) = 0; };
template<typename F>
struct XT : fop {           // universal functor
    F fp;
    XT(F &f) : fp(f) {}
    void operator()(IU c) { fp(c); }
};
#else
typedef void (*fop)();
#endif // LAMBDA_CAP
///
/// universal Code class
/// Note:
///   * 8-byte on 32-bit machine, 16-byte on 64-bit machine
///
#if LAMBDA_CAP
struct Code {
    const char *name = 0;   /// name field
    union {                 /// either a primitive or colon word
        fop *xt = 0;        /// lambda pointer
        struct {            /// a colon word
            U16 def:  1;    /// colon defined word
            U16 immd: 1;    /// immediate flag
            U16 len:  14;   /// len of pfa
            IU  pfa;         /// offset to pmem space
        };
    };
    template<typename F>    /// template function for lambda
    Code(const char *n, F f, bool im=false) : name(n) {
        xt = new XT<F>(f);
        immd = im ? 1 : 0;
    }
    Code() {}               /// create a blank struct (for initilization)
};
#else
struct Code {
    const char *name = 0;   /// name field
    union {                 /// either a primitive or colon word
        fop xt = 0;         /// lambda pointer
        struct {            /// a colon word
            U16 def:  1;    /// colon defined word
            U16 immd: 1;    /// immediate flag
            U16 len:  14;   /// len of pf (16K max)
            IU  pfa;        /// offset to pmem space (16-bit for 64K range)
        };
    };
    Code(const char *n, fop f, bool im=false) : name(n), xt(f) {
        immd = im ? 1 : 0;
    }
    Code() {}               /// create a blank struct (for initilization)
};
#endif // LAMBDA_CAP
///==============================================================================
///
/// main storages in RAM
/// Note:
///   1.By separating pmem from dictionary, it makes dictionary uniform size
///   * (i.e. the RISC vs CISC debate) which eliminates the need for link field
///   * however, it requires size tuning manually
///   2.For ease of byte counting, we use U8 for pmem instead of U16.
///   * this makes IP increment by 2 instead of word size. If needed, it can be
///   * readjusted.
///
List<DU,   64>      ss;   /// data stack, can reside in registers for some processors
List<DU,   64>      rs;   /// return stack
List<Code, 1024>    dict; /// fixed sized dictionary (RISC vs CISC)
List<U8,   48*1024> pmem; /// parameter memory i.e. storage for all colon definitions
///
/// system variables
///
bool compile = false;
DU   top = -1, base = 10;
DU   ucase = 1;                   /// case sensitivity control
IU   WP = 0;                      /// current word pointer
U8   *PMEM0 = &pmem[0];           /// cached memory base (saved 200ms/1M cycles)
U8   *IP = PMEM0, *IP0 = PMEM0;   /// current instruction pointer and cached base pointer
///
/// macros to abstract dict and pmem physical implementation
/// Note:
///   so we can change pmem implementation anytime without affecting opcodes defined below
///
#define STRLEN(s) (ALIGN(strlen(s)+1))      /** calculate string size with alignment     */
#define XIP       (dict[-1].len)            /** parameter field tail of latest word      */
#define PFA(w)    ((U8*)&pmem[dict[w].pfa]) /** parameter field pointer of a word        */
#define PFLEN(w)  (dict[w].len)             /** parameter field length of a word         */
#define CELL(a)   (*(DU*)&pmem[a])          /** fetch a cell from parameter memory       */
#define STR(a)    ((char*)&pmem[a])         /** fetch string pointer to parameter memory */
#define JMPIP     (IP0 + *(IU*)IP)          /** branching target address                 */
#define SETJMP(a) (*(IU*)(PFA(-1) + (a)))   /** address offset for branching opcodes     */
#define HERE      (pmem.idx)                /** current parameter memory index           */
#define IPOFF     ((IU)(IP - PMEM0))        /** IP offset relative parameter memory root */
///
/// TODO: token indirect threaded is portable
///       but very expensive for pipelined design
///
#if LAMBDA_CAP
#define CALL(c) \
	if (dict[c].def) nest(c); \
    else (*(fop*)(((uintptr_t)dict[c].xt)&~0x3))(c)
#else
#define CALL(c) \
	if (dict[c].def) nest(c); \
    else (*(fop)(((uintptr_t)dict[c].xt)&~0x3))()
#endif // LAMBDA_CAP
///==============================================================================
///
/// dictionary search functions - can be adapted for ROM+RAM
///
inline int  streq(const char *s1, const char *s2) {
    return ucase ? strcasecmp(s1, s2)==0 : strcmp(s1, s2)==0;
}
int find(const char *s) {
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (streq(s, dict[i].name)) return i;
    }
    return -1;
}
///
/// inline functions to add (i.e. 'comma') object into parameter memory
///
inline void add_iu(IU i)   { pmem.push((U8*)&i, sizeof(IU));  XIP += sizeof(IU); }  /** add an instruction into pmem */
inline void add_du(DU v)   { pmem.push((U8*)&v, sizeof(DU)),  XIP += sizeof(DU); }  /** add a cell into pmem         */
inline void add_str(const char *s) {                                                /** add a string to pmem         */
    int sz = STRLEN(s); pmem.push((U8*)s,  sz); XIP += sz;
}
///==============================================================================
///                   
/// colon word compiler
/// Note:
///   * we separate dict and pmem space to make word uniform in size
///   * if they are combined then can behaves similar to classic Forth
///   * with an addition link field added.
///
enum {
    NOP = 0, DOVAR, DOLIT, DOSTR, DOTSTR, BRAN, ZBRAN, DONEXT, DOES, TOR
} forth_opcode;

void colon(const char *name) {
    char *nfa = STR(HERE);                  // current pmem pointer
    int sz = STRLEN(name);                  // string length, aligned
    pmem.push((U8*)name,  sz);              // setup raw name field
#if LAMBDA_CAP
    Code c(nfa, [](int){});                 // create a new word on dictionary
#else
    Code c(nfa, NULL);
#endif // LAMBDA_CAP
    c.def = 1;                              // specify a colon word
    c.len = 0;                              // advance counter (by number of U16)
    c.pfa = HERE;                           // capture code field index
    dict.push(c);                           // deep copy Code struct into dictionary
};
///
/// Forth inner interpreter (colon word handler)
///
/// TODO: move ENTER,EXIT to their own opcode (save PFLEN, PFA)
///
void nest(IU c) {
	/// ENTER
	rs.push(IP - PMEM0); rs.push(WP);       /// * setup call frame
    IP0 = IP = PFA(WP=c);                   // CC: this takes 30ms/1K, need work
//PFA(w) : ((U8*)&pmem[dict[w].pfa])
	try {                                   // CC: is dict[c] kept in cache?
        U8 *ipx = IP + PFLEN(c);            // CC: this saves 350ms/1M
//PFLEN(w) : (dict[w].len)
    	while (IP < ipx) {        			/// * recursively call all children
        	IU c1 = *IP; IP += sizeof(IU);  // CC: cost of (ipx, c1) on stack?
	        CALL(c1);                       ///> execute child word
//CALL : if (dict[c1].def) nest(c1);
//       else (*(fop*)(((uintptr_t)dict[c1].xt)&~0x3))(c1)
        }                                   ///> can do IP++ if pmem unit is 16-bit
	}
	catch(...) {}                           ///> protect if any exeception
	/// EXIT
	IP0 = PFA(WP=rs.pop());                 /// * restore call frame (NEXT)
//PFA(w) : ((U8*)&pmem[dict[w].pfa])
	IP  = PMEM0 + rs.pop();
    yield();                                ///> give other tasks some time
}
///==============================================================================
///
/// utilize C++ standard template libraries for core IO functions only
/// Note:
///   * we use STL for its convinence, but
///   * if it takes too much memory for target MCU,
///   * these functions can be replaced with our own implementation
///
#include <sstream>      // iostream, stringstream
#include <iomanip>      // setbase
#include <string>       // string class
istringstream   fin;    // forth_in
ostringstream   fout;   // forth_out
void (*fout_cb)(int, const char*);   // forth output callback function
string strbuf;          // input string buffer
///==============================================================================
///
/// debug functions
///
void dot_r(int n, int v) { fout << setw(n) << setfill(' ') << v; }
void to_s(IU c) {
    fout << dict[c].name << " " << c << (dict[c].immd ? "* " : " ");
}
///
/// recursively disassemble colon word
///
void see(IU *cp, IU *ip, int dp=0) {
    fout << ENDL; for (int i=dp; i>0; i--) fout << "  ";            // indentation
    if (dp) fout << "[" << setw(2) << *ip << ": ";                  // ip offset
    else    fout << "[ ";
    IU c = *cp;
    to_s(c);                                                        // name field
    if (dict[c].def) {                                              // a colon word
        for (IU n=dict[c].len, ip1=0; ip1<n; ip1+=sizeof(IU)) {     // walk through children
            IU *cp1 = (IU*)(PFA(c) + ip1);                          // next children node
            see(cp1, &ip1, dp+1);                                   // dive recursively
        }
    }
    switch (c) {
    case DOVAR: case DOLIT:
        fout << "= " << *(DU*)(cp+1); *ip += sizeof(DU); break;
    case DOSTR: case DOTSTR:
        fout << "= \"" << (char*)(cp+1) << '"';
        *ip += STRLEN((char*)(cp+1)); break;
    case BRAN: case ZBRAN: case DONEXT:
        fout << "j" << *(cp+1); *ip += sizeof(IU); break;
    }
    fout << "] ";
}
void words() {
	fout << setbase(16);
    for (int i=0; i<dict.idx; i++) {
        if ((i%10)==0) { fout << ENDL; yield(); }
        to_s(i);
    }
    fout << setbase(base);
}
void ss_dump() {
    fout << " <"; for (int i=0; i<ss.idx; i++) { fout << ss[i] << " "; }
    fout << top << "> ok" << ENDL;
}
void mem_dump(IU p0, DU sz) {
    fout << setbase(16) << setfill('0') << ENDL;
    for (IU i=ALIGN16(p0); i<=ALIGN16(p0+sz); i+=16) {
        fout << setw(4) << i << ": ";
        for (int j=0; j<16; j++) {
            U8 c = pmem[i+j];
            fout << setw(2) << (int)c << (j%4==3 ? "  " : " ");
        }
        for (int j=0; j<16; j++) {   // print and advance to next byte
            U8 c = pmem[i+j] & 0x7f;
            fout << (char)((c==0x7f||c<0x20) ? '_' : c);
        }
        fout << ENDL;
        yield();
    }
    fout << setbase(base);
}
void _does() {
}
///================================================================================
///
/// macros to reduce verbosity
///
inline char *next_word()  { fin >> strbuf; return (char*)strbuf.c_str(); } // get next idiom
inline char *scan(char c) { getline(fin, strbuf, c); return (char*)strbuf.c_str(); }
inline DU   POP()         { DU n=top; top=ss.pop(); return n; }
#define     PUSH(v)       { ss.push(top); top = v; }
#if LAMBDA_CAP
#define     CODE(s, g)    { s, [](IU c){ g; }, 0 }
#define     IMMD(s, g)    { s, [](IU c){ g; }, 1 }
#else
#define     CODE(s, g)    { s, []{ g; }, 0 }
#define     IMMD(s, g)    { s, []{ g; }, 1 }
#endif // LAMBDA_CAP
#define     BOOL(f)       ((f)?-1:0)
///
/// global memory access macros
///
#define     PEEK(a)    (DU)(*(DU*)((uintptr_t)(a)))
#define     POKE(a, c) (*(DU*)((uintptr_t)(a))=(DU)(c))
///================================================================================
///
/// primitives (ROMable)
/// Note:
///   * we merge prim into dictionary in main()
///   * However, since primitive is statically compiled
///   * it can be stored in ROM, and only
///   * find() needs to be modified to support ROM+RAM
///
static Code prim[] PROGMEM = {
	///
	/// @defgroup Execution flow ops
    /// @brief - DO NOT change the sequence here (see forth_opcode enum)
	/// @{
	CODE("nop",     {}),
    CODE("dovar",   PUSH(IPOFF); IP += sizeof(DU)),
    CODE("dolit",   PUSH(*(DU*)IP); IP += sizeof(DU)),
    CODE("dostr",
        const char *s = (const char*)IP;           // get string pointer
        PUSH(IPOFF); IP += STRLEN(s)),
    CODE("dotstr",
        const char *s = (const char*)IP;           // get string pointer
        fout << s;  IP += STRLEN(s)),              // send to output console
    CODE("branch" , IP = JMPIP),                           // unconditional branch
    CODE("0branch", IP = POP() ? IP + sizeof(IU) : JMPIP), // conditional branch
    CODE("donext",
         if ((rs[-1] -= 1) >= 0) IP = JMPIP;       // rs[-1]-=1 saved 200ms/1M cycles
         else { IP += sizeof(IU); rs.pop(); }),
    CODE("does",                                   // CREATE...DOES... meta-program
         U8 *ip  = PFA(WP);
         U8 *ipx = ip + PFLEN(WP);                 // range check
         while (ip < ipx && *(IU*)ip != DOES) ip+=sizeof(IU);  // find DOES
         while ((ip += sizeof(IU)) < ipx) add_iu(*(IU*)ip);    // copy&paste code
         IP = ipx),                                            // done
    CODE(">r",   rs.push(POP())),
    CODE("r>",   PUSH(rs.pop())),
    CODE("r@",   PUSH(rs[-1])),
    /// @}
    /// @defgroup Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",  PUSH(top)),
    CODE("drop", top = ss.pop()),
    CODE("over", PUSH(ss[-1])),
    CODE("swap", DU n = ss.pop(); PUSH(n)),
    CODE("rot",  DU n = ss.pop(); DU m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("pick", DU i = top; top = ss[-i]),
    /// @}
    /// @defgroup Stack ops - double
    /// @{
    CODE("2dup", PUSH(ss[-1]); PUSH(ss[-1])),
    CODE("2drop",ss.pop(); top = ss.pop()),
    CODE("2over",PUSH(ss[-3]); PUSH(ss[-3])),
    CODE("2swap",
        DU n = ss.pop(); DU m = ss.pop(); DU l = ss.pop();
        ss.push(n); PUSH(l); PUSH(m)),
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",    top += ss.pop()),
    CODE("*",    top *= ss.pop()),
    CODE("-",    top =  ss.pop() - top),
    CODE("/",    top =  ss.pop() / top),
    CODE("mod",  top =  ss.pop() % top),
    CODE("*/",   top =  ss.pop() * ss.pop() / top),
    CODE("/mod",
        DU n = ss.pop(); DU t = top;
        ss.push(n % t); top = (n / t)),
    CODE("*/mod",
        DU n = ss.pop() * ss.pop();
        DU t = top;
        ss.push(n % t); top = (n / t)),
    CODE("and",  top = ss.pop() & top),
    CODE("or",   top = ss.pop() | top),
    CODE("xor",  top = ss.pop() ^ top),
    CODE("abs",  top = abs(top)),
    CODE("negate", top = -top),
    CODE("max",  DU n=ss.pop(); top = (top>n)?top:n),
    CODE("min",  DU n=ss.pop(); top = (top<n)?top:n),
    CODE("2*",   top *= 2),
    CODE("2/",   top /= 2),
    CODE("1+",   top += 1),
    CODE("1-",   top -= 1),
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0= ",  top = BOOL(top == 0)),
    CODE("0<",   top = BOOL(top <  0)),
    CODE("0>",   top = BOOL(top >  0)),
    CODE("=",    top = BOOL(ss.pop() == top)),
    CODE(">",    top = BOOL(ss.pop() >  top)),
    CODE("<",    top = BOOL(ss.pop() <  top)),
    CODE("<>",   top = BOOL(ss.pop() != top)),
    CODE(">=",   top = BOOL(ss.pop() >= top)),
    CODE("<=",   top = BOOL(ss.pop() <= top)),
    /// @}
    /// @defgroup IO ops
    /// @{
    CODE("base@",   PUSH(base)),
    CODE("base!",   fout << setbase(base = POP())),
    CODE("hex",     fout << setbase(base = 16)),
    CODE("decimal", fout << setbase(base = 10)),
    CODE("cr",      fout << ENDL),
    CODE(".",       fout << POP() << " "),
    CODE(".r",      DU n = POP(); dot_r(n, POP())),
    CODE("u.r",     DU n = POP(); dot_r(n, abs(POP()))),
    CODE(".f",      DU n = POP(); fout << setprecision(n) << POP()),
    CODE("key",     PUSH(next_word()[0])),
    CODE("emit",    char b = (char)POP(); fout << b),
    CODE("space",   fout << " "),
    CODE("spaces",  for (DU n = POP(), i = 0; i < n; i++) fout << " "),
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("[",       compile = false),
    CODE("]",       compile = true),
    IMMD("(",       scan(')')),
    IMMD(".(",      fout << scan(')')),
    CODE("\\",      scan('\n')),
    CODE("$\"",
        const char *s = scan('"')+1;        // string skip first blank
        add_iu(DOSTR);                      // dostr, (+parameter field)
        add_str(s)),                        // byte0, byte1, byte2, ..., byteN
    IMMD(".\"",
        const char *s = scan('"')+1;        // string skip first blank
        add_iu(DOTSTR);                     // dostr, (+parameter field)
        add_str(s)),                        // byte0, byte1, byte2, ..., byteN
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    IMMD("if",      add_iu(ZBRAN); PUSH(XIP); add_iu(0)),      // if    ( -- here ) 
    IMMD("else",                                                 // else ( here -- there )
        add_iu(BRAN);
        IU h=XIP;   add_iu(0); SETJMP(POP()) = XIP; PUSH(h)),
    IMMD("then",    SETJMP(POP()) = XIP),                        // backfill jump address
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",   PUSH(XIP)),
    IMMD("again",   add_iu(BRAN);  add_iu(POP())),               // again    ( there -- )
    IMMD("until",   add_iu(ZBRAN); add_iu(POP())),               // until    ( there -- ) 
    IMMD("while",   add_iu(ZBRAN); PUSH(XIP); add_iu(0)),        // while    ( there -- there here ) 
    IMMD("repeat",  add_iu(BRAN);                                // repeat    ( there1 there2 -- ) 
        IU t=POP(); add_iu(POP()); SETJMP(t) = XIP),             // set forward and loop back address
    /// @}
    /// @defgrouop For loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for" ,    add_iu(TOR); PUSH(XIP)),                     // for ( -- here )
    IMMD("next",    add_iu(DONEXT); add_iu(POP())),              // next ( here -- )
    IMMD("aft",                                                  // aft ( here -- here there )
        POP(); add_iu(BRAN);
        IU h=XIP; add_iu(0); PUSH(XIP); PUSH(h)),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE(":", colon(next_word()); compile=true),
    IMMD(";", compile = false),
    CODE("variable",                                             // create a variable
        colon(next_word());                                      // create a new word on dictionary
        add_iu(DOVAR);                                           // dovar (+parameter field)
        int n = 0; add_du(n)),                                   // data storage (32-bit integer now)
    CODE("constant",                                             // create a constant
        colon(next_word());                                      // create a new word on dictionary
        add_iu(DOLIT);                                           // dovar (+parameter field)
        add_du(POP())),                                          // data storage (32-bit integer now)
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exit",  IP = PFA(WP) + PFLEN(WP)),                     // quit current word execution
    CODE("exec",  CALL(POP())),                                  // execute word
    CODE("create",
        colon(next_word());                                      // create a new word on dictionary
        add_iu(DOVAR)),                                          // dovar (+ parameter field)
    CODE("to",              // 3 to x                            // alter the value of a constant
    	IU w = find(next_word());                                // to save the extra @ of a variable
	    *(DU*)(PFA(w) + sizeof(IU)) = POP()),
	CODE("is",              // ' y is x                          // alias a word
		IU w = find(next_word());                                // can serve as a function pointer
        dict[POP()].pfa = dict[w].pfa),                          // but might leave a dangled block
    CODE("[to]",            // : xx 3 [to] y ;                   // alter constant in compile mode
        IU w = *(IU*)IP; IP += sizeof(IU);                       // fetch constant pfa from 'here'
	    *(DU*)(PFA(w) + sizeof(IU)) = POP()),
    ///
    /// be careful with memory access, especially BYTE because
    /// it could make access misaligned which slows the access speed by 2x
    ///
    CODE("@",     IU w = POP(); PUSH(CELL(w))),                  // w -- n
    CODE("!",     IU w = POP(); CELL(w) = POP();),               // n w --
    CODE(",",     DU n = POP(); add_du(n)),
    CODE("allot", DU v = 0; for (IU n = POP(), i = 0; i < n; i++) add_du(v)), // n --
    CODE("+!",    IU w = POP(); CELL(w) += POP()),               // n w --
    CODE("?",     IU w = POP(); fout << CELL(w) << " "),         // w --
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("here",  PUSH(HERE)),
    CODE("ucase", ucase = POP()),
    CODE("words", words()),
    CODE("'",     IU w = find(next_word()); PUSH(w)),
    CODE(".s",    ss_dump()),
    CODE("see",   IU w = find(next_word()); IU ip=0; see(&w, &ip)),
    CODE("dump",  DU n = POP(); IU a = POP(); mem_dump(a, n)),
    CODE("peek",  DU a = POP(); PUSH(PEEK(a))),
    CODE("poke",  DU a = POP(); POKE(a, POP())),
    CODE("forget",
        IU w = find(next_word());
        if (w<0) return;
        IU b = find("boot")+1;
        dict.clear(w > b ? w : b)),
    CODE("clock", PUSH(millis())),
    CODE("delay", delay(POP())),
#if ARDUINO
    /// @}
    /// @defgroup Arduino specific ops
    /// @{
    CODE("pin",   DU p = POP(); pinMode(p, POP())),
    CODE("in",    PUSH(digitalRead(POP()))),
    CODE("out",   DU p = POP(); digitalWrite(p, POP())),
    CODE("adc",   PUSH(analogRead(POP()))),
    CODE("duty",  DU p = POP(); analogWrite(p, POP(), 255)),
    CODE("attach",DU p  = POP(); ledcAttachPin(p, POP())),
    CODE("setup", DU ch = POP(); DU freq=POP(); ledcSetup(ch, freq, POP())),
    CODE("tone",  DU ch = POP(); ledcWriteTone(ch, POP())),
#endif // ARDUINO
    /// @}
    CODE("bye",   exit(0)),
    CODE("boot",  dict.clear(find("boot") + 1); pmem.clear())
};
const int PSZ = sizeof(prim)/sizeof(Code);
///================================================================================
/// Forth Virtual Machine
///
///   dictionary initialization
///
void forth_init() {
    for (int i=0; i<PSZ; i++) {              /// copy prim(ROM) into fast RAM dictionary,
        dict.push(prim[i]);                  /// find() can be modified to support
    }                                        /// searching both spaces
}
///
/// outer interpreter
///
void forth_outer(const char *cmd, void(*callback)(int, const char*)) {
    fin.clear();                             /// clear input stream error bit if any
    fin.str(cmd);                            /// feed user command into input stream
    fout_cb = callback;                      /// setup callback function
    fout.str("");                            /// clean output buffer, ready for next
    while (fin >> strbuf) {
        const char *idiom = strbuf.c_str();
        //printf("%s=>",idiom);
        int w = find(idiom);                 /// * search through dictionary
        if (w>=0) {                          /// * word found?
            //printf("%s %d\n", dict[w].name, w);
            if (compile && !dict[w].immd) {  /// * in compile mode?
                add_iu(w);                   /// * add found word to new colon word
            }
            else { CALL(w); }                /// * execute forth word
            continue;
        }
        // try as a number
        char *p;
        int n = static_cast<int>(strtol(idiom, &p, base));
        //printf("%d\n", n);
        if (*p != '\0') {                    /// * not number
            fout << idiom << "? " << ENDL;   ///> display error prompt
            compile = false;                 ///> reset to interpreter mode
            break;                           ///> skip the entire input buffer
        }
        // is a number
        if (compile) {                       /// * add literal when in compile mode
            add_iu(DOLIT);                   ///> dovar (+parameter field)
            add_du(n);                       ///> data storage (32-bit integer now)
        }
        else PUSH(n);                        ///> or, add value onto data stack
    }
    if (!compile) ss_dump();
}

#include <iostream>     // cin, cout
int main(int ac, char* av[]) {
    static auto send_to_con = [](int len, const char *rst) { cout << rst; };
    forth_init();
    cout << unitbuf << "ceForth8" << endl;
    string line;
    while (getline(cin, line)) {             /// fetch line from user console input
        forth_outer(line.c_str(), send_to_con);
    }
    cout << "Done." << endl;
    return 0;
}
