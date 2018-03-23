/* Barrelfish THC language extensions */

#ifndef _THC_INTERNAL_H_
#define _THC_INTERNAL_H_

/***********************************************************************/
typedef struct ptstate_t PTState_t;
typedef struct thcstack_t thcstack_t;
typedef struct finish_t finish_t;

struct awe_t {
  // Fields representing the code to run when the AWE is executed.
  void  *eip;
  void  *ebp;
  void  *esp;

  // Link from an AWE to the per-thread state for the thread it
  // runs in.
  //PTState_t *pts;

  // Link from an AWE to the immediately-enclosing finish
  //finish_t *current_fb;

  // Fields used by the runtime system to link together AWEs, e.g.,
  // on a thread's run-queue, or on a list of waiters on a
  // synchronization object.
  struct awe_t *prev;
  struct awe_t *next;
};

/***********************************************************************/

// Definition of a finish block's data structure.  
//
// Finish blocks are held on a linked list threaded through the start_node
// and end_node fields.  The blocks dynamically nested within a given
// finish block are held between these two nodes.  (This enables easy
// iteration through all these dynamically nested blocks).

typedef struct finish_list_t finish_list_t;

struct finish_list_t {
  finish_list_t  *prev;
  finish_list_t  *next;
  finish_t       *fb;
};

struct finish_t {
  void           *old_sp;    /* stack pointer when entering do {} finish */ 
  unsigned long   count;
  struct awe_t    *finish_awe;
  //finish_list_t   start_node;
  //finish_list_t   end_node;
};

/***********************************************************************/

// Per-thread runtime system state
// A thread stack can be either on the free list of available stacks, or 
// on a pending list --- stacks that are pending to be deallocated
struct thcstack_t {
  struct thcstack_t *next;
};

struct awe_table;

struct ptstate_t {

  // Thread-local fields: .............................................

  // Head/tail sentinels of the dispatch list
  struct awe_t aweHead;
  struct awe_t aweTail;

  // Immediately-enclosing finish block for the currently running code
  finish_t *current_fb;

  // Initialization / termination flags
  int doneInit;
  int shouldExit;

  // Stack that the thread's dispatch loop will run on
  void *dispatchStack;

  // Function to execute whenever the dispatch loop is idle (e.g.,
  // to block the thread until an incoming message which might change
  // the state of the dispatch loop).
  THCIdleFn_t idle_fn;
  void *idle_args;
  void *idle_stack;

  // Stack to be de-allocated on the next execution of the dispatch loop
  // (an async call terminates by re-entering the dispatch loop with
  // pendingFree set to the stack it was using.  It cannot dealloacte
  // its own stack while it is in use).
  struct thcstack_t *pendingFree;

  // AWE to enter for the dispatch loop on this thread
  struct awe_t dispatch_awe;

  // Free stacks for re-use
  struct thcstack_t *free_stacks;

  // Map for resolving integer IDs to awe's. This is used in the
  // async ipc code.
  struct awe_table *awe_map;
  

  // Pointer to the direct continutation (i.e., when ASYNC is done or blocks 
  // it's a pointer to the do...finish continuation -- it's nown directly and 
  // does not require going into the dispatch loop
  awe_t *direct_cont; 

};

PTState_t *PTS(void);

typedef void (*THCContFn_t)(void *cont, void *args);
typedef void (*THCContPTSFn_t)(void *cont, void *args, void *pts);

void *_thc_allocstack(void);
void _thc_freestack_void(PTState_t *pts, void *s);
void _thc_freestack(PTState_t *pts, struct thcstack_t *s);

void _thc_onaltstack(void *s, void *fn, void *args);
void _thc_startasync(void *f, void *stack);

void _thc_endasync(void *f, void *s);
void _thc_startfinishblock(finish_t *fb);
void _thc_endfinishblock(finish_t *fb);

void _thc_do_cancel_request(finish_t *fb);
void _thc_callcont(struct awe_t *awe, THCContFn_t fn, void *args) __attribute__((returns_twice));
void _thc_callcont_direct(struct awe_t *awe, void *args, THCContFn_t fn) __attribute__((returns_twice));

void _thc_callcont_pts(struct awe_t *awe, THCContFn_t fn, void *args, PTState_t *pts) __attribute__((returns_twice));

void _thc_callcont_pts_direct(struct awe_t *awe, void *args, PTState_t *pts, THCContFn_t fn) __attribute__((returns_twice));


void _thc_exec_awe_direct(struct awe_t *awe_from, struct awe_t *awe_to) __attribute__((returns_twice));

int  _thc_schedulecont(struct awe_t *awe) __attribute__((returns_twice));
void _thc_lazy_awe_marker(void);
void _thc_pendingfree(void);

/***********************************************************************/

// Symbols declared in the .text.nx section

extern int _start_text_nx;
extern int _end_text_nx;

/***********************************************************************/

/* Macro to force callee-saves to be spilled to the stack */

#if defined(__x86_64__) 
#define KILL_CALLEE_SAVES()						\
  __asm__ volatile ("" : : : "rbx", "r12", "r13", "r14", "r15",         \
		    "memory", "cc")
#elif defined(__i386__)
#ifdef __pic__
#define KILL_CALLEE_SAVES()					        \
  __asm__ volatile ("" : : : "edi", "esi", "esp", "memory", "cc")
#else
#define KILL_CALLEE_SAVES()						\
  __asm__ volatile ("" : : : "ebx", "edi", "esi", "esp", "memory", "cc")
#endif
#else
#error "Need definition of KILL_CALLEE_SAVES" 
#endif

#define __WORD_SIZE (sizeof(void*))

 
/* EAGER_THC */

/***********************************************************************/

// not required in the  lazy CALL_CONT in the eager version
#define FORCE_FRAME_POINTER_USE      /* Not used */ do {} while(0)
#define GET_STACK_POINTER(_)         /* Not used */
#define RESTORE_OLD_STACK_POINTER(_) /* Not used */


// SWIZZLE_DEF:
//  - _NAME: name of the function
//  - _NS:   new stack, address just above top of commited region
//  - _FN:   (nested) function to call:  void _FN(void)

#if (defined(__x86_64__) && (defined(linux) || defined(BARRELFISH) || defined(LINUX_KERNEL)))
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
#define SWIZZLE_DEF(_NAME,_NS,_FN) SWIZZLE_DEF_(_NAME,_NS,_FN)
#elif (defined(__i386__) && (defined(linux) || defined(BARRELFISH) || defined(LINUX_KERNEL)))
#define SWIZZLE_DEF(_NAME,_NS,_FN)                                      \
  __attribute__((noinline)) void _NAME(void) {                          \
    __asm__ volatile("movl %0, %%edx           \n\t"			\
                     "subl $4, %%edx           \n\t"			\
                     "movl %%esp, (%%edx)      \n\t"			\
                     "movl %%edx, %%esp        \n\t"			\
                     "call " _FN "             \n\t"			\
                     "pop %%esp                \n\t"			\
                     :							\
                     : "m" (_NS)                                        \
                     : "memory", "cc", "eax", "edx");			\
  }
#else
#error "No definition of SWIZZLE_DEF for THC"
#endif

/***********************************************************************/

#define SCHEDULE_CONT(_AWE_PTR)                 \
  ({                                            \
    KILL_CALLEE_SAVES();                        \
    _thc_schedulecont((awe_t*)_AWE_PTR);        \
  })

#define CALL_CONT(_FN,_ARG)                                     \
  do {                                                          \
    awe_t _awe;                                                 \
    KILL_CALLEE_SAVES();                                        \
    _thc_callcont_direct(&_awe, (_ARG), (THCContFn_t)(_FN));    \
    /*  _thc_callcont(&_awe, (THCContFn_t)(_FN), (_ARG)); */    \
  } while (0)

#define CALL_CONT_PTS(_FN,_ARG,_pts)                                     \
  do {                                                          \
    awe_t _awe;                                                 \
    KILL_CALLEE_SAVES();                                        \
    _thc_callcont_pts_direct(&_awe, (_ARG), _pts, (THCContFn_t)(_FN));           \
  } while (0)


#define CALL_CONT_AWE(_awe, _FN,_ARG)                           \
  do {                                                          \
    KILL_CALLEE_SAVES();                                        \
    _thc_callcont_direct(_awe, (_ARG), (THCContFn_t)(_FN));           \
  } while (0)


// no lazy CALL_CONT in the eager version
#define CALL_CONT_LAZY CALL_CONT

#define CALL_CONT_AND_SAVE(_FN,_IDNUM,_ARG)                      \
  do {                                                           \
    awe_t* _awe;                                                 \
    _awe = awe_mapper_get_awe(_IDNUM);	                         \
    KILL_CALLEE_SAVES();                                         \
    _thc_callcont_direct(_awe, (_ARG), (THCContFn_t)(_FN));   \
    /* _thc_callcont(_awe, (THCContFn_t)(_FN), (_ARG)); */       \
  } while (0)

#define CALL_CONT_LAZY_AND_SAVE CALL_CONT_AND_SAVE

#define CALL_CONT_AND_SAVE_PTS(_FN,_IDNUM,_ARG, _pts)           \
  do {                                                          \
    awe_t* _awe;                                                \
    _awe = _awe_mapper_get_awe(_pts->awe_map, _IDNUM);	        \
    KILL_CALLEE_SAVES();                                        \
    _thc_callcont_pts_direct(_awe, (_ARG), _pts, (THCContFn_t)(_FN));   \
  } while (0)


#define EXEC_AWE_AND_SAVE(_IDNUM,_AWE_TO)                       \
  do {                                                          \
    awe_t* _awe;                                                 \
    _awe = awe_mapper_get_awe(_IDNUM);			      	\
    KILL_CALLEE_SAVES();                                        \
    _thc_exec_awe_direct(_awe, (_AWE_TO));                     \
  } while (0)

#define EXEC_AWE(_awe_from,_awe_to)                             \
  do {                                                          \
    KILL_CALLEE_SAVES();                                        \
    _thc_exec_awe_direct(_awe_from, _awe_to);                   \
  } while (0)



#endif // _THC_INTERNAL_H_
