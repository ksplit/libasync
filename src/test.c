#include <thc.h>
#include <thcinternal.h>
#include <awe_mapper.h>
#include <stdio.h>
#include <test_helpers.h>

#define NUM_INNER_ASYNCS 10
#define NUM_SWITCH_MEASUREMENTS 1000000
//#define NUM_SWITCH_MEASUREMENTS 8

void test_async(){

    printf("Basic do...finish/async test\n");

    DO_FINISH({
        ASYNC({
            printf("ASYNC 1: inside\n");
        });             
    });

    printf("Done with do...finish\n");
    return;   
}

void test_async_yield() {
  unsigned int id_1, id_2;

  printf("Basic do...finish/async yield test\n");

  DO_FINISH({
        ASYNC({
            awe_mapper_create_id(&id_1);
            printf("ASYNC 1: Ready to yield\n");
            THCYieldAndSaveNoDispatch(id_1);
            printf("ASYNC 1: Got control back\n");

            //t4 = test_fipc_stop_stopwatch();
            awe_mapper_remove_id(id_1);
        });             
        ASYNC({
            awe_mapper_create_id(&id_2);
            //t3 = test_fipc_start_stopwatch(); 
            printf("ASYNC 2: Ready to yield\n");
            THCYieldToIdAndSaveNoDispatch(id_1,id_2);
            printf("ASYNC 2: Got control back\n");

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

    printf("Done with do...finish\n");


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

    printf("Average time per do{ i++ }finish(): %lu cycles (%s)\n", 
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

    printf("Average time per non blocking do{async{i++}}finish(): %lu cycles (%s)\n", 
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

    printf("Average time per %d non blocking asyncs inside one do{ }finish(): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS ? "Passed" : "Failed");
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

    printf("Average time per %d blocking asyncs inside one do{ }finish(): %lu cycles (%s)\n", 
          NUM_INNER_ASYNCS, (t2 - t1)/NUM_SWITCH_MEASUREMENTS, 
          num == NUM_SWITCH_MEASUREMENTS*NUM_INNER_ASYNCS ? "Passed" : "Failed");
    return;   
}

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
                     awe_mapper_create_id(&(ids[local_j]));
                     //printf("id (%d):%d\n", local_j, ids[local_j]);
                     assert(ids[j] < AWE_TABLE_COUNT); 
                     THCYieldAndSave(ids[local_j]);
                     //printf("rel id (%d):%d\n", local_j, ids[local_j]);
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

    printf("Average time per %d blocking asyncs inside one do{ }finish() (yield via awe mapper): %lu cycles (%s)\n", 
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
                     awe_mapper_create_id(&ids[j]);
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

    printf("Average time per %d blocking asyncs inside one do{ }finish() and %d yield backs (yield via awe mapper): %lu cycles (%s)\n", 
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
                        awe_mapper_create_id(&id_1);
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
    printf("Average time per do .. finish and two blocking yields: %lu cycles (%s)\n", 
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
                        awe_mapper_create_id(&id_1);
                        THCYieldAndSaveNoDispatch(id_1);
                        awe_mapper_remove_id(id_1);
                        num ++;
                    });             
                    ASYNC({
                        awe_mapper_create_id(&id_2);
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

    printf("Average time per do .. finish and two blocking yields (no dispatch): %lu cycles (%s)\n", 
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
    
    printf("Average time per context switch: %lu cycles\n", 
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
            awe_mapper_create_id(&id_1);
            //printf("ASYNC 1: warm up yield\n");
            THCYieldAndSaveNoDispatch(id_1);
            //printf("ASYNC 1: got control back\n");
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //printf("ASYNC 1: ready to yield\n");
                THCYieldToIdAndSaveNoDispatch(id_2,id_1);
                //printf("ASYNC 1: got control back\n");
            }
            awe_mapper_remove_id(id_1);
            awe_mapper_remove_id(id_2);
        });
        
        ASYNC({
            int i;
            awe_mapper_create_id(&id_2);
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //printf("ASYNC 2: ready to yield\n");
                THCYieldToIdAndSaveNoDispatch(id_1,id_2);
                //printf("ASYNC 2: got control back\n");
            }
        });
        THCYieldToIdNoDispatch_TopLevel(id_1);
    });

    t2 = test_fipc_stop_stopwatch();

    printf("Average time per context switch (no dispatch): %lu cycles\n", 
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
            awe_mapper_create_id(&id_1);
            //printf("ASYNC 1: warm up yield\n");
            THCYieldAndSaveNoDispatch(id_1);
            //printf("ASYNC 1: got control back\n");
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //printf("ASYNC 1: ready to yield\n");
                THCYieldToIdAndSaveNoDispatchDirect(id_2,id_1);
                //printf("ASYNC 1: got control back\n");
            }
            awe_mapper_remove_id(id_1);
            awe_mapper_remove_id(id_2);
        });
        
        ASYNC({
            int i;
            awe_mapper_create_id(&id_2);
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //printf("ASYNC 2: ready to yield\n");
                THCYieldToIdAndSaveNoDispatchDirect(id_1,id_2);
                //printf("ASYNC 2: got control back\n");
            }
        });
        THCYieldToIdNoDispatch_TopLevel(id_1);
    });

    t2 = test_fipc_stop_stopwatch();

    printf("Average time per context switch (no dispatch, direct): %lu cycles\n", 
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
            awe_mapper_create_id(&id_1);
            //printf("ASYNC 1: warm up yield\n");
            THCYieldAndSaveNoDispatch(id_1);
            //printf("ASYNC 1: got control back\n");
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //printf("ASYNC 1: ready to yield\n");
                THCYieldToIdAndSaveNoDispatchDirectTrusted(id_2,id_1);
                //printf("ASYNC 1: got control back\n");
            }
            awe_mapper_remove_id(id_1);
            awe_mapper_remove_id(id_2);
        });
        
        ASYNC({
            int i;
            awe_mapper_create_id(&id_2);
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //printf("ASYNC 2: ready to yield\n");
                THCYieldToIdAndSaveNoDispatchDirectTrusted(id_1,id_2);
                //printf("ASYNC 2: got control back\n");
            }
        });
        THCYieldToIdNoDispatch_TopLevel(id_1);
    });

    t2 = test_fipc_stop_stopwatch();

    printf("Average time per context switch (no dispatch, direct, trusted): %lu cycles\n", 
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
                //printf("ASYNC 1: warm up yield\n");
                THCYieldWithAwe(&awe_1);
                //printf("ASYNC 1: got control back\n");
                            
                //printf("ASYNC 1: ready to yield\n");
                //THCYieldToAwe(&awe_1, &awe_2);
                //printf("ASYNC 1: got control back\n");
               
            });
        
            ASYNC({
                //printf("ASYNC 2: ready to yield\n");
                THCYieldToAwe(&awe_2, &awe_1);
                //printf("ASYNC 2: got control back\n");
            });

            THCYieldToAweNoDispatch_TopLevel(&awe_1);

        });
    };

    t2 = test_fipc_stop_stopwatch();

    printf("Average time to do...finish and two yeilds (direct awe): %lu cycles\n", 
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
            //printf("ASYNC 1: warm up yield\n");
            THCYieldWithAwe(&awe_1);
            //printf("ASYNC 1: got control back\n");
                            
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //printf("ASYNC 1: ready to yield\n");
                THCYieldToAwe(&awe_1, &awe_2);
                //printf("ASYNC 1: got control back\n");
            }
        });
        
        ASYNC({
            int i;
            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
            {
                //printf("ASYNC 2: ready to yield\n");
                THCYieldToAwe(&awe_2, &awe_1);
                //printf("ASYNC 2: got control back\n");
            }
        });
        THCYieldToAweNoDispatch_TopLevel(&awe_1);

    });

    t2 = test_fipc_stop_stopwatch();

    printf("Average time per context switch (direct awe): %lu cycles\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);


    return 0;
}


int main (void) {
    
    thc_init();
    test_async();
    test_async_yield();

    test_basic_do_finish_create();
    test_basic_nonblocking_async_create();  
    test_basic_N_nonblocking_asyncs_create();

    test_basic_N_blocking_asyncs_create(); 
    test_basic_N_blocking_id_asyncs();
    test_basic_N_blocking_id_asyncs_and_N_yields_back();

    test_do_finish_yield();
    test_do_finish_yield_no_dispatch();
   // test_create_and_ctx_switch_to_awe();

    test_ctx_switch_no_dispatch();
    test_ctx_switch_no_dispatch_direct();
    test_ctx_switch_no_dispatch_direct_trusted();
    test_ctx_switch_to_awe();
    
    thc_done();
    return 0; 
}

