# Why is PJIT Fast?

Don't worry, I will be doing a follow-up to this called "why PJIT is slow", comparing it to bleeding-edge JIT engines like Emu68. But against classic interpreters, PJIT is very, very fast. 

We'll be examining four approaches that lead to our performance advantage over classic interpreters
- subroutine threading
- single instruction decode
- flag elimination
- instruction inlining

## Subroutine Threading

One of the fastest ARM 68000 interpreters comes in the form of Cyclone 68000. It achieves this by writing out -- in assembly -- every opcode and then performs a direct-token-threaded dispatch to the next opcode. Let's take a look at a very basic opcode -- MOVE.B:
```
;@ ---------- [1000] move.b d0, d0 uses Op1000 ----------
Op1000:
                   
  and r1,r8,#0x000f        ;@ EaCalc : Get register index into r1
  mov r1,r1,lsl #2
  ldrsb r1,[r7,r1]         ;@ EaRead : Read register[r1] into r1:

  adds r1,r1,#0            ;@ Defines NZ, clears CV
  mrs r9,cpsr              ;@ r9=NZCV flags
                   
  and r0,r8,#0x0e00        ;@ EaCalc : Get register index into r0:
  strb r1,[r7,r0,lsr #7]   ;@ EaWrite: r1 into register[r0]:

  ldrh r8,[r4],#2          ;@ Fetch next opcode
  subs r5,r5,#4            ;@ Subtract cycles
  ldrge pc,[r6,r8,asl #2]  ;@ Jump to opcode handler
```

Here we see very broadly four blocks of code; the first half of the move to a temporary register, testing and then saving the flags, the second half of the move which saves the byte to the other register and then the dispatch to the next opcode. This takes ten instructions and about nine clock cycles -- or does it -- that last opcode will never be predicted correctly and will cause a pipeline stall, and on the Cortex A8 that's thirteen clock cycles. Surprisingly, this is still really fast for a classic interpreter.

Our first improvement is to change the token-threading to subroutine-threading and avoid the branch penalty. This relocates the instruction decode to occur only when the cache does not already contain the correct opcodes while the cache itself is simply an uninterrupted sequence of branches.

This changes the above to:
```
  ...
  b #offs                  ;@ Jump to opcode handler from our cache
  ...

;@ ---------- [1000] move.b d0, d0 uses Op1000 ----------
Op1000:
                   
  and r1,r8,#0x000f        ;@ EaCalc : Get register index into r1
  mov r1,r1,lsl #2
  ldrsb r1,[r7,r1]         ;@ EaRead : Read register[r1] into r1:

  adds r1,r1,#0            ;@ Defines NZ, clears CV
  mrs r9,cpsr              ;@ r9=NZCV flags
                   
  and r0,r8,#0x0e00        ;@ EaCalc : Get register index into r0:
  strb r1,[r7,r0,lsr #7]   ;@ EaWrite: r1 into register[r0]:

  bx lr                    ;@ Return
```

We've shaved off three instructions; two of which are load operations and we've eliminated the branch misprediction penalty bringing our cycle count down to only seven cycles. That's a huge improvement for one small change!

## Single Instruction Decode

Now we have the two halves of the MOVE operation. Much of the work here is decoding the opcode twice-again. Again? Well yeah, the opcode is decoded once generating the branch, but the opcode table repeats this same opcode for each permutation of source and destination registers. It's on the opcode to then also decode WHICH registers these are to perform the MOVE. But, we already knew this back on the first dispatch.

Instead, we'll "unroll" this opcode handler for each and every register combination and only decode this once.
```
  ...
  b #offs                  ;@ to get here...            
  ...

;@ ---------- [1000] move.b d0, d0 uses Op1000 ----------
Op1000:

  ldrb r2,[r7,#4]          ;@ load byte from D1
  adds r2,r2,#0            ;@ Defines NZ, clears CV
  mrs r9,cpsr              ;@ r9=NZCV flags
  strb r2,[r7]             ;@ store in D0
  bx lr                    ;@ and return
```

This substantially reduces the size of the individual opcode, but this does need more over-all space since we need this routine for all 64 combinations of source and destination register. This may put more stress on the L2 cache if we exceed 256KB, so we'll have to watch this!

## Flag Elimination

I think that's looking a lot better! But if we don't need to set the flags because the next instruction does, then we can omit that entirely! This requires our opcode decode routine to be a little smarter.
```
  b #offs                  ;@ to get here...            

;@ ---------- [1000] move.b d0, d0 uses Op1000 ----------
Op1000:

  ldrb r2,[r7,#4]          ;@ load byte from D1
  strb r2,[r7]             ;@ store in D0

  bx lr                    ;@ and return
```

## Instruction Inlining

Let's review how a PJIT thread is executed.
```
[ op 1 ] --> [ bl op_1_op ] -> op_1_op: ... b lr
                                             |
                .-------------<<-------------'
                |
[ data ] --> [ nop ]
                |
                v
[ op 3 ] --> [ bl op_3_op ] -> op_3_op: ... b lr
                                             |
                .-------------<<-------------'
                |
[ op 4 ] --> [ bl op_4_op ] -> op_4_op: ... b lr
                                             |
                .-------------<<-------------'
                |
[ data ] --> [ nop ]
                |
                v
[ data ] --> [ nop ]
                |
                v
[ op 7 ] --> [ bl op_7_op ] -> op_4_op: ... b lr
                                             |
                .-------------<<-------------'
                |
[ op 8 ] --> [ bl op_8_op ] -> op_4_op: ... b lr

^------^                                         Original 68K program
             ^------------^                      PJIT "cache"
                               ^---------------^ Actual opcode routine
```

The PJIT cache is populated by default with a routine which reads the
68K program memory and performs a substitution of the cache line with
the branch-and-link insutrction to the proper opcode. Before we go to
the next 68K opcode, we'll execute this opcode immediately.

When the replacement opcode is smaller than the ARM branch we can simply execute it in-place and omit both branches. The most important of these is the branch -- when a branch is within the same L3 page, then we can always reduce this to one ARM instruction and inline it. Not other form of JIT is able to jump into random locations of memory! 

While this would naively include all single ARM instruction opcodes, it actually includes many more. Extension words and operands are also part of the original instruction and in the thread table we replace all of these with NOPs; this allows for potentially much larger opcodes to get inlined.

Anyway, next time I'll compare PJIT to Emu68 in "Why is PJIT Slow?".
