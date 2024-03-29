# Forth - is it still relevant?
With all the advantages, it is unfortunate that Forth lost out to C language over the years and have been reduced to a niche. Per ChatGPT: *due to C's border appeal, standardization, and support ecosystem likely contributed to its greater adoption and use in mainstream computing*.

So, the question is, how to encourage today's world of C programmers to take a look at Forth. How do we convince them that Forth can be 10 times more productive? Well, we do know that by keep saying how elegant Forth is or even bashing how bad C can be probably won't get us anywhere.

Dr. Ting, a pillar of Forth community, created eForth along with Bill Muench for educational purpose. He described Forth in his well-written eForth [genesis](https://chochain.github.io/eforth/docs/eForthAndZen.pdf) and [overview](https://chochain.github.io/eforth/docs/eForthOverviewv5.pdf)

> The language consists of a collection of words, which reside in the memory of a computer and can be executed by entering their names on the computer keyboard. A list of words can be compiled, given a new name and made a new word. In fact, most words in Forth are defined as lists of existing words. A small set of primitive words are defined in machine code of the native CPU. All other words are built from this primitive words and eventually refer to them when executed.

> Forth is a computer model which can be implemented on any real CPU with reasonable resources. This model is often called a virtual Forth computer. The minimal components of a virtual Forth computer are:
> 1. A dictionary in memory to hold all the execution procedures.
> 2. A return stack to hold return addresses of procedures yet to be executed.
> 3. A data stack to hold parameters passing between procedures.
> 4. A user area in RAM memory to hold all the system variables.
> 5. A CPU to move date among stacks and memory, and to do ALU operations to parameters stored on the data stack.

### ceForth - on the shoulder of a giant
Most classic Forth systems are build with a few low-level primitives in assembly language and bootstrap the high-level words in Forth itself. Over the years, Dr. Ting have implemented many Forth systems using the same model. See [here](https://www.forth.org/OffeteStore/OffeteStore.html) for the detailed list. However, he eventually stated that it was silly trying to explain Forth in Forth to new comers. There are just not many people know Forth, period.

Utilizing modern OS and tool chains, a new generation of Forths implemented in just a few hundreds lines of C code can help someone who did not know Forth to gain the core understanding much quickly. He called the insight **Forth without Forth**.

In 2021-07-04, I got in touched with Dr. Ting mentioning that he taught at the university when I attended. He, as the usual kind and generous him, included me in his last projects all the way till his passing. I am honored that he considered me one of the frogs living in the bottom of the deep well with him looking up to the small opening of the sky together. With cross-platform portability as our guild-line, we built ooeForth in Java, jeForth in Javascript, wineForth for Windows, and esp32forth for ESP micro-controllers using the same code-base. With his last breath in the hospital, he attempted to build it onto an FPGA using Verilog. see [ceForth_403](https://chochain.github.io/eforth/docs/ceforth_403.pdf) and [eJsv32](https://github.com/chochain/eJsv32) for details.

### How To Compile/Build/Run this pure C/C++ eForth
<pre>
> git clone https://github.com/chochain/eforth to your local machine
> cd eforth
</pre>

#### Build eForth on Linux or Cygwin
<pre>
> make
> ./tests/eforth
> to quit, type 'bye' or Ctrl-C
</pre>

#### Build for WASM
<pre>
> make wasm
> python3 -m http.server
> from your browser, open http://localhost:80/tests/eforth.html
</pre>

#### Build for ESP32
<pre>
> ensure your Arduino IDE have ESP32 libraries installed
> open eforth.ino with Arduino IDE
> inside eforth.ino, modify WIFI_SSID and WIFI_PASS to point to your router
> open Arduino Serial Monitor, set baud 115200 and linefeed to 'Both NL & CR'
> compile and load
> if successful, web server IP address/port and eForth prompt shown in Serial Monitor
> from your browser, enter the IP address to access the ESP32 web server
</pre>

#### Build Dr. Ting's legacy eForth, 32-bit token-threaded
<pre>
> make 36b
> ./tests/ceforth36b
</pre>

#### Build array-based, linear-memory eForth, subroutine-threaded with 16-bit xt offset
<pre>
> make 40x
> ./tests/eforth40x
</pre>

### Changes - What have we done!
Even with vocabulary, multi-tasking, and meta-compilation (the black-belt stuffs of Forth greatness) deliberately dropped to reduce the complexity, eForth customarily uses linear memory blocks to host stacks and words in the dictionary. Forth manages code and their parameters, in bytes or CELLs, with a backward linked-list known for the term 'threading'. Often, the low-level core are built from ground up with assembly, then meta-compiled or boot-strapped with high-level Forth scripts. This model is crucial in the days when memory is scarce or compiler resource is just underwhelming. It gave the power to Forth, but now became the show-stopper for many newbies. Because, one have to learn the assembly language, understand the memory model before appreciating the internal of Forth.

Traditially, high-level languages like C or C++ are generally avoided for implementing Forth. However, through rounds of experiments, we concluded that by utilizing the dynamic allocation, streaming IO, and other C++ standard libraries can clarify the intention of our implementation of Forth to millions of C programmers potentially. We hope it can serve as a stepping stone for learning Forth even building their own, one day.
 
### Source Code directories
<pre>
+ ~/src           common source for ceforth, esp32forth, wineForth, and weForth
+ ~/orig/33b      refactored ceForth_33, separate ASM from VM (used in eForth1 for Adruino UNO)
+ ~/orig/ting     ceForth source codes collaborated with Dr. Ting
+ ~/orig/esp32    esp32forth source codes collaborated with Dr. Ting
+ ~/orig/40x      my experimental work, refactor _40 into vector-based subroutine-threaded with 16-bit offset enhancement
</pre>

### Evolution - continuation of Dr. Ting's final work
Kept under ~/orig and details [here](https://chochain.github.io/eforth/orig/index.html)
#### ~/orig/ting - Dr. Ting's original ceForth
<pre>
  + ting/_23                 - cross-compiled ROM, token-threaded
  + ting/_33                 - assembler with C functions as macro, token-threaded
  + 33b/                     - from _33, I refactored assembler from inner interpreter
  + ting/_40, _40b, _40c     - my input, array-based, token-threaded
  + ting/_401, _402, _403    - email exchange on _40 with Dr. Ting
  + ting/_36, _36b, _36x     - from _33, Dr. Ting updated his linear memory, subroutine-threaded
  + ting/_410                - from _403, I refactored with initializer_list, and bug fixes
</pre>
#### ~/orig/esp32 - the origin of ESP32Forth
<pre>
  + esp32/_54, _59           - cross-compiled ROM, token-threaded
  + esp32/_62, _63           - assembler with C functions as macros, from ceForth_33
  + esp32/_705               - ESP32Forth v7, C macro-based assembler with Brad and Peter
  + esp32/_802               - my interim work shown to Dr. Ting, sync ceForth_403
  + esp32/_82, _83, _84, _85 - from _63, Dr. Ting adapted array-based, token-threaded
</pre>
#### ~/orig/40x - experiments on _40 for various implementation and tuning
<pre>
  + 40x/ceforth - array-based subroutine-threaded, with 16-bit offset enhanced
</pre>  
#### life after _410 and 40x - my current work
<pre>
  + src/ceforth - multi-platform supporting code base
  + platform/   - platform specific for C++, ESP32, WASM
</pre>

### Benchmark
#### Desktop PC - 10K*10K cycles on 3.2GHz AMD**
<pre>
+ 4452ms: ~/orig/ting/ceforth_36b, linear memory, 32-bit, token threading
+ 1450ms: ~/orig/ting/ceForth_403, dict/pf array-based, subroutine threading
+ 1050ms: ~/orig/40x/ceforth, subroutine indirect threading, with 16-bit offset
+  890ms: ~/orig/40x/ceforth, inner interpreter with cached xt offsets
+  740ms: ~/src/ceforth, dynamic vector, token threading
</pre>

#### ESP32 - 1K*1K cycles on 240MHz NodeMCU**
<pre>
+ 1440ms: Dr. Ting's ~/esp32forth/orig/esp32forth_82
+ 1045ms: ~/orig/esp32/ceforth802, array-based, token threading
+  990ms: ~/orig/40x/ceforth, linear-memory, subroutine threading, with 16-bit offset
+  930ms: ~/orig/40x/ceforth, inner interpreter with cached xt offsets
+  644ms: ~/src/ceforth dynamic vector, token threading
</pre>

### Revision History
* Dr. Ting's work on eForth between 1995~2011
  [eForth references](http://forth.org/library/eforth_SOC) and their [Source Code Repo](http://forth.org/library/eforth_SOC/eforth_SOC_source)
* CC 20210314: Initial
  > Started with ~orig/33b code-base, refactor with enum and VA_ARGS macros targeting 100% C/C++.
* CC 20210707: Refactor
  > Incorporated list-based dict, ss, rs (i.e. ~orig/ting/ceForth40 and ~orig/802) which I proposed to Dr. Ting in our email exchanges.
* CC 20210816: Code Merge
  > Targeting multi-platform. Common source by consolidating ceForth, wineForth, ESP32forth (kept in ~/orig/*). Officially version 8.0
* CC 20220512: Refactor
  >  Though the goal of Dr. Ting's eForth is to demonstrate how a Forth can be easily understood and cleanly constructed. However, the token threading method used is costly (slow) because each call needs 2 indirect lookups (token->dict, dict->xt). On top of that, C/C++ callframe needs to be setup/teardown. It is worsen by the branch prediction missing every call stalling the CPU pipeline. Bad stuffs!
  > Refactor to subroutine indirect threading. It's not portable but does speed up 25% (see benchmark above).
  > Using 16-bit offsets for pointer arithmetic which speed up another 5% while maintaining 16-bit parameter space consumption.
  > Since C++ code is at least 4-byte aligned and parameter is 2-byte aligned, the LSB of a given parameter is utilized for colon word identification.
* CC 20221118: Refactor
  > WASM function pointer is U32 (index). Token-indirect worked but the two indirect look-up is even slower. Since WASM uses 64K linear memory block, 16-bit pointer offset is a better option. However, the xt "function pointer" in code space is simply an index to the shared _indirect_function_table. Since LSB is used, so we are forced to use MSB to differentiate primitive word from colon word. This left us 15-bit, i.e. 32K, parameter offset available.
* CC 20231011: Review
  > Since the original intention of having a pre-compiled ROM dictionary still end up in C++ static initialization run before main(), moved dictionary compilation into dict_compile as function calls gives a little more debugging control and opportunity for fine tuning.
  > LAMBDA_OK option was originally intended for full VM implementation but 2x slower. Dropped to reduce source clutter.
* CC 20240308: Refactor for multi-platform, adopt dynamic vectors
  > To support cross-platform, i.g. WIN32, Arduino/ESP, Linux, and WASM, there were many conditional compilation branches which make the code really messy.
  + Separate cross-platform and configuration into ~/src/config.h
  + Separate platform specific code into ~/platform/*.cpp
  + add include opcode for Forth script loading

