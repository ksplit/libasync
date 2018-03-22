// THC runtime system
//
// Naming conventions:
//
//  THCStudlyCaps    - Functions for use from user code.
//
//  _thc_...         - Intrinsic functions, called from compiler-generated
//                     code.  Their prototypes must match Intrinsics.td
//
//  thc_lower_case   - Internal functions used in this library.
//
//  thc_lower_case_0 - Arch-OS specific functions (implemented at the
//                     bottom of this file).

#ifdef LINUX_KERNEL
#undef linux
#endif

#ifndef LINUX_KERNEL
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#endif

#include <thc.h>
#include <awe_mapper.h>

#ifdef LINUX_KERNEL
#ifdef LCD_DOMAINS
#include <lcd_config/pre_hook.h>
#endif
#include <asm/page.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/sched.h>
#include <awe_mapper.h>
#ifdef LCD_DOMAINS
#include <lcd_config/post_hook.h>
#endif
#undef DEBUG_STACK
#endif

#ifndef LINUX_KERNEL_MODULE
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)
#endif

#ifdef linux
#include <pthread.h>
#endif

/* It is necessary to set the esp of a lazy awe some way into it's lazy */
/* allocated stack, so that it can pass arguments below its current esp */
/* This value defines the size of the buffer (should be more than size  */
/* of arguments passed to any function call).                            */
#define LAZY_STACK_BUFFER 512

#define NOT_REACHED assert(0 && "Not reached")


#if defined(LINUX_KERNEL)
#define DEBUGPRINTF printk
#else
#define DEBUGPRINTF printf
#endif

#ifdef VERBOSE_DEBUG
#define DEBUG_YIELD(XX) do{ XX; } while (0)
#define DEBUG_STACK(XX) do{ XX; } while (0)
#define DEBUG_AWE(XX) do{ XX; } while (0)
#define DEBUG_FINISH(XX) do{ XX; } while (0)
#define DEBUG_CANCEL(XX) do{ XX; } while (0)
#define DEBUG_INIT(XX) do{ XX; } while (0)
#define DEBUG_DISPATCH(XX) do{ XX; } while (0)
#else
#define DEBUG_YIELD(XX)
#define DEBUG_STACK(XX)
#define DEBUG_AWE(XX)
#define DEBUG_FINISH(XX) 
#define DEBUG_CANCEL(XX)
#define DEBUG_INIT(XX)
#define DEBUG_DISPATCH(XX)
#endif

#if !defined(VERBOSE_ASSERT)
#undef assert
#define assert(XX)
#endif

#define DEBUG_YIELD_PREFIX        "         yield:    "
#define DEBUG_STACK_PREFIX        "         stack:    "
#define DEBUG_AWE_PREFIX          "         awe:      "
#define DEBUG_FINISH_PREFIX       "         finish:   "
#define DEBUG_CANCEL_PREFIX       "         cancel:   "
#define DEBUG_INIT_PREFIX         "         init:     "
#define DEBUG_DISPATCH_PREFIX     "         dispatch: "

// Prototypes
// 
// NB: those marked as "extern" are actually defined in this same file,
// but the entire function (including label, prolog, epilogue, etc) is
// in inline-asm, and so the definition is not visible to the compiler.

static void thc_awe_init(awe_t *awe, void *eip, void *ebp, void *esp);
static void thc_dispatch(PTState_t *pts);

extern void thc_awe_execute_0(awe_t *awe);
//extern void thc_on_alt_stack_0(void *stacktop, void *fn, void *args);
static void *thc_alloc_new_stack_0(void);
extern void thc_link_to_frame_0(void *frame, void *fn, void *args);

static PTState_t *thc_get_pts_0(void);
static void thc_set_pts_0(PTState_t *pts);

static inline void thc_schedule_local(awe_t *awe);

void _THCScheduleBack(PTState_t *pts, awe_t *awe_ptr);



// Per-thread state

inline PTState_t * PTS(void) {
  return thc_get_pts_0();
}
EXPORT_SYMBOL(PTS);

#if !defined(LINUX_KERNEL)
volatile int TlsDoneInit = 0;
pthread_key_t TlsKey = 0;
#endif



static inline void InitPTS(void) {
#ifdef LINUX_KERNEL
  PTState_t *pts = kzalloc(sizeof(PTState_t), GFP_KERNEL);
#else
  PTState_t *pts = malloc(sizeof(PTState_t));
  memset(pts, 0, sizeof(PTState_t));

  if (!TlsDoneInit) {
      int r = pthread_key_create(&TlsKey, NULL);
      if(r) {
          printf("pthread_key_create failed");
          return;
      }
      TlsDoneInit = 1;
  }

#endif

  thc_set_pts_0(pts);
}

// Stack management

// An value of type thcstack_t represents a stack which has been allocated
// but which is not currently in use.  It is placed at the top of the
// memory reserved for the stack.

#ifndef THC_NR_STACK_COMMIT_PAGES
#define THC_NR_STACK_COMMIT_PAGES 16
#endif

#ifndef THC_NR_STACK_GUARD_PAGES
#define THC_NR_STACK_GUARD_PAGES 1
#endif

#define STACK_COMMIT_BYTES (THC_NR_STACK_COMMIT_PAGES*4096)
#define STACK_GUARD_BYTES  (THC_NR_STACK_GUARD_PAGES*4096)



// Allocate a new stack, returning an address just above the top of
// the committed region.  The stack comprises STACK_COMMIT_BYTES
// followed by an inaccessible STACK_GUARD_BYTES.
//
// There is currently no support for extending a stack, or allowing it
// to be discontiguous
void * 
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

// De-allocate a stack back to THC's pool of free stacks

void
inline 
_thc_freestack(void *s) {
  PTState_t *pts = PTS();
  struct thcstack_t *stack = (struct thcstack_t*)(s - sizeof(struct thcstack_t));
  DEBUG_STACK(DEBUGPRINTF(DEBUG_STACK_PREFIX "> FreeStack(%p)\n", stack));
  stack->next = pts->free_stacks;
  pts->free_stacks = stack;
  DEBUG_STACK(DEBUGPRINTF(DEBUG_STACK_PREFIX "< FreeStack\n"));
}

__attribute__ ((unused))
static void re_init_dispatch_awe(void *a, void *arg) {
  
  PTState_t *pts = PTS();
  
  awe_t *awe = (awe_t *)a;
  pts->dispatch_awe = *awe;
  thc_dispatch(pts);
}

static inline void thc_pendingfree(PTState_t * pts) {
  if (pts->pendingFree) {
    DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX
                               "  pending free of stack %p\n",
                               PTS()->pendingFree));
    _thc_freestack(pts->pendingFree);
    pts->pendingFree = NULL;
  }
}

void inline 
_thc_pendingfree(void) {
  thc_pendingfree(PTS());
}
EXPORT_SYMBOL(_thc_pendingfree);


static void thc_run_idle_fn(void) {
  void *s;
  PTState_t *pts = PTS();
  s = pts->idle_stack;
  
  DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX "  calling idle fn\n"));
  pts->idle_fn(pts->idle_args);
  DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX "  returned from idle fn\n"));
  
  pts->pendingFree = s;
  thc_dispatch(pts);
  NOT_REACHED;
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

static void thc_init_dispatch_loop(void) {
  PTState_t *pts = PTS();
  pts->dispatchStack = _thc_allocstack();
  // Set start of stack-frame marker
  *((void**)(pts->dispatchStack - LAZY_STACK_BUFFER + __WORD_SIZE)) = NULL;
  thc_awe_init(&pts->dispatch_awe, &thc_dispatch_loop, 
               pts->dispatchStack - LAZY_STACK_BUFFER,
               pts->dispatchStack - LAZY_STACK_BUFFER);
  pts->aweHead.next = &(pts->aweTail);
  pts->aweTail.prev = &(pts->aweHead);

  DEBUG_INIT(DEBUGPRINTF(DEBUG_INIT_PREFIX
                         "  initialized dispatch awe %p\n",
                         &pts->dispatch_awe));
  DEBUG_INIT(DEBUGPRINTF(DEBUG_INIT_PREFIX
                         "  (%p, %p, %p)\n",
                         pts->dispatch_awe.eip,
                         pts->dispatch_awe.ebp,
                         pts->dispatch_awe.esp));
}

static void thc_exit_dispatch_loop(void) {
  PTState_t *pts = PTS();
  assert(!pts->shouldExit);
  pts->shouldExit = 1;
  // Wait for idle loop to finish
  while (pts->aweHead.next != &(pts->aweTail)) {
    THCYield();
  }
  // Exit
  assert((pts->aweHead.next == &(pts->aweTail)) && 
         "Dispatch queue not empty at exit");
  DEBUG_INIT(DEBUGPRINTF(DEBUG_INIT_PREFIX
                         "  NULLing out dispatch AWE\n"));
  thc_awe_init(&pts->dispatch_awe, NULL, NULL, NULL);
  _thc_freestack(pts->dispatchStack);
}

// Enter the dispatch function via dispatch_awe.
//
// (Hence the dispatch loop will run on its own stack, rather than
// the caller's)

static inline void thc_dispatch(PTState_t *pts) {
  assert(pts && pts->doneInit && "Not initialized RTS");
  
  /* If there is a direct awe jump there directly without going to 
     dispatch loop */
  if(pts->direct_cont) {
     awe_t *direct_awe = pts->direct_cont;

     DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX
                               "  dispatch direct awe (direct_cont:%p), awe:%p\n",
                               PTS()->direct_cont, direct_awe));

     pts->direct_cont = NULL;
     thc_awe_execute_0(direct_awe);
  }

  thc_awe_execute_0(&pts->dispatch_awe);
}



static void thc_start_rts(void) {
  InitPTS();
  assert(PTS() && (!PTS()->doneInit) && "Already initialized RTS");
  DEBUG_INIT(DEBUGPRINTF(DEBUG_INIT_PREFIX "> Starting\n"));
  thc_init_dispatch_loop();
  PTS()->doneInit = 1;
  //printf("pts->doneInit:%d, pts:%p\n", PTS()->doneInit, PTS()); 

  DEBUG_INIT(DEBUGPRINTF(DEBUG_INIT_PREFIX "< Starting\n"));
}

static void thc_end_rts(void) {
  void *next_stack;
  PTState_t *pts = PTS();
 

  //printf("pts->doneInit:%d, pts:%p\n", pts->doneInit, pts); 
 
  assert(pts->doneInit);
  DEBUG_INIT(DEBUGPRINTF(DEBUG_INIT_PREFIX "> Ending\n"));
  thc_exit_dispatch_loop();

  // Count up the stacks that we have left.  This is merely for
  // book-keeping: once the dispatch loop is done, then the
  // number of stacks on our free list should equal the number
  // allocated from the OS.
  while (pts->free_stacks != NULL) {
    next_stack = pts->free_stacks->next;
#ifdef LINUX_KERNEL
    // Get top of stack
    stack = ((void *)pts->free_stacks) + sizeof(struct thcstack_t);
    // Subtract off stack size to get to the bottom. This is the
    // address we need to kfree.
    stack -= (STACK_GUARD_BYTES + STACK_COMMIT_BYTES);
    kfree(stack);
#endif
    pts->free_stacks = next_stack;
  }
  // Done
  //thc_print_pts_stats(PTS(), 0);
  PTS()->doneInit = 0;
  DEBUG_INIT(DEBUGPRINTF(DEBUG_INIT_PREFIX "< Ending\n"));
}



/***********************************************************************/

// AWE management

static inline void thc_awe_init(awe_t *awe, void *eip, void *ebp, void *esp) {
  //PTState_t *pts = PTS();
  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "> AWEInit(%p, %p, %p, %p)\n",
                        awe, eip, ebp, esp));
  
  awe->eip = eip;
  awe->ebp = ebp;
  awe->esp = esp;
  //awe->pts = pts;
  
  //awe->current_fb = NULL;
  awe->next = NULL;
  awe->prev = NULL;
  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "< AWEInit\n"));
}

// This function is not meant to be used externally, but its only use
// here is from within the inline assembly language functions.  The
// C "used" attribute is not currently maintained through Clang & LLVM
// with the C backend, so we cannot rely on that.
extern void _thc_schedulecont_c(awe_t *awe);
void _thc_schedulecont_c(awe_t *awe) {
  //PTState_t *pts = PTS();
  //awe->pts = pts;
  //thc_schedule_local(awe);
  DEBUG_DISPATCH(DEBUGPRINTF(DEBUG_DISPATCH_PREFIX
                               "  add direct awe (direct_cont:%p), awe:%p\n",
                               PTS()->direct_cont, awe));
  assert(PTS()->direct_cont == NULL);
  PTS()->direct_cont = awe;  
}

// This function is not meant to be used externally, but its only use
// here is from within the inline assembly language functions.  The
// C "used" attribute is not currently maintained through Clang & LLVM
// with the C backend, so we cannot rely on that.
extern void _thc_callcont_c(awe_t *awe, THCContFn_t fn, void *args);
void 
LIBASYNC_FUNC_ATTR 
_thc_callcont_c(awe_t *awe,
                     THCContFn_t fn,
                     void *args) {
  //PTState_t *pts = PTS();

  //awe->pts = pts;
  //awe->current_fb = pts->current_fb;
  fn(awe, args);
}

// This function is not meant to be used externally, but its only use
// here is from within the inline assembly language functions.  The
// C "used" attribute is not currently maintained through Clang & LLVM
// with the C backend, so we cannot rely on that.
extern void _thc_callcont_c(awe_t *awe, THCContFn_t fn, void *args);
void 
LIBASYNC_FUNC_ATTR 
_thc_callcont_pts_c(awe_t *awe,
                    THCContPTSFn_t fn,
                    void *args,
                    PTState_t *pts) 
{
  //PTState_t *pts = PTS();

  //awe->pts = pts;
  //awe->current_fb = pts->current_fb;
  fn(awe, args, pts);
}

/* do...finish blocks */
// Implementation of finish blocks
//
// The implementation of finish blocks is straightforward:
// _thc_endfinishblock yields back to the dispatch loop if it finds
// the count non-zero, and stashes away a continuation in// fb->finish_awe which will be resumed when the final async
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
EXPORT_SYMBOL(_thc_startfinishblock);

__attribute__ ((unused))
static inline void _thc_endfinishblock0(void *a, void *f) {
  finish_t *fb = (finish_t*)f;

  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "  Waiting f=%p awe=%p\n",
                           fb, a));

  //_thc_pendingfree();

  assert(fb->finish_awe == NULL);
  fb->finish_awe = a;
  thc_dispatch(PTS());
  NOT_REACHED;
}


void inline 
_thc_endfinishblock(finish_t *fb) {

  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "> EndFinishBlock(%p)\n",
                           fb));
  
  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "  count=%d\n",
                           (int)fb->count));
  if (fb->count == 0) {
    // Zero first time.  Check there's not an AWE waiting.
    assert(fb->finish_awe == NULL);
    //_thc_pendingfree();
  } else {
    // Non-zero first time, add ourselves as the waiting AWE.
    CALL_CONT((unsigned char*)&_thc_endfinishblock0, fb);
    _thc_pendingfree();  
 
  }

}
EXPORT_SYMBOL(_thc_endfinishblock);

//helper function for thc_yield_with_cont* functions
//use_dispatch indicates whether to assume AWEs are in the dispatch loop
static inline void thc_yield_with_cont_should_dispatch_pts(awe_t *awe, void *arg, PTState_t *pts, int use_dispatch)
{
  if( use_dispatch )
  {
    _THCScheduleBack(pts, awe);
  }
  thc_dispatch(pts);
}


//helper function for thc_yield_with_cont* functions
//use_dispatch indicates whether to assume AWEs are in the dispatch loop
static inline void thc_yield_with_cont_should_dispatch(void *a, void *arg, int use_dispatch)
{
  awe_t *awe = (awe_t*)a; 

  if( use_dispatch )
  {
    THCScheduleBack(awe);
  }
  thc_dispatch(PTS());
}

__attribute__ ((unused))
static void thc_yield_with_cont(void *a, void *arg) {
    thc_yield_with_cont_should_dispatch(a, arg, 1);
}

__attribute__ ((unused))
static void thc_yield_with_cont_pts(void *awe, void *arg, void *pts) {
    thc_yield_with_cont_should_dispatch_pts((awe_t *)awe, arg, (PTState_t*)pts, 1);
}

__attribute__ ((unused))
static void thc_yield_with_cont_no_dispatch(void *a, void *arg) {
    thc_yield_with_cont_should_dispatch(a, arg, 0);
}

// Yields and saves awe_ptr to correspond to the provided id number
void 
LIBASYNC_FUNC_ATTR 
THCYieldAndSave(uint32_t id_num)
{
  CALL_CONT_AND_SAVE_PTS((void*)&thc_yield_with_cont_pts, id_num, NULL, PTS());
}
EXPORT_SYMBOL(THCYieldAndSave);

// Yields and saves awe_ptr to correspond to the provided id number
void 
LIBASYNC_FUNC_ATTR 
THCYieldAndSaveNoDispatch(uint32_t id_num)
{
  CALL_CONT_AND_SAVE((void*)&thc_yield_with_cont_no_dispatch, id_num, NULL);
}
EXPORT_SYMBOL(THCYieldAndSaveNoDispatch);



void 
LIBASYNC_FUNC_ATTR 
THCYield(void) {
  CALL_CONT((void*)&thc_yield_with_cont, NULL);
}
EXPORT_SYMBOL(THCYield);

static inline void remove_awe_from_list(awe_t* awe)
{
    awe->next->prev = awe->prev;
    awe->prev->next = awe->next;
}

//helper function for thc_yieldto_with_cont* functions
//this function assumes that the caller should be put in the dispatch queue, but that
//the AWE that is being yielded to is not in the dispatch queue.
static inline void 
thc_yieldto_with_cont_no_dispatch_top_level(void* a, void* arg)
{
  awe_t *last_awe = (awe_t*)a; 
  awe_t *awe;
  DEBUG_YIELD(DEBUGPRINTF(DEBUG_YIELD_PREFIX "! %p (%p,%p,%p) yield\n",
                          a,
                          ((awe_t*)a)->eip,
                          ((awe_t*)a)->ebp,
                          ((awe_t*)a)->esp));


  THCScheduleBack(last_awe);
  awe = (awe_t *)arg;

  //awe->pts->current_fb = awe->current_fb;

  thc_awe_execute_0(awe);
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

  awe_t *last_awe = (awe_t*)a; 

  awe = (awe_t *)arg;

  if( use_dispatch )
  {
      THCScheduleBack(last_awe);
      // Bug in original Barrelfish version; awe wasn't removed from
      // dispatch queue when we yielded to it here. (Note that in
      // dispatch loop awe's are removed from the dispatch queue
      // when we yield to them.)
      remove_awe_from_list(awe);
  }

  //awe->pts->current_fb = awe->current_fb;
  thc_awe_execute_0(awe);
}

//helper function for thc_yieldto_with_cont* functions
//use_dispatch indicates whether to assume AWEs are in the dispatch loop
static inline void 
thc_yieldto_with_cont_should_dispatch_pts(void* a, void* arg , void *pts, int use_dispatch)
{
  awe_t *awe;
  DEBUG_YIELD(DEBUGPRINTF(DEBUG_YIELD_PREFIX "! %p (%p,%p,%p) yield\n",
                          a,
                          ((awe_t*)a)->eip,
                          ((awe_t*)a)->ebp,
                          ((awe_t*)a)->esp));

  awe_t *last_awe = (awe_t*)a; 

  awe = (awe_t *)arg;

  if( use_dispatch )
  {
      _THCScheduleBack(pts, last_awe);
      // Bug in original Barrelfish version; awe wasn't removed from
      // dispatch queue when we yielded to it here. (Note that in
      // dispatch loop awe's are removed from the dispatch queue
      // when we yield to them.)
      remove_awe_from_list(awe);
  }

  //awe->pts->current_fb = awe->current_fb;
  thc_awe_execute_0(awe);
}


__attribute__ ((unused))
static inline void thc_yieldto_with_cont_pts(void *a, void *arg, void *pts) {
    thc_yieldto_with_cont_should_dispatch_pts(a, arg, pts, 1);
}

__attribute__ ((unused))
static inline void thc_yieldto_with_cont(void *a, void *arg) {
    thc_yieldto_with_cont_should_dispatch(a, arg, 1);
}
__attribute__ ((unused))
static inline void thc_yieldto_with_cont_no_dispatch(void *a, void *arg) {
    thc_yieldto_with_cont_should_dispatch(a, arg, 0);
}

int
LIBASYNC_FUNC_ATTR 
THCYieldToIdAndSave(uint32_t id_to, uint32_t id_from) {
  PTState_t *pts = PTS();
  awe_t *awe;

  // This check should be done at the application layer, i.e., 
  // receive IPC with an awe id, check for whether id is valid
  //if (!awe_mapper_awe_valid(id_to))
  //  return -1; // id_to not valid

  awe = _awe_mapper_get_awe(pts->awe_map, id_to);

  // Switch to awe_ptr
  CALL_CONT_AND_SAVE_PTS((void*)&thc_yieldto_with_cont_pts, id_from, (void*)awe, pts);
  return 0;
}
EXPORT_SYMBOL(THCYieldToIdAndSave);

//NOTE: THCYieldToIdAndSaveNoDispatch assumes single thread, which for the case of LCDs is fine
int
LIBASYNC_FUNC_ATTR 
THCYieldToIdAndSaveNoDispatch(uint32_t id_to, uint32_t id_from) {
  awe_t *awe_ptr = awe_mapper_get_awe(id_to);

  if (!awe_ptr)
    return -1; // id_to not valid

  CALL_CONT_AND_SAVE((void*)&thc_yieldto_with_cont_no_dispatch, id_from, (void*)awe_ptr);

  return 0;
}
EXPORT_SYMBOL(THCYieldToIdAndSaveNoDispatch);

int
LIBASYNC_FUNC_ATTR 
THCYieldToIdAndSaveNoDispatchDirect(uint32_t id_to, uint32_t id_from) {
  awe_t *awe_to = awe_mapper_get_awe(id_to);

  if (!awe_to)
    return -1; // id_to not valid

  EXEC_AWE_AND_SAVE(id_from, (void*)awe_to);

  return 0;
}
EXPORT_SYMBOL(THCYieldToIdAndSaveNoDispatchDirect);

void inline  
THCYieldToIdAndSaveNoDispatchDirectTrusted(uint32_t id_to, uint32_t id_from) {
  awe_t *awe_to = awe_mapper_get_awe_ptr_trusted(id_to);

  EXEC_AWE_AND_SAVE(id_from, (void*)awe_to);

  return;
}
EXPORT_SYMBOL(THCYieldToIdAndSaveNoDispatchDirectTrusted);

// Yields and savees awe
void inline
THCYieldWithAwe(awe_t *awe_from)
{
  CALL_CONT_AWE(awe_from, (void*)&thc_yield_with_cont_no_dispatch, NULL);
}
EXPORT_SYMBOL(THCYieldWithAwe);


void inline  
THCYieldToAwe(awe_t *awe_from, awe_t *awe_to) {
  EXEC_AWE(awe_from, awe_to);
  return;
}
EXPORT_SYMBOL(THCYieldToAwe);



int
LIBASYNC_FUNC_ATTR 
THCYieldToIdNoDispatch_TopLevel(uint32_t id_to)
{
  awe_t *awe_ptr = awe_mapper_get_awe(id_to);

  if (!awe_ptr) {
    return -1;
  }

  CALL_CONT((void*)&thc_yieldto_with_cont_no_dispatch_top_level, (void*)awe_ptr);
  
  return 0;
}
EXPORT_SYMBOL(THCYieldToIdNoDispatch_TopLevel);

int inline  
THCYieldToAweNoDispatch_TopLevel(awe_t *awe_to)
{
  CALL_CONT((void*)&thc_yieldto_with_cont_no_dispatch_top_level, (void*)awe_to);
  return 0;
}
EXPORT_SYMBOL(THCYieldToIdNoDispatch_TopLevel);


int inline 
_THCYieldToId(PTState_t *pts, uint32_t id_to)
{
  awe_t *awe = _awe_mapper_get_awe(pts->awe_map, id_to);

  // Switch to target awe
  // We were woken up
  //CALL_CONT_PTS((void*)&thc_yieldto_with_cont_pts, (void*)awe, (void*)pts);
  CALL_CONT((void*)&thc_yieldto_with_cont, (void*)awe);

  return 0;
}
EXPORT_SYMBOL(THCYieldToId);


int
LIBASYNC_FUNC_ATTR 
THCYieldToId(uint32_t id_to)
{
  _THCYieldToId(PTS(), id_to);
  return 0;
}
EXPORT_SYMBOL(THCYieldToId);

int THCYieldToIdNoDispatch(uint32_t id_to)
{
  awe_t *awe_ptr = awe_mapper_get_awe(id_to);

  if (!awe_ptr) {
    return -1;
  }

  CALL_CONT((void*)&thc_yieldto_with_cont_no_dispatch, (void*)awe_ptr);

  return 0;
}
EXPORT_SYMBOL(THCYieldToIdNoDispatch);

void 
LIBASYNC_FUNC_ATTR 
THCYieldTo(awe_t *awe_ptr) {
  CALL_CONT((void*)&thc_yieldto_with_cont, (void*)awe_ptr);
}
EXPORT_SYMBOL(THCYieldTo);

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


void 
LIBASYNC_FUNC_ATTR 
THCSchedule(awe_t *awe) {
  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "> THCSchedule(%p)\n",
                        awe));
    
  // Work is for us
  awe->prev = &(PTS()->aweHead);
  awe->next = PTS()->aweHead.next;
  PTS()->aweHead.next->prev = awe;
  PTS()->aweHead.next = awe;

  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "  added AWE between %p %p\n",
                        awe->prev, awe->next));
  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "< THCSchedule\n"));
}
EXPORT_SYMBOL(THCSchedule);

void 
LIBASYNC_FUNC_ATTR 
_THCScheduleBack(PTState_t *pts, awe_t *awe) {
  DEBUG_AWE(DEBUGPRINTF(DEBUG_AWE_PREFIX "> THCSchedule(%p)\n",
                        awe));
  awe->prev = pts->aweTail.prev;
  awe->next = &(pts->aweTail);
  pts->aweTail.prev->next = awe;
  pts->aweTail.prev = awe;

}

void 
LIBASYNC_FUNC_ATTR 
THCScheduleBack(awe_t *awe) {
  return _THCScheduleBack(PTS(), awe); 
}
EXPORT_SYMBOL(THCScheduleBack);

void inline   
_thc_startasync(void *f, void *stack) {
  finish_t *fb = (finish_t*)f;
  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "> StartAsync(%p,%p)\n",
                           fb, stack));
  fb->count ++;
  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "< StartAsync count now %d\n",
                           (int)fb->count));
}
EXPORT_SYMBOL(_thc_startasync);

void inline 
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
       thc_awe_execute_0(fb->finish_awe);
      //thc_schedule_local(fb->finish_awe);
      //fb->finish_awe = NULL;
    }  
  }

  DEBUG_FINISH(DEBUGPRINTF(DEBUG_FINISH_PREFIX "< EndAsync\n"));
  thc_dispatch(pts);
  NOT_REACHED;
}
EXPORT_SYMBOL(_thc_endasync);


#ifdef LINUX_KERNEL

static void IdleFn(void *arg) {
  PTState_t *pts = PTS();

  // We should never become idle; but if we do, just spin in a loop
  // and handle an awe's.
  while (!pts->shouldExit) {

    // Yield while some real work is now available
    while (pts->aweHead.next != &pts->aweTail &&
           !pts->shouldExit) {
      THCYield();
    }

  }
}

#else /* ! LINUX_KERNEL */

static void IdleFn(void *arg) {
  //int me = ++idle_ct;
  //struct waitset *ws = get_default_waitset();

  PTState_t *pts = PTS();

  while (!pts->shouldExit) {
    
    /* 
    // Block for the next event to occur
    errval_t err = event_dispatch(ws);
    if (err_is_fail(err)) {
      assert(0 && "event_dispatch failed in THC idle function");
      abort();
    }

    // Exit if a new idle loop has started (this will happen
    // if the handler called from event_dispatch blocks, e.g.,
    // in the bottom-half of a THC receive function)
    if (me != idle_ct) {
      break;
    } 
    */

    // Yield while some real work is now available
    while (pts->aweHead.next != &pts->aweTail &&
           !pts->shouldExit) {
      THCYield();
    }
  }
}

#endif /* LINUX_KERNEL */

#ifndef LINUX_KERNEL
//__attribute__((constructor))
#endif
void 
LIBASYNC_FUNC_ATTR 
thc_init(void) {
  thc_start_rts();
  PTS()->idle_fn = IdleFn;
  PTS()->idle_args = NULL;
  PTS()->idle_stack = NULL;
  PTS()->direct_cont = NULL;	
  awe_mapper_init();
}
EXPORT_SYMBOL(thc_init);

//#ifndef LINUX_KERNEL
//__attribute__((destructor))
//#endif
void 
LIBASYNC_FUNC_ATTR 
thc_done(void) {
  awe_mapper_uninit();
  thc_end_rts();
}
EXPORT_SYMBOL(thc_done);

int 
LIBASYNC_FUNC_ATTR 
thc_global_init(void)
{
	return 0; // no-op for now
}
EXPORT_SYMBOL(thc_global_init);

void 
LIBASYNC_FUNC_ATTR 
thc_global_fini(void)
{
	return; // no-op for now
}
EXPORT_SYMBOL(thc_global_fini);

#if !defined(LINUX_KERNEL)
static PTState_t * global_pts = NULL;

static inline PTState_t *thc_get_pts_0(void) {
   //return (PTState_t *) (pthread_getspecific(TlsKey));
   return global_pts;
}

static inline void thc_set_pts_0(PTState_t *st) {
  assert(TlsDoneInit);
  global_pts = st; 
  //pthread_setspecific(TlsKey, (void*)st);
}

#elif defined(LINUX_KERNEL)
static PTState_t *thc_get_pts_0(void) {
  return current->ptstate;
}

static void thc_set_pts_0(PTState_t *st) {
  current->ptstate = st;
}
#endif

// 2. Execution on an alternative stack

#if (defined(__x86_64__) && (defined(linux) || defined(BARRELFISH) || \
				defined(LINUX_KERNEL)))
// Callee invoked via Linux x64 conventions (args in EDI)

/*
         static void thc_on_alt_stack_0(void *stack,   // rdi
                                        void *fn,      // rsi
                                        void *args)    // rdx
*/
__asm__ ("      .text \n\t"
         "      .align  16                  \n\t"
         "thc_on_alt_stack_0:               \n\t"
         " sub $8, %rdi                     \n\t"
         " mov %rsp, (%rdi)                 \n\t" // Save old ESP on new stack
         " mov %rdi, %rsp                   \n\t" // Set up new stack pointer
         " mov %rdx, %rdi                   \n\t" // Move args into rdi
         " call *%rsi                       \n\t" // Call callee (args in rdi)
         " pop %rsp                         \n\t" // Restore old ESP
         " ret                              \n\t");

#elif (defined(__i386__) && (defined(linux) || defined(BARRELFISH)))
// Callee invoked via stdcall (args on stack, removed by callee)

/*
         static void thc_on_alt_stack_0(void *stack,   //  4
                                       void *fn,       //  8
                                       void *args) {   // 12
*/
__asm__ ("      .text \n\t"
         "      .align  16                  \n\t"
         "thc_on_alt_stack_0:               \n\t"
         " mov 4(%esp), %edx                \n\t" // New stack
         " mov 8(%esp), %eax                \n\t" // Callee
         " mov 12(%esp), %ecx               \n\t" // Args
         " subl $4, %edx                    \n\t"
         " mov %esp, (%edx)                 \n\t" // Save old ESP on new stack
         " mov %edx, %esp                   \n\t" // Set up new stack pointer
         " push %ecx                        \n\t" // Push args on new stack
         " call *%eax                       \n\t" // Call callee (it pops args)
         " pop %esp                         \n\t" // Restore old ESP
         " ret \n\t");

#elif (defined(__i386__) && (defined(WINDOWS) || defined(__CYGWIN__)))
// Callee invoked via stdcall (args on stack, removed by callee)

/*
         static void thc_on_alt_stack_0(void *stack,   //  4
                                       void *fn,       //  8
                                       void *args) {   // 12
*/
__asm__ ("      .text \n\t"
         "      .align  16                  \n\t"
         "      .globl  _thc_on_alt_stack_0 \n\t"
         "_thc_on_alt_stack_0:              \n\t"
         " mov 4(%esp), %edx                \n\t" // New stack
         " mov 8(%esp), %eax                \n\t" // Callee
         " mov 12(%esp), %ecx               \n\t" // Args
         " subl $4, %edx                    \n\t"
         " mov %esp, (%edx)                 \n\t" // Save old ESP on new stack
         " mov %edx, %esp                   \n\t" // Set up new stack pointer
         " push %ecx                        \n\t" // Push args on new stack
         " call *%eax                       \n\t" // Call callee (it pops args)
         " pop %esp                         \n\t" // Restore old ESP
         " ret \n\t");
#else
void thc_on_alt_stack_0(void *stack,   
                        void *fn,   
                        void *args) {
  assert(0 && "thc_on_alt_stack_0 not implemented for this architecture");
}
#endif

/***********************************************************************/

// 3. AWE execution
//
// These functions are particularly delicate:
//
// (a) The _thc_schedulecont and _thc_callcont functions are called
//     with a pointer to an awe_t which has been alloca'd on the
//     caller's stack frame.  Aside from the stack/frame-pointers,
//     the caller is responsible for saving any registers that may
//     be live at the point of the call (including those which are
//     conventionally callee-save).  The _thc_schedulecont and
//     _thc_callcont functions initialize the AWE with the
//     stack/frame-pointer values for when the call returns, and
//     initializing the saved EIP with the instruction immediately
//     after that call.
//
// (b) A call to _thc_schedulecont returns normally with 0.
//
// (c) When an AWE is executed, the stack/frame-pointers are restored
//     and the register used for return values (e.g., EAX) is
//     initialized to non-0.

#if (defined(__x86_64__) && (defined(linux) || defined(BARRELFISH) || \
				defined(LINUX_KERNEL)))
/*
            static void thc_awe_execute_0(awe_t *awe)    // rdi
*/
__asm__ ("      .text \n\t"
         "      .align  16                 \n\t"
         "thc_awe_execute_0:               \n\t"
         " mov 8(%rdi), %rbp               \n\t"
         " mov 16(%rdi), %rsp              \n\t"
         " jmp *0(%rdi)                    \n\t");

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
         //" movq $0, %rax             \n\t"
         //" call _swizzle             \n\t"
         " ret                       \n\t");

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

/*
           void _thc_callcont_direct(awe_t *awe,   // rdi
                   void *args,               //rsi
                   THCContFn_t fn)          // rdx
*/

__asm__ ("      .text \n\t"
         "      .align  16           \n\t"
         "      .globl  _thc_callcont_direct \n\t"
         "      .type   _thc_callcont_direct, @function \n\t"
         "_thc_callcont_direct:             \n\t"
         " mov  0(%rsp), %rax        \n\t"
         " mov  %rax,  0(%rdi)       \n\t" // EIP (our return address)
         " mov  %rbp,  8(%rdi)       \n\t" // EBP
         " mov  %rsp, 16(%rdi)       \n\t" // ESP+8 (after return)
         " addq $8,   16(%rdi)       \n\t"
         // AWE now initialized.  Call the function
         // rdi : AWE , rsi : args , rdx : fn
         " jmpq  %rdx                \n\t"
         " int3\n\t");


/*
           void _thc_callcont_pts(awe_t *awe,   // rdi
                   THCContFn_t fn,              // rsi
                   void *args,                  // rdx
                   PTState_t *pts);             // rcx
*/

__asm__ ("      .text \n\t"
         "      .align  16           \n\t"
         "      .globl  _thc_callcont_pts \n\t"
         "      .type   _thc_callcont_pts, @function \n\t"
         "_thc_callcont_pts:             \n\t"
         " mov  0(%rsp), %rax        \n\t"
         " mov  %rax,  0(%rdi)       \n\t" // EIP (our return address)
         " mov  %rbp,  8(%rdi)       \n\t" // EBP
         " mov  %rsp, 16(%rdi)       \n\t" // ESP+8 (after return)
         " addq $8,   16(%rdi)       \n\t"
         // AWE now initialized.  Call into C for the rest.
         // rdi : AWE , rsi : fn , rdx : args, rcx: pts
         " call _thc_callcont_pts_c      \n\t"
         " int3\n\t");

/*
           void _thc_callcont_pts_direct(awe_t *awe,   // rdi
                    void *args,                        // rsi
                    PTState_t *pts,                    // rdx
                    THCContFn_t fn);                   // rcx
*/

__asm__ ("      .text \n\t"
         "      .align  16           \n\t"
         "      .globl  _thc_callcont_pts_direct \n\t"
         "      .type   _thc_callcont_pts_direct, @function \n\t"
         "_thc_callcont_pts_direct:             \n\t"
         " mov  0(%rsp), %rax        \n\t"
         " mov  %rax,  0(%rdi)       \n\t" // EIP (our return address)
         " mov  %rbp,  8(%rdi)       \n\t" // EBP
         " mov  %rsp, 16(%rdi)       \n\t" // ESP+8 (after return)
         " addq $8,   16(%rdi)       \n\t"
         // AWE now initialized.  Call the function.
         // rdi : AWE , rsi : args , rdx : pts, rcx: fn
         " jmpq  %rcx                \n\t"
         " int3\n\t");


__asm__ ("      .text \n\t"
         "      .align  16           \n\t"
         "      .globl  _thc_exec_awe_direct \n\t"
         "      .type   _thc_exec_awe_direct, @function \n\t"
         "_thc_exec_awe_direct:             \n\t"
         " mov  0(%rsp), %rax        \n\t"
         " mov  %rax,  0(%rdi)       \n\t" // EIP (our return address)
         " mov  %rbp,  8(%rdi)       \n\t" // EBP
         " mov  %rsp, 16(%rdi)       \n\t" // ESP+8 (after return)
         " addq $8,   16(%rdi)       \n\t"
         // AWE now initialized.  Call into C for the rest.
         // rdi : AWE_from , rsi : AWE_to
         // 
         " mov   0x8(%rsi),%rbp      \n\t"
         " mov   0x10(%rsi),%rsp     \n\t"
         " jmp  *0(%rsi)             \n\t"
         " int3\n\t");


/*
            static void _thc_lazy_awe_marker()   
*/

__asm__ ("      .text \n\t"
         "      .align  16            \n\t"
         "      .globl  _thc_lazy_awe \n\t"
         "      .globl  _thc_lazy_awe_marker \n\t"
	 " _thc_lazy_awe:            \n\t" /* This is for debugging so we get */
         " nop                       \n\t" /* a sensible call stack           */
	 " _thc_lazy_awe_marker:     \n\t"
	 " int3                      \n\t" /* should never be called */
	 );

#endif

#if defined(linux)
#include <sys/mman.h>
#include <errno.h>

static void *thc_alloc_new_stack_0(void) {
  void *res = mmap(NULL,
                   STACK_COMMIT_BYTES + STACK_GUARD_BYTES,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS,
                   0, 0);
  if (!res) {
    fprintf(stderr, "URK: mmap returned errno=%d\n", errno);
    exit(-1);
  }

  DEBUG_STACK(DEBUGPRINTF(DEBUG_STACK_PREFIX "  mmap %p..%p\n",
                          res, res+STACK_COMMIT_BYTES+STACK_GUARD_BYTES));

  int r = mprotect(res, STACK_GUARD_BYTES, PROT_NONE);
  if (r) {
    fprintf(stderr, "URK: mprotect returned errno=%d\n", errno);
    exit(-1);
  }

  res += STACK_GUARD_BYTES + STACK_COMMIT_BYTES;
  return res;
}
#elif defined(LINUX_KERNEL)
static void *thc_alloc_new_stack_0(void) {
  void *res = kmalloc(STACK_COMMIT_BYTES + STACK_GUARD_BYTES, GFP_KERNEL);
  if (!res) {

    printk(KERN_ERR "async stack allocation failed");

    // The rest of the async code isn't prepared to
    // handle a failed malloc (boo), so we crash inside
    // here. (We could certainly patch up the code in the
    // future.)
    BUG();

  }

  // Note that sizeof(void) = 1 not 8.
  res += STACK_GUARD_BYTES + STACK_COMMIT_BYTES;
  return res;
}
#else
#error No definition for _thc_alloc_new_stack_0
#endif


