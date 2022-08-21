   1 \ -*- text -*-
   2 \       A sometimes minimal FORTH compiler and tutorial for Linux / i386 systems. -*- asm -*-
   3 \       By Richard W.M. Jones <rich@annexia.org> http://annexia.org/forth
   4 \       This is PUBLIC DOMAIN (see public domain release statement below).
   5 \       $Id: jonesforth.f,v 1.18 2009-09-11 08:32:33 rich Exp $
   6 \
   7 \       The first part of this tutorial is in jonesforth.S.  Get if from http://annexia.org/forth
   8 \
   9 \       PUBLIC DOMAIN ----------------------------------------------------------------------
  10 \
  11 \       I, the copyright holder of this work, hereby release it into the public domain. This applies worldwide.
  12 \
  13 \       In case this is not legally possible, I grant any entity the right to use this work for any purpose,
  14 \       without any conditions, unless such conditions are required by law.
  15 \
  16 \       SETTING UP ----------------------------------------------------------------------
  17 \
  18 \       Let's get a few housekeeping things out of the way.  Firstly because I need to draw lots of
  19 \       ASCII-art diagrams to explain concepts, the best way to look at this is using a window which
  20 \       uses a fixed width font and is at least this wide:
  21 \
  22 \<------------------------------------------------------------------------------------------------------------------------>
  23 \
  24 \       Secondly make sure TABS are set to 8 characters.  The following should be a vertical
  25 \       line.  If not, sort out your tabs.
  26 \
  27 \               |
  28 \               |
  29 \               |
  30 \
  31 \       Thirdly I assume that your screen is at least 50 characters high.
  32 \
  33 \       START OF FORTH CODE ----------------------------------------------------------------------
  34 \
  35 \       We've now reached the stage where the FORTH system is running and self-hosting.  All further
  36 \       words can be written as FORTH itself, including words like IF, THEN, .", etc which in most
  37 \       languages would be considered rather fundamental.
  38 \
  39 \       Some notes about the code:
  40 \
  41 \       I use indenting to show structure.  The amount of whitespace has no meaning to FORTH however
  42 \       except that you must use at least one whitespace character between words, and words themselves
  43 \       cannot contain whitespace.
  44 \
  45 \       FORTH is case-sensitive.  Use capslock!
  46
  47 \ The primitive word /MOD (DIVMOD) leaves both the quotient and the remainder on the stack.  (On
  48 \ i386, the idivl instruction gives both anyway).  Now we can define the / and MOD in terms of /MOD
  49 \ and a few other primitives.
  50 : / /MOD SWAP DROP ;
  51 : MOD /MOD DROP ;
  52
  53 \ Define some character constants
  54 : '\n' 10 ;
  55 : BL   32 ; \ BL (BLank) is a standard FORTH word for space.
  56
  57 \ CR prints a carriage return
  58 : CR '\n' EMIT ;
  59
  60 \ SPACE prints a space
  61 : SPACE BL EMIT ;
  62
  63 \ NEGATE leaves the negative of a number on the stack.
  64 : NEGATE 0 SWAP - ;
  65
  66 \ Standard words for booleans.
  67 : TRUE  1 ;
  68 : FALSE 0 ;
  69 : NOT   0= ;
  70
  71 \ LITERAL takes whatever is on the stack and compiles LIT <foo>
  72 : LITERAL IMMEDIATE
  73         ' LIT ,         \ compile LIT
  74         ,               \ compile the literal itself (from the stack)
  75         ;
  76
  77 \ Now we can use [ and ] to insert literals which are calculated at compile time.  (Recall that
  78 \ [ and ] are the FORTH words which switch into and out of immediate mode.)
  79 \ Within definitions, use [ ... ] LITERAL anywhere that '...' is a constant expression which you
  80 \ would rather only compute once (at compile time, rather than calculating it each time your word runs).
  81 : ':'
  82         [               \ go into immediate mode (temporarily)
  83         CHAR :          \ push the number 58 (ASCII code of colon) on the parameter stack
  84         ]               \ go back to compile mode
  85         LITERAL         \ compile LIT 58 as the definition of ':' word
  86 ;
  87
  88 \ A few more character constants defined the same way as above.
  89 : ';' [ CHAR ; ] LITERAL ;
  90 : '(' [ CHAR ( ] LITERAL ;
  91 : ')' [ CHAR ) ] LITERAL ;
  92 : '"' [ CHAR " ] LITERAL ;
  93 : 'A' [ CHAR A ] LITERAL ;
  94 : '0' [ CHAR 0 ] LITERAL ;
  95 : '-' [ CHAR - ] LITERAL ;
  96 : '.' [ CHAR . ] LITERAL ;
  97
  98 \ While compiling, '[COMPILE] word' compiles 'word' if it would otherwise be IMMEDIATE.
  99 : [COMPILE] IMMEDIATE
 100         WORD            \ get the next word
 101         FIND            \ find it in the dictionary
 102         >CFA            \ get its codeword
 103         ,               \ and compile that
 104 ;
 105
 106 \ RECURSE makes a recursive call to the current word that is being compiled.
 107 \
 108 \ Normally while a word is being compiled, it is marked HIDDEN so that references to the
 109 \ same word within are calls to the previous definition of the word.  However we still have
 110 \ access to the word which we are currently compiling through the LATEST pointer so we
 111 \ can use that to compile a recursive call.
 112 : RECURSE IMMEDIATE
 113         LATEST @        \ LATEST points to the word being compiled at the moment
 114         >CFA            \ get the codeword
 115         ,               \ compile it
 116 ;
 117
 118 \       CONTROL STRUCTURES ----------------------------------------------------------------------
 119 \
 120 \ So far we have defined only very simple definitions.  Before we can go further, we really need to
 121 \ make some control structures, like IF ... THEN and loops.  Luckily we can define arbitrary control
 122 \ structures directly in FORTH.
 123 \
 124 \ Please note that the control structures as I have defined them here will only work inside compiled
 125 \ words.  If you try to type in expressions using IF, etc. in immediate mode, then they won't work.
 126 \ Making these work in immediate mode is left as an exercise for the reader.
 127
 128 \ condition IF true-part THEN rest
 129 \       -- compiles to: --> condition 0BRANCH OFFSET true-part rest
 130 \       where OFFSET is the offset of 'rest'
 131 \ condition IF true-part ELSE false-part THEN
 132 \       -- compiles to: --> condition 0BRANCH OFFSET true-part BRANCH OFFSET2 false-part rest
 133 \       where OFFSET if the offset of false-part and OFFSET2 is the offset of rest
 134
 135 \ IF is an IMMEDIATE word which compiles 0BRANCH followed by a dummy offset, and places
 136 \ the address of the 0BRANCH on the stack.  Later when we see THEN, we pop that address
 137 \ off the stack, calculate the offset, and back-fill the offset.
 138 : IF IMMEDIATE
 139         ' 0BRANCH ,     \ compile 0BRANCH
 140         HERE @          \ save location of the offset on the stack
 141         0 ,             \ compile a dummy offset
 142 ;
 143
 144 : THEN IMMEDIATE
 145         DUP
 146         HERE @ SWAP -   \ calculate the offset from the address saved on the stack
 147         SWAP !          \ store the offset in the back-filled location
 148 ;
 149
 150 : ELSE IMMEDIATE
 151         ' BRANCH ,      \ definite branch to just over the false-part
 152         HERE @          \ save location of the offset on the stack
 153         0 ,             \ compile a dummy offset
 154         SWAP            \ now back-fill the original (IF) offset
 155         DUP             \ same as for THEN word above
 156         HERE @ SWAP -
 157         SWAP !
 158 ;
 159
 160 \ BEGIN loop-part condition UNTIL
 161 \       -- compiles to: --> loop-part condition 0BRANCH OFFSET
 162 \       where OFFSET points back to the loop-part
 163 \ This is like do { loop-part } while (condition) in the C language
 164 : BEGIN IMMEDIATE
 165         HERE @          \ save location on the stack
 166 ;
 167
 168 : UNTIL IMMEDIATE
 169         ' 0BRANCH ,     \ compile 0BRANCH
 170         HERE @ -        \ calculate the offset from the address saved on the stack
 171         ,               \ compile the offset here
 172 ;
 173
 174 \ BEGIN loop-part AGAIN
 175 \       -- compiles to: --> loop-part BRANCH OFFSET
 176 \       where OFFSET points back to the loop-part
 177 \ In other words, an infinite loop which can only be returned from with EXIT
 178 : AGAIN IMMEDIATE
 179         ' BRANCH ,      \ compile BRANCH
 180         HERE @ -        \ calculate the offset back
 181         ,               \ compile the offset here
 182 ;
 183
 184 \ BEGIN condition WHILE loop-part REPEAT
 185 \       -- compiles to: --> condition 0BRANCH OFFSET2 loop-part BRANCH OFFSET
 186 \       where OFFSET points back to condition (the beginning) and OFFSET2 points to after the whole piece of code
 187 \ So this is like a while (condition) { loop-part } loop in the C language
 188 : WHILE IMMEDIATE
 189         ' 0BRANCH ,     \ compile 0BRANCH
 190         HERE @          \ save location of the offset2 on the stack
 191         0 ,             \ compile a dummy offset2
 192 ;
 193
 194 : REPEAT IMMEDIATE
 195         ' BRANCH ,      \ compile BRANCH
 196         SWAP            \ get the original offset (from BEGIN)
 197         HERE @ - ,      \ and compile it after BRANCH
 198         DUP
 199         HERE @ SWAP -   \ calculate the offset2
 200         SWAP !          \ and back-fill it in the original location
 201 ;
 202
 203 \ UNLESS is the same as IF but the test is reversed.
 204 \
 205 \ Note the use of [COMPILE]: Since IF is IMMEDIATE we don't want it to be executed while UNLESS
 206 \ is compiling, but while UNLESS is running (which happens to be when whatever word using UNLESS is
 207 \ being compiled -- whew!).  So we use [COMPILE] to reverse the effect of marking IF as immediate.
 208 \ This trick is generally used when we want to write our own control words without having to
 209 \ implement them all in terms of the primitives 0BRANCH and BRANCH, but instead reusing simpler
 210 \ control words like (in this instance) IF.
 211 : UNLESS IMMEDIATE
 212         ' NOT ,         \ compile NOT (to reverse the test)
 213         [COMPILE] IF    \ continue by calling the normal IF
 214 ;
 215
 216 \       COMMENTS ----------------------------------------------------------------------
 217 \
 218 \ FORTH allows ( ... ) as comments within function definitions.  This works by having an IMMEDIATE
 219 \ word called ( which just drops input characters until it hits the corresponding ).
 220 : ( IMMEDIATE
 221         1               \ allowed nested parens by keeping track of depth
 222         BEGIN
 223                 KEY             \ read next character
 224                 DUP '(' = IF    \ open paren?
 225                         DROP            \ drop the open paren
 226                         1+              \ depth increases
 227                 ELSE
 228                         ')' = IF        \ close paren?
 229                                 1-              \ depth decreases
 230                         THEN
 231                 THEN
 232         DUP 0= UNTIL            \ continue until we reach matching close paren, depth 0
 233         DROP            \ drop the depth counter
 234 ;
 235
 236 (
 237         From now on we can use ( ... ) for comments.
 238
 239         STACK NOTATION ----------------------------------------------------------------------
 240
 241         In FORTH style we can also use ( ... -- ... ) to show the effects that a word has on the
 242         parameter stack.  For example:
 243
 244         ( n -- )        means that the word consumes an integer (n) from the parameter stack.
 245         ( b a -- c )    means that the word uses two integers (a and b, where a is at the top of stack)
 246                                 and returns a single integer (c).
 247         ( -- )          means the word has no effect on the stack
 248 )
 249
 250 ( Some more complicated stack examples, showing the stack notation. )
 251 : NIP ( x y -- y ) SWAP DROP ;
 252 : TUCK ( x y -- y x y ) SWAP OVER ;
 253 : PICK ( x_u ... x_1 x_0 u -- x_u ... x_1 x_0 x_u )
 254         1+              ( add one because of 'u' on the stack )
 255         4 *             ( multiply by the word size )
 256         DSP@ +          ( add to the stack pointer )
 257         @               ( and fetch )
 258 ;
 259
 260 ( With the looping constructs, we can now write SPACES, which writes n spaces to stdout. )
 261 : SPACES        ( n -- )
 262         BEGIN
 263                 DUP 0>          ( while n > 0 )
 264         WHILE
 265                 SPACE           ( print a space )
 266                 1-              ( until we count down to 0 )
 267         REPEAT
 268         DROP
 269 ;
 270
 271 ( Standard words for manipulating BASE. )
 272 : DECIMAL ( -- ) 10 BASE ! ;
 273 : HEX ( -- ) 16 BASE ! ;
 274
 275 (
 276         PRINTING NUMBERS ----------------------------------------------------------------------
 277
 278         The standard FORTH word . (DOT) is very important.  It takes the number at the top
 279         of the stack and prints it out.  However first I'm going to implement some lower-level
 280         FORTH words:
 281
 282         U.R     ( u width -- )  which prints an unsigned number, padded to a certain width
 283         U.      ( u -- )        which prints an unsigned number
 284         .R      ( n width -- )  which prints a signed number, padded to a certain width.
 285
 286         For example:
 287                 -123 6 .R
 288         will print out these characters:
 289                 <space> <space> - 1 2 3
 290
 291         In other words, the number padded left to a certain number of characters.
 292
 293         The full number is printed even if it is wider than width, and this is what allows us to
 294         define the ordinary functions U. and . (we just set width to zero knowing that the full
 295         number will be printed anyway).
 296
 297         Another wrinkle of . and friends is that they obey the current base in the variable BASE.
 298         BASE can be anything in the range 2 to 36.
 299
 300         While we're defining . &c we can also define .S which is a useful debugging tool.  This
 301         word prints the current stack (non-destructively) from top to bottom.
 302 )
 303
 304 ( This is the underlying recursive definition of U. )
 305 : U.            ( u -- )
 306         BASE @ /MOD     ( width rem quot )
 307         ?DUP IF                 ( if quotient <> 0 then )
 308                 RECURSE         ( print the quotient )
 309         THEN
 310
 311         ( print the remainder )
 312         DUP 10 < IF
 313                 '0'             ( decimal digits 0..9 )
 314         ELSE
 315                 10 -            ( hex and beyond digits A..Z )
 316                 'A'
 317         THEN
 318         +
 319         EMIT
 320 ;
 321
 322 (
 323         FORTH word .S prints the contents of the stack.  It doesn't alter the stack.
 324         Very useful for debugging.
 325 )
 326 : .S            ( -- )
 327         DSP@            ( get current stack pointer )
 328         BEGIN
 329                 DUP S0 @ <
 330         WHILE
 331                 DUP @ U.        ( print the stack element )
 332                 SPACE
 333                 4+              ( move up )
 334         REPEAT
 335         DROP
 336 ;
 337
 338 ( This word returns the width (in characters) of an unsigned number in the current base )
 339 : UWIDTH        ( u -- width )
 340         BASE @ /        ( rem quot )
 341         ?DUP IF         ( if quotient <> 0 then )
 342                 RECURSE 1+      ( return 1+recursive call )
 343         ELSE
 344                 1               ( return 1 )
 345         THEN
 346 ;
 347
 348 : U.R           ( u width -- )
 349         SWAP            ( width u )
 350         DUP             ( width u u )
 351         UWIDTH          ( width u uwidth )
 352         ROT             ( u uwidth width )
 353         SWAP -          ( u width-uwidth )
 354         ( At this point if the requested width is narrower, we'll have a negative number on the stack.
 355           Otherwise the number on the stack is the number of spaces to print.  But SPACES won't print
 356           a negative number of spaces anyway, so it's now safe to call SPACES ... )
 357         SPACES
 358         ( ... and then call the underlying implementation of U. )
 359         U.
 360 ;
 361
 362 (
 363         .R prints a signed number, padded to a certain width.  We can't just print the sign
 364         and call U.R because we want the sign to be next to the number ('-123' instead of '-  123').
 365 )
 366 : .R            ( n width -- )
 367         SWAP            ( width n )
 368         DUP 0< IF
 369                 NEGATE          ( width u )
 370                 1               ( save a flag to remember that it was negative | width n 1 )
 371                 SWAP            ( width 1 u )
 372                 ROT             ( 1 u width )
 373                 1-              ( 1 u width-1 )
 374         ELSE
 375                 0               ( width u 0 )
 376                 SWAP            ( width 0 u )
 377                 ROT             ( 0 u width )
 378         THEN
 379         SWAP            ( flag width u )
 380         DUP             ( flag width u u )
 381         UWIDTH          ( flag width u uwidth )
 382         ROT             ( flag u uwidth width )
 383         SWAP -          ( flag u width-uwidth )
 384
 385         SPACES          ( flag u )
 386         SWAP            ( u flag )
 387
 388         IF                      ( was it negative? print the - character )
 389                 '-' EMIT
 390         THEN
 391
 392         U.
 393 ;
 394
 395 ( Finally we can define word . in terms of .R, with a trailing space. )
 396 : . 0 .R SPACE ;
 397
 398 ( The real U., note the trailing space. )
 399 : U. U. SPACE ;
 400
 401 ( ? fetches the integer at an address and prints it. )
 402 : ? ( addr -- ) @ . ;
 403
 404 ( c a b WITHIN returns true if a <= c and c < b )
 405 (  or define without ifs: OVER - >R - R>  U<  )
 406 : WITHIN
 407         -ROT            ( b c a )
 408         OVER            ( b c a c )
 409         <= IF
 410                 > IF            ( b c -- )
 411                         TRUE
 412                 ELSE
 413                         FALSE
 414                 THEN
 415         ELSE
 416                 2DROP           ( b c -- )
 417                 FALSE
 418         THEN
 419 ;
 420
 421 ( DEPTH returns the depth of the stack. )
 422 : DEPTH         ( -- n )
 423         S0 @ DSP@ -
 424         4-                      ( adjust because S0 was on the stack when we pushed DSP )
 425 ;
 426
 427 (
 428         ALIGNED takes an address and rounds it up (aligns it) to the next 4 byte boundary.
 429 )
 430 : ALIGNED       ( addr -- addr )
 431         3 + 3 INVERT AND        ( (addr+3) & ~3 )
 432 ;
 433
 434 (
 435         ALIGN aligns the HERE pointer, so the next word appended will be aligned properly.
 436 )
 437 : ALIGN HERE @ ALIGNED HERE ! ;
 438
 439 (
 440         STRINGS ----------------------------------------------------------------------
 441
 442         S" string" is used in FORTH to define strings.  It leaves the address of the string and
 443         its length on the stack, (length at the top of stack).  The space following S" is the normal
 444         space between FORTH words and is not a part of the string.
 445
 446         This is tricky to define because it has to do different things depending on whether
 447         we are compiling or in immediate mode.  (Thus the word is marked IMMEDIATE so it can
 448         detect this and do different things).
 449
 450         In compile mode we append
 451                 LITSTRING <string length> <string rounded up 4 bytes>
 452         to the current word.  The primitive LITSTRING does the right thing when the current
 453         word is executed.
 454
 455         In immediate mode there isn't a particularly good place to put the string, but in this
 456         case we put the string at HERE (but we _don't_ change HERE).  This is meant as a temporary
 457         location, likely to be overwritten soon after.
 458 )
 459 ( C, appends a byte to the current compiled word. )
 460 : C,
 461         HERE @ C!       ( store the character in the compiled image )
 462         1 HERE +!       ( increment HERE pointer by 1 byte )
 463 ;
 464
 465 : S" IMMEDIATE          ( -- addr len )
 466         STATE @ IF      ( compiling? )
 467                 ' LITSTRING ,   ( compile LITSTRING )
 468                 HERE @          ( save the address of the length word on the stack )
 469                 0 ,             ( dummy length - we don't know what it is yet )
 470                 BEGIN
 471                         KEY             ( get next character of the string )
 472                         DUP '"' <>
 473                 WHILE
 474                         C,              ( copy character )
 475                 REPEAT
 476                 DROP            ( drop the double quote character at the end )
 477                 DUP             ( get the saved address of the length word )
 478                 HERE @ SWAP -   ( calculate the length )
 479                 4-              ( subtract 4 (because we measured from the start of the length word) )
 480                 SWAP !          ( and back-fill the length location )
 481                 ALIGN           ( round up to next multiple of 4 bytes for the remaining code )
 482         ELSE            ( immediate mode )
 483                 HERE @          ( get the start address of the temporary space )
 484                 BEGIN
 485                         KEY
 486                         DUP '"' <>
 487                 WHILE
 488                         OVER C!         ( save next character )
 489                         1+              ( increment address )
 490                 REPEAT
 491                 DROP            ( drop the final " character )
 492                 HERE @ -        ( calculate the length )
 493                 HERE @          ( push the start address )
 494                 SWAP            ( addr len )
 495         THEN
 496 ;
 497
 498 (
 499         ." is the print string operator in FORTH.  Example: ." Something to print"
 500         The space after the operator is the ordinary space required between words and is not
 501         a part of what is printed.
 502
 503         In immediate mode we just keep reading characters and printing them until we get to
 504         the next double quote.
 505
 506         In compile mode we use S" to store the string, then add TELL afterwards:
 507                 LITSTRING <string length> <string rounded up to 4 bytes> TELL
 508
 509         It may be interesting to note the use of [COMPILE] to turn the call to the immediate
 510         word S" into compilation of that word.  It compiles it into the definition of .",
 511         not into the definition of the word being compiled when this is running (complicated
 512         enough for you?)
 513 )
 514 : ." IMMEDIATE          ( -- )
 515         STATE @ IF      ( compiling? )
 516                 [COMPILE] S"    ( read the string, and compile LITSTRING, etc. )
 517                 ' TELL ,        ( compile the final TELL )
 518         ELSE
 519                 ( In immediate mode, just read characters and print them until we get
 520                   to the ending double quote. )
 521                 BEGIN
 522                         KEY
 523                         DUP '"' = IF
 524                                 DROP    ( drop the double quote character )
 525                                 EXIT    ( return from this function )
 526                         THEN
 527                         EMIT
 528                 AGAIN
 529         THEN
 530 ;
 531
 532 (
 533         CONSTANTS AND VARIABLES ----------------------------------------------------------------------
 534
 535         In FORTH, global constants and variables are defined like this:
 536
 537         10 CONSTANT TEN         when TEN is executed, it leaves the integer 10 on the stack
 538         VARIABLE VAR            when VAR is executed, it leaves the address of VAR on the stack
 539
 540         Constants can be read but not written, eg:
 541
 542         TEN . CR                prints 10
 543
 544         You can read a variable (in this example called VAR) by doing:
 545
 546         VAR @                   leaves the value of VAR on the stack
 547         VAR @ . CR              prints the value of VAR
 548         VAR ? CR                same as above, since ? is the same as @ .
 549
 550         and update the variable by doing:
 551
 552         20 VAR !                sets VAR to 20
 553
 554         Note that variables are uninitialised (but see VALUE later on which provides initialised
 555         variables with a slightly simpler syntax).
 556
 557         How can we define the words CONSTANT and VARIABLE?
 558
 559         The trick is to define a new word for the variable itself (eg. if the variable was called
 560         'VAR' then we would define a new word called VAR).  This is easy to do because we exposed
 561         dictionary entry creation through the CREATE word (part of the definition of : above).
 562         A call to WORD [TEN] CREATE (where [TEN] means that "TEN" is the next word in the input)
 563         leaves the dictionary entry:
 564
 565                                    +--- HERE
 566                                    |
 567                                    V
 568         +---------+---+---+---+---+
 569         | LINK    | 3 | T | E | N |
 570         +---------+---+---+---+---+
 571                    len
 572
 573         For CONSTANT we can continue by appending DOCOL (the codeword), then LIT followed by
 574         the constant itself and then EXIT, forming a little word definition that returns the
 575         constant:
 576
 577         +---------+---+---+---+---+------------+------------+------------+------------+
 578         | LINK    | 3 | T | E | N | DOCOL      | LIT        | 10         | EXIT       |
 579         +---------+---+---+---+---+------------+------------+------------+------------+
 580                    len              codeword
 581
 582         Notice that this word definition is exactly the same as you would have got if you had
 583         written : TEN 10 ;
 584
 585         Note for people reading the code below: DOCOL is a constant word which we defined in the
 586         assembler part which returns the value of the assembler symbol of the same name.
 587 )
 588 : CONSTANT
 589         WORD            ( get the name (the name follows CONSTANT) )
 590         CREATE          ( make the dictionary entry )
 591         DOCOL ,         ( append DOCOL (the codeword field of this word) )
 592         ' LIT ,         ( append the codeword LIT )
 593         ,               ( append the value on the top of the stack )
 594         ' EXIT ,        ( append the codeword EXIT )
 595 ;
 596
 597 (
 598         VARIABLE is a little bit harder because we need somewhere to put the variable.  There is
 599         nothing particularly special about the user memory (the area of memory pointed to by HERE
 600         where we have previously just stored new word definitions).  We can slice off bits of this
 601         memory area to store anything we want, so one possible definition of VARIABLE might create
 602         this:
 603
 604            +--------------------------------------------------------------+
 605            |                                                              |
 606            V                                                              |
 607         +---------+---------+---+---+---+---+------------+------------+---|--------+------------+
 608         | <var>   | LINK    | 3 | V | A | R | DOCOL      | LIT        | <addr var> | EXIT       |
 609         +---------+---------+---+---+---+---+------------+------------+------------+------------+
 610                              len              codeword
 611
 612         where <var> is the place to store the variable, and <addr var> points back to it.
 613
 614         To make this more general let's define a couple of words which we can use to allocate
 615         arbitrary memory from the user memory.
 616
 617         First ALLOT, where n ALLOT allocates n bytes of memory.  (Note when calling this that
 618         it's a very good idea to make sure that n is a multiple of 4, or at least that next time
 619         a word is compiled that HERE has been left as a multiple of 4).
 620 )
 621 : ALLOT         ( n -- addr )
 622         HERE @ SWAP     ( here n )
 623         HERE +!         ( adds n to HERE, after this the old value of HERE is still on the stack )
 624 ;
 625
 626 (
 627         Second, CELLS.  In FORTH the phrase 'n CELLS ALLOT' means allocate n integers of whatever size
 628         is the natural size for integers on this machine architecture.  On this 32 bit machine therefore
 629         CELLS just multiplies the top of stack by 4.
 630 )
 631 : CELLS ( n -- n ) 4 * ;
 632
 633 (
 634         So now we can define VARIABLE easily in much the same way as CONSTANT above.  Refer to the
 635         diagram above to see what the word that this creates will look like.
 636 )
 637 : VARIABLE
 638         1 CELLS ALLOT   ( allocate 1 cell of memory, push the pointer to this memory )
 639         WORD CREATE     ( make the dictionary entry (the name follows VARIABLE) )
 640         DOCOL ,         ( append DOCOL (the codeword field of this word) )
 641         ' LIT ,         ( append the codeword LIT )
 642         ,               ( append the pointer to the new memory )
 643         ' EXIT ,        ( append the codeword EXIT )
 644 ;
 645
 646 (
 647         VALUES ----------------------------------------------------------------------
 648
 649         VALUEs are like VARIABLEs but with a simpler syntax.  You would generally use them when you
 650         want a variable which is read often, and written infrequently.
 651
 652         20 VALUE VAL    creates VAL with initial value 20
 653         VAL             pushes the value (20) directly on the stack
 654         30 TO VAL       updates VAL, setting it to 30
 655         VAL             pushes the value (30) directly on the stack
 656
 657         Notice that 'VAL' on its own doesn't return the address of the value, but the value itself,
 658         making values simpler and more obvious to use than variables (no indirection through '@').
 659         The price is a more complicated implementation, although despite the complexity there is no
 660         performance penalty at runtime.
 661
 662         A naive implementation of 'TO' would be quite slow, involving a dictionary search each time.
 663         But because this is FORTH we have complete control of the compiler so we can compile TO more
 664         efficiently, turning:
 665                 TO VAL
 666         into:
 667                 LIT <addr> !
 668         and calculating <addr> (the address of the value) at compile time.
 669
 670         Now this is the clever bit.  We'll compile our value like this:
 671
 672         +---------+---+---+---+---+------------+------------+------------+------------+
 673         | LINK    | 3 | V | A | L | DOCOL      | LIT        | <value>    | EXIT       |
 674         +---------+---+---+---+---+------------+------------+------------+------------+
 675                    len              codeword
 676
 677         where <value> is the actual value itself.  Note that when VAL executes, it will push the
 678         value on the stack, which is what we want.
 679
 680         But what will TO use for the address <addr>?  Why of course a pointer to that <value>:
 681
 682                 code compiled   - - - - --+------------+------------+------------+-- - - - -
 683                 by TO VAL                 | LIT        | <addr>     | !          |
 684                                 - - - - --+------------+-----|------+------------+-- - - - -
 685                                                              |
 686                                                              V
 687         +---------+---+---+---+---+------------+------------+------------+------------+
 688         | LINK    | 3 | V | A | L | DOCOL      | LIT        | <value>    | EXIT       |
 689         +---------+---+---+---+---+------------+------------+------------+------------+
 690                    len              codeword
 691
 692         In other words, this is a kind of self-modifying code.
 693
 694         (Note to the people who want to modify this FORTH to add inlining: values defined this
 695         way cannot be inlined).
 696 )
 697 : VALUE         ( n -- )
 698         WORD CREATE     ( make the dictionary entry (the name follows VALUE) )
 699         DOCOL ,         ( append DOCOL )
 700         ' LIT ,         ( append the codeword LIT )
 701         ,               ( append the initial value )
 702         ' EXIT ,        ( append the codeword EXIT )
 703 ;
 704
 705 : TO IMMEDIATE  ( n -- )
 706         WORD            ( get the name of the value )
 707         FIND            ( look it up in the dictionary )
 708         >DFA            ( get a pointer to the first data field (the 'LIT') )
 709         4+              ( increment to point at the value )
 710         STATE @ IF      ( compiling? )
 711                 ' LIT ,         ( compile LIT )
 712                 ,               ( compile the address of the value )
 713                 ' ! ,           ( compile ! )
 714         ELSE            ( immediate mode )
 715                 !               ( update it straightaway )
 716         THEN
 717 ;
 718
 719 ( x +TO VAL adds x to VAL )
 720 : +TO IMMEDIATE
 721         WORD            ( get the name of the value )
 722         FIND            ( look it up in the dictionary )
 723         >DFA            ( get a pointer to the first data field (the 'LIT') )
 724         4+              ( increment to point at the value )
 725         STATE @ IF      ( compiling? )
 726                 ' LIT ,         ( compile LIT )
 727                 ,               ( compile the address of the value )
 728                 ' +! ,          ( compile +! )
 729         ELSE            ( immediate mode )
 730                 +!              ( update it straightaway )
 731         THEN
 732 ;
 733
 734 (
 735         PRINTING THE DICTIONARY ----------------------------------------------------------------------
 736
 737         ID. takes an address of a dictionary entry and prints the word's name.
 738
 739         For example: LATEST @ ID. would print the name of the last word that was defined.
 740 )
 741 : ID.
 742         4+              ( skip over the link pointer )
 743         DUP C@          ( get the flags/length byte )
 744         F_LENMASK AND   ( mask out the flags - just want the length )
 745
 746         BEGIN
 747                 DUP 0>          ( length > 0? )
 748         WHILE
 749                 SWAP 1+         ( addr len -- len addr+1 )
 750                 DUP C@          ( len addr -- len addr char | get the next character)
 751                 EMIT            ( len addr char -- len addr | and print it)
 752                 SWAP 1-         ( len addr -- addr len-1    | subtract one from length )
 753         REPEAT
 754         2DROP           ( len addr -- )
 755 ;
 756
 757 (
 758         'WORD word FIND ?HIDDEN' returns true if 'word' is flagged as hidden.
 759
 760         'WORD word FIND ?IMMEDIATE' returns true if 'word' is flagged as immediate.
 761 )
 762 : ?HIDDEN
 763         4+              ( skip over the link pointer )
 764         C@              ( get the flags/length byte )
 765         F_HIDDEN AND    ( mask the F_HIDDEN flag and return it (as a truth value) )
 766 ;
 767 : ?IMMEDIATE
 768         4+              ( skip over the link pointer )
 769         C@              ( get the flags/length byte )
 770         F_IMMED AND     ( mask the F_IMMED flag and return it (as a truth value) )
 771 ;
 772
 773 (
 774         WORDS prints all the words defined in the dictionary, starting with the word defined most recently.
 775         However it doesn't print hidden words.
 776
 777         The implementation simply iterates backwards from LATEST using the link pointers.
 778 )
 779 : WORDS
 780         LATEST @        ( start at LATEST dictionary entry )
 781         BEGIN
 782                 ?DUP            ( while link pointer is not null )
 783         WHILE
 784                 DUP ?HIDDEN NOT IF      ( ignore hidden words )
 785                         DUP ID.         ( but if not hidden, print the word )
 786                         SPACE
 787                 THEN
 788                 @               ( dereference the link pointer - go to previous word )
 789         REPEAT
 790         CR
 791 ;
 792
 793 (
 794         FORGET ----------------------------------------------------------------------
 795
 796         So far we have only allocated words and memory.  FORTH provides a rather primitive method
 797         to deallocate.
 798
 799         'FORGET word' deletes the definition of 'word' from the dictionary and everything defined
 800         after it, including any variables and other memory allocated after.
 801
 802         The implementation is very simple - we look up the word (which returns the dictionary entry
 803         address).  Then we set HERE to point to that address, so in effect all future allocations
 804         and definitions will overwrite memory starting at the word.  We also need to set LATEST to
 805         point to the previous word.
 806
 807         Note that you cannot FORGET built-in words (well, you can try but it will probably cause
 808         a segfault).
 809
 810         XXX: Because we wrote VARIABLE to store the variable in memory allocated before the word,
 811         in the current implementation VARIABLE FOO FORGET FOO will leak 1 cell of memory.
 812 )
 813 : FORGET
 814         WORD FIND       ( find the word, gets the dictionary entry address )
 815         DUP @ LATEST !  ( set LATEST to point to the previous word )
 816         HERE !          ( and store HERE with the dictionary address )
 817 ;
 818
 819 (
 820         DUMP ----------------------------------------------------------------------
 821
 822         DUMP is used to dump out the contents of memory, in the 'traditional' hexdump format.
 823
 824         Notice that the parameters to DUMP (address, length) are compatible with string words
 825         such as WORD and S".
 826
 827         You can dump out the raw code for the last word you defined by doing something like:
 828
 829                 LATEST @ 128 DUMP
 830 )
 831 : DUMP          ( addr len -- )
 832         BASE @ -ROT             ( save the current BASE at the bottom of the stack )
 833         HEX                     ( and switch to hexadecimal mode )
 834
 835         BEGIN
 836                 ?DUP            ( while len > 0 )
 837         WHILE
 838                 OVER 8 U.R      ( print the address )
 839                 SPACE
 840
 841                 ( print up to 16 words on this line )
 842                 2DUP            ( addr len addr len )
 843                 1- 15 AND 1+    ( addr len addr linelen )
 844                 BEGIN
 845                         ?DUP            ( while linelen > 0 )
 846                 WHILE
 847                         SWAP            ( addr len linelen addr )
 848                         DUP C@          ( addr len linelen addr byte )
 849                         2 .R SPACE      ( print the byte )
 850                         1+ SWAP 1-      ( addr len linelen addr -- addr len addr+1 linelen-1 )
 851                 REPEAT
 852                 DROP            ( addr len )
 853
 854                 ( print the ASCII equivalents )
 855                 2DUP 1- 15 AND 1+ ( addr len addr linelen )
 856                 BEGIN
 857                         ?DUP            ( while linelen > 0)
 858                 WHILE
 859                         SWAP            ( addr len linelen addr )
 860                         DUP C@          ( addr len linelen addr byte )
 861                         DUP 32 128 WITHIN IF    ( 32 <= c < 128? )
 862                                 EMIT
 863                         ELSE
 864                                 DROP '.' EMIT
 865                         THEN
 866                         1+ SWAP 1-      ( addr len linelen addr -- addr len addr+1 linelen-1 )
 867                 REPEAT
 868                 DROP            ( addr len )
 869                 CR
 870
 871                 DUP 1- 15 AND 1+ ( addr len linelen )
 872                 TUCK            ( addr linelen len linelen )
 873                 -               ( addr linelen len-linelen )
 874                 >R + R>         ( addr+linelen len-linelen )
 875         REPEAT
 876
 877         DROP                    ( restore stack )
 878         BASE !                  ( restore saved BASE )
 879 ;
 880
 881 (
 882         CASE ----------------------------------------------------------------------
 883
 884         CASE...ENDCASE is how we do switch statements in FORTH.  There is no generally
 885         agreed syntax for this, so I've gone for the syntax mandated by the ISO standard
 886         FORTH (ANS-FORTH).
 887
 888                 ( some value on the stack )
 889                 CASE
 890                 test1 OF ... ENDOF
 891                 test2 OF ... ENDOF
 892                 testn OF ... ENDOF
 893                 ... ( default case )
 894                 ENDCASE
 895
 896         The CASE statement tests the value on the stack by comparing it for equality with
 897         test1, test2, ..., testn and executes the matching piece of code within OF ... ENDOF.
 898         If none of the test values match then the default case is executed.  Inside the ... of
 899         the default case, the value is still at the top of stack (it is implicitly DROP-ed
 900         by ENDCASE).  When ENDOF is executed it jumps after ENDCASE (ie. there is no "fall-through"
 901         and no need for a break statement like in C).
 902
 903         The default case may be omitted.  In fact the tests may also be omitted so that you
 904         just have a default case, although this is probably not very useful.
 905
 906         An example (assuming that 'q', etc. are words which push the ASCII value of the letter
 907         on the stack):
 908
 909                 0 VALUE QUIT
 910                 0 VALUE SLEEP
 911                 KEY CASE
 912                         'q' OF 1 TO QUIT ENDOF
 913                         's' OF 1 TO SLEEP ENDOF
 914                         ( default case: )
 915                         ." Sorry, I didn't understand key <" DUP EMIT ." >, try again." CR
 916                 ENDCASE
 917
 918         (In some versions of FORTH, more advanced tests are supported, such as ranges, etc.
 919         Other versions of FORTH need you to write OTHERWISE to indicate the default case.
 920         As I said above, this FORTH tries to follow the ANS FORTH standard).
 921
 922         The implementation of CASE...ENDCASE is somewhat non-trivial.  I'm following the
 923         implementations from here:
 924         http://www.uni-giessen.de/faq/archiv/forthfaq.case_endcase/msg00000.html
 925
 926         The general plan is to compile the code as a series of IF statements:
 927
 928         CASE                            (push 0 on the immediate-mode parameter stack)
 929         test1 OF ... ENDOF              test1 OVER = IF DROP ... ELSE
 930         test2 OF ... ENDOF              test2 OVER = IF DROP ... ELSE
 931         testn OF ... ENDOF              testn OVER = IF DROP ... ELSE
 932         ... ( default case )            ...
 933         ENDCASE                         DROP THEN [THEN [THEN ...]]
 934
 935         The CASE statement pushes 0 on the immediate-mode parameter stack, and that number
 936         is used to count how many THEN statements we need when we get to ENDCASE so that each
 937         IF has a matching THEN.  The counting is done implicitly.  If you recall from the
 938         implementation above of IF, each IF pushes a code address on the immediate-mode stack,
 939         and these addresses are non-zero, so by the time we get to ENDCASE the stack contains
 940         some number of non-zeroes, followed by a zero.  The number of non-zeroes is how many
 941         times IF has been called, so how many times we need to match it with THEN.
 942
 943         This code uses [COMPILE] so that we compile calls to IF, ELSE, THEN instead of
 944         actually calling them while we're compiling the words below.
 945
 946         As is the case with all of our control structures, they only work within word
 947         definitions, not in immediate mode.
 948 )
 949 : CASE IMMEDIATE
 950         0               ( push 0 to mark the bottom of the stack )
 951 ;
 952
 953 : OF IMMEDIATE
 954         ' OVER ,        ( compile OVER )
 955         ' = ,           ( compile = )
 956         [COMPILE] IF    ( compile IF )
 957         ' DROP ,        ( compile DROP )
 958 ;
 959
 960 : ENDOF IMMEDIATE
 961         [COMPILE] ELSE  ( ENDOF is the same as ELSE )
 962 ;
 963
 964 : ENDCASE IMMEDIATE
 965         ' DROP ,        ( compile DROP )
 966
 967         ( keep compiling THEN until we get to our zero marker )
 968         BEGIN
 969                 ?DUP
 970         WHILE
 971                 [COMPILE] THEN
 972         REPEAT
 973 ;
 974
 975 (
 976         DECOMPILER ----------------------------------------------------------------------
 977
 978         CFA> is the opposite of >CFA.  It takes a codeword and tries to find the matching
 979         dictionary definition.  (In truth, it works with any pointer into a word, not just
 980         the codeword pointer, and this is needed to do stack traces).
 981
 982         In this FORTH this is not so easy.  In fact we have to search through the dictionary
 983         because we don't have a convenient back-pointer (as is often the case in other versions
 984         of FORTH).  Because of this search, CFA> should not be used when performance is critical,
 985         so it is only used for debugging tools such as the decompiler and printing stack
 986         traces.
 987
 988         This word returns 0 if it doesn't find a match.
 989 )
 990 : CFA>
 991         LATEST @        ( start at LATEST dictionary entry )
 992         BEGIN
 993                 ?DUP            ( while link pointer is not null )
 994         WHILE
 995                 2DUP SWAP       ( cfa curr curr cfa )
 996                 < IF            ( current dictionary entry < cfa? )
 997                         NIP             ( leave curr dictionary entry on the stack )
 998                         EXIT
 999                 THEN
1000                 @               ( follow link pointer back )
1001         REPEAT
1002         DROP            ( restore stack )
1003         0               ( sorry, nothing found )
1004 ;
1005
1006 (
1007         SEE decompiles a FORTH word.
1008
1009         We search for the dictionary entry of the word, then search again for the next
1010         word (effectively, the end of the compiled word).  This results in two pointers:
1011
1012         +---------+---+---+---+---+------------+------------+------------+------------+
1013         | LINK    | 3 | T | E | N | DOCOL      | LIT        | 10         | EXIT       |
1014         +---------+---+---+---+---+------------+------------+------------+------------+
1015          ^                                                                             ^
1016          |                                                                             |
1017         Start of word                                                         End of word
1018
1019         With this information we can have a go at decompiling the word.  We need to
1020         recognise "meta-words" like LIT, LITSTRING, BRANCH, etc. and treat those separately.
1021 )
1022 : SEE
1023         WORD FIND       ( find the dictionary entry to decompile )
1024
1025         ( Now we search again, looking for the next word in the dictionary.  This gives us
1026           the length of the word that we will be decompiling.  (Well, mostly it does). )
1027         HERE @          ( address of the end of the last compiled word )
1028         LATEST @        ( word last curr )
1029         BEGIN
1030                 2 PICK          ( word last curr word )
1031                 OVER            ( word last curr word curr )
1032                 <>              ( word last curr word<>curr? )
1033         WHILE                   ( word last curr )
1034                 NIP             ( word curr )
1035                 DUP @           ( word curr prev (which becomes: word last curr) )
1036         REPEAT
1037
1038         DROP            ( at this point, the stack is: start-of-word end-of-word )
1039         SWAP            ( end-of-word start-of-word )
1040
1041         ( begin the definition with : NAME [IMMEDIATE] )
1042         ':' EMIT SPACE DUP ID. SPACE
1043         DUP ?IMMEDIATE IF ." IMMEDIATE " THEN
1044
1045         >DFA            ( get the data address, ie. points after DOCOL | end-of-word start-of-data )
1046
1047         ( now we start decompiling until we hit the end of the word )
1048         BEGIN           ( end start )
1049                 2DUP >
1050         WHILE
1051                 DUP @           ( end start codeword )
1052
1053                 CASE
1054                 ' LIT OF                ( is it LIT ? )
1055                         4 + DUP @               ( get next word which is the integer constant )
1056                         .                       ( and print it )
1057                 ENDOF
1058                 ' LITSTRING OF          ( is it LITSTRING ? )
1059                         [ CHAR S ] LITERAL EMIT '"' EMIT SPACE ( print S"<space> )
1060                         4 + DUP @               ( get the length word )
1061                         SWAP 4 + SWAP           ( end start+4 length )
1062                         2DUP TELL               ( print the string )
1063                         '"' EMIT SPACE          ( finish the string with a final quote )
1064                         + ALIGNED               ( end start+4+len, aligned )
1065                         4 -                     ( because we're about to add 4 below )
1066                 ENDOF
1067                 ' 0BRANCH OF            ( is it 0BRANCH ? )
1068                         ." 0BRANCH ( "
1069                         4 + DUP @               ( print the offset )
1070                         .
1071                         ." ) "
1072                 ENDOF
1073                 ' BRANCH OF             ( is it BRANCH ? )
1074                         ." BRANCH ( "
1075                         4 + DUP @               ( print the offset )
1076                         .
1077                         ." ) "
1078                 ENDOF
1079                 ' ' OF                  ( is it ' (TICK) ? )
1080                         [ CHAR ' ] LITERAL EMIT SPACE
1081                         4 + DUP @               ( get the next codeword )
1082                         CFA>                    ( and force it to be printed as a dictionary entry )
1083                         ID. SPACE
1084                 ENDOF
1085                 ' EXIT OF               ( is it EXIT? )
1086                         ( We expect the last word to be EXIT, and if it is then we don't print it
1087                           because EXIT is normally implied by ;.  EXIT can also appear in the middle
1088                           of words, and then it needs to be printed. )
1089                         2DUP                    ( end start end start )
1090                         4 +                     ( end start end start+4 )
1091                         <> IF                   ( end start | we're not at the end )
1092                                 ." EXIT "
1093                         THEN
1094                 ENDOF
1095                                         ( default case: )
1096                         DUP                     ( in the default case we always need to DUP before using )
1097                         CFA>                    ( look up the codeword to get the dictionary entry )
1098                         ID. SPACE               ( and print it )
1099                 ENDCASE
1100
1101                 4 +             ( end start+4 )
1102         REPEAT
1103
1104         ';' EMIT CR
1105
1106         2DROP           ( restore stack )
1107 ;
1108
1109 (
1110         EXECUTION TOKENS ----------------------------------------------------------------------
1111
1112         Standard FORTH defines a concept called an 'execution token' (or 'xt') which is very
1113         similar to a function pointer in C.  We map the execution token to a codeword address.
1114
1115                         execution token of DOUBLE is the address of this codeword
1116                                                     |
1117                                                     V
1118         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1119         | LINK    | 6 | D | O | U | B | L | E | 0 | DOCOL      | DUP        | +          | EXIT       |
1120         +---------+---+---+---+---+---+---+---+---+------------+------------+------------+------------+
1121                    len                         pad  codeword                                           ^
1122
1123         There is one assembler primitive for execution tokens, EXECUTE ( xt -- ), which runs them.
1124
1125         You can make an execution token for an existing word the long way using >CFA,
1126         ie: WORD [foo] FIND >CFA will push the xt for foo onto the stack where foo is the
1127         next word in input.  So a very slow way to run DOUBLE might be:
1128
1129                 : DOUBLE DUP + ;
1130                 : SLOW WORD FIND >CFA EXECUTE ;
1131                 5 SLOW DOUBLE . CR      \ prints 10
1132
1133         We also offer a simpler and faster way to get the execution token of any word FOO:
1134
1135                 ['] FOO
1136
1137         (Exercises for readers: (1) What is the difference between ['] FOO and ' FOO?
1138         (2) What is the relationship between ', ['] and LIT?)
1139
1140         More useful is to define anonymous words and/or to assign xt's to variables.
1141
1142         To define an anonymous word (and push its xt on the stack) use :NONAME ... ; as in this
1143         example:
1144
1145                 :NONAME ." anon word was called" CR ;   \ pushes xt on the stack
1146                 DUP EXECUTE EXECUTE                     \ executes the anon word twice
1147
1148         Stack parameters work as expected:
1149
1150                 :NONAME ." called with parameter " . CR ;
1151                 DUP
1152                 10 SWAP EXECUTE         \ prints 'called with parameter 10'
1153                 20 SWAP EXECUTE         \ prints 'called with parameter 20'
1154
1155         Notice that the above code has a memory leak: the anonymous word is still compiled
1156         into the data segment, so even if you lose track of the xt, the word continues to
1157         occupy memory.  A good way to keep track of the xt and thus avoid the memory leak is
1158         to assign it to a CONSTANT, VARIABLE or VALUE:
1159
1160                 0 VALUE ANON
1161                 :NONAME ." anon word was called" CR ; TO ANON
1162                 ANON EXECUTE
1163                 ANON EXECUTE
1164
1165         Another use of :NONAME is to create an array of functions which can be called quickly
1166         (think: fast switch statement).  This example is adapted from the ANS FORTH standard:
1167
1168                 10 CELLS ALLOT CONSTANT CMD-TABLE
1169                 : SET-CMD CELLS CMD-TABLE + ! ;
1170                 : CALL-CMD CELLS CMD-TABLE + @ EXECUTE ;
1171
1172                 :NONAME ." alternate 0 was called" CR ;  0 SET-CMD
1173                 :NONAME ." alternate 1 was called" CR ;  1 SET-CMD
1174                         \ etc...
1175                 :NONAME ." alternate 9 was called" CR ;  9 SET-CMD
1176
1177                 0 CALL-CMD
1178                 1 CALL-CMD
1179 )
1180
1181 : :NONAME
1182         0 0 CREATE      ( create a word with no name - we need a dictionary header because ; expects it )
1183         HERE @          ( current HERE value is the address of the codeword, ie. the xt )
1184         DOCOL ,         ( compile DOCOL (the codeword) )
1185         ]               ( go into compile mode )
1186 ;
1187
1188 : ['] IMMEDIATE
1189         ' LIT ,         ( compile LIT )
1190 ;
1191
1192 (
1193         EXCEPTIONS ----------------------------------------------------------------------
1194
1195         Amazingly enough, exceptions can be implemented directly in FORTH, in fact rather easily.
1196
1197         The general usage is as follows:
1198
1199                 : FOO ( n -- ) THROW ;
1200
1201                 : TEST-EXCEPTIONS
1202                         25 ['] FOO CATCH        \ execute 25 FOO, catching any exception
1203                         ?DUP IF
1204                                 ." called FOO and it threw exception number: "
1205                                 . CR
1206                                 DROP            \ we have to drop the argument of FOO (25)
1207                         THEN
1208                 ;
1209                 \ prints: called FOO and it threw exception number: 25
1210
1211         CATCH runs an execution token and detects whether it throws any exception or not.  The
1212         stack signature of CATCH is rather complicated:
1213
1214                 ( a_n-1 ... a_1 a_0 xt -- r_m-1 ... r_1 r_0 0 )         if xt did NOT throw an exception
1215                 ( a_n-1 ... a_1 a_0 xt -- ?_n-1 ... ?_1 ?_0 e )         if xt DID throw exception 'e'
1216
1217         where a_i and r_i are the (arbitrary number of) argument and return stack contents
1218         before and after xt is EXECUTEd.  Notice in particular the case where an exception
1219         is thrown, the stack pointer is restored so that there are n of _something_ on the
1220         stack in the positions where the arguments a_i used to be.  We don't really guarantee
1221         what is on the stack -- perhaps the original arguments, and perhaps other nonsense --
1222         it largely depends on the implementation of the word that was executed.
1223
1224         THROW, ABORT and a few others throw exceptions.
1225
1226         Exception numbers are non-zero integers.  By convention the positive numbers can be used
1227         for app-specific exceptions and the negative numbers have certain meanings defined in
1228         the ANS FORTH standard.  (For example, -1 is the exception thrown by ABORT).
1229
1230         0 THROW does nothing.  This is the stack signature of THROW:
1231
1232                 ( 0 -- )
1233                 ( * e -- ?_n-1 ... ?_1 ?_0 e )  the stack is restored to the state from the corresponding CATCH
1234
1235         The implementation hangs on the definitions of CATCH and THROW and the state shared
1236         between them.
1237
1238         Up to this point, the return stack has consisted merely of a list of return addresses,
1239         with the top of the return stack being the return address where we will resume executing
1240         when the current word EXITs.  However CATCH will push a more complicated 'exception stack
1241         frame' on the return stack.  The exception stack frame records some things about the
1242         state of execution at the time that CATCH was called.
1243
1244         When called, THROW walks up the return stack (the process is called 'unwinding') until
1245         it finds the exception stack frame.  It then uses the data in the exception stack frame
1246         to restore the state allowing execution to continue after the matching CATCH.  (If it
1247         unwinds the stack and doesn't find the exception stack frame then it prints a message
1248         and drops back to the prompt, which is also normal behaviour for so-called 'uncaught
1249         exceptions').
1250
1251         This is what the exception stack frame looks like.  (As is conventional, the return stack
1252         is shown growing downwards from higher to lower memory addresses).
1253
1254                 +------------------------------+
1255                 | return address from CATCH    |   Notice this is already on the
1256                 |                              |   return stack when CATCH is called.
1257                 +------------------------------+
1258                 | original parameter stack     |
1259                 | pointer                      |
1260                 +------------------------------+  ^
1261                 | exception stack marker       |  |
1262                 | (EXCEPTION-MARKER)           |  |   Direction of stack
1263                 +------------------------------+  |   unwinding by THROW.
1264                                                   |
1265                                                   |
1266
1267         The EXCEPTION-MARKER marks the entry as being an exception stack frame rather than an
1268         ordinary return address, and it is this which THROW "notices" as it is unwinding the
1269         stack.  (If you want to implement more advanced exceptions such as TRY...WITH then
1270         you'll need to use a different value of marker if you want the old and new exception stack
1271         frame layouts to coexist).
1272
1273         What happens if the executed word doesn't throw an exception?  It will eventually
1274         return and call EXCEPTION-MARKER, so EXCEPTION-MARKER had better do something sensible
1275         without us needing to modify EXIT.  This nicely gives us a suitable definition of
1276         EXCEPTION-MARKER, namely a function that just drops the stack frame and itself
1277         returns (thus "returning" from the original CATCH).
1278
1279         One thing to take from this is that exceptions are a relatively lightweight mechanism
1280         in FORTH.
1281 )
1282
1283 : EXCEPTION-MARKER
1284         RDROP                   ( drop the original parameter stack pointer )
1285         0                       ( there was no exception, this is the normal return path )
1286 ;
1287
1288 : CATCH         ( xt -- exn? )
1289         DSP@ 4+ >R              ( save parameter stack pointer (+4 because of xt) on the return stack )
1290         ' EXCEPTION-MARKER 4+   ( push the address of the RDROP inside EXCEPTION-MARKER ... )
1291         >R                      ( ... on to the return stack so it acts like a return address )
1292         EXECUTE                 ( execute the nested function )
1293 ;
1294
1295 : THROW         ( n -- )
1296         ?DUP IF                 ( only act if the exception code <> 0 )
1297                 RSP@                    ( get return stack pointer )
1298                 BEGIN
1299                         DUP R0 4- <             ( RSP < R0 )
1300                 WHILE
1301                         DUP @                   ( get the return stack entry )
1302                         ' EXCEPTION-MARKER 4+ = IF      ( found the EXCEPTION-MARKER on the return stack )
1303                                 4+                      ( skip the EXCEPTION-MARKER on the return stack )
1304                                 RSP!                    ( restore the return stack pointer )
1305
1306                                 ( Restore the parameter stack. )
1307                                 DUP DUP DUP             ( reserve some working space so the stack for this word
1308                                                           doesn't coincide with the part of the stack being restored )
1309                                 R>                      ( get the saved parameter stack pointer | n dsp )
1310                                 4-                      ( reserve space on the stack to store n )
1311                                 SWAP OVER               ( dsp n dsp )
1312                                 !                       ( write n on the stack )
1313                                 DSP! EXIT               ( restore the parameter stack pointer, immediately exit )
1314                         THEN
1315                         4+
1316                 REPEAT
1317
1318                 ( No matching catch - print a message and restart the INTERPRETer. )
1319                 DROP
1320
1321                 CASE
1322                 0 1- OF ( ABORT )
1323                         ." ABORTED" CR
1324                 ENDOF
1325                         ( default case )
1326                         ." UNCAUGHT THROW "
1327                         DUP . CR
1328                 ENDCASE
1329                 QUIT
1330         THEN
1331 ;
1332
1333 : ABORT         ( -- )
1334         0 1- THROW
1335 ;
1336
1337 ( Print a stack trace by walking up the return stack. )
1338 : PRINT-STACK-TRACE
1339         RSP@                            ( start at caller of this function )
1340         BEGIN
1341                 DUP R0 4- <             ( RSP < R0 )
1342         WHILE
1343                 DUP @                   ( get the return stack entry )
1344                 CASE
1345                 ' EXCEPTION-MARKER 4+ OF        ( is it the exception stack frame? )
1346                         ." CATCH ( DSP="
1347                         4+ DUP @ U.             ( print saved stack pointer )
1348                         ." ) "
1349                 ENDOF
1350                                                 ( default case )
1351                         DUP
1352                         CFA>                    ( look up the codeword to get the dictionary entry )
1353                         ?DUP IF                 ( and print it )
1354                                 2DUP                    ( dea addr dea )
1355                                 ID.                     ( print word from dictionary entry )
1356                                 [ CHAR + ] LITERAL EMIT
1357                                 SWAP >DFA 4+ - .        ( print offset )
1358                         THEN
1359                 ENDCASE
1360                 4+                      ( move up the stack )
1361         REPEAT
1362         DROP
1363         CR
1364 ;
1365
1366 (
1367         C STRINGS ----------------------------------------------------------------------
1368
1369         FORTH strings are represented by a start address and length kept on the stack or in memory.
1370
1371         Most FORTHs don't handle C strings, but we need them in order to access the process arguments
1372         and environment left on the stack by the Linux kernel, and to make some system calls.
1373
1374         Operation       Input           Output          FORTH word      Notes
1375         ----------------------------------------------------------------------
1376
1377         Create FORTH string             addr len        S" ..."
1378
1379         Create C string                 c-addr          Z" ..."
1380
1381         C -> FORTH      c-addr          addr len        DUP STRLEN
1382
1383         FORTH -> C      addr len        c-addr          CSTRING         Allocated in a temporary buffer, so
1384                                                                         should be consumed / copied immediately.
1385                                                                         FORTH string should not contain NULs.
1386
1387         For example, DUP STRLEN TELL prints a C string.
1388 )
1389
1390 (
1391         Z" .." is like S" ..." except that the string is terminated by an ASCII NUL character.
1392
1393         To make it more like a C string, at runtime Z" just leaves the address of the string
1394         on the stack (not address & length as with S").  To implement this we need to add the
1395         extra NUL to the string and also a DROP instruction afterwards.  Apart from that the
1396         implementation just a modified S".
1397 )
1398 : Z" IMMEDIATE
1399         STATE @ IF      ( compiling? )
1400                 ' LITSTRING ,   ( compile LITSTRING )
1401                 HERE @          ( save the address of the length word on the stack )
1402                 0 ,             ( dummy length - we don't know what it is yet )
1403                 BEGIN
1404                         KEY             ( get next character of the string )
1405                         DUP '"' <>
1406                 WHILE
1407                         HERE @ C!       ( store the character in the compiled image )
1408                         1 HERE +!       ( increment HERE pointer by 1 byte )
1409                 REPEAT
1410                 0 HERE @ C!     ( add the ASCII NUL byte )
1411                 1 HERE +!
1412                 DROP            ( drop the double quote character at the end )
1413                 DUP             ( get the saved address of the length word )
1414                 HERE @ SWAP -   ( calculate the length )
1415                 4-              ( subtract 4 (because we measured from the start of the length word) )
1416                 SWAP !          ( and back-fill the length location )
1417                 ALIGN           ( round up to next multiple of 4 bytes for the remaining code )
1418                 ' DROP ,        ( compile DROP (to drop the length) )
1419         ELSE            ( immediate mode )
1420                 HERE @          ( get the start address of the temporary space )
1421                 BEGIN
1422                         KEY
1423                         DUP '"' <>
1424                 WHILE
1425                         OVER C!         ( save next character )
1426                         1+              ( increment address )
1427                 REPEAT
1428                 DROP            ( drop the final " character )
1429                 0 SWAP C!       ( store final ASCII NUL )
1430                 HERE @          ( push the start address )
1431         THEN
1432 ;
1433
1434 : STRLEN        ( str -- len )
1435         DUP             ( save start address )
1436         BEGIN
1437                 DUP C@ 0<>      ( zero byte found? )
1438         WHILE
1439                 1+
1440         REPEAT
1441
1442         SWAP -          ( calculate the length )
1443 ;
1444
1445 : CSTRING       ( addr len -- c-addr )
1446         SWAP OVER       ( len saddr len )
1447         HERE @ SWAP     ( len saddr daddr len )
1448         CMOVE           ( len )
1449
1450         HERE @ +        ( daddr+len )
1451         0 SWAP C!       ( store terminating NUL char )
1452
1453         HERE @          ( push start address )
1454 ;
1455
1456 (
1457         THE ENVIRONMENT ----------------------------------------------------------------------
1458
1459         Linux makes the process arguments and environment available to us on the stack.
1460
1461         The top of stack pointer is saved by the early assembler code when we start up in the FORTH
1462         variable S0, and starting at this pointer we can read out the command line arguments and the
1463         environment.
1464
1465         Starting at S0, S0 itself points to argc (the number of command line arguments).
1466
1467         S0+4 points to argv[0], S0+8 points to argv[1] etc up to argv[argc-1].
1468
1469         argv[argc] is a NULL pointer.
1470
1471         After that the stack contains environment variables, a set of pointers to strings of the
1472         form NAME=VALUE and on until we get to another NULL pointer.
1473
1474         The first word that we define, ARGC, pushes the number of command line arguments (note that
1475         as with C argc, this includes the name of the command).
1476 )
1477 : ARGC
1478         S0 @ @
1479 ;
1480
1481 (
1482         n ARGV gets the nth command line argument.
1483
1484         For example to print the command name you would do:
1485                 0 ARGV TELL CR
1486 )
1487 : ARGV ( n -- str u )
1488         1+ CELLS S0 @ + ( get the address of argv[n] entry )
1489         @               ( get the address of the string )
1490         DUP STRLEN      ( and get its length / turn it into a FORTH string )
1491 ;
1492
1493 (
1494         ENVIRON returns the address of the first environment string.  The list of strings ends
1495         with a NULL pointer.
1496
1497         For example to print the first string in the environment you could do:
1498                 ENVIRON @ DUP STRLEN TELL
1499 )
1500 : ENVIRON       ( -- addr )
1501         ARGC            ( number of command line parameters on the stack to skip )
1502         2 +             ( skip command line count and NULL pointer after the command line args )
1503         CELLS           ( convert to an offset )
1504         S0 @ +          ( add to base stack address )
1505 ;
1506
1507 (
1508         SYSTEM CALLS AND FILES  ----------------------------------------------------------------------
1509
1510         Miscellaneous words related to system calls, and standard access to files.
1511 )
1512
1513 ( BYE exits by calling the Linux exit(2) syscall. )
1514 : BYE           ( -- )
1515         0               ( return code (0) )
1516         SYS_EXIT        ( system call number )
1517         SYSCALL1
1518 ;
1519
1520 (
1521         UNUSED returns the number of cells remaining in the user memory (data segment).
1522
1523         For our implementation we will use Linux brk(2) system call to find out the end
1524         of the data segment and subtract HERE from it.
1525 )
1526 : GET-BRK       ( -- brkpoint )
1527         0 SYS_BRK SYSCALL1      ( call brk(0) )
1528 ;
1529
1530 : UNUSED        ( -- n )
1531         GET-BRK         ( get end of data segment according to the kernel )
1532         HERE @          ( get current position in data segment )
1533         -
1534         4 /             ( returns number of cells )
1535 ;
1536
1537 (
1538         MORECORE increases the data segment by the specified number of (4 byte) cells.
1539
1540         NB. The number of cells requested should normally be a multiple of 1024.  The
1541         reason is that Linux can't extend the data segment by less than a single page
1542         (4096 bytes or 1024 cells).
1543
1544         This FORTH doesn't automatically increase the size of the data segment "on demand"
1545         (ie. when , (COMMA), ALLOT, CREATE, and so on are used).  Instead the programmer
1546         needs to be aware of how much space a large allocation will take, check UNUSED, and
1547         call MORECORE if necessary.  A simple programming exercise is to change the
1548         implementation of the data segment so that MORECORE is called automatically if
1549         the program needs more memory.
1550 )
1551 : BRK           ( brkpoint -- )
1552         SYS_BRK SYSCALL1
1553 ;
1554
1555 : MORECORE      ( cells -- )
1556         CELLS GET-BRK + BRK
1557 ;
1558
1559 (
1560         Standard FORTH provides some simple file access primitives which we model on
1561         top of Linux syscalls.
1562
1563         The main complication is converting FORTH strings (address & length) into C
1564         strings for the Linux kernel.
1565
1566         Notice there is no buffering in this implementation.
1567 )
1568
1569 : R/O ( -- fam ) O_RDONLY ;
1570 : R/W ( -- fam ) O_RDWR ;
1571
1572 : OPEN-FILE     ( addr u fam -- fd 0 (if successful) | c-addr u fam -- fd errno (if there was an error) )
1573         -ROT            ( fam addr u )
1574         CSTRING         ( fam cstring )
1575         SYS_OPEN SYSCALL2 ( open (filename, flags) )
1576         DUP             ( fd fd )
1577         DUP 0< IF       ( errno? )
1578                 NEGATE          ( fd errno )
1579         ELSE
1580                 DROP 0          ( fd 0 )
1581         THEN
1582 ;
1583
1584 : CREATE-FILE   ( addr u fam -- fd 0 (if successful) | c-addr u fam -- fd errno (if there was an error) )
1585         O_CREAT OR
1586         O_TRUNC OR
1587         -ROT            ( fam addr u )
1588         CSTRING         ( fam cstring )
1589         420 -ROT        ( 0644 fam cstring )
1590         SYS_OPEN SYSCALL3 ( open (filename, flags|O_TRUNC|O_CREAT, 0644) )
1591         DUP             ( fd fd )
1592         DUP 0< IF       ( errno? )
1593                 NEGATE          ( fd errno )
1594         ELSE
1595                 DROP 0          ( fd 0 )
1596         THEN
1597 ;
1598
1599 : CLOSE-FILE    ( fd -- 0 (if successful) | fd -- errno (if there was an error) )
1600         SYS_CLOSE SYSCALL1
1601         NEGATE
1602 ;
1603
1604 : READ-FILE     ( addr u fd -- u2 0 (if successful) | addr u fd -- 0 0 (if EOF) | addr u fd -- u2 errno (if error) )
1605         >R SWAP R>      ( u addr fd )
1606         SYS_READ SYSCALL3
1607
1608         DUP             ( u2 u2 )
1609         DUP 0< IF       ( errno? )
1610                 NEGATE          ( u2 errno )
1611         ELSE
1612                 DROP 0          ( u2 0 )
1613         THEN
1614 ;
1615
1616 (
1617         PERROR prints a message for an errno, similar to C's perror(3) but we don't have the extensive
1618         list of strerror strings available, so all we can do is print the errno.
1619 )
1620 : PERROR        ( errno addr u -- )
1621         TELL
1622         ':' EMIT SPACE
1623         ." ERRNO="
1624         . CR
1625 ;
1626
1627 (
1628         ASSEMBLER CODE ----------------------------------------------------------------------
1629
1630         This is just the outline of a simple assembler, allowing you to write FORTH primitives
1631         in assembly language.
1632
1633         Assembly primitives begin ': NAME' in the normal way, but are ended with ;CODE.  ;CODE
1634         updates the header so that the codeword isn't DOCOL, but points instead to the assembled
1635         code (in the DFA part of the word).
1636
1637         We provide a convenience macro NEXT (you guessed what it does).  However you don't need to
1638         use it because ;CODE will put a NEXT at the end of your word.
1639
1640         The rest consists of some immediate words which expand into machine code appended to the
1641         definition of the word.  Only a very tiny part of the i386 assembly space is covered, just
1642         enough to write a few assembler primitives below.
1643 )
1644
1645 HEX
1646
1647 ( Equivalent to the NEXT macro )
1648 : NEXT IMMEDIATE AD C, FF C, 20 C, ;
1649
1650 : ;CODE IMMEDIATE
1651         [COMPILE] NEXT          ( end the word with NEXT macro )
1652         ALIGN                   ( machine code is assembled in bytes so isn't necessarily aligned at the end )
1653         LATEST @ DUP
1654         HIDDEN                  ( unhide the word )
1655         DUP >DFA SWAP >CFA !    ( change the codeword to point to the data area )
1656         [COMPILE] [             ( go back to immediate mode )
1657 ;
1658
1659 ( The i386 registers )
1660 : EAX IMMEDIATE 0 ;
1661 : ECX IMMEDIATE 1 ;
1662 : EDX IMMEDIATE 2 ;
1663 : EBX IMMEDIATE 3 ;
1664 : ESP IMMEDIATE 4 ;
1665 : EBP IMMEDIATE 5 ;
1666 : ESI IMMEDIATE 6 ;
1667 : EDI IMMEDIATE 7 ;
1668
1669 ( i386 stack instructions )
1670 : PUSH IMMEDIATE 50 + C, ;
1671 : POP IMMEDIATE 58 + C, ;
1672
1673 ( RDTSC instruction )
1674 : RDTSC IMMEDIATE 0F C, 31 C, ;
1675
1676 DECIMAL
1677
1678 (
1679         RDTSC is an assembler primitive which reads the Pentium timestamp counter (a very fine-
1680         grained counter which counts processor clock cycles).  Because the TSC is 64 bits wide
1681         we have to push it onto the stack in two slots.
1682 )
1683 : RDTSC         ( -- lsb msb )
1684         RDTSC           ( writes the result in %edx:%eax )
1685         EAX PUSH        ( push lsb )
1686         EDX PUSH        ( push msb )
1687 ;CODE
1688
1689 (
1690         INLINE can be used to inline an assembler primitive into the current (assembler)
1691         word.
1692
1693         For example:
1694
1695                 : 2DROP INLINE DROP INLINE DROP ;CODE
1696
1697         will build an efficient assembler word 2DROP which contains the inline assembly code
1698         for DROP followed by DROP (eg. two 'pop %eax' instructions in this case).
1699
1700         Another example.  Consider this ordinary FORTH definition:
1701
1702                 : C@++ ( addr -- addr+1 byte ) DUP 1+ SWAP C@ ;
1703
1704         (it is equivalent to the C operation '*p++' where p is a pointer to char).  If we
1705         notice that all of the words used to define C@++ are in fact assembler primitives,
1706         then we can write a faster (but equivalent) definition like this:
1707
1708                 : C@++ INLINE DUP INLINE 1+ INLINE SWAP INLINE C@ ;CODE
1709
1710         One interesting point to note is that this "concatenative" style of programming
1711         allows you to write assembler words portably.  The above definition would work
1712         for any CPU architecture.
1713
1714         There are several conditions that must be met for INLINE to be used successfully:
1715
1716         (1) You must be currently defining an assembler word (ie. : ... ;CODE).
1717
1718         (2) The word that you are inlining must be known to be an assembler word.  If you try
1719         to inline a FORTH word, you'll get an error message.
1720
1721         (3) The assembler primitive must be position-independent code and must end with a
1722         single NEXT macro.
1723
1724         Exercises for the reader: (a) Generalise INLINE so that it can inline FORTH words when
1725         building FORTH words. (b) Further generalise INLINE so that it does something sensible
1726         when you try to inline FORTH into assembler and vice versa.
1727
1728         The implementation of INLINE is pretty simple.  We find the word in the dictionary,
1729         check it's an assembler word, then copy it into the current definition, byte by byte,
1730         until we reach the NEXT macro (which is not copied).
1731 )
1732 HEX
1733 : =NEXT         ( addr -- next? )
1734            DUP C@ AD <> IF DROP FALSE EXIT THEN
1735         1+ DUP C@ FF <> IF DROP FALSE EXIT THEN
1736         1+     C@ 20 <> IF      FALSE EXIT THEN
1737         TRUE
1738 ;
1739 DECIMAL
1740
1741 ( (INLINE) is the lowlevel inline function. )
1742 : (INLINE)      ( cfa -- )
1743         @                       ( remember codeword points to the code )
1744         BEGIN                   ( copy bytes until we hit NEXT macro )
1745                 DUP =NEXT NOT
1746         WHILE
1747                 DUP C@ C,
1748                 1+
1749         REPEAT
1750         DROP
1751 ;
1752
1753 : INLINE IMMEDIATE
1754         WORD FIND               ( find the word in the dictionary )
1755         >CFA                    ( codeword )
1756
1757         DUP @ DOCOL = IF        ( check codeword <> DOCOL (ie. not a FORTH word) )
1758                 ." Cannot INLINE FORTH words" CR ABORT
1759         THEN
1760
1761         (INLINE)
1762 ;
1763
1764 HIDE =NEXT
1765
1766 (
1767         NOTES ----------------------------------------------------------------------
1768
1769         DOES> isn't possible to implement with this FORTH because we don't have a separate
1770         data pointer.
1771 )
1772
1773 (
1774         WELCOME MESSAGE ----------------------------------------------------------------------
1775
1776         Print the version and OK prompt.
1777 )
1778
1779 : WELCOME
1780         S" TEST-MODE" FIND NOT IF
1781                 ." JONESFORTH VERSION " VERSION . CR
1782                 UNUSED . ." CELLS REMAINING" CR
1783                 ." OK "
1784         THEN
1785 ;
1786
1787 WELCOME
1788 HIDE WELCOME
