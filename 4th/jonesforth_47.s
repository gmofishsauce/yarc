http://git.annexia.org/?p=jonesforth.git;a=blob;f=jonesforth.S;h=45e6e854a5d2a4c3f26af264dfce56379d401425;hb=HEAD
gitgit.annexia.org / jonesforth.git / blob

commit
? search: 
 re
summary | shortlog | log | commit | commitdiff | tree
history | raw | HEAD
Version 47
[jonesforth.git] / jonesforth.S
   1 /*      A sometimes minimal FORTH compiler and tutorial for Linux / i386 systems. -*- asm -*-
   2         By Richard W.M. Jones <rich@annexia.org> http://annexia.org/forth
   3         This is PUBLIC DOMAIN (see public domain release statement below).
   4         $Id: jonesforth.S,v 1.47 2009-09-11 08:33:13 rich Exp $
   5 
   6         gcc -m32 -nostdlib -static -Wl,-Ttext,0 -Wl,--build-id=none -o jonesforth jonesforth.S
   7 */
   8         .set JONES_VERSION,47
   9 /*
  10         INTRODUCTION ----------------------------------------------------------------------
  11 
  12         FORTH is one of those alien languages which most working programmers regard in the same
  13         way as Haskell, LISP, and so on.  Something so strange that they'd rather any thoughts
  14         of it just go away so they can get on with writing this paying code.  But that's wrong
  15         and if you care at all about programming then you should at least understand all these
  16         languages, even if you will never use them.
  17 
  18         LISP is the ultimate high-level language, and features from LISP are being added every
  19         decade to the more common languages.  But FORTH is in some ways the ultimate in low level
  20         programming.  Out of the box it lacks features like dynamic memory management and even
  21         strings.  In fact, at its primitive level it lacks even basic concepts like IF-statements
  22         and loops.
  23 
  24         Why then would you want to learn FORTH?  There are several very good reasons.  First
  25         and foremost, FORTH is minimal.  You really can write a complete FORTH in, say, 2000
  26         lines of code.  I don't just mean a FORTH program, I mean a complete FORTH operating
  27         system, environment and language.  You could boot such a FORTH on a bare PC and it would
  28         come up with a prompt where you could start doing useful work.  The FORTH you have here
  29         isn't minimal and uses a Linux process as its 'base PC' (both for the purposes of making
  30         it a good tutorial). It's possible to completely understand the system.  Who can say they
  31         completely understand how Linux works, or gcc?
  32 
  33         Secondly FORTH has a peculiar bootstrapping property.  By that I mean that after writing
  34         a little bit of assembly to talk to the hardware and implement a few primitives, all the
  35         rest of the language and compiler is written in FORTH itself.  Remember I said before
  36         that FORTH lacked IF-statements and loops?  Well of course it doesn't really because
  37         such a lanuage would be useless, but my point was rather that IF-statements and loops are
  38         written in FORTH itself.
  39 
  40         Now of course this is common in other languages as well, and in those languages we call
  41         them 'libraries'.  For example in C, 'printf' is a library function written in C.  But
  42         in FORTH this goes way beyond mere libraries.  Can you imagine writing C's 'if' in C?
  43         And that brings me to my third reason: If you can write 'if' in FORTH, then why restrict
  44         yourself to the usual if/while/for/switch constructs?  You want a construct that iterates
  45         over every other element in a list of numbers?  You can add it to the language.  What
  46         about an operator which pulls in variables directly from a configuration file and makes
  47         them available as FORTH variables?  Or how about adding Makefile-like dependencies to
  48         the language?  No problem in FORTH.  How about modifying the FORTH compiler to allow
  49         complex inlining strategies -- simple.  This concept isn't common in programming languages,
  50         but it has a name (in fact two names): "macros" (by which I mean LISP-style macros, not
  51         the lame C preprocessor) and "domain specific languages" (DSLs).
  52 
  53         This tutorial isn't about learning FORTH as the language.  I'll point you to some references
  54         you should read if you're not familiar with using FORTH.  This tutorial is about how to
  55         write FORTH.  In fact, until you understand how FORTH is written, you'll have only a very
  56         superficial understanding of how to use it.
  57 
  58         So if you're not familiar with FORTH or want to refresh your memory here are some online
  59         references to read:
  60 
  61         http://en.wikipedia.org/wiki/Forth_%28programming_language%29
  62 
  63         http://galileo.phys.virginia.edu/classes/551.jvn.fall01/primer.htm
  64 
  65         http://wiki.laptop.org/go/Forth_Lessons
  66 
  67         http://www.albany.net/~hello/simple.htm
  68 
  69         Here is another "Why FORTH?" essay: http://www.jwdt.com/~paysan/why-forth.html
  70 
  71         Discussion and criticism of this FORTH here: http://lambda-the-ultimate.org/node/2452
  72 
  73         ACKNOWLEDGEMENTS ----------------------------------------------------------------------
  74 
  75         This code draws heavily on the design of LINA FORTH (http://home.hccnet.nl/a.w.m.van.der.horst/lina.html)
  76         by Albert van der Horst.  Any similarities in the code are probably not accidental.
  77 
  78         Some parts of this FORTH are also based on this IOCCC entry from 1992:
  79         http://ftp.funet.fi/pub/doc/IOCCC/1992/buzzard.2.design.
  80         I was very proud when Sean Barrett, the original author of the IOCCC entry, commented in the LtU thread
  81         http://lambda-the-ultimate.org/node/2452#comment-36818 about this FORTH.
  82 
  83         And finally I'd like to acknowledge the (possibly forgotten?) authors of ARTIC FORTH because their
  84         original program which I still have on original cassette tape kept nagging away at me all these years.
  85         http://en.wikipedia.org/wiki/Artic_Software
  86 
  87         PUBLIC DOMAIN ----------------------------------------------------------------------
  88 
  89         I, the copyright holder of this work, hereby release it into the public domain. This applies worldwide.
  90 
  91         In case this is not legally possible, I grant any entity the right to use this work for any purpose,
  92         without any conditions, unless such conditions are required by law.
  93 
  94         SETTING UP ----------------------------------------------------------------------
  95 
  96         Let's get a few housekeeping things out of the way.  Firstly because I need to draw lots of
  97         ASCII-art diagrams to explain concepts, the best way to look at this is using a window which
  98         uses a fixed width font and is at least this wide:
  99 
 100  <------------------------------------------------------------------------------------------------------------------------>
 101 
 102         Secondly make sure TABS are set to 8 characters.  The following should be a vertical
 103         line.  If not, sort out your tabs.
 104 
 105                 |
 106                 |
 107                 |
 108 
 109         Thirdly I assume that your screen is at least 50 characters high.
 110 
 111         ASSEMBLING ----------------------------------------------------------------------
 112 
 113         If you want to actually run this FORTH, rather than just read it, you will need Linux on an
 114         i386.  Linux because instead of programming directly to the hardware on a bare PC which I
 115         could have done, I went for a simpler tutorial by assuming that the 'hardware' is a Linux
 116         process with a few basic system calls (read, write and exit and that's about all).  i386
 117         is needed because I had to write the assembly for a processor, and i386 is by far the most
 118         common.  (Of course when I say 'i386', any 32- or 64-bit x86 processor will do.  I'm compiling
 119         this on a 64 bit AMD Opteron).
 120 
 121         Again, to assemble this you will need gcc and gas (the GNU assembler).  The commands to
 122         assemble and run the code (save this file as 'jonesforth.S') are:
 123 
 124         gcc -m32 -nostdlib -static -Wl,-Ttext,0 -Wl,--build-id=none -o jonesforth jonesforth.S
 125         cat jonesforth.f - | ./jonesforth
 126 
 127         If you want to run your own FORTH programs you can do:
 128 
 129         cat jonesforth.f myprog.f | ./jonesforth
 130 
 131         If you want to load your own FORTH code and then continue reading user commands, you can do:
 132 
 133         cat jonesforth.f myfunctions.f - | ./jonesforth
 134 
 135         ASSEMBLER ----------------------------------------------------------------------
 136 
 137         (You can just skip to the next section -- you don't need to be able to read assembler to
 138         follow this tutorial).
 139 
 140         However if you do want to read the assembly code here are a few notes about gas (the GNU assembler):
 141 
 142         (1) Register names are prefixed with '%', so %eax is the 32 bit i386 accumulator.  The registers
 143             available on i386 are: %eax, %ebx, %ecx, %edx, %esi, %edi, %ebp and %esp, and most of them
 144             have special purposes.
 145 
 146         (2) Add, mov, etc. take arguments in the form SRC,DEST.  So mov %eax,%ecx moves %eax -> %ecx
 147 
 148         (3) Constants are prefixed with '$', and you mustn't forget it!  If you forget it then it
 149             causes a read from memory instead, so:
 150             mov $2,%eax         moves number 2 into %eax
 151             mov 2,%eax          reads the 32 bit word from address 2 into %eax (ie. most likely a mistake)
 152 
 153         (4) gas has a funky syntax for local labels, where '1f' (etc.) means label '1:' "forwards"
 154             and '1b' (etc.) means label '1:' "backwards".  Notice that these labels might be mistaken
 155             for hex numbers (eg. you might confuse 1b with $0x1b).
 156 
 157         (5) 'ja' is "jump if above", 'jb' for "jump if below", 'je' "jump if equal" etc.
 158 
 159         (6) gas has a reasonably nice .macro syntax, and I use them a lot to make the code shorter and
 160             less repetitive.
 161 
 162         For more help reading the assembler, do "info gas" at the Linux prompt.
 163 
 164         Now the tutorial starts in earnest.
 165 
 166         THE DICTIONARY ----------------------------------------------------------------------
 167 
 168         In FORTH as you will know, functions are called "words", and just as in other languages they
 169         have a name and a definition.  Here are two FORTH words:
 170 
 171         : DOUBLE DUP + ;                \ name is "DOUBLE", definition is "DUP +"
 172         : QUADRUPLE DOUBLE DOUBLE ;     \ name is "QUADRUPLE", definition is "DOUBLE DOUBLE"
 173 
 174         Words, both built-in ones and ones which the programmer defines later, are stored in a dictionary
 175         which is just a linked list of dictionary entries.
 176 
 177         <--- DICTIONARY ENTRY (HEADER) ----------------------->
 178         +------------------------+--------+---------- - - - - +----------- - - - -
 179         | LINK POINTER           | LENGTH/| NAME              | DEFINITION
 180         |                        | FLAGS  |                   |
 181         +--- (4 bytes) ----------+- byte -+- n bytes  - - - - +----------- - - - -
 182 
 183         I'll come to the definition of the word later.  For now just look at the header.  The first
 184         4 bytes are the link pointer.  This points back to the previous word in the dictionary, or, for
 185         the first word in the dictionary it is just a NULL pointer.  Then comes a length/flags byte.
 186         The length of the word can be up to 31 characters (5 bits used) and the top three bits are used
 187         for various flags which I'll come to later.  This is followed by the name itself, and in this
 188         implementation the name is rounded up to a multiple of 4 bytes by padding it with zero bytes.
 189         That's just to ensure that the definition starts on a 32 bit boundary.
 190 
 191         A FORTH variable called LATEST contains a pointer to the most recently defined word, in
 192         other words, the head of this linked list.
 193 
 194         DOUBLE and QUADRUPLE might look like this:
 195 
 196           pointer to previous word
 197            ^
 198            |
 199         +--|------+---+---+---+---+---+---+---+---+------------- - - - -
 200         | LINK    | 6 | D | O | U | B | L | E | 0 | (definition ...)
 201         +---------+---+---+---+---+---+---+---+---+------------- - - - -
 202            ^       len                         padding
 203            |
 204         +--|------+---+---+---+---+---+---+---+---+---+---+---+---+------------- - - - -
 205         | LINK    | 9 | Q | U | A | D | R | U | P | L | E | 0 | 0 | (definition ...)
 206         +---------+---+---+---+---+---+---+---+---+---+---+---+---+------------- - - - -
 207            ^       len                                     padding
 208            |
 209            |
 210           LATEST
 211 
 212         You should be able to see from this how you might implement functions to find a word in
 213         the dictionary (just walk along the dictionary entries starting at LATEST and matching
 214         the names until you either find a match or hit the NULL pointer at the end of the dictionary);
 215         and add a word to the dictionary (create a new definition, set its LINK to LATEST, and set
 216         LATEST to point to the new word).  We'll see precisely these functions implemented in
 217         assembly code later on.
 218 
 219         One interesting consequence of using a linked list is that you can redefine words, and
 220         a newer definition of a word overrides an older one.  This is an important concept in
 221         FORTH because it means that any word (even "built-in" or "standard" words) can be
 222         overridden with a new definition, either to enhance it, to make it faster or even to
 223         disable it.  However because of the way that FORTH words get compiled, which you'll
 224         understand below, words defined using the old definition of a word continue to use
 225         the old definition.  Only words defined after the new definition use the new definition.
 226 
 227         DIRECT THREADED CODE ----------------------------------------------------------------------
 228 
 229         Now we'll get to the really crucial bit in understanding FORTH, so go and get a cup of tea
 230         or coffee and settle down.  It's fair to say that if you don't understand this section, then you
 231         won't "get" how FORTH works, and that would be a failure on my part for not explaining it well.
 232         So if after reading this section a few times you don't understand it, please email me
 233         (rich@annexia.org).
 234 
 235         Let's talk first about what "threaded code" means.  Imagine a peculiar version of C where
 236         you are only allowed to call functions without arguments.  (Don't worry for now that such a
 237         language would be completely useless!)  So in our peculiar C, code would look like this:
 238 
 239         f ()
 240         {
 241           a ();
 242           b ();
 243           c ();
 244         }
 245 
 246         and so on.  How would a function, say 'f' above, be compiled by a standard C compiler?
 247         Probably into assembly code like this.  On the right hand side I've written the actual
 248         i386 machine code.
 249 
 250         f:
 251           CALL a                        E8 08 00 00 00
 252           CALL b                        E8 1C 00 00 00
 253           CALL c                        E8 2C 00 00 00
 254           ; ignore the return from the function for now
 255 
 256         "E8" is the x86 machine code to "CALL" a function.  In the first 20 years of computing
 257         memory was hideously expensive and we might have worried about the wasted space being used
 258         by the repeated "E8" bytes.  We can save 20% in code size (and therefore, in expensive memory)
 259         by compressing this into just:
 260 
 261         08 00 00 00             Just the function addresses, without
 262         1C 00 00 00             the CALL prefix.
 263         2C 00 00 00
 264 
 265         On a 16-bit machine like the ones which originally ran FORTH the savings are even greater - 33%.
 266 
 267         [Historical note: If the execution model that FORTH uses looks strange from the following
 268         paragraphs, then it was motivated entirely by the need to save memory on early computers.
 269         This code compression isn't so important now when our machines have more memory in their L1
 270         caches than those early computers had in total, but the execution model still has some
 271         useful properties].
 272 
 273         Of course this code won't run directly on the CPU any more.  Instead we need to write an
 274         interpreter which takes each set of bytes and calls it.
 275 
 276         On an i386 machine it turns out that we can write this interpreter rather easily, in just
 277         two assembly instructions which turn into just 3 bytes of machine code.  Let's store the
 278         pointer to the next word to execute in the %esi register:
 279 
 280                 08 00 00 00     <- We're executing this one now.  %esi is the _next_ one to execute.
 281         %esi -> 1C 00 00 00
 282                 2C 00 00 00
 283 
 284         The all-important i386 instruction is called LODSL (or in Intel manuals, LODSW).  It does
 285         two things.  Firstly it reads the memory at %esi into the accumulator (%eax).  Secondly it
 286         increments %esi by 4 bytes.  So after LODSL, the situation now looks like this:
 287 
 288                 08 00 00 00     <- We're still executing this one
 289                 1C 00 00 00     <- %eax now contains this address (0x0000001C)
 290         %esi -> 2C 00 00 00
 291 
 292         Now we just need to jump to the address in %eax.  This is again just a single x86 instruction
 293         written JMP *(%eax).  And after doing the jump, the situation looks like:
 294 
 295                 08 00 00 00
 296                 1C 00 00 00     <- Now we're executing this subroutine.
 297         %esi -> 2C 00 00 00
 298 
 299         To make this work, each subroutine is followed by the two instructions 'LODSL; JMP *(%eax)'
 300         which literally make the jump to the next subroutine.
 301 
 302         And that brings us to our first piece of actual code!  Well, it's a macro.
 303 */
 304 
 305 /* NEXT macro. */
 306         .macro NEXT
 307         lodsl
 308         jmp *(%eax)
 309         .endm
 310 
 311 /*      The macro is called NEXT.  That's a FORTH-ism.  It expands to those two instructions.
 312 
 313         Every FORTH primitive that we write has to be ended by NEXT.  Think of it kind of like
 314         a return.
 315 
 316         The above describes what is known as direct threaded code.
 317 
 318         To sum up: We compress our function calls down to a list of addresses and use a somewhat
 319         magical macro to act as a "jump to next function in the list".  We also use one register (%esi)
 320         to act as a kind of instruction pointer, pointing to the next function in the list.
 321 
 322         I'll just give you a hint of what is to come by saying that a FORTH definition such as:
 323 
 324         : QUADRUPLE DOUBLE DOUBLE ;
 325 
 326         actually compiles (almost, not precisely but we'll see why in a moment) to a list of
 327         function addresses for DOUBLE, DOUBLE and a special function called EXIT to finish off.
 328 
 329         At this point, REALLY EAGLE-EYED ASSEMBLY EXPERTS are saying "JONES, YOU'VE MADE A MISTAKE!".
 330 
 331         I lied about JMP *(%eax).  
 332 
 333         INDIRECT THREADED CODE ----------------------------------------------------------------------
 334 
 335         It turns out that direct threaded code is interesting but only if you want to just execute
 336         a list of functions written in assembly language.  So QUADRUPLE would work only if DOUBLE
 337         was an assembly language function.  In the direct threaded code, QUADRUPLE would look like:
 338 
 339                 +------------------+
 340                 | addr of DOUBLE  --------------------> (assembly code to do the double)
 341                 +------------------+                    NEXT
 342         %esi -> | addr of DOUBLE   |
 343                 +------------------+
 344 
 345         We can add an extra indirection to allow us to run both words written in assembly language
 346         (primitives written for speed) and words written in FORTH themselves as lists of addresses.
 347 
 348         The extra indirection is the reason for the brackets in JMP *(%eax).
 349 
 350         Let's have a look at how QUADRUPLE and DOUBLE really look in FORTH:
 351 
 352                 : QUADRUPLE DOUBLE DOUBLE ;
 353 
 354                 +------------------+
 355                 | codeword         |               : DOUBLE DUP + ;
 356                 +------------------+
 357                 | addr of DOUBLE  ---------------> +------------------+
 358                 +------------------+               | codeword         |
 359                 | addr of DOUBLE   |               +------------------+
 360                 +------------------+               | addr of DUP   --------------> +------------------+
 361                 | addr of EXIT     |               +------------------+            | codeword      -------+
 362                 +------------------+       %esi -> | addr of +     --------+       +------------------+   |
 363                                                    +------------------+    |       | assembly to    <-----+
 364                                                    | addr of EXIT     |    |       | implement DUP    |
 365                                                    +------------------+    |       |    ..            |
 366                                                                            |       |    ..            |
 367                                                                            |       | NEXT             |
 368                                                                            |       +------------------+
 369                                                                            |
 370                                                                            +-----> +------------------+
 371                                                                                    | codeword      -------+
 372                                                                                    +------------------+   |
 373                                                                                    | assembly to   <------+
 374                                                                                    | implement +      |
 375                                                                                    |    ..            |
 376                                                                                    |    ..            |
 377                                                                                    | NEXT             |
 378                                                                                    +------------------+
 379 
 380         This is the part where you may need an extra cup of tea/coffee/favourite caffeinated
 381         beverage.  What has changed is that I've added an extra pointer to the beginning of
 382         the definitions.  In FORTH this is sometimes called the "codeword".  The codeword is
 383         a pointer to the interpreter to run the function.  For primitives written in
 384         assembly language, the "interpreter" just points to the actual assembly code itself.
 385         They don't need interpreting, they just run.
 386 
 387         In words written in FORTH (like QUADRUPLE and DOUBLE), the codeword points to an interpreter
 388         function.
 389 
 390         I'll show you the interpreter function shortly, but let's recall our indirect
 391         JMP *(%eax) with the "extra" brackets.  Take the case where we're executing DOUBLE
 392         as shown, and DUP has been called.  Note that %esi is pointing to the address of +
 393 
 394         The assembly code for DUP eventually does a NEXT.  That:
 395 
 396         (1) reads the address of + into %eax            %eax points to the codeword of +
 397         (2) increments %esi by 4
 398         (3) jumps to the indirect %eax                  jumps to the address in the codeword of +,
 399                                                         ie. the assembly code to implement +
 400 
 401                 +------------------+
 402                 | codeword         |
 403                 +------------------+
 404                 | addr of DOUBLE  ---------------> +------------------+
 405                 +------------------+               | codeword         |
 406                 | addr of DOUBLE   |               +------------------+
 407                 +------------------+               | addr of DUP   --------------> +------------------+
 408                 | addr of EXIT     |               +------------------+            | codeword      -------+
 409                 +------------------+               | addr of +     --------+       +------------------+   |
 410                                                    +------------------+    |       | assembly to    <-----+
 411                                            %esi -> | addr of EXIT     |    |       | implement DUP    |
 412                                                    +------------------+    |       |    ..            |
 413                                                                            |       |    ..            |
 414                                                                            |       | NEXT             |
 415                                                                            |       +------------------+
 416                                                                            |
 417                                                                            +-----> +------------------+
 418                                                                                    | codeword      -------+
 419                                                                                    +------------------+   |
 420                                                                         now we're  | assembly to    <-----+
 421                                                                         executing  | implement +      |
 422                                                                         this       |    ..            |
 423                                                                         function   |    ..            |
 424                                                                                    | NEXT             |
 425                                                                                    +------------------+
 426 
 427         So I hope that I've convinced you that NEXT does roughly what you'd expect.  This is
 428         indirect threaded code.
 429 
 430         I've glossed over four things.  I wonder if you can guess without reading on what they are?
 431 
 432         .
 433         .
 434         .
 435 
 436         My list of four things are: (1) What does "EXIT" do?  (2) which is related to (1) is how do
 437         you call into a function, ie. how does %esi start off pointing at part of QUADRUPLE, but
 438         then point at part of DOUBLE.  (3) What goes in the codeword for the words which are written
 439         in FORTH?  (4) How do you compile a function which does anything except call other functions
 440         ie. a function which contains a number like : DOUBLE 2 * ; ?
 441 
 442         THE INTERPRETER AND RETURN STACK ------------------------------------------------------------
 443 
 444         Going at these in no particular order, let's talk about issues (3) and (2), the interpreter
 445         and the return stack.
 446 
 447         Words which are defined in FORTH need a codeword which points to a little bit of code to
 448         give them a "helping hand" in life.  They don't need much, but they do need what is known
 449         as an "interpreter", although it doesn't really "interpret" in the same way that, say,
 450         Java bytecode used to be interpreted (ie. slowly).  This interpreter just sets up a few
 451         machine registers so that the word can then execute at full speed using the indirect
 452         threaded model above.
 453 
 454         One of the things that needs to happen when QUADRUPLE calls DOUBLE is that we save the old
 455         %esi ("instruction pointer") and create a new one pointing to the first word in DOUBLE.
 456         Because we will need to restore the old %esi at the end of DOUBLE (this is, after all, like
 457         a function call), we will need a stack to store these "return addresses" (old values of %esi).
 458 
 459         As you will have seen in the background documentation, FORTH has two stacks, an ordinary
 460         stack for parameters, and a return stack which is a bit more mysterious.  But our return
 461         stack is just the stack I talked about in the previous paragraph, used to save %esi when
 462         calling from a FORTH word into another FORTH word.
 463 
 464         In this FORTH, we are using the normal stack pointer (%esp) for the parameter stack.
 465         We will use the i386's "other" stack pointer (%ebp, usually called the "frame pointer")
 466         for our return stack.
 467 
 468         I've got two macros which just wrap up the details of using %ebp for the return stack.
 469         You use them as for example "PUSHRSP %eax" (push %eax on the return stack) or "POPRSP %ebx"
 470         (pop top of return stack into %ebx).
 471 */
 472 
 473 /* Macros to deal with the return stack. */
 474         .macro PUSHRSP reg
 475         lea -4(%ebp),%ebp       // push reg on to return stack
 476         movl \reg,(%ebp)
 477         .endm
 478 
 479         .macro POPRSP reg
 480         mov (%ebp),\reg         // pop top of return stack to reg
 481         lea 4(%ebp),%ebp
 482         .endm
 483 
 484 /*
 485         And with that we can now talk about the interpreter.
 486 
 487         In FORTH the interpreter function is often called DOCOL (I think it means "DO COLON" because
 488         all FORTH definitions start with a colon, as in : DOUBLE DUP + ;
 489 
 490         The "interpreter" (it's not really "interpreting") just needs to push the old %esi on the
 491         stack and set %esi to the first word in the definition.  Remember that we jumped to the
 492         function using JMP *(%eax)?  Well a consequence of that is that conveniently %eax contains
 493         the address of this codeword, so just by adding 4 to it we get the address of the first
 494         data word.  Finally after setting up %esi, it just does NEXT which causes that first word
 495         to run.
 496 */
 497 
 498 /* DOCOL - the interpreter! */
 499         .text
 500         .align 4
 501 DOCOL:
 502         PUSHRSP %esi            // push %esi on to the return stack
 503         addl $4,%eax            // %eax points to codeword, so make
 504         movl %eax,%esi          // %esi point to first data word
 505         NEXT
 506 
 507 /*
 508         Just to make this absolutely clear, let's see how DOCOL works when jumping from QUADRUPLE
 509         into DOUBLE:
 510 
 511                 QUADRUPLE:
 512                 +------------------+
 513                 | codeword         |
 514                 +------------------+               DOUBLE:
 515                 | addr of DOUBLE  ---------------> +------------------+
 516                 +------------------+       %eax -> | addr of DOCOL    |
 517         %esi -> | addr of DOUBLE   |               +------------------+
 518                 +------------------+               | addr of DUP      |
 519                 | addr of EXIT     |               +------------------+
 520                 +------------------+               | etc.             |
 521 
 522         First, the call to DOUBLE calls DOCOL (the codeword of DOUBLE).  DOCOL does this:  It
 523         pushes the old %esi on the return stack.  %eax points to the codeword of DOUBLE, so we
 524         just add 4 on to it to get our new %esi:
 525 
 526                 QUADRUPLE:
 527                 +------------------+
 528                 | codeword         |
 529                 +------------------+               DOUBLE:
 530                 | addr of DOUBLE  ---------------> +------------------+
 531 top of return   +------------------+       %eax -> | addr of DOCOL    |
 532 stack points -> | addr of DOUBLE   |       + 4 =   +------------------+
 533                 +------------------+       %esi -> | addr of DUP      |
 534                 | addr of EXIT     |               +------------------+
 535                 +------------------+               | etc.             |
 536 
 537         Then we do NEXT, and because of the magic of threaded code that increments %esi again
 538         and calls DUP.
 539 
 540         Well, it seems to work.
 541 
 542         One minor point here.  Because DOCOL is the first bit of assembly actually to be defined
 543         in this file (the others were just macros), and because I usually compile this code with the
 544         text segment starting at address 0, DOCOL has address 0.  So if you are disassembling the
 545         code and see a word with a codeword of 0, you will immediately know that the word is
 546         written in FORTH (it's not an assembler primitive) and so uses DOCOL as the interpreter.
 547 
 548         STARTING UP ----------------------------------------------------------------------
 549 
 550         Now let's get down to nuts and bolts.  When we start the program we need to set up
 551         a few things like the return stack.  But as soon as we can, we want to jump into FORTH
 552         code (albeit much of the "early" FORTH code will still need to be written as
 553         assembly language primitives).
 554 
 555         This is what the set up code does.  Does a tiny bit of house-keeping, sets up the
 556         separate return stack (NB: Linux gives us the ordinary parameter stack already), then
 557         immediately jumps to a FORTH word called QUIT.  Despite its name, QUIT doesn't quit
 558         anything.  It resets some internal state and starts reading and interpreting commands.
 559         (The reason it is called QUIT is because you can call QUIT from your own FORTH code
 560         to "quit" your program and go back to interpreting).
 561 */
 562 
 563 /* Assembler entry point. */
 564         .text
 565         .globl _start
 566 _start:
 567         cld
 568         mov %esp,var_S0         // Save the initial data stack pointer in FORTH variable S0.
 569         mov $return_stack_top,%ebp // Initialise the return stack.
 570         call set_up_data_segment
 571 
 572         mov $cold_start,%esi    // Initialise interpreter.
 573         NEXT                    // Run interpreter!
 574 
 575         .section .rodata
 576 cold_start:                     // High-level code without a codeword.
 577         .int QUIT
 578 
 579 /*
 580         BUILT-IN WORDS ----------------------------------------------------------------------
 581 
 582         Remember our dictionary entries (headers)?  Let's bring those together with the codeword
 583         and data words to see how : DOUBLE DUP + ; really looks in memory.
 584 
 585           pointer to previous word
 586            ^
 587            |
 588         +--|------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
 589         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        | +          | EXIT       |
 590         +---------+---+---+---+---+---+---+---+---+------------+--|---------+------------+------------+
 591            ^       len                         pad  codeword      |
 592            |                                                      V
 593           LINK in next word                             points to codeword of DUP
 594         
 595         Initially we can't just write ": DOUBLE DUP + ;" (ie. that literal string) here because we
 596         don't yet have anything to read the string, break it up at spaces, parse each word, etc. etc.
 597         So instead we will have to define built-in words using the GNU assembler data constructors
 598         (like .int, .byte, .string, .ascii and so on -- look them up in the gas info page if you are
 599         unsure of them).
 600 
 601         The long way would be:
 602 
 603         .int <link to previous word>
 604         .byte 6                 // len
 605         .ascii "DOUBLE"         // string
 606         .byte 0                 // padding
 607 DOUBLE: .int DOCOL              // codeword
 608         .int DUP                // pointer to codeword of DUP
 609         .int PLUS               // pointer to codeword of +
 610         .int EXIT               // pointer to codeword of EXIT
 611 
 612         That's going to get quite tedious rather quickly, so here I define an assembler macro
 613         so that I can just write:
 614 
 615         defword "DOUBLE",6,,DOUBLE
 616         .int DUP,PLUS,EXIT
 617 
 618         and I'll get exactly the same effect.
 619 
 620         Don't worry too much about the exact implementation details of this macro - it's complicated!
 621 */
 622 
 623 /* Flags - these are discussed later. */
 624         .set F_IMMED,0x80
 625         .set F_HIDDEN,0x20
 626         .set F_LENMASK,0x1f     // length mask
 627 
 628         // Store the chain of links.
 629         .set link,0
 630 
 631         .macro defword name, namelen, flags=0, label
 632         .section .rodata
 633         .align 4
 634         .globl name_\label
 635 name_\label :
 636         .int link               // link
 637         .set link,name_\label
 638         .byte \flags+\namelen   // flags + length byte
 639         .ascii "\name"          // the name
 640         .align 4                // padding to next 4 byte boundary
 641         .globl \label
 642 \label :
 643         .int DOCOL              // codeword - the interpreter
 644         // list of word pointers follow
 645         .endm
 646 
 647 /*
 648         Similarly I want a way to write words written in assembly language.  There will quite a few
 649         of these to start with because, well, everything has to start in assembly before there's
 650         enough "infrastructure" to be able to start writing FORTH words, but also I want to define
 651         some common FORTH words in assembly language for speed, even though I could write them in FORTH.
 652 
 653         This is what DUP looks like in memory:
 654 
 655           pointer to previous word
 656            ^
 657            |
 658         +--|------+---+---+---+---+------------+
 659         | LINK    | 3 | D | U | P | code_DUP ---------------------> points to the assembly
 660         +---------+---+---+---+---+------------+                    code used to write DUP,
 661            ^       len              codeword                        which ends with NEXT.
 662            |
 663           LINK in next word
 664 
 665         Again, for brevity in writing the header I'm going to write an assembler macro called defcode.
 666         As with defword above, don't worry about the complicated details of the macro.
 667 */
 668 
 669         .macro defcode name, namelen, flags=0, label
 670         .section .rodata
 671         .align 4
 672         .globl name_\label
 673 name_\label :
 674         .int link               // link
 675         .set link,name_\label
 676         .byte \flags+\namelen   // flags + length byte
 677         .ascii "\name"          // the name
 678         .align 4                // padding to next 4 byte boundary
 679         .globl \label
 680 \label :
 681         .int code_\label        // codeword
 682         .text
 683         //.align 4
 684         .globl code_\label
 685 code_\label :                   // assembler code follows
 686         .endm
 687 
 688 /*
 689         Now some easy FORTH primitives.  These are written in assembly for speed.  If you understand
 690         i386 assembly language then it is worth reading these.  However if you don't understand assembly
 691         you can skip the details.
 692 */
 693 
 694         defcode "DROP",4,,DROP
 695         pop %eax                // drop top of stack
 696         NEXT
 697 
 698         defcode "SWAP",4,,SWAP
 699         pop %eax                // swap top two elements on stack
 700         pop %ebx
 701         push %eax
 702         push %ebx
 703         NEXT
 704 
 705         defcode "DUP",3,,DUP
 706         mov (%esp),%eax         // duplicate top of stack
 707         push %eax
 708         NEXT
 709 
 710         defcode "OVER",4,,OVER
 711         mov 4(%esp),%eax        // get the second element of stack
 712         push %eax               // and push it on top
 713         NEXT
 714 
 715         defcode "ROT",3,,ROT
 716         pop %eax
 717         pop %ebx
 718         pop %ecx
 719         push %ebx
 720         push %eax
 721         push %ecx
 722         NEXT
 723 
 724         defcode "-ROT",4,,NROT
 725         pop %eax
 726         pop %ebx
 727         pop %ecx
 728         push %eax
 729         push %ecx
 730         push %ebx
 731         NEXT
 732 
 733         defcode "2DROP",5,,TWODROP // drop top two elements of stack
 734         pop %eax
 735         pop %eax
 736         NEXT
 737 
 738         defcode "2DUP",4,,TWODUP // duplicate top two elements of stack
 739         mov (%esp),%eax
 740         mov 4(%esp),%ebx
 741         push %ebx
 742         push %eax
 743         NEXT
 744 
 745         defcode "2SWAP",5,,TWOSWAP // swap top two pairs of elements of stack
 746         pop %eax
 747         pop %ebx
 748         pop %ecx
 749         pop %edx
 750         push %ebx
 751         push %eax
 752         push %edx
 753         push %ecx
 754         NEXT
 755 
 756         defcode "?DUP",4,,QDUP  // duplicate top of stack if non-zero
 757         movl (%esp),%eax
 758         test %eax,%eax
 759         jz 1f
 760         push %eax
 761 1:      NEXT
 762 
 763         defcode "1+",2,,INCR
 764         incl (%esp)             // increment top of stack
 765         NEXT
 766 
 767         defcode "1-",2,,DECR
 768         decl (%esp)             // decrement top of stack
 769         NEXT
 770 
 771         defcode "4+",2,,INCR4
 772         addl $4,(%esp)          // add 4 to top of stack
 773         NEXT
 774 
 775         defcode "4-",2,,DECR4
 776         subl $4,(%esp)          // subtract 4 from top of stack
 777         NEXT
 778 
 779         defcode "+",1,,ADD
 780         pop %eax                // get top of stack
 781         addl %eax,(%esp)        // and add it to next word on stack
 782         NEXT
 783 
 784         defcode "-",1,,SUB
 785         pop %eax                // get top of stack
 786         subl %eax,(%esp)        // and subtract it from next word on stack
 787         NEXT
 788 
 789         defcode "*",1,,MUL
 790         pop %eax
 791         pop %ebx
 792         imull %ebx,%eax
 793         push %eax               // ignore overflow
 794         NEXT
 795 
 796 /*
 797         In this FORTH, only /MOD is primitive.  Later we will define the / and MOD words in
 798         terms of the primitive /MOD.  The design of the i386 assembly instruction idiv which
 799         leaves both quotient and remainder makes this the obvious choice.
 800 */
 801 
 802         defcode "/MOD",4,,DIVMOD
 803         xor %edx,%edx
 804         pop %ebx
 805         pop %eax
 806         idivl %ebx
 807         push %edx               // push remainder
 808         push %eax               // push quotient
 809         NEXT
 810 
 811 /*
 812         Lots of comparison operations like =, <, >, etc..
 813 
 814         ANS FORTH says that the comparison words should return all (binary) 1's for
 815         TRUE and all 0's for FALSE.  However this is a bit of a strange convention
 816         so this FORTH breaks it and returns the more normal (for C programmers ...)
 817         1 meaning TRUE and 0 meaning FALSE.
 818 */
 819 
 820         defcode "=",1,,EQU      // top two words are equal?
 821         pop %eax
 822         pop %ebx
 823         cmp %ebx,%eax
 824         sete %al
 825         movzbl %al,%eax
 826         pushl %eax
 827         NEXT
 828 
 829         defcode "<>",2,,NEQU    // top two words are not equal?
 830         pop %eax
 831         pop %ebx
 832         cmp %ebx,%eax
 833         setne %al
 834         movzbl %al,%eax
 835         pushl %eax
 836         NEXT
 837 
 838         defcode "<",1,,LT
 839         pop %eax
 840         pop %ebx
 841         cmp %eax,%ebx
 842         setl %al
 843         movzbl %al,%eax
 844         pushl %eax
 845         NEXT
 846 
 847         defcode ">",1,,GT
 848         pop %eax
 849         pop %ebx
 850         cmp %eax,%ebx
 851         setg %al
 852         movzbl %al,%eax
 853         pushl %eax
 854         NEXT
 855 
 856         defcode "<=",2,,LE
 857         pop %eax
 858         pop %ebx
 859         cmp %eax,%ebx
 860         setle %al
 861         movzbl %al,%eax
 862         pushl %eax
 863         NEXT
 864 
 865         defcode ">=",2,,GE
 866         pop %eax
 867         pop %ebx
 868         cmp %eax,%ebx
 869         setge %al
 870         movzbl %al,%eax
 871         pushl %eax
 872         NEXT
 873 
 874         defcode "0=",2,,ZEQU    // top of stack equals 0?
 875         pop %eax
 876         test %eax,%eax
 877         setz %al
 878         movzbl %al,%eax
 879         pushl %eax
 880         NEXT
 881 
 882         defcode "0<>",3,,ZNEQU  // top of stack not 0?
 883         pop %eax
 884         test %eax,%eax
 885         setnz %al
 886         movzbl %al,%eax
 887         pushl %eax
 888         NEXT
 889 
 890         defcode "0<",2,,ZLT     // comparisons with 0
 891         pop %eax
 892         test %eax,%eax
 893         setl %al
 894         movzbl %al,%eax
 895         pushl %eax
 896         NEXT
 897 
 898         defcode "0>",2,,ZGT
 899         pop %eax
 900         test %eax,%eax
 901         setg %al
 902         movzbl %al,%eax
 903         pushl %eax
 904         NEXT
 905 
 906         defcode "0<=",3,,ZLE
 907         pop %eax
 908         test %eax,%eax
 909         setle %al
 910         movzbl %al,%eax
 911         pushl %eax
 912         NEXT
 913 
 914         defcode "0>=",3,,ZGE
 915         pop %eax
 916         test %eax,%eax
 917         setge %al
 918         movzbl %al,%eax
 919         pushl %eax
 920         NEXT
 921 
 922         defcode "AND",3,,AND    // bitwise AND
 923         pop %eax
 924         andl %eax,(%esp)
 925         NEXT
 926 
 927         defcode "OR",2,,OR      // bitwise OR
 928         pop %eax
 929         orl %eax,(%esp)
 930         NEXT
 931 
 932         defcode "XOR",3,,XOR    // bitwise XOR
 933         pop %eax
 934         xorl %eax,(%esp)
 935         NEXT
 936 
 937         defcode "INVERT",6,,INVERT // this is the FORTH bitwise "NOT" function (cf. NEGATE and NOT)
 938         notl (%esp)
 939         NEXT
 940 
 941 /*
 942         RETURNING FROM FORTH WORDS ----------------------------------------------------------------------
 943 
 944         Time to talk about what happens when we EXIT a function.  In this diagram QUADRUPLE has called
 945         DOUBLE, and DOUBLE is about to exit (look at where %esi is pointing):
 946 
 947                 QUADRUPLE
 948                 +------------------+
 949                 | codeword         |
 950                 +------------------+               DOUBLE
 951                 | addr of DOUBLE  ---------------> +------------------+
 952                 +------------------+               | codeword         |
 953                 | addr of DOUBLE   |               +------------------+
 954                 +------------------+               | addr of DUP      |
 955                 | addr of EXIT     |               +------------------+
 956                 +------------------+               | addr of +        |
 957                                                    +------------------+
 958                                            %esi -> | addr of EXIT     |
 959                                                    +------------------+
 960 
 961         What happens when the + function does NEXT?  Well, the following code is executed.
 962 */
 963 
 964         defcode "EXIT",4,,EXIT
 965         POPRSP %esi             // pop return stack into %esi
 966         NEXT
 967 
 968 /*
 969         EXIT gets the old %esi which we saved from before on the return stack, and puts it in %esi.
 970         So after this (but just before NEXT) we get:
 971 
 972                 QUADRUPLE
 973                 +------------------+
 974                 | codeword         |
 975                 +------------------+               DOUBLE
 976                 | addr of DOUBLE  ---------------> +------------------+
 977                 +------------------+               | codeword         |
 978         %esi -> | addr of DOUBLE   |               +------------------+
 979                 +------------------+               | addr of DUP      |
 980                 | addr of EXIT     |               +------------------+
 981                 +------------------+               | addr of +        |
 982                                                    +------------------+
 983                                                    | addr of EXIT     |
 984                                                    +------------------+
 985 
 986         And NEXT just completes the job by, well, in this case just by calling DOUBLE again :-)
 987 
 988         LITERALS ----------------------------------------------------------------------
 989 
 990         The final point I "glossed over" before was how to deal with functions that do anything
 991         apart from calling other functions.  For example, suppose that DOUBLE was defined like this:
 992 
 993         : DOUBLE 2 * ;
 994 
 995         It does the same thing, but how do we compile it since it contains the literal 2?  One way
 996         would be to have a function called "2" (which you'd have to write in assembler), but you'd need
 997         a function for every single literal that you wanted to use.
 998 
 999         FORTH solves this by compiling the function using a special word called LIT:
1000 
1001         +---------------------------+-------+-------+-------+-------+-------+
1002         | (usual header of DOUBLE)  | DOCOL | LIT   | 2     | *     | EXIT  |
1003         +---------------------------+-------+-------+-------+-------+-------+
1004 
1005         LIT is executed in the normal way, but what it does next is definitely not normal.  It
1006         looks at %esi (which now points to the number 2), grabs it, pushes it on the stack, then
1007         manipulates %esi in order to skip the number as if it had never been there.
1008 
1009         What's neat is that the whole grab/manipulate can be done using a single byte single
1010         i386 instruction, our old friend LODSL.  Rather than me drawing more ASCII-art diagrams,
1011         see if you can find out how LIT works:
1012 */
1013 
1014         defcode "LIT",3,,LIT
1015         // %esi points to the next command, but in this case it points to the next
1016         // literal 32 bit integer.  Get that literal into %eax and increment %esi.
1017         // On x86, it's a convenient single byte instruction!  (cf. NEXT macro)
1018         lodsl
1019         push %eax               // push the literal number on to stack
1020         NEXT
1021 
1022 /*
1023         MEMORY ----------------------------------------------------------------------
1024 
1025         As important point about FORTH is that it gives you direct access to the lowest levels
1026         of the machine.  Manipulating memory directly is done frequently in FORTH, and these are
1027         the primitive words for doing it.
1028 */
1029 
1030         defcode "!",1,,STORE
1031         pop %ebx                // address to store at
1032         pop %eax                // data to store there
1033         mov %eax,(%ebx)         // store it
1034         NEXT
1035 
1036         defcode "@",1,,FETCH
1037         pop %ebx                // address to fetch
1038         mov (%ebx),%eax         // fetch it
1039         push %eax               // push value onto stack
1040         NEXT
1041 
1042         defcode "+!",2,,ADDSTORE
1043         pop %ebx                // address
1044         pop %eax                // the amount to add
1045         addl %eax,(%ebx)        // add it
1046         NEXT
1047 
1048         defcode "-!",2,,SUBSTORE
1049         pop %ebx                // address
1050         pop %eax                // the amount to subtract
1051         subl %eax,(%ebx)        // add it
1052         NEXT
1053 
1054 /*
1055         ! and @ (STORE and FETCH) store 32-bit words.  It's also useful to be able to read and write bytes
1056         so we also define standard words C@ and C!.
1057 
1058         Byte-oriented operations only work on architectures which permit them (i386 is one of those).
1059  */
1060 
1061         defcode "C!",2,,STOREBYTE
1062         pop %ebx                // address to store at
1063         pop %eax                // data to store there
1064         movb %al,(%ebx)         // store it
1065         NEXT
1066 
1067         defcode "C@",2,,FETCHBYTE
1068         pop %ebx                // address to fetch
1069         xor %eax,%eax
1070         movb (%ebx),%al         // fetch it
1071         push %eax               // push value onto stack
1072         NEXT
1073 
1074 /* C@C! is a useful byte copy primitive. */
1075         defcode "C@C!",4,,CCOPY
1076         movl 4(%esp),%ebx       // source address
1077         movb (%ebx),%al         // get source character
1078         pop %edi                // destination address
1079         stosb                   // copy to destination
1080         push %edi               // increment destination address
1081         incl 4(%esp)            // increment source address
1082         NEXT
1083 
1084 /* and CMOVE is a block copy operation. */
1085         defcode "CMOVE",5,,CMOVE
1086         mov %esi,%edx           // preserve %esi
1087         pop %ecx                // length
1088         pop %edi                // destination address
1089         pop %esi                // source address
1090         rep movsb               // copy source to destination
1091         mov %edx,%esi           // restore %esi
1092         NEXT
1093 
1094 /*
1095         BUILT-IN VARIABLES ----------------------------------------------------------------------
1096 
1097         These are some built-in variables and related standard FORTH words.  Of these, the only one that we
1098         have discussed so far was LATEST, which points to the last (most recently defined) word in the
1099         FORTH dictionary.  LATEST is also a FORTH word which pushes the address of LATEST (the variable)
1100         on to the stack, so you can read or write it using @ and ! operators.  For example, to print
1101         the current value of LATEST (and this can apply to any FORTH variable) you would do:
1102 
1103         LATEST @ . CR
1104 
1105         To make defining variables shorter, I'm using a macro called defvar, similar to defword and
1106         defcode above.  (In fact the defvar macro uses defcode to do the dictionary header).
1107 */
1108 
1109         .macro defvar name, namelen, flags=0, label, initial=0
1110         defcode \name,\namelen,\flags,\label
1111         push $var_\name
1112         NEXT
1113         .data
1114         .align 4
1115 var_\name :
1116         .int \initial
1117         .endm
1118 
1119 /*
1120         The built-in variables are:
1121 
1122         STATE           Is the interpreter executing code (0) or compiling a word (non-zero)?
1123         LATEST          Points to the latest (most recently defined) word in the dictionary.
1124         HERE            Points to the next free byte of memory.  When compiling, compiled words go here.
1125         S0              Stores the address of the top of the parameter stack.
1126         BASE            The current base for printing and reading numbers.
1127 
1128 */
1129         defvar "STATE",5,,STATE
1130         defvar "HERE",4,,HERE
1131         defvar "LATEST",6,,LATEST,name_SYSCALL0 // SYSCALL0 must be last in built-in dictionary
1132         defvar "S0",2,,SZ
1133         defvar "BASE",4,,BASE,10
1134 
1135 /*
1136         BUILT-IN CONSTANTS ----------------------------------------------------------------------
1137 
1138         It's also useful to expose a few constants to FORTH.  When the word is executed it pushes a
1139         constant value on the stack.
1140 
1141         The built-in constants are:
1142 
1143         VERSION         Is the current version of this FORTH.
1144         R0              The address of the top of the return stack.
1145         DOCOL           Pointer to DOCOL.
1146         F_IMMED         The IMMEDIATE flag's actual value.
1147         F_HIDDEN        The HIDDEN flag's actual value.
1148         F_LENMASK       The length mask in the flags/len byte.
1149 
1150         SYS_*           and the numeric codes of various Linux syscalls (from <asm/unistd.h>)
1151 */
1152 
1153 //#include <asm-i386/unistd.h>  // you might need this instead
1154 #include <asm/unistd.h>
1155 
1156         .macro defconst name, namelen, flags=0, label, value
1157         defcode \name,\namelen,\flags,\label
1158         push $\value
1159         NEXT
1160         .endm
1161 
1162         defconst "VERSION",7,,VERSION,JONES_VERSION
1163         defconst "R0",2,,RZ,return_stack_top
1164         defconst "DOCOL",5,,__DOCOL,DOCOL
1165         defconst "F_IMMED",7,,__F_IMMED,F_IMMED
1166         defconst "F_HIDDEN",8,,__F_HIDDEN,F_HIDDEN
1167         defconst "F_LENMASK",9,,__F_LENMASK,F_LENMASK
1168 
1169         defconst "SYS_EXIT",8,,SYS_EXIT,__NR_exit
1170         defconst "SYS_OPEN",8,,SYS_OPEN,__NR_open
1171         defconst "SYS_CLOSE",9,,SYS_CLOSE,__NR_close
1172         defconst "SYS_READ",8,,SYS_READ,__NR_read
1173         defconst "SYS_WRITE",9,,SYS_WRITE,__NR_write
1174         defconst "SYS_CREAT",9,,SYS_CREAT,__NR_creat
1175         defconst "SYS_BRK",7,,SYS_BRK,__NR_brk
1176 
1177         defconst "O_RDONLY",8,,__O_RDONLY,0
1178         defconst "O_WRONLY",8,,__O_WRONLY,1
1179         defconst "O_RDWR",6,,__O_RDWR,2
1180         defconst "O_CREAT",7,,__O_CREAT,0100
1181         defconst "O_EXCL",6,,__O_EXCL,0200
1182         defconst "O_TRUNC",7,,__O_TRUNC,01000
1183         defconst "O_APPEND",8,,__O_APPEND,02000
1184         defconst "O_NONBLOCK",10,,__O_NONBLOCK,04000
1185 
1186 /*
1187         RETURN STACK ----------------------------------------------------------------------
1188 
1189         These words allow you to access the return stack.  Recall that the register %ebp always points to
1190         the top of the return stack.
1191 */
1192 
1193         defcode ">R",2,,TOR
1194         pop %eax                // pop parameter stack into %eax
1195         PUSHRSP %eax            // push it on to the return stack
1196         NEXT
1197 
1198         defcode "R>",2,,FROMR
1199         POPRSP %eax             // pop return stack on to %eax
1200         push %eax               // and push on to parameter stack
1201         NEXT
1202 
1203         defcode "RSP@",4,,RSPFETCH
1204         push %ebp
1205         NEXT
1206 
1207         defcode "RSP!",4,,RSPSTORE
1208         pop %ebp
1209         NEXT
1210 
1211         defcode "RDROP",5,,RDROP
1212         addl $4,%ebp            // pop return stack and throw away
1213         NEXT
1214 
1215 /*
1216         PARAMETER (DATA) STACK ----------------------------------------------------------------------
1217 
1218         These functions allow you to manipulate the parameter stack.  Recall that Linux sets up the parameter
1219         stack for us, and it is accessed through %esp.
1220 */
1221 
1222         defcode "DSP@",4,,DSPFETCH
1223         mov %esp,%eax
1224         push %eax
1225         NEXT
1226 
1227         defcode "DSP!",4,,DSPSTORE
1228         pop %esp
1229         NEXT
1230 
1231 /*
1232         INPUT AND OUTPUT ----------------------------------------------------------------------
1233 
1234         These are our first really meaty/complicated FORTH primitives.  I have chosen to write them in
1235         assembler, but surprisingly in "real" FORTH implementations these are often written in terms
1236         of more fundamental FORTH primitives.  I chose to avoid that because I think that just obscures
1237         the implementation.  After all, you may not understand assembler but you can just think of it
1238         as an opaque block of code that does what it says.
1239 
1240         Let's discuss input first.
1241 
1242         The FORTH word KEY reads the next byte from stdin (and pushes it on the parameter stack).
1243         So if KEY is called and someone hits the space key, then the number 32 (ASCII code of space)
1244         is pushed on the stack.
1245 
1246         In FORTH there is no distinction between reading code and reading input.  We might be reading
1247         and compiling code, we might be reading words to execute, we might be asking for the user
1248         to type their name -- ultimately it all comes in through KEY.
1249 
1250         The implementation of KEY uses an input buffer of a certain size (defined at the end of this
1251         file).  It calls the Linux read(2) system call to fill this buffer and tracks its position
1252         in the buffer using a couple of variables, and if it runs out of input buffer then it refills
1253         it automatically.  The other thing that KEY does is if it detects that stdin has closed, it
1254         exits the program, which is why when you hit ^D the FORTH system cleanly exits.
1255 
1256      buffer                           bufftop
1257         |                                |
1258         V                                V
1259         +-------------------------------+--------------------------------------+
1260         | INPUT READ FROM STDIN ....... | unused part of the buffer            |
1261         +-------------------------------+--------------------------------------+
1262                           ^
1263                           |
1264                        currkey (next character to read)
1265 
1266         <---------------------- BUFFER_SIZE (4096 bytes) ---------------------->
1267 */
1268 
1269         defcode "KEY",3,,KEY
1270         call _KEY
1271         push %eax               // push return value on stack
1272         NEXT
1273 _KEY:
1274         mov (currkey),%ebx
1275         cmp (bufftop),%ebx
1276         jge 1f                  // exhausted the input buffer?
1277         xor %eax,%eax
1278         mov (%ebx),%al          // get next key from input buffer
1279         inc %ebx
1280         mov %ebx,(currkey)      // increment currkey
1281         ret
1282 
1283 1:      // Out of input; use read(2) to fetch more input from stdin.
1284         xor %ebx,%ebx           // 1st param: stdin
1285         mov $buffer,%ecx        // 2nd param: buffer
1286         mov %ecx,currkey
1287         mov $BUFFER_SIZE,%edx   // 3rd param: max length
1288         mov $__NR_read,%eax     // syscall: read
1289         int $0x80
1290         test %eax,%eax          // If %eax <= 0, then exit.
1291         jbe 2f
1292         addl %eax,%ecx          // buffer+%eax = bufftop
1293         mov %ecx,bufftop
1294         jmp _KEY
1295 
1296 2:      // Error or end of input: exit the program.
1297         xor %ebx,%ebx
1298         mov $__NR_exit,%eax     // syscall: exit
1299         int $0x80
1300 
1301         .data
1302         .align 4
1303 currkey:
1304         .int buffer             // Current place in input buffer (next character to read).
1305 bufftop:
1306         .int buffer             // Last valid data in input buffer + 1.
1307 
1308 /*
1309         By contrast, output is much simpler.  The FORTH word EMIT writes out a single byte to stdout.
1310         This implementation just uses the write system call.  No attempt is made to buffer output, but
1311         it would be a good exercise to add it.
1312 */
1313 
1314         defcode "EMIT",4,,EMIT
1315         pop %eax
1316         call _EMIT
1317         NEXT
1318 _EMIT:
1319         mov $1,%ebx             // 1st param: stdout
1320 
1321         // write needs the address of the byte to write
1322         mov %al,emit_scratch
1323         mov $emit_scratch,%ecx  // 2nd param: address
1324 
1325         mov $1,%edx             // 3rd param: nbytes = 1
1326 
1327         mov $__NR_write,%eax    // write syscall
1328         int $0x80
1329         ret
1330 
1331         .data                   // NB: easier to fit in the .data section
1332 emit_scratch:
1333         .space 1                // scratch used by EMIT
1334 
1335 /*
1336         Back to input, WORD is a FORTH word which reads the next full word of input.
1337 
1338         What it does in detail is that it first skips any blanks (spaces, tabs, newlines and so on).
1339         Then it calls KEY to read characters into an internal buffer until it hits a blank.  Then it
1340         calculates the length of the word it read and returns the address and the length as
1341         two words on the stack (with the length at the top of stack).
1342 
1343         Notice that WORD has a single internal buffer which it overwrites each time (rather like
1344         a static C string).  Also notice that WORD's internal buffer is just 32 bytes long and
1345         there is NO checking for overflow.  31 bytes happens to be the maximum length of a
1346         FORTH word that we support, and that is what WORD is used for: to read FORTH words when
1347         we are compiling and executing code.  The returned strings are not NUL-terminated.
1348 
1349         Start address+length is the normal way to represent strings in FORTH (not ending in an
1350         ASCII NUL character as in C), and so FORTH strings can contain any character including NULs
1351         and can be any length.
1352 
1353         WORD is not suitable for just reading strings (eg. user input) because of all the above
1354         peculiarities and limitations.
1355 
1356         Note that when executing, you'll see:
1357         WORD FOO
1358         which puts "FOO" and length 3 on the stack, but when compiling:
1359         : BAR WORD FOO ;
1360         is an error (or at least it doesn't do what you might expect).  Later we'll talk about compiling
1361         and immediate mode, and you'll understand why.
1362 */
1363 
1364         defcode "WORD",4,,WORD
1365         call _WORD
1366         push %edi               // push base address
1367         push %ecx               // push length
1368         NEXT
1369 
1370 _WORD:
1371         /* Search for first non-blank character.  Also skip \ comments. */
1372 1:
1373         call _KEY               // get next key, returned in %eax
1374         cmpb $'\\',%al          // start of a comment?
1375         je 3f                   // if so, skip the comment
1376         cmpb $' ',%al
1377         jbe 1b                  // if so, keep looking
1378 
1379         /* Search for the end of the word, storing chars as we go. */
1380         mov $word_buffer,%edi   // pointer to return buffer
1381 2:
1382         stosb                   // add character to return buffer
1383         call _KEY               // get next key, returned in %al
1384         cmpb $' ',%al           // is blank?
1385         ja 2b                   // if not, keep looping
1386 
1387         /* Return the word (well, the static buffer) and length. */
1388         sub $word_buffer,%edi
1389         mov %edi,%ecx           // return length of the word
1390         mov $word_buffer,%edi   // return address of the word
1391         ret
1392 
1393         /* Code to skip \ comments to end of the current line. */
1394 3:
1395         call _KEY
1396         cmpb $'\n',%al          // end of line yet?
1397         jne 3b
1398         jmp 1b
1399 
1400         .data                   // NB: easier to fit in the .data section
1401         // A static buffer where WORD returns.  Subsequent calls
1402         // overwrite this buffer.  Maximum word length is 32 chars.
1403 word_buffer:
1404         .space 32
1405 
1406 /*
1407         As well as reading in words we'll need to read in numbers and for that we are using a function
1408         called NUMBER.  This parses a numeric string such as one returned by WORD and pushes the
1409         number on the parameter stack.
1410 
1411         The function uses the variable BASE as the base (radix) for conversion, so for example if
1412         BASE is 2 then we expect a binary number.  Normally BASE is 10.
1413 
1414         If the word starts with a '-' character then the returned value is negative.
1415 
1416         If the string can't be parsed as a number (or contains characters outside the current BASE)
1417         then we need to return an error indication.  So NUMBER actually returns two items on the stack.
1418         At the top of stack we return the number of unconverted characters (ie. if 0 then all characters
1419         were converted, so there is no error).  Second from top of stack is the parsed number or a
1420         partial value if there was an error.
1421 */
1422         defcode "NUMBER",6,,NUMBER
1423         pop %ecx                // length of string
1424         pop %edi                // start address of string
1425         call _NUMBER
1426         push %eax               // parsed number
1427         push %ecx               // number of unparsed characters (0 = no error)
1428         NEXT
1429 
1430 _NUMBER:
1431         xor %eax,%eax
1432         xor %ebx,%ebx
1433 
1434         test %ecx,%ecx          // trying to parse a zero-length string is an error, but will return 0.
1435         jz 5f
1436 
1437         movl var_BASE,%edx      // get BASE (in %dl)
1438 
1439         // Check if first character is '-'.
1440         movb (%edi),%bl         // %bl = first character in string
1441         inc %edi
1442         push %eax               // push 0 on stack
1443         cmpb $'-',%bl           // negative number?
1444         jnz 2f
1445         pop %eax
1446         push %ebx               // push <> 0 on stack, indicating negative
1447         dec %ecx
1448         jnz 1f
1449         pop %ebx                // error: string is only '-'.
1450         movl $1,%ecx
1451         ret
1452 
1453         // Loop reading digits.
1454 1:      imull %edx,%eax         // %eax *= BASE
1455         movb (%edi),%bl         // %bl = next character in string
1456         inc %edi
1457 
1458         // Convert 0-9, A-Z to a number 0-35.
1459 2:      subb $'0',%bl           // < '0'?
1460         jb 4f
1461         cmp $10,%bl             // <= '9'?
1462         jb 3f
1463         subb $17,%bl            // < 'A'? (17 is 'A'-'0')
1464         jb 4f
1465         addb $10,%bl
1466 
1467 3:      cmp %dl,%bl             // >= BASE?
1468         jge 4f
1469 
1470         // OK, so add it to %eax and loop.
1471         add %ebx,%eax
1472         dec %ecx
1473         jnz 1b
1474 
1475         // Negate the result if first character was '-' (saved on the stack).
1476 4:      pop %ebx
1477         test %ebx,%ebx
1478         jz 5f
1479         neg %eax
1480 
1481 5:      ret
1482 
1483 /*
1484         DICTIONARY LOOK UPS ----------------------------------------------------------------------
1485 
1486         We're building up to our prelude on how FORTH code is compiled, but first we need yet more infrastructure.
1487 
1488         The FORTH word FIND takes a string (a word as parsed by WORD -- see above) and looks it up in the
1489         dictionary.  What it actually returns is the address of the dictionary header, if it finds it,
1490         or 0 if it didn't.
1491 
1492         So if DOUBLE is defined in the dictionary, then WORD DOUBLE FIND returns the following pointer:
1493 
1494     pointer to this
1495         |
1496         |
1497         V
1498         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1499         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        | +          | EXIT       |
1500         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1501 
1502         See also >CFA and >DFA.
1503 
1504         FIND doesn't find dictionary entries which are flagged as HIDDEN.  See below for why.
1505 */
1506 
1507         defcode "FIND",4,,FIND
1508         pop %ecx                // %ecx = length
1509         pop %edi                // %edi = address
1510         call _FIND
1511         push %eax               // %eax = address of dictionary entry (or NULL)
1512         NEXT
1513 
1514 _FIND:
1515         push %esi               // Save %esi so we can use it in string comparison.
1516 
1517         // Now we start searching backwards through the dictionary for this word.
1518         mov var_LATEST,%edx     // LATEST points to name header of the latest word in the dictionary
1519 1:      test %edx,%edx          // NULL pointer?  (end of the linked list)
1520         je 4f
1521 
1522         // Compare the length expected and the length of the word.
1523         // Note that if the F_HIDDEN flag is set on the word, then by a bit of trickery
1524         // this won't pick the word (the length will appear to be wrong).
1525         xor %eax,%eax
1526         movb 4(%edx),%al        // %al = flags+length field
1527         andb $(F_HIDDEN|F_LENMASK),%al // %al = name length
1528         cmpb %cl,%al            // Length is the same?
1529         jne 2f
1530 
1531         // Compare the strings in detail.
1532         push %ecx               // Save the length
1533         push %edi               // Save the address (repe cmpsb will move this pointer)
1534         lea 5(%edx),%esi        // Dictionary string we are checking against.
1535         repe cmpsb              // Compare the strings.
1536         pop %edi
1537         pop %ecx
1538         jne 2f                  // Not the same.
1539 
1540         // The strings are the same - return the header pointer in %eax
1541         pop %esi
1542         mov %edx,%eax
1543         ret
1544 
1545 2:      mov (%edx),%edx         // Move back through the link field to the previous word
1546         jmp 1b                  // .. and loop.
1547 
1548 4:      // Not found.
1549         pop %esi
1550         xor %eax,%eax           // Return zero to indicate not found.
1551         ret
1552 
1553 /*
1554         FIND returns the dictionary pointer, but when compiling we need the codeword pointer (recall
1555         that FORTH definitions are compiled into lists of codeword pointers).  The standard FORTH
1556         word >CFA turns a dictionary pointer into a codeword pointer.
1557 
1558         The example below shows the result of:
1559 
1560                 WORD DOUBLE FIND >CFA
1561 
1562         FIND returns a pointer to this
1563         |                               >CFA converts it to a pointer to this
1564         |                                          |
1565         V                                          V
1566         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1567         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        | +          | EXIT       |
1568         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1569                                                    codeword
1570 
1571         Notes:
1572 
1573         Because names vary in length, this isn't just a simple increment.
1574 
1575         In this FORTH you cannot easily turn a codeword pointer back into a dictionary entry pointer, but
1576         that is not true in most FORTH implementations where they store a back pointer in the definition
1577         (with an obvious memory/complexity cost).  The reason they do this is that it is useful to be
1578         able to go backwards (codeword -> dictionary entry) in order to decompile FORTH definitions
1579         quickly.
1580 
1581         What does CFA stand for?  My best guess is "Code Field Address".
1582 */
1583 
1584         defcode ">CFA",4,,TCFA
1585         pop %edi
1586         call _TCFA
1587         push %edi
1588         NEXT
1589 _TCFA:
1590         xor %eax,%eax
1591         add $4,%edi             // Skip link pointer.
1592         movb (%edi),%al         // Load flags+len into %al.
1593         inc %edi                // Skip flags+len byte.
1594         andb $F_LENMASK,%al     // Just the length, not the flags.
1595         add %eax,%edi           // Skip the name.
1596         addl $3,%edi            // The codeword is 4-byte aligned.
1597         andl $~3,%edi
1598         ret
1599 
1600 /*
1601         Related to >CFA is >DFA which takes a dictionary entry address as returned by FIND and
1602         returns a pointer to the first data field.
1603 
1604         FIND returns a pointer to this
1605         |                               >CFA converts it to a pointer to this
1606         |                                          |
1607         |                                          |    >DFA converts it to a pointer to this
1608         |                                          |             |
1609         V                                          V             V
1610         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1611         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        | +          | EXIT       |
1612         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1613                                                    codeword
1614 
1615         (Note to those following the source of FIG-FORTH / ciforth: My >DFA definition is
1616         different from theirs, because they have an extra indirection).
1617 
1618         You can see that >DFA is easily defined in FORTH just by adding 4 to the result of >CFA.
1619 */
1620 
1621         defword ">DFA",4,,TDFA
1622         .int TCFA               // >CFA         (get code field address)
1623         .int INCR4              // 4+           (add 4 to it to get to next word)
1624         .int EXIT               // EXIT         (return from FORTH word)
1625 
1626 /*
1627         COMPILING ----------------------------------------------------------------------
1628 
1629         Now we'll talk about how FORTH compiles words.  Recall that a word definition looks like this:
1630 
1631                 : DOUBLE DUP + ;
1632 
1633         and we have to turn this into:
1634 
1635           pointer to previous word
1636            ^
1637            |
1638         +--|------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1639         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        | +          | EXIT       |
1640         +---------+---+---+---+---+---+---+---+---+------------+--|---------+------------+------------+
1641            ^       len                         pad  codeword      |
1642            |                                                      V
1643           LATEST points here                            points to codeword of DUP
1644 
1645         There are several problems to solve.  Where to put the new word?  How do we read words?  How
1646         do we define the words : (COLON) and ; (SEMICOLON)?
1647 
1648         FORTH solves this rather elegantly and as you might expect in a very low-level way which
1649         allows you to change how the compiler works on your own code.
1650 
1651         FORTH has an INTERPRET function (a true interpreter this time, not DOCOL) which runs in a
1652         loop, reading words (using WORD), looking them up (using FIND), turning them into codeword
1653         pointers (using >CFA) and deciding what to do with them.
1654 
1655         What it does depends on the mode of the interpreter (in variable STATE).
1656 
1657         When STATE is zero, the interpreter just runs each word as it looks them up.  This is known as
1658         immediate mode.
1659 
1660         The interesting stuff happens when STATE is non-zero -- compiling mode.  In this mode the
1661         interpreter appends the codeword pointer to user memory (the HERE variable points to the next
1662         free byte of user memory -- see DATA SEGMENT section below).
1663 
1664         So you may be able to see how we could define : (COLON).  The general plan is:
1665 
1666         (1) Use WORD to read the name of the function being defined.
1667 
1668         (2) Construct the dictionary entry -- just the header part -- in user memory:
1669 
1670     pointer to previous word (from LATEST)                      +-- Afterwards, HERE points here, where
1671            ^                                                    |   the interpreter will start appending
1672            |                                                    V   codewords.
1673         +--|------+---+---+---+---+---+---+---+---+------------+
1674         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      |
1675         +---------+---+---+---+---+---+---+---+---+------------+
1676                    len                         pad  codeword
1677 
1678         (3) Set LATEST to point to the newly defined word, ...
1679 
1680         (4) .. and most importantly leave HERE pointing just after the new codeword.  This is where
1681             the interpreter will append codewords.
1682 
1683         (5) Set STATE to 1.  This goes into compile mode so the interpreter starts appending codewords to
1684             our partially-formed header.
1685 
1686         After : has run, our input is here:
1687 
1688         : DOUBLE DUP + ;
1689                  ^
1690                  |
1691                 Next byte returned by KEY will be the 'D' character of DUP
1692 
1693         so the interpreter (now it's in compile mode, so I guess it's really the compiler) reads "DUP",
1694         looks it up in the dictionary, gets its codeword pointer, and appends it:
1695 
1696                                                                              +-- HERE updated to point here.
1697                                                                              |
1698                                                                              V
1699         +---------+---+---+---+---+---+---+---+---+------------+------------+
1700         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        |
1701         +---------+---+---+---+---+---+---+---+---+------------+------------+
1702                    len                         pad  codeword
1703 
1704         Next we read +, get the codeword pointer, and append it:
1705 
1706                                                                                           +-- HERE updated to point here.
1707                                                                                           |
1708                                                                                           V
1709         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+
1710         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        | +          |
1711         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+
1712                    len                         pad  codeword
1713 
1714         The issue is what happens next.  Obviously what we _don't_ want to happen is that we
1715         read ";" and compile it and go on compiling everything afterwards.
1716 
1717         At this point, FORTH uses a trick.  Remember the length byte in the dictionary definition
1718         isn't just a plain length byte, but can also contain flags.  One flag is called the
1719         IMMEDIATE flag (F_IMMED in this code).  If a word in the dictionary is flagged as
1720         IMMEDIATE then the interpreter runs it immediately _even if it's in compile mode_.
1721 
1722         This is how the word ; (SEMICOLON) works -- as a word flagged in the dictionary as IMMEDIATE.
1723 
1724         And all it does is append the codeword for EXIT on to the current definition and switch
1725         back to immediate mode (set STATE back to 0).  Shortly we'll see the actual definition
1726         of ; and we'll see that it's really a very simple definition, declared IMMEDIATE.
1727 
1728         After the interpreter reads ; and executes it 'immediately', we get this:
1729 
1730         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1731         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        | +          | EXIT       |
1732         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1733                    len                         pad  codeword                                           ^
1734                                                                                                        |
1735                                                                                                       HERE
1736         STATE is set to 0.
1737 
1738         And that's it, job done, our new definition is compiled, and we're back in immediate mode
1739         just reading and executing words, perhaps including a call to test our new word DOUBLE.
1740 
1741         The only last wrinkle in this is that while our word was being compiled, it was in a
1742         half-finished state.  We certainly wouldn't want DOUBLE to be called somehow during
1743         this time.  There are several ways to stop this from happening, but in FORTH what we
1744         do is flag the word with the HIDDEN flag (F_HIDDEN in this code) just while it is
1745         being compiled.  This prevents FIND from finding it, and thus in theory stops any
1746         chance of it being called.
1747 
1748         The above explains how compiling, : (COLON) and ; (SEMICOLON) works and in a moment I'm
1749         going to define them.  The : (COLON) function can be made a little bit more general by writing
1750         it in two parts.  The first part, called CREATE, makes just the header:
1751 
1752                                                    +-- Afterwards, HERE points here.
1753                                                    |
1754                                                    V
1755         +---------+---+---+---+---+---+---+---+---+
1756         | LINK    | 6 | D | O | U | B | L | E | 0 |
1757         +---------+---+---+---+---+---+---+---+---+
1758                    len                         pad
1759 
1760         and the second part, the actual definition of : (COLON), calls CREATE and appends the
1761         DOCOL codeword, so leaving:
1762 
1763                                                                 +-- Afterwards, HERE points here.
1764                                                                 |
1765                                                                 V
1766         +---------+---+---+---+---+---+---+---+---+------------+
1767         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      |
1768         +---------+---+---+---+---+---+---+---+---+------------+
1769                    len                         pad  codeword
1770 
1771         CREATE is a standard FORTH word and the advantage of this split is that we can reuse it to
1772         create other types of words (not just ones which contain code, but words which contain variables,
1773         constants and other data).
1774 */
1775 
1776         defcode "CREATE",6,,CREATE
1777 
1778         // Get the name length and address.
1779         pop %ecx                // %ecx = length
1780         pop %ebx                // %ebx = address of name
1781 
1782         // Link pointer.
1783         movl var_HERE,%edi      // %edi is the address of the header
1784         movl var_LATEST,%eax    // Get link pointer
1785         stosl                   // and store it in the header.
1786 
1787         // Length byte and the word itself.
1788         mov %cl,%al             // Get the length.
1789         stosb                   // Store the length/flags byte.
1790         push %esi
1791         mov %ebx,%esi           // %esi = word
1792         rep movsb               // Copy the word
1793         pop %esi
1794         addl $3,%edi            // Align to next 4 byte boundary.
1795         andl $~3,%edi
1796 
1797         // Update LATEST and HERE.
1798         movl var_HERE,%eax
1799         movl %eax,var_LATEST
1800         movl %edi,var_HERE
1801         NEXT
1802 
1803 /*
1804         Because I want to define : (COLON) in FORTH, not assembler, we need a few more FORTH words
1805         to use.
1806 
1807         The first is , (COMMA) which is a standard FORTH word which appends a 32 bit integer to the user
1808         memory pointed to by HERE, and adds 4 to HERE.  So the action of , (COMMA) is:
1809 
1810                                                         previous value of HERE
1811                                                                  |
1812                                                                  V
1813         +---------+---+---+---+---+---+---+---+---+-- - - - - --+------------+
1814         | LINK    | 6 | D | O | U | B | L | E | 0 |             |  <data>    |
1815         +---------+---+---+---+---+---+---+---+---+-- - - - - --+------------+
1816                    len                         pad                            ^
1817                                                                               |
1818                                                                         new value of HERE
1819 
1820         and <data> is whatever 32 bit integer was at the top of the stack.
1821 
1822         , (COMMA) is quite a fundamental operation when compiling.  It is used to append codewords
1823         to the current word that is being compiled.
1824 */
1825 
1826         defcode ",",1,,COMMA
1827         pop %eax                // Code pointer to store.
1828         call _COMMA
1829         NEXT
1830 _COMMA:
1831         movl var_HERE,%edi      // HERE
1832         stosl                   // Store it.
1833         movl %edi,var_HERE      // Update HERE (incremented)
1834         ret
1835 
1836 /*
1837         Our definitions of : (COLON) and ; (SEMICOLON) will need to switch to and from compile mode.
1838 
1839         Immediate mode vs. compile mode is stored in the global variable STATE, and by updating this
1840         variable we can switch between the two modes.
1841 
1842         For various reasons which may become apparent later, FORTH defines two standard words called
1843         [ and ] (LBRAC and RBRAC) which switch between modes:
1844 
1845         Word    Assembler       Action          Effect
1846         [       LBRAC           STATE := 0      Switch to immediate mode.
1847         ]       RBRAC           STATE := 1      Switch to compile mode.
1848 
1849         [ (LBRAC) is an IMMEDIATE word.  The reason is as follows: If we are in compile mode and the
1850         interpreter saw [ then it would compile it rather than running it.  We would never be able to
1851         switch back to immediate mode!  So we flag the word as IMMEDIATE so that even in compile mode
1852         the word runs immediately, switching us back to immediate mode.
1853 */
1854 
1855         defcode "[",1,F_IMMED,LBRAC
1856         xor %eax,%eax
1857         movl %eax,var_STATE     // Set STATE to 0.
1858         NEXT
1859 
1860         defcode "]",1,,RBRAC
1861         movl $1,var_STATE       // Set STATE to 1.
1862         NEXT
1863 
1864 /*
1865         Now we can define : (COLON) using CREATE.  It just calls CREATE, appends DOCOL (the codeword), sets
1866         the word HIDDEN and goes into compile mode.
1867 */
1868 
1869         defword ":",1,,COLON
1870         .int WORD               // Get the name of the new word
1871         .int CREATE             // CREATE the dictionary entry / header
1872         .int LIT, DOCOL, COMMA  // Append DOCOL  (the codeword).
1873         .int LATEST, FETCH, HIDDEN // Make the word hidden (see below for definition).
1874         .int RBRAC              // Go into compile mode.
1875         .int EXIT               // Return from the function.
1876 
1877 /*
1878         ; (SEMICOLON) is also elegantly simple.  Notice the F_IMMED flag.
1879 */
1880 
1881         defword ";",1,F_IMMED,SEMICOLON
1882         .int LIT, EXIT, COMMA   // Append EXIT (so the word will return).
1883         .int LATEST, FETCH, HIDDEN // Toggle hidden flag -- unhide the word (see below for definition).
1884         .int LBRAC              // Go back to IMMEDIATE mode.
1885         .int EXIT               // Return from the function.
1886 
1887 /*
1888         EXTENDING THE COMPILER ----------------------------------------------------------------------
1889 
1890         Words flagged with IMMEDIATE (F_IMMED) aren't just for the FORTH compiler to use.  You can define
1891         your own IMMEDIATE words too, and this is a crucial aspect when extending basic FORTH, because
1892         it allows you in effect to extend the compiler itself.  Does gcc let you do that?
1893 
1894         Standard FORTH words like IF, WHILE, ." and so on are all written as extensions to the basic
1895         compiler, and are all IMMEDIATE words.
1896 
1897         The IMMEDIATE word toggles the F_IMMED (IMMEDIATE flag) on the most recently defined word,
1898         or on the current word if you call it in the middle of a definition.
1899 
1900         Typical usage is:
1901 
1902         : MYIMMEDWORD IMMEDIATE
1903                 ...definition...
1904         ;
1905 
1906         but some FORTH programmers write this instead:
1907 
1908         : MYIMMEDWORD
1909                 ...definition...
1910         ; IMMEDIATE
1911 
1912         The two usages are equivalent, to a first approximation.
1913 */
1914 
1915         defcode "IMMEDIATE",9,F_IMMED,IMMEDIATE
1916         movl var_LATEST,%edi    // LATEST word.
1917         addl $4,%edi            // Point to name/flags byte.
1918         xorb $F_IMMED,(%edi)    // Toggle the IMMED bit.
1919         NEXT
1920 
1921 /*
1922         'addr HIDDEN' toggles the hidden flag (F_HIDDEN) of the word defined at addr.  To hide the
1923         most recently defined word (used above in : and ; definitions) you would do:
1924 
1925                 LATEST @ HIDDEN
1926 
1927         'HIDE word' toggles the flag on a named 'word'.
1928 
1929         Setting this flag stops the word from being found by FIND, and so can be used to make 'private'
1930         words.  For example, to break up a large word into smaller parts you might do:
1931 
1932                 : SUB1 ... subword ... ;
1933                 : SUB2 ... subword ... ;
1934                 : SUB3 ... subword ... ;
1935                 : MAIN ... defined in terms of SUB1, SUB2, SUB3 ... ;
1936                 HIDE SUB1
1937                 HIDE SUB2
1938                 HIDE SUB3
1939 
1940         After this, only MAIN is 'exported' or seen by the rest of the program.
1941 */
1942 
1943         defcode "HIDDEN",6,,HIDDEN
1944         pop %edi                // Dictionary entry.
1945         addl $4,%edi            // Point to name/flags byte.
1946         xorb $F_HIDDEN,(%edi)   // Toggle the HIDDEN bit.
1947         NEXT
1948 
1949         defword "HIDE",4,,HIDE
1950         .int WORD               // Get the word (after HIDE).
1951         .int FIND               // Look up in the dictionary.
1952         .int HIDDEN             // Set F_HIDDEN flag.
1953         .int EXIT               // Return.
1954 
1955 /*
1956         ' (TICK) is a standard FORTH word which returns the codeword pointer of the next word.
1957 
1958         The common usage is:
1959 
1960         ' FOO ,
1961 
1962         which appends the codeword of FOO to the current word we are defining (this only works in compiled code).
1963 
1964         You tend to use ' in IMMEDIATE words.  For example an alternate (and rather useless) way to define
1965         a literal 2 might be:
1966 
1967         : LIT2 IMMEDIATE
1968                 ' LIT ,         \ Appends LIT to the currently-being-defined word
1969                 2 ,             \ Appends the number 2 to the currently-being-defined word
1970         ;
1971 
1972         So you could do:
1973 
1974         : DOUBLE LIT2 * ;
1975 
1976         (If you don't understand how LIT2 works, then you should review the material about compiling words
1977         and immediate mode).
1978 
1979         This definition of ' uses a cheat which I copied from buzzard92.  As a result it only works in
1980         compiled code.  It is possible to write a version of ' based on WORD, FIND, >CFA which works in
1981         immediate mode too.
1982 */
1983         defcode "'",1,,TICK
1984         lodsl                   // Get the address of the next word and skip it.
1985         pushl %eax              // Push it on the stack.
1986         NEXT
1987 
1988 /*
1989         BRANCHING ----------------------------------------------------------------------
1990 
1991         It turns out that all you need in order to define looping constructs, IF-statements, etc.
1992         are two primitives.
1993 
1994         BRANCH is an unconditional branch. 0BRANCH is a conditional branch (it only branches if the
1995         top of stack is zero).
1996 
1997         The diagram below shows how BRANCH works in some imaginary compiled word.  When BRANCH executes,
1998         %esi starts by pointing to the offset field (compare to LIT above):
1999 
2000         +---------------------+-------+---- - - ---+------------+------------+---- - - - ----+------------+
2001         | (Dictionary header) | DOCOL |            | BRANCH     | offset     | (skipped)     | word       |
2002         +---------------------+-------+---- - - ---+------------+-----|------+---- - - - ----+------------+
2003                                                                    ^  |                       ^
2004                                                                    |  |                       |
2005                                                                    |  +-----------------------+
2006                                                                   %esi added to offset
2007 
2008         The offset is added to %esi to make the new %esi, and the result is that when NEXT runs, execution
2009         continues at the branch target.  Negative offsets work as expected.
2010 
2011         0BRANCH is the same except the branch happens conditionally.
2012 
2013         Now standard FORTH words such as IF, THEN, ELSE, WHILE, REPEAT, etc. can be implemented entirely
2014         in FORTH.  They are IMMEDIATE words which append various combinations of BRANCH or 0BRANCH
2015         into the word currently being compiled.
2016 
2017         As an example, code written like this:
2018 
2019                 condition-code IF true-part THEN rest-code
2020 
2021         compiles to:
2022 
2023                 condition-code 0BRANCH OFFSET true-part rest-code
2024                                           |             ^
2025                                           |             |
2026                                           +-------------+
2027 */
2028 
2029         defcode "BRANCH",6,,BRANCH
2030         add (%esi),%esi         // add the offset to the instruction pointer
2031         NEXT
2032 
2033         defcode "0BRANCH",7,,ZBRANCH
2034         pop %eax
2035         test %eax,%eax          // top of stack is zero?
2036         jz code_BRANCH          // if so, jump back to the branch function above
2037         lodsl                   // otherwise we need to skip the offset
2038         NEXT
2039 
2040 /*
2041         LITERAL STRINGS ----------------------------------------------------------------------
2042 
2043         LITSTRING is a primitive used to implement the ." and S" operators (which are written in
2044         FORTH).  See the definition of those operators later.
2045 
2046         TELL just prints a string.  It's more efficient to define this in assembly because we
2047         can make it a single Linux syscall.
2048 */
2049 
2050         defcode "LITSTRING",9,,LITSTRING
2051         lodsl                   // get the length of the string
2052         push %esi               // push the address of the start of the string
2053         push %eax               // push it on the stack
2054         addl %eax,%esi          // skip past the string
2055         addl $3,%esi            // but round up to next 4 byte boundary
2056         andl $~3,%esi
2057         NEXT
2058 
2059         defcode "TELL",4,,TELL
2060         mov $1,%ebx             // 1st param: stdout
2061         pop %edx                // 3rd param: length of string
2062         pop %ecx                // 2nd param: address of string
2063         mov $__NR_write,%eax    // write syscall
2064         int $0x80
2065         NEXT
2066 
2067 /*
2068         QUIT AND INTERPRET ----------------------------------------------------------------------
2069 
2070         QUIT is the first FORTH function called, almost immediately after the FORTH system "boots".
2071         As explained before, QUIT doesn't "quit" anything.  It does some initialisation (in particular
2072         it clears the return stack) and it calls INTERPRET in a loop to interpret commands.  The
2073         reason it is called QUIT is because you can call it from your own FORTH words in order to
2074         "quit" your program and start again at the user prompt.
2075 
2076         INTERPRET is the FORTH interpreter ("toploop", "toplevel" or "REPL" might be a more accurate
2077         description -- see: http://en.wikipedia.org/wiki/REPL).
2078 */
2079 
2080         // QUIT must not return (ie. must not call EXIT).
2081         defword "QUIT",4,,QUIT
2082         .int RZ,RSPSTORE        // R0 RSP!, clear the return stack
2083         .int INTERPRET          // interpret the next word
2084         .int BRANCH,-8          // and loop (indefinitely)
2085 
2086 /*
2087         This interpreter is pretty simple, but remember that in FORTH you can always override
2088         it later with a more powerful one!
2089  */
2090         defcode "INTERPRET",9,,INTERPRET
2091         call _WORD              // Returns %ecx = length, %edi = pointer to word.
2092 
2093         // Is it in the dictionary?
2094         xor %eax,%eax
2095         movl %eax,interpret_is_lit // Not a literal number (not yet anyway ...)
2096         call _FIND              // Returns %eax = pointer to header or 0 if not found.
2097         test %eax,%eax          // Found?
2098         jz 1f
2099 
2100         // In the dictionary.  Is it an IMMEDIATE codeword?
2101         mov %eax,%edi           // %edi = dictionary entry
2102         movb 4(%edi),%al        // Get name+flags.
2103         push %ax                // Just save it for now.
2104         call _TCFA              // Convert dictionary entry (in %edi) to codeword pointer.
2105         pop %ax
2106         andb $F_IMMED,%al       // Is IMMED flag set?
2107         mov %edi,%eax
2108         jnz 4f                  // If IMMED, jump straight to executing.
2109 
2110         jmp 2f
2111 
2112 1:      // Not in the dictionary (not a word) so assume it's a literal number.
2113         incl interpret_is_lit
2114         call _NUMBER            // Returns the parsed number in %eax, %ecx > 0 if error
2115         test %ecx,%ecx
2116         jnz 6f
2117         mov %eax,%ebx
2118         mov $LIT,%eax           // The word is LIT
2119 
2120 2:      // Are we compiling or executing?
2121         movl var_STATE,%edx
2122         test %edx,%edx
2123         jz 4f                   // Jump if executing.
2124 
2125         // Compiling - just append the word to the current dictionary definition.
2126         call _COMMA
2127         mov interpret_is_lit,%ecx // Was it a literal?
2128         test %ecx,%ecx
2129         jz 3f
2130         mov %ebx,%eax           // Yes, so LIT is followed by a number.
2131         call _COMMA
2132 3:      NEXT
2133 
2134 4:      // Executing - run it!
2135         mov interpret_is_lit,%ecx // Literal?
2136         test %ecx,%ecx          // Literal?
2137         jnz 5f
2138 
2139         // Not a literal, execute it now.  This never returns, but the codeword will
2140         // eventually call NEXT which will reenter the loop in QUIT.
2141         jmp *(%eax)
2142 
2143 5:      // Executing a literal, which means push it on the stack.
2144         push %ebx
2145         NEXT
2146 
2147 6:      // Parse error (not a known word or a number in the current BASE).
2148         // Print an error message followed by up to 40 characters of context.
2149         mov $2,%ebx             // 1st param: stderr
2150         mov $errmsg,%ecx        // 2nd param: error message
2151         mov $errmsgend-errmsg,%edx // 3rd param: length of string
2152         mov $__NR_write,%eax    // write syscall
2153         int $0x80
2154 
2155         mov (currkey),%ecx      // the error occurred just before currkey position
2156         mov %ecx,%edx
2157         sub $buffer,%edx        // %edx = currkey - buffer (length in buffer before currkey)
2158         cmp $40,%edx            // if > 40, then print only 40 characters
2159         jle 7f
2160         mov $40,%edx
2161 7:      sub %edx,%ecx           // %ecx = start of area to print, %edx = length
2162         mov $__NR_write,%eax    // write syscall
2163         int $0x80
2164 
2165         mov $errmsgnl,%ecx      // newline
2166         mov $1,%edx
2167         mov $__NR_write,%eax    // write syscall
2168         int $0x80
2169 
2170         NEXT
2171 
2172         .section .rodata
2173 errmsg: .ascii "PARSE ERROR: "
2174 errmsgend:
2175 errmsgnl: .ascii "\n"
2176 
2177         .data                   // NB: easier to fit in the .data section
2178         .align 4
2179 interpret_is_lit:
2180         .int 0                  // Flag used to record if reading a literal
2181 
2182 /*
2183         ODDS AND ENDS ----------------------------------------------------------------------
2184 
2185         CHAR puts the ASCII code of the first character of the following word on the stack.  For example
2186         CHAR A puts 65 on the stack.
2187 
2188         EXECUTE is used to run execution tokens.  See the discussion of execution tokens in the
2189         FORTH code for more details.
2190 
2191         SYSCALL0, SYSCALL1, SYSCALL2, SYSCALL3 make a standard Linux system call.  (See <asm/unistd.h>
2192         for a list of system call numbers).  As their name suggests these forms take between 0 and 3
2193         syscall parameters, plus the system call number.
2194 
2195         In this FORTH, SYSCALL0 must be the last word in the built-in (assembler) dictionary because we
2196         initialise the LATEST variable to point to it.  This means that if you want to extend the assembler
2197         part, you must put new words before SYSCALL0, or else change how LATEST is initialised.
2198 */
2199 
2200         defcode "CHAR",4,,CHAR
2201         call _WORD              // Returns %ecx = length, %edi = pointer to word.
2202         xor %eax,%eax
2203         movb (%edi),%al         // Get the first character of the word.
2204         push %eax               // Push it onto the stack.
2205         NEXT
2206 
2207         defcode "EXECUTE",7,,EXECUTE
2208         pop %eax                // Get xt into %eax
2209         jmp *(%eax)             // and jump to it.
2210                                 // After xt runs its NEXT will continue executing the current word.
2211 
2212         defcode "SYSCALL3",8,,SYSCALL3
2213         pop %eax                // System call number (see <asm/unistd.h>)
2214         pop %ebx                // First parameter.
2215         pop %ecx                // Second parameter
2216         pop %edx                // Third parameter
2217         int $0x80
2218         push %eax               // Result (negative for -errno)
2219         NEXT
2220 
2221         defcode "SYSCALL2",8,,SYSCALL2
2222         pop %eax                // System call number (see <asm/unistd.h>)
2223         pop %ebx                // First parameter.
2224         pop %ecx                // Second parameter
2225         int $0x80
2226         push %eax               // Result (negative for -errno)
2227         NEXT
2228 
2229         defcode "SYSCALL1",8,,SYSCALL1
2230         pop %eax                // System call number (see <asm/unistd.h>)
2231         pop %ebx                // First parameter.
2232         int $0x80
2233         push %eax               // Result (negative for -errno)
2234         NEXT
2235 
2236         defcode "SYSCALL0",8,,SYSCALL0
2237         pop %eax                // System call number (see <asm/unistd.h>)
2238         int $0x80
2239         push %eax               // Result (negative for -errno)
2240         NEXT
2241 
2242 /*
2243         DATA SEGMENT ----------------------------------------------------------------------
2244 
2245         Here we set up the Linux data segment, used for user definitions and variously known as just
2246         the 'data segment', 'user memory' or 'user definitions area'.  It is an area of memory which
2247         grows upwards and stores both newly-defined FORTH words and global variables of various
2248         sorts.
2249 
2250         It is completely analogous to the C heap, except there is no generalised 'malloc' and 'free'
2251         (but as with everything in FORTH, writing such functions would just be a Simple Matter
2252         Of Programming).  Instead in normal use the data segment just grows upwards as new FORTH
2253         words are defined/appended to it.
2254 
2255         There are various "features" of the GNU toolchain which make setting up the data segment
2256         more complicated than it really needs to be.  One is the GNU linker which inserts a random
2257         "build ID" segment.  Another is Address Space Randomization which means we can't tell
2258         where the kernel will choose to place the data segment (or the stack for that matter).
2259 
2260         Therefore writing this set_up_data_segment assembler routine is a little more complicated
2261         than it really needs to be.  We ask the Linux kernel where it thinks the data segment starts
2262         using the brk(2) system call, then ask it to reserve some initial space (also using brk(2)).
2263 
2264         You don't need to worry about this code.
2265 */
2266         .text
2267         .set INITIAL_DATA_SEGMENT_SIZE,65536
2268 set_up_data_segment:
2269         xor %ebx,%ebx           // Call brk(0)
2270         movl $__NR_brk,%eax
2271         int $0x80
2272         movl %eax,var_HERE      // Initialise HERE to point at beginning of data segment.
2273         addl $INITIAL_DATA_SEGMENT_SIZE,%eax    // Reserve nn bytes of memory for initial data segment.
2274         movl %eax,%ebx          // Call brk(HERE+INITIAL_DATA_SEGMENT_SIZE)
2275         movl $__NR_brk,%eax
2276         int $0x80
2277         ret
2278 
2279 /*
2280         We allocate static buffers for the return static and input buffer (used when
2281         reading in files and text that the user types in).
2282 */
2283         .set RETURN_STACK_SIZE,8192
2284         .set BUFFER_SIZE,4096
2285 
2286         .bss
2287 /* FORTH return stack. */
2288         .align 4096
2289 return_stack:
2290         .space RETURN_STACK_SIZE
2291 return_stack_top:               // Initial top of return stack.
2292 
2293 /* This is used as a temporary input buffer when reading from files or the terminal. */
2294         .align 4096
2295 buffer:
2296         .space BUFFER_SIZE
2297 
2298 /*
2299         START OF FORTH CODE ----------------------------------------------------------------------
2300 
2301         We've now reached the stage where the FORTH system is running and self-hosting.  All further
2302         words can be written as FORTH itself, including words like IF, THEN, .", etc which in most
2303         languages would be considered rather fundamental.
2304 
2305         I used to append this here in the assembly file, but I got sick of fighting against gas's
2306         crack-smoking (lack of) multiline string syntax.  So now that is in a separate file called
2307         jonesforth.f
2308 
2309         If you don't already have that file, download it from http://annexia.org/forth in order
2310         to continue the tutorial.
2311 */
2312 
2313 /* END OF jonesforth.S */
JONESFORTH - A sometimes minimal FORTH compiler and tutorialRSSAtom

