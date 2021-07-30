#include <iostream>
#include <vector>
#include <functional>
#include <cstring>
#include <string>

using namespace std;

#define FALSE   0
#define TRUE    -1

template<class T>			/// template class as vector proxy
struct ForthList {
	vector<T> v;
	T operator[](int i)    { return i<0 ? v[v.size()+i] : v[i]; }
	T operator<<(T t)      { v.push_back(t); }
	T pop()                { T t=v.back(); v.pop_back(); return t; }
	T push(T t)		       { v.push_back(t); }
	T merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
	void clear()           { v.clear(); }
};
#define POP()    (top=ss.pop())
#define PUSH(v)  (ss.push(top),top=(v))

class Code;									/// forward declaration
typedef void (*fop)(Code*);					/// Forth operator

class Code {
public:
    static int fence;						/// token incremental counter
    string name;							/// name of word
    int    token = 0;						/// dictionary order token
    bool   immd  = false;					/// immediate flag
    fop    xt    = NULL;					/// primitive function
    string literal;							/// string literal
    int    stage = 0;						/// branching stage
    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<int>   qf;

    Code(string n, fop fn, bool im=false);	/// primitive
    Code(string n, bool f=false);			/// new colon word or temp
    Code(string n, int d);					/// dolit
    Code(string n, string l);				/// dostr

    Code *immediate();						/// set immediate flag
    Code *addcode(Code *w);				    /// append colon word
    void exec();						    /// execute word
};

ForthList<int>   rs; 						/// return stack
ForthList<int>   ss; 						/// parameter stack
ForthList<Code*> dict;						/// dictionary
//
// internal variables
bool cmpi = false;
int  base = 10;
int  top  = 0;                                /// cached top of stack
int  P, IP, WP;
///
/// dictionary search function
///
Code *find(string s)	{
	for (Code *w:dict.v) if (s==w->name) return w;
    return NULL;
}
//
// external function examples (instead of inline)
//
void _sub() {
    top = ss[-1] - top;
    ss.pop();
}
void _over() {
	int v = ss[-1];
	PUSH(v);
}
void _ddup() {
	_over(); _over();
}
void ss_dump() {
	for (int i:ss.v) {
		cout << i << "_";
	}
	cout << top << "_ok" << endl;
}
void words() {
	int i=0;
	for (Code *w:dict.v) {
		cout << w->name << " " << w->token << (w->immd ? "*" : " ") << " ";
		if ((++i%10)==0) cout << endl;
	}
	cout << endl;
}

#define CODE(s, g) new Code(s, [](Code *c){ g; })
#define IMMD(s, g) new Code(s, [](Code *c){ g; }, true)

void _dovar(Code *c) {
         string s; cin >> s;
         dict.push(new Code(s, true));
         Code *last=dict[-1]
             ->addcode(new Code("dovar", 0));
         last->pf[0]->token=last->token;
}    

vector<Code*> prim = {
	CODE("hi",    cout << "---->hi!" << endl),
    IMMD("bye",   exit(0)),					  	// lambda using macro to shorten
    CODE("qrx",   PUSH(getchar()); if (top!=0) PUSH(TRUE)),
    CODE("txsto", putchar((char)top); POP()),
    CODE("dup",   PUSH(top)),
    CODE("drop",  POP()),
    CODE("+",     top+=POP()),
    IMMD("if",
    	dict.push(new Code("branch"));
    	dict.push(new Code("temp"))),
    IMMD("else",
         Code *temp=dict[-1];
         Code *last=dict[-2]->pf[-1];
         last->pf.merge(temp->pf.v);
         temp->pf.clear();
         last->stage=1),
    IMMD("then",
         Code *temp=dict[-1];
         Code *last=dict[-2]->pf[-1];
         if (last->stage==0) {
             last->pf.merge(temp->pf.v);
             dict.pop();
         }
         else {
             last->pf1.merge(temp->pf.v);
             if (last->stage==1) dict.pop();
             else temp->pf.clear();
         }),
    IMMD(".\"",
         string s; getline(cin, s, '"');		// scan upto delimiter
         dict[-1]->addcode(new Code("dostr",s))),
    CODE("dolit", PUSH(c->qf[0])),
    CODE("dovar", PUSH(c->token)),
    CODE("sub",   _sub()),                    	// direct functions
    CODE("over",  _over()),
    CODE("2dup",  _ddup()),                   	// compiled
    CODE(".s",    ss_dump()),
    CODE("words", words()),
    CODE(":",
         string s; cin >> s;
         dict.push(new Code(s, true));
         cmpi=true),
    IMMD(";",     cmpi=false),
    CODE("dovar", PUSH(c->token)),
    IMMD("variable", _dovar(c))
};
///
/// Code class implementation
///
/// constructors
int Code::fence = 0;
Code::Code(string n, fop fn, bool im) { name=n;	token=fence++; xt=fn; immd=im; }
Code::Code(string n, bool f)   { Code *c=find(name=n); if (c) xt=c->xt; if (f) token=fence++; }
Code::Code(string n, int d)    { xt=find(name=n)->xt; qf.push(d); }
Code::Code(string n, string l) { xt=find(name=n)->xt; literal=l;  }
/// public methods
Code *Code::immediate()      { immd=true; return this;        }
Code *Code::addcode(Code *w) {
	pf.push(w);
	return this;
}
void  Code::exec() {
	if (xt) { xt(this); return; }		/// * execute primitive word and return
    rs.push(WP); rs.push(IP);           /// * execute colon word
    WP=token; IP=0;                    	/// * setup dolist call frame
    for (Code *w: pf.v) {				/// * inner interpreter
        try { w->xt(this); IP++; }
        catch (int e) {}
    }
    IP=rs.pop(); WP=rs.pop();           /// * return to caller
}
///
/// main class
///
void dict_setup() {
	dict.merge(prim);					/// * populate dictionary
	words();
}
void outer() {
	string tok;
	while (cin >> tok) {
        cout << tok << endl;
		if (tok=="bye") break;
        Code *w = find(tok);
        if (w) {
            if (cmpi && !w->immd) {
            	Code *last = dict[-1];
            	last->addcode(w);
            }
            else {
            	try { w->exec(); }
            	catch (exception &e) {
            		cout << e.what() << endl;
            	}
            }
        }
        else {
            try {
                int n = stoi(tok, nullptr, base);
                if (cmpi) {
                	dict[-1]->addcode(new Code("dolit",n));
                }
                else PUSH(n);
            }
            catch(...) {
                cout << tok << "? " << endl;
                cmpi = false;
            }
        }
        if (!cmpi) ss_dump();
	}
}
//
// Main Program
//
int main(int ac, char* av[]) {
	dict_setup();
    outer();
    cout << "done!" << endl;
    return 0;
}
