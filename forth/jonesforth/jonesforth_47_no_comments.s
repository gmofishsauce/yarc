/* This is a chopped copy of jonesforth.s, retaining the line numbers */

   8         .set JONES_VERSION,47

 305 /* NEXT macro. */
 306         .macro NEXT
 307         lodsl
 308         jmp *(%eax)
 309         .endm
 310 

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
 498 /* DOCOL - the interpreter! */
 499         .text
 500         .align 4
 501 DOCOL:
 502         PUSHRSP %esi            // push %esi on to the return stack
 503         addl $4,%eax            // %eax points to codeword, so make
 504         movl %eax,%esi          // %esi point to first data word
 505         NEXT
 506 

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
 648         Similarly I want a way to write words written in assembly language.
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
 689         Now some easy FORTH primitives.
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
 964         defcode "EXIT",4,,EXIT
 965         POPRSP %esi             // pop return stack into %esi
 966         NEXT
 967 
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
1619 */
1620 
1621         defword ">DFA",4,,TDFA
1622         .int TCFA               // >CFA         (get code field address)
1623         .int INCR4              // 4+           (add 4 to it to get to next word)
1624         .int EXIT               // EXIT         (return from FORTH word)
1625 
1626 /*
1627         COMPILING ----------------------------------------------------------------------
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
1913 */
1914 
1915         defcode "IMMEDIATE",9,F_IMMED,IMMEDIATE
1916         movl var_LATEST,%edi    // LATEST word.
1917         addl $4,%edi            // Point to name/flags byte.
1918         xorb $F_IMMED,(%edi)    // Toggle the IMMED bit.
1919         NEXT
1920 
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
1983         defcode "'",1,,TICK
1984         lodsl                   // Get the address of the next word and skip it.
1985         pushl %eax              // Push it on the stack.
1986         NEXT
1987 
1988 /*
1989         BRANCHING ----------------------------------------------------------------------
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

