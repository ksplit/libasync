
#define DO_FINISH(_CODE)					\
  do {                                                                  \
    finish_t _fb;                                                       \
    finish_t *_fb_info __attribute__((unused)) = &_fb;                  \
    finish_t *_fb_info_ ## ___ __attribute__((unused)) = _fb_info;     \
    _thc_startfinishblock(_fb_info);				\
    do { _CODE } while (0);                                             \
    _thc_endfinishblock(_fb_info);				\
} while (0)


/* do...finish blocks */
// Implementation of finish blocks
//
// The implementation of finish blocks is straightforward:
// _thc_endfinishblock yields back to the dispatch loop if it finds
// the count non-zero, and stashes away a continuation in
// fb->finish_awe which will be resumed when the final async
// call finsihes.  _thc_endasync picks this up.

void
LIBASYNC_FUNC_ATTR 
_thc_startfinishblock(finish_t *fb) {
  PTState_t *pts = PTS();
 
  fb->count = 0;
  fb->finish_awe = NULL;
  pts->current_fb = fb;

  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "> StartFinishBlock (%p)\n",
                           fb));

}

__attribute__ ((unused))
static inline void _thc_endfinishblock0(void *a, void *f) {
  finish_t *fb = (finish_t*)f;

  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "  Waiting f=%p awe=%p\n",
                           fb, a));
  assert(fb->finish_awe == NULL);
  fb->finish_awe = a;
  thc_dispatch(PTS());
  NOT_REACHED;
}

void
LIBASYNC_FUNC_ATTR 
_thc_endfinishblock(finish_t *fb) {

  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "> EndFinishBlock(%p)\n",
                           fb));
  
  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "  count=%d\n",
                           (int)fb->count));
  if (fb->count == 0) {
    // Zero first time.  Check there's not an AWE waiting.
    assert(fb->finish_awe == NULL);
  } else {
    // Non-zero first time, add ourselves as the waiting AWE.
    CALL_CONT_LAZY((unsigned char*)&_thc_endfinishblock0, fb);
  }

}
EXPORT_SYMBOL(_thc_endfinishblock);

#define CALL_CONT(_FN,_ARG)                                     \
  do {                                                          \
    awe_t _awe;                                                 \
    KILL_CALLEE_SAVES();                                        \
    _thc_callcont(&_awe, (THCContFn_t)(_FN), (_ARG));           \
  } while (0)

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



void
LIBASYNC_FUNC_ATTR 
_thc_startasync(void *f, void *stack) {
  finish_t *fb = (finish_t*)f;
  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "> StartAsync(%p,%p)\n",
                           fb, stack));
  fb->count ++;
  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "< StartAsync count now %d\n",
                           (int)fb->count));
}
EXPORT_SYMBOL(_thc_startasync);

void 
LIBASYNC_FUNC_ATTR 
_thc_endasync(void *f, void *s) {
  finish_t *fb = (finish_t*)f;
  PTState_t *pts = PTS();
  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "> EndAsync(%p,%p)\n",
                           fb, s));
  assert(fb->count > 0);
  fb->count --;
  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "  count now %d\n",
                           (int)fb->count));
  assert(pts->pendingFree == NULL);

  pts->pendingFree = s;

  if (fb->count == 0) {
    if (fb->finish_awe) {
      DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "  waiting AWE %p\n",
                               fb->finish_awe));
      thc_schedule_local(fb->finish_awe);
      fb->finish_awe = NULL;
    }  
  }

  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "< EndAsync\n"));
  thc_dispatch(pts);
  NOT_REACHED;
}
EXPORT_SYMBOL(_thc_endasync);

static inline void thc_dispatch(PTState_t *pts) {
  assert(pts && pts->doneInit && "Not initialized RTS");
  thc_awe_execute_0(&pts->dispatch_awe);
}

__attribute__ ((unused))
static void re_init_dispatch_awe(void *a, void *arg) {
  
  PTState_t *pts = PTS();
  
  awe_t *awe = (awe_t *)a;
  pts->dispatch_awe = *awe;
  thc_dispatch(pts);
}


// Dispatch loop
//
// Currently, this maintains a doubly-linked list of runnable AWEs.
// New AWEs are added to the tail of the list.  Execution proceeds from
// the head.
//
// dispatch_awe is initialized to refer to the entry point for the
// "dispatch loop" function.

static void thc_dispatch_loop(void) {
  PTState_t *pts = PTS();
  awe_t *awe;

  // Re-initialize pts->dispatch_awe to this point, just after we have
  // read PTS.  This will save the per-thread-state access on future
  // executions of the function.
  CALL_CONT((unsigned char*)&re_init_dispatch_awe, NULL);

  DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX "> dispatch_loop\n"));

  thc_pendingfree(pts);

  if (pts->aweHead.next == &pts->aweTail) {
    awe_t idle_awe;
    void *idle_stack = _thc_allocstack();

    DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX "  queue empty\n"));
    assert(pts->idle_fn != NULL && "Dispatch loop idle, and no idle_fn work");

    // Set start of stack-frame marker
    *((void**)(idle_stack  - LAZY_STACK_BUFFER + __WORD_SIZE)) = NULL;
    thc_awe_init(&idle_awe, &thc_run_idle_fn, idle_stack  -LAZY_STACK_BUFFER,
                 idle_stack -LAZY_STACK_BUFFER);
    pts->idle_stack = idle_stack;
    pts->current_fb = NULL;
    
    DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX "  executing idle function\n"));
    thc_awe_execute_0(&idle_awe);
    NOT_REACHED;
  }
  awe = pts->aweHead.next;

  DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX "  got AWE %p "
			     "(ip=%p, sp=%p, fp=%p)\n",
			     awe, awe->eip, awe->esp, awe->ebp));
  pts->aweHead.next = awe->next;
  //pts->current_fb = awe->current_fb;
  awe->next->prev = &(pts->aweHead);
  thc_awe_execute_0(awe);
}



#define ASYNC_(_BODY, _C)						\
  do {									\
    awe_t _awe;                                                         \
    void *_new_stack = _thc_allocstack();		       		\
    /* Define nested function containing the body */			\
    auto void __attribute__ ((noinline))                 \
     __attribute__((used)) _thc_nested_async(void) __asm__(NESTED_FN_STRING(_C));    \
    __attribute__((used)) void __attribute__ ((noinline)) _thc_nested_async(void) {       \
      void *_my_fb = _fb_info;						\
      void *_my_stack = _new_stack;                                     \
      _thc_startasync(_my_fb, _my_stack);                               \
      do { _BODY; } while (0);						\
      _thc_endasync(_my_fb, _my_stack);					\
      /*assert(0 && "_thc_endasync returned");*/				\
    }									\
                                                                        \
    /* Define function to enter _nested on a new stack */               \
    auto void _swizzle(void) __asm__(SWIZZLE_FN_STRING(_C));            \
    SWIZZLE_DEF(_swizzle, _new_stack, NESTED_FN_STRING(_C));            \
    /*_awe.current_fb = _fb_info;*/					\
                                                                        \
    /* Add AWE for our continuation, then run "_nested" on new stack */	\
    if (!SCHEDULE_CONT(&_awe)) {                                        \
      _swizzle();							\
      /*assert(0 && "_swizzle returned");*/					\
    }                                                                   \
  } while (0)


// SWIZZLE_DEF:
//  - _NAME: name of the function
//  - _NS:   new stack, address just above top of commited region
//  - _FN:   (nested) function to call:  void _FN(void)


#define SWIZZLE_DEF_(_NAME,_NS,_FN)                                     \
  __attribute__ ((noinline)) void _NAME(void) {                                           \
    __asm__ volatile("movq %0, %%rdi      \n\t" /* put NS to %rdi   */  \
                     "subq $8, %%rdi      \n\t" /* fix NS address   */  \
                     "movq %%rsp, (%%rdi) \n\t" /* store sp to NS   */  \
                     "movq %%rdi, %%rsp   \n\t" /* set sp to NS     */  \
                     "call " _FN "        \n\t" /* call _FN         */  \
                     "popq %%rsp          \n\t" /* restore old sp   */  \
                     :                                                  \
                     : "m" (_NS)                                        \
                     : "memory", "cc", "rsi", "rdi");                   \
  }


#define SCHEDULE_CONT(_AWE_PTR)                 \
  ({                                            \
    KILL_CALLEE_SAVES();                        \
    _thc_schedulecont((awe_t*)_AWE_PTR);        \
  })

/*
           int _thc_schedulecont(awe_t *awe)   // rdi
*/

__asm__ ("      .text \n\t"
         "      .align  16           \n\t"
         "      .globl  _thc_schedulecont \n\t"
         "      .type   _thc_schedulecont, @function \n\t"
         "_thc_schedulecont:         \n\t"
         " mov  0(%rsp), %rsi        \n\t"
         " mov  %rsi,  0(%rdi)       \n\t" // EIP   (our return address)
         " mov  %rbp,  8(%rdi)       \n\t" // EBP
         " mov  %rsp, 16(%rdi)       \n\t" // ESP+8 (after return)
         " addq $8,   16(%rdi)       \n\t"
         // AWE now initialized.  Call C function for scheduling.
         // It will return normally to us.  The AWE will resume
         // directly in our caller.
         " call _thc_schedulecont_c  \n\t"  // AWE still in rdi
         " movq $0, %rax             \n\t"
         " ret                       \n\t");

// This function is not meant to be used externally, but its only use
// here is from within the inline assembly language functions.  The
// C "used" attribute is not currently maintained through Clang & LLVM
// with the C backend, so we cannot rely on that.
extern void _thc_schedulecont_c(awe_t *awe);
void _thc_schedulecont_c(awe_t *awe) {
  //PTState_t *pts = PTS();
  //awe->pts = pts;
  thc_schedule_local(awe);
}

// Add the supplied AWE to the dispatch queue
//
// By default we add to the head.  This means that in the implementation
// of "X ; async { Y } ; Z" we will run X;Y;Z in sequence (assuming that
// Y does not block).  This relies on Z being put at the head of the
// queue.

static inline void thc_schedule_local(awe_t *awe) {
  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "> THCSchedule(%p)\n",
                        awe));
  awe->prev = &(PTS()->aweHead);
  awe->next = PTS()->aweHead.next;
  PTS()->aweHead.next->prev = awe;
  PTS()->aweHead.next = awe;
  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "  added AWE between %p %p\n",
                        awe->prev, awe->next));
  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "< THCSchedule\n"));
}


// Allocate a new stack, returning an address just above the top of
// the committed region.  The stack comprises STACK_COMMIT_BYTES
// followed by an inaccessible STACK_GUARD_BYTES.
//
// There is currently no support for extending a stack, or allowing it
// to be discontiguous
void * 
LIBASYNC_FUNC_ATTR 
_thc_allocstack(void) {
  PTState_t *pts = PTS();
  void *result = NULL;
  DEBUG_STACK(DEBUGPRINTF(DEBUG_STACK_PREFIX "> AllocStack\n"));
  if (pts->free_stacks != NULL) {
    // Re-use previously freed stack
    struct thcstack_t *r = pts->free_stacks;
    DEBUG_STACK(DEBUGPRINTF(DEBUG_STACK_PREFIX "  Re-using free stack\n"));
    pts->free_stacks = pts->free_stacks->next;
    result = ((void*)r) + sizeof(struct thcstack_t);
  } else {
    result = (void*)thc_alloc_new_stack_0();
  }
  DEBUG_STACK(DEBUGPRINTF(DEBUG_STACK_PREFIX "< AllocStack = %p\n", result));
  return result;
}

