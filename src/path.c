//NOTE: THCYieldToIdAndSaveNoDispatch assumes single thread, which for the case of LCDs is fine
int
LIBASYNC_FUNC_ATTR 
THCYieldToIdAndSaveNoDispatch(uint32_t id_to, uint32_t id_from) {
  awe_t *awe_to = (awe_t *)awe_mapper_get_awe_ptr(id_to);

  if (!awe_to)
    return -1; // id_to not valid

  CALL_CONT_LAZY_AND_SAVE((void*)&thc_yieldto_with_cont_no_dispatch, id_from, (void*)awe_to);

  return 0;
}

#define CALL_CONT_AND_SAVE(_FN,_IDNUM,_ARG)                     \
  do {                                                          \
    awe_t _awe_from;                                            \
    awe_mapper_set_id((_IDNUM), &_awe_from);            	\
    KILL_CALLEE_SAVES();                                        \
    _thc_callcont(&_awe_from, (THCContFn_t)(_FN), (_ARG));      \
  } while (0)




/*
           void _thc_callcont(awe_t *awe,   // rdi
                   THCContFn_t fn,          // rsi
                   void *args) {            // rdx
*/

__asm__ ("      .text \n\t"
         "      .align  16           \n\t"
         "      .globl  _thc_callcont \n\t"
         "      .type   _thc_callcont, @function \n\t"
         "_thc_callcont:             \n\t"
         " mov  0(%rsp), %rax        \n\t"
         " mov  %rax,  0(%rdi)       \n\t" // EIP (our return address)
         " mov  %rbp,  8(%rdi)       \n\t" // EBP
         " mov  %rsp, 16(%rdi)       \n\t" // ESP+8 (after return)
         " addq $8,   16(%rdi)       \n\t"
         // AWE now initialized.  Call into C for the rest.
         // rdi : AWE , rsi : fn , rdx : args
         " call _thc_callcont_c      \n\t"
         " int3\n\t");

// This function is not meant to be used externally, but its only use
// here is from within the inline assembly language functions.  The
// C "used" attribute is not currently maintained through Clang & LLVM
// with the C backend, so we cannot rely on that.
extern void _thc_callcont_c(awe_t *awe_from, THCContFn_t fn, void *args);
void 
LIBASYNC_FUNC_ATTR 
_thc_callcont_c(awe_t *awe_from,
                     THCContFn_t fn,
                     void *args) {
  fn(awe_from, args);
}


__attribute__ ((unused))
static inline void thc_yieldto_with_cont_no_dispatch(void *a, void *arg) {
    thc_yieldto_with_cont_should_dispatch(a, arg, 0);
}


//helper function for thc_yieldto_with_cont* functions
//use_dispatch indicates whether to assume AWEs are in the dispatch loop
static inline void 
thc_yieldto_with_cont_should_dispatch(void* a, void* arg , int use_dispatch)
{
  awe_t *awe;
  DEBUG_YIELD(DEBUGPRINTF(DEBUG_YIELD_PREFIX "! %p (%p,%p,%p) yield\n",
                          a,
                          ((awe_t*)a)->eip,
                          ((awe_t*)a)->ebp,
                          ((awe_t*)a)->esp));

  awe_t *awe_from = (awe_t*)a; 

  awe_to = (awe_t *)arg;

  if( use_dispatch )
  {
      THCScheduleBack(awe_from);
      // Bug in original Barrelfish version; awe wasn't removed from
      // dispatch queue when we yielded to it here. (Note that in
      // dispatch loop awe's are removed from the dispatch queue
      // when we yield to them.)
      remove_awe_from_list(awe_to);
  }

  awe_to->pts->current_fb = awe_to->current_fb;
  thc_awe_execute_0(awe_to);
}

/*
            static void thc_awe_execute_0(awe_t *awe)    // rdi
*/
__asm__ ("      .text \n\t"
         "      .align  16                 \n\t"
         "thc_awe_execute_0:               \n\t"
         " mov 8(%rdi), %rbp               \n\t"
         " mov 16(%rdi), %rsp              \n\t"
         " jmp *0(%rdi)                    \n\t");


Dump of assembler code for function THCYieldAndSaveNoDispatch:
   0x00000000004014fe <+0>:	push   %rbp
   0x00000000004014ff <+1>:	mov    %rsp,%rbp
   0x0000000000401502 <+4>:	push   %r15
   0x0000000000401504 <+6>:	push   %r14
   0x0000000000401506 <+8>:	push   %r13
   0x0000000000401508 <+10>:	push   %r12
   0x000000000040150a <+12>:	push   %rbx
   0x000000000040150b <+13>:	sub    $0x58,%rsp
   0x000000000040150f <+17>:	mov    %edi,-0x74(%rbp)
   0x0000000000401512 <+20>:	lea    -0x70(%rbp),%rdx
   0x0000000000401516 <+24>:	mov    -0x74(%rbp),%eax
   0x0000000000401519 <+27>:	mov    %rdx,%rsi
   0x000000000040151c <+30>:	mov    %eax,%edi
   0x000000000040151e <+32>:	callq  0x400982 <awe_mapper_set_id>
   0x0000000000401523 <+37>:	lea    -0x70(%rbp),%rax
   0x0000000000401527 <+41>:	mov    $0x0,%edx
   0x000000000040152c <+46>:	mov    $0x40148a,%esi
   0x0000000000401531 <+51>:	mov    %rax,%rdi
   0x0000000000401534 <+54>:	callq  0x401e90 <_thc_callcont>
=> 0x0000000000401539 <+59>:	add    $0x58,%rsp
   0x000000000040153d <+63>:	pop    %rbx
   0x000000000040153e <+64>:	pop    %r12
   0x0000000000401540 <+66>:	pop    %r13
   0x0000000000401542 <+68>:	pop    %r14
   0x0000000000401544 <+70>:	pop    %r15
   0x0000000000401546 <+72>:	pop    %rbp
   0x0000000000401547 <+73>:	retq

