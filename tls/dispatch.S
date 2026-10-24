.text
.globl __Startup__
__Startup__:
 move.l 6(%sp),%a0
 lea MTDispatch(%pc),%a1
 move.l %a1,(%a0)
 clr.l 4(%a0)
 moveq #0,%d0
 rts
MTDispatch:
 .word MTName-MTDispatch
 .word MossTLSOpen-MTDispatch
 .word MossTLSClose-MTDispatch
 .word MossTLSSleep-MTDispatch
 .word MossTLSWake-MTDispatch
 .word MossTLSVersion-MTDispatch
 .word MossTLSCreate-MTDispatch
 .word MossTLSPump-MTDispatch
 .word MossTLSWrite-MTDispatch
 .word MossTLSRead-MTDispatch
 .word MossTLSDestroy-MTDispatch
 .word MossTLSLastX509Error-MTDispatch
 .word MossTLSGetStats-MTDispatch
MTName:
 .asciz "Moss TLS Library"
.even
.globl MTInvoke
MTInvoke:
 movem.l %a2-%a3/%d2,-(%sp)
 move.l %sp,%a2
 move.l 32(%a2),%sp
 move.l 28(%a2),-(%sp)
 move.l 24(%a2),-(%sp)
 move.l 20(%a2),-(%sp)
 move.l 16(%a2),%a0
 jsr (%a0)
 move.l %a2,%sp
 movem.l (%sp)+,%a2-%a3/%d2
 rts
