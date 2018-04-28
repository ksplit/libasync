#include <thc.h>
#include <thcinternal.h>
#include <awe_mapper.h>
#include <stdio.h>
#include <test_helpers.h>

#define NUM_INNER_ASYNCS 10
#define NUM_SWITCH_MEASUREMENTS 10000000
//#define NUM_SWITCH_MEASUREMENTS 8

void test_async(){

    thc_printf("Basic do...finish/async test\n");

    DO_FINISH({
        ASYNC({
            thc_printf("ASYNC 1: inside\n");
        });             
    });

    thc_printf("Done with do...finish\n");
    return;   
}

void test_async_yield() {
  unsigned int id_1, id_2;

  thc_printf("Basic do...finish/async yield test\n");

  DO_FINISH({
        ASYNC({
            id_1 = awe_mapper_create_id();
            assert(id_1);
            thc_printf("ASYNC 1: Ready to yield\n");
            THCYieldAndSaveNoDispatch(id_1);
            thc_printf("ASYNC 1: Got control back\n");

            //t4 = test_fipc_stop_stopwatch();
            awe_mapper_remove_id(id_1);
        });             
        ASYNC({
            id_2 = awe_mapper_create_id();
            assert(id_2);
            //t3 = test_fipc_start_stopwatch(); 
            thc_printf("ASYNC 2: Ready to yield\n");
            THCYieldToIdAndSaveNoDispatch(id_1,id_2);
            thc_printf("ASYNC 2: Got control back\n");

        });             
                    
        // The Yield below is important since earlier we 
        // were relying on the dispatch queue
        // to clean up AWEs, but now, extra care needs 
        // to be taken to clean them up.
        
        //This is a hack for now, the semantics of 
        //not using a dispatch loop
        //probably need to be hashed out more.
        THCYieldToIdNoDispatch_TopLevel(id_2);
        awe_mapper_remove_id(id_2);


    });

    thc_printf("Done with do...finish\n");


}

void test_basic_do_finish_create(){
    unsigned long t1, t2;
    unsigned long num = 0;
    int i;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             num++;             
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per do{ i++ }finish(): %lu cycles (%s)\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS ? "Passed" : "Failed");
    
    return;   
}

void test_basic_nonblocking_async_create(){
    unsigned long t1, t2;
    unsigned long num = 0;
    int i;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             ASYNC({
                 num++;
             });             
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per non blocking do{async{i++}}finish(): %lu cycles (%s)\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS ? "Passed" : "Failed");
        
    return;   
}

void test_basic_N_nonblocking_asyncs_create(){
    unsigned long t1, t2;
    unsigned long num = 0;
    int i, j;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             for (j = 0; j < NUM_INNER_ASYNCS; j++) {
                 ASYNC({
                     num++;
                 });
             };             
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per %d non blocking asyncs inside one do{ }finish(): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS ? "Passed" : "Failed");
    return;   
}

void test_basic_1_blocking_asyncs_create(){
    unsigned long t1, t2;
    unsigned long num = 0;
    int i;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
                 ASYNC({
                     THCYield();
                     num++;
                 });
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per 1 blocking asyncs inside one do{ }finish(): %lu cycles (%s)\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS ? "Passed" : "Failed");
    return;   
}

void test_basic_N_blocking_asyncs_create(){
    unsigned long t1, t2;
    unsigned long num = 0;
    int i, j;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             for (j = 0; j < NUM_INNER_ASYNCS; j++) {
                 ASYNC({
                     THCYield();
                     num++;
                 });
             };             
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per %d blocking asyncs inside one do{ }finish(): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS ? "Passed" : "Failed");
    return;   
}

void test_basic_N_blocking_asyncs_create_pts(){
    unsigned long t1, t2;
    unsigned long num = 0;
    int i, j;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             for (j = 0; j < NUM_INNER_ASYNCS; j++) {
                 ASYNC({
                     THCYieldPTS();
                     num++;
                 });
             };             
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per %d blocking asyncs inside one do{ }finish() (thread pts): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS ? "Passed" : "Failed");
    return;   
}

/* This test emulates NUM_INNER_ASYNCS message sends (same as below,
   but completes them via the dispatch loop vs waking up through another ASYNC 
   like above).

   It's tempting to get rid of awe id's here, but then there will be no 
   way for the dispatch loop to finish them when it reaches the end of 
   the do...finish block inside of the endfinish(). 

   My conclusion is that this test faithfully estimates the overhead
   do..finish/async when all messages block and then are completed as part
   of the endfinish function.

   Ironically it is slower than the test above... 
*/

void test_basic_N_blocking_id_asyncs(){
    unsigned long t1, t2;
    unsigned long num = 0;
    unsigned int ids[NUM_INNER_ASYNCS];
    int i, j;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             for (j = 0; j < NUM_INNER_ASYNCS; j++) {
                 ASYNC({
                     int local_j = j;
                     ids[local_j] = awe_mapper_create_id();
                     assert(ids[local_j]);
                     //thc_printf("id (%d):%d\n", local_j, ids[local_j]);
                     assert(ids[j] < AWE_TABLE_COUNT); 
                     THCYieldAndSave(ids[local_j]);
                     //thc_printf("rel id (%d):%d\n", local_j, ids[local_j]);
                     awe_mapper_remove_id(ids[local_j]);
                     num++;
                 });
             };                    
             //for (j = 0; j < NUM_INNER_ASYNCS; j++) {
             //    ASYNC({
             //        THCYieldToId(ids[j]);
             //    });
             //};     
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per %d blocking asyncs inside one do{ }finish() (yield via awe mapper): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS ? "Passed" : "Failed");
    return;   
}

/* Same as above but thread PTS through yields */

void test_basic_N_blocking_id_asyncs_pts(){
    unsigned long t1, t2;
    unsigned long num = 0;
    unsigned int ids[NUM_INNER_ASYNCS];
    int i, j;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             for (j = 0; j < NUM_INNER_ASYNCS; j++) {
                 ASYNC({
                     int local_j = j;
                     ids[local_j] = awe_mapper_create_id();
                     assert(ids[local_j]);
                     //thc_printf("id (%d):%d\n", local_j, ids[local_j]);
                     assert(ids[j] < AWE_TABLE_COUNT); 
                     THCYieldAndSavePTS(ids[local_j]);
                     //thc_printf("rel id (%d):%d\n", local_j, ids[local_j]);
                     awe_mapper_remove_id(ids[local_j]);
                     num++;
                 });
             };                    
             //for (j = 0; j < NUM_INNER_ASYNCS; j++) {
             //    ASYNC({
             //        THCYieldToId(ids[j]);
             //    });
             //};     
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per %d blocking asyncs inside one do{ }finish() (yield via awe mapper, thread pts): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS ? "Passed" : "Failed");
    return;   
}

void test_basic_N_blocking_id_asyncs_and_N_yields_back(){
    unsigned long t1, t2;
    unsigned long num = 0;
    unsigned int ids[NUM_INNER_ASYNCS];
    int i, j;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             for (j = 0; j < NUM_INNER_ASYNCS/2; j++) {
                 ASYNC({
                     ids[j] = awe_mapper_create_id();
                     assert(ids[j]);
                     THCYieldAndSave(ids[j]);
                     awe_mapper_remove_id(ids[j]);
                     num++;
                 });
             };                    
             for (j = 0; j < NUM_INNER_ASYNCS/2; j++) {
                 ASYNC({
                     THCYieldToId(ids[j]);
                 });
             };     
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per %d blocking asyncs inside one do{ }finish() and %d yield backs (yield via awe mapper): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS/2, NUM_INNER_ASYNCS/2, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS/2 ? "Passed" : "Failed");
    return;   
}

/* This test I think simulates NUM_INNER_ASYNCS message sends. The first 
   half of sends blocks and finds the queue empty, yielding to the main 
   execution loop. The second half of asyncs simulates the case when one 
   message send finds another receive message in the queue and yields to 
   it. This last half is then finished by the do_finish block, i.e., the 
   fihish function yields back to each of the pending NUM_INNER_ASYNCS/2 
   second half asyncs.
   
   So my conclusion is that this test is the overhead do..finish/async 
   introduces compared to normal synchronous execution. 
*/

void test_basic_N_blocking_id_asyncs_and_N_yields_back_extrnl_ids(){
    unsigned long t1, t2;
    unsigned long num = 0;
    awe_t awes[NUM_INNER_ASYNCS];
    int i, j;

    t1 = test_fipc_start_stopwatch();
   
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
         DO_FINISH({
             for (j = 0; j < NUM_INNER_ASYNCS/2; j++) {
                 ASYNC({
                     THCYieldWithAwe(&(awes[j]));
                     num++;
                 });
             };                    
             for (j = 0; j < NUM_INNER_ASYNCS/2; j++) {
                 ASYNC({
                     THCYieldToAweNoDispatch_TopLevel(&(awes[j]));
                 });
             };     
         });
    };

    t2 = test_fipc_start_stopwatch(); 

    thc_printf("Average time per %d blocking asyncs inside one do{ }finish() and %d yield backs (yield via awe mapper, external): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS/2, NUM_INNER_ASYNCS/2, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS/2 ? "Passed" : "Failed");
    return;   
}

static int test_do_finish_yield(void)
{
    unsigned long t1, t2;
    unsigned int id_1;
    int i = 0, num = 0;


    t1 = test_fipc_start_stopwatch();

    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
            DO_FINISH({
                    ASYNC({
                        id_1 = awe_mapper_create_id();
                        assert(id_1);
                        THCYieldAndSave(id_1);
                        awe_mapper_remove_id(id_1);
                        num ++; 
                    });             
                    ASYNC({
                        THCYieldToId(id_1);
                        num++;
                    });             
            });
    }
    t2 = test_fipc_start_stopwatch(); 
    thc_printf("Average time per do .. finish and two blocking yields: %lu cycles (%s)\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*2 ? "Passed" : "Failed");
 

    return 0;
}


static int test_do_finish_yield_no_dispatch(void)
{
    unsigned long t1, t2;
    unsigned int id_1, id_2;
    int i = 0, num = 0;
   
    t1 = test_fipc_start_stopwatch();

    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
            DO_FINISH({
                    
                    ASYNC({
                        id_1 = awe_mapper_create_id();
                        assert(id_1);
                        THCYieldAndSaveNoDispatch(id_1);
                        awe_mapper_remove_id(id_1);
                        num ++;
                    });             
                    ASYNC({
                        id_2 = awe_mapper_create_id();
                        assert(id_2);
                        THCYieldToIdAndSaveNoDispatch(id_1,id_2);
                        num ++;
                    });             
                    //The Yield below is important since earlier we were relying on the dispatch queue
                    //to clean up AWEs, but now, extra care needs to be taken to clean them up.
                    //This is a hack for now, the semantics of not using a dispatch loop
                    //probably need to be hashed out more.
                    THCYieldToIdNoDispatch_TopLevel(id_2);
                    awe_mapper_remove_id(id_2);
            });
    }

    t2 = test_fipc_stop_stopwatch();

    thc_printf("Average time per do .. finish and two blocking yields (no dispatch): %lu cycles (%s)\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*2 ? "Passed" : "Failed");

    return 0;
}


#if 0
static int test_ctx_switch(void)
{
    unsigned long t1, t2;
    unsigned int id_1, id_2;


    t1 = test_fipc_start_stopwatch();

    //Average time per context switch
    DO_FINISH({

        ASYNC({
            int i;
            awe_mapper_create_id(&id_1);
            THCYieldAndSave(id_1);
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                THCYieldToIdAndSave(id_2,id_1);
            }
            t2 = test_fipc_stop_stopwatch();
            awe_mapper_remove_id(id_1);
            awe_mapper_remove_id(id_2);
        });
        
        ASYNC({
            int i;
            awe_mapper_create_id(&id_2);
            t1 = test_fipc_start_stopwatch(); 
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                THCYieldToIdAndSave(id_1,id_2);
            }
        });
    });
    
    thc_printf("Average time per context switch: %lu cycles\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);
                                        


    return 0;
}
#endif

static int test_ctx_switch_no_dispatch(void)
{
    unsigned long t1, t2;
    unsigned int id_1, id_2;
   
    t1 = test_fipc_start_stopwatch();

    DO_FINISH({

        ASYNC({
            int i;
            id_1 = awe_mapper_create_id();
            assert(id_1);
            //thc_printf("ASYNC 1: warm up yield\n");
            THCYieldAndSaveNoDispatch(id_1);
            //thc_printf("ASYNC 1: got control back\n");
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //thc_printf("ASYNC 1: ready to yield\n");
                THCYieldToIdAndSaveNoDispatch(id_2,id_1);
                //thc_printf("ASYNC 1: got control back\n");
            }
            awe_mapper_remove_id(id_1);
            awe_mapper_remove_id(id_2);
        });
        
        ASYNC({
            int i;
            id_2 = awe_mapper_create_id();
            assert(id_2);
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //thc_printf("ASYNC 2: ready to yield\n");
                THCYieldToIdAndSaveNoDispatch(id_1,id_2);
                //thc_printf("ASYNC 2: got control back\n");
            }
        });
        THCYieldToIdNoDispatch_TopLevel(id_1);
    });

    t2 = test_fipc_stop_stopwatch();

    thc_printf("Average time per context switch (no dispatch): %lu cycles\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);


    return 0;
}


static int test_ctx_switch_no_dispatch_direct(void)
{
    unsigned long t1, t2;
    unsigned int id_1, id_2;
   
    t1 = test_fipc_start_stopwatch();

    DO_FINISH({

        ASYNC({
            int i;
            id_1 = awe_mapper_create_id();
            assert(id_1);
            //thc_printf("ASYNC 1: warm up yield\n");
            THCYieldAndSaveNoDispatch(id_1);
            //thc_printf("ASYNC 1: got control back\n");
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //thc_printf("ASYNC 1: ready to yield\n");
                THCYieldToIdAndSaveNoDispatchDirect(id_2,id_1);
                //thc_printf("ASYNC 1: got control back\n");
            }
            awe_mapper_remove_id(id_1);
            awe_mapper_remove_id(id_2);
        });
        
        ASYNC({
            int i;
            id_2 = awe_mapper_create_id();
            assert(id_2);
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //thc_printf("ASYNC 2: ready to yield\n");
                THCYieldToIdAndSaveNoDispatchDirect(id_1,id_2);
                //thc_printf("ASYNC 2: got control back\n");
            }
        });
        THCYieldToIdNoDispatch_TopLevel(id_1);
    });

    t2 = test_fipc_stop_stopwatch();

    thc_printf("Average time per context switch (no dispatch, direct): %lu cycles\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);


    return 0;
}


/* The trusted test assumes that we don't have to check for 
   whether awe_id is valid and in range */
static int test_ctx_switch_no_dispatch_direct_trusted(void)
{
    unsigned long t1, t2;
    unsigned int id_1, id_2;
   
    t1 = test_fipc_start_stopwatch();

    DO_FINISH({

        ASYNC({
            int i;
            id_1 = awe_mapper_create_id();
            assert(id_1);
            //thc_printf("ASYNC 1: warm up yield\n");
            THCYieldAndSaveNoDispatch(id_1);
            //thc_printf("ASYNC 1: got control back\n");
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //thc_printf("ASYNC 1: ready to yield\n");
                THCYieldToIdAndSaveNoDispatchDirectTrusted(id_2,id_1);
                //thc_printf("ASYNC 1: got control back\n");
            }
            awe_mapper_remove_id(id_1);
            awe_mapper_remove_id(id_2);
        });
        
        ASYNC({
            int i;
            id_2 = awe_mapper_create_id();
            assert(id_2);
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //thc_printf("ASYNC 2: ready to yield\n");
                THCYieldToIdAndSaveNoDispatchDirectTrusted(id_1,id_2);
                //thc_printf("ASYNC 2: got control back\n");
            }
        });
        THCYieldToIdNoDispatch_TopLevel(id_1);
    });

    t2 = test_fipc_stop_stopwatch();

    thc_printf("Average time per context switch (no dispatch, direct, trusted): %lu cycles\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);


    return 0;
}

static int test_create_and_ctx_switch_to_awe(void)
{
    unsigned long t1, t2;
    awe_t awe_1, awe_2;
    int i; 
   
    t1 = test_fipc_start_stopwatch();

    for(i = 0; i < NUM_SWITCH_MEASUREMENTS; i++)
    {

        DO_FINISH({

            ASYNC({
                //thc_printf("ASYNC 1: warm up yield\n");
                THCYieldWithAwe(&awe_1);
                //thc_printf("ASYNC 1: got control back\n");
                            
                //thc_printf("ASYNC 1: ready to yield\n");
                //THCYieldToAwe(&awe_1, &awe_2);
                //thc_printf("ASYNC 1: got control back\n");
               
            });
        
            ASYNC({
                //thc_printf("ASYNC 2: ready to yield\n");
                THCYieldToAwe(&awe_2, &awe_1);
                //thc_printf("ASYNC 2: got control back\n");
            });

            THCYieldToAweNoDispatch_TopLevel(&awe_1);

        });
    };

    t2 = test_fipc_stop_stopwatch();

    thc_printf("Average time to do...finish and two yeilds (direct awe): %lu cycles\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);


    return 0;
}

static int test_ctx_switch_to_awe(void)
{
    unsigned long t1, t2;
    awe_t awe_1, awe_2;
   
    t1 = test_fipc_start_stopwatch();

    DO_FINISH({

        ASYNC({
            int i;
            //thc_printf("ASYNC 1: warm up yield\n");
            THCYieldWithAwe(&awe_1);
            //thc_printf("ASYNC 1: got control back\n");
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //thc_printf("ASYNC 1: ready to yield\n");
                THCYieldToAwe(&awe_1, &awe_2);
                //thc_printf("ASYNC 1: got control back\n");
            }
        });
        
        ASYNC({
            int i;
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //thc_printf("ASYNC 2: ready to yield\n");
                THCYieldToAwe(&awe_2, &awe_1);
                //thc_printf("ASYNC 2: got control back\n");
            }
        });
        THCYieldToAweNoDispatch_TopLevel(&awe_1);

    });

    t2 = test_fipc_stop_stopwatch();

    thc_printf("Average time per context switch (direct awe): %lu cycles\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);


    return 0;
}

static int test_create_awe(void)
{
    unsigned long t1, t2;
    unsigned int id[NUM_INNER_ASYNCS];
    int i;
    awe_t *a;

    t1 = test_fipc_start_stopwatch();

    for(i = 0; i < NUM_SWITCH_MEASUREMENTS; i++)
    {
	int j;
        for(j = 0; j < NUM_INNER_ASYNCS; j++) {
		awe_mapper_create_id(&id[j]);
		a = awe_mapper_get_awe(&id[j]);
	}
        for(j = 0; j < NUM_INNER_ASYNCS; j++) {
		awe_mapper_remove_id(id[j]);
	}
    }

    t2 = test_fipc_stop_stopwatch();

    thc_printf("Average time to create and remove and awe_ids: %lu cycles\n",
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);
    return 0;
}

int main (void) {
    
    thc_init();
#if 1
    test_async();
    test_async_yield();

    test_basic_do_finish_create();
    test_basic_nonblocking_async_create();  
    test_basic_N_nonblocking_asyncs_create();

    test_basic_1_blocking_asyncs_create();
    test_basic_N_blocking_asyncs_create(); 
    test_basic_N_blocking_asyncs_create_pts();
    test_basic_N_blocking_id_asyncs();
    test_basic_N_blocking_id_asyncs_pts();
    test_basic_N_blocking_id_asyncs_and_N_yields_back();
#endif
    test_basic_N_blocking_id_asyncs_and_N_yields_back_extrnl_ids();

    test_do_finish_yield();
    test_do_finish_yield_no_dispatch();
   // test_create_and_ctx_switch_to_awe();

    test_ctx_switch_no_dispatch();
    test_ctx_switch_no_dispatch_direct();
    test_ctx_switch_no_dispatch_direct_trusted();
    test_ctx_switch_to_awe();
    test_create_awe();

    thc_done();
    return 0; 
}

