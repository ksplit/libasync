#include <thc.h>
#include <thcinternal.h>
#include <awe_mapper.h>
#include <stdio.h>
#include <test_helpers.h>

#define NUM_SWITCH_MEASUREMENTS 1000000
//#define NUM_SWITCH_MEASUREMENTS 8

void test_async(){

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

static int test_do_finish_yield(void)
{
    unsigned long t1, t2;
    unsigned int id_1;
    int i = 0;


    t1 = test_fipc_start_stopwatch();

    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
            DO_FINISH({
                    ASYNC({
                        awe_mapper_create_id(&id_1);
                        THCYieldAndSave(id_1);
                        awe_mapper_remove_id(id_1);
                    });             
                    ASYNC({
                            THCYieldToId(id_1);
                        });             
            });
    }
    t2 = test_fipc_start_stopwatch(); 
    printf("Average time per do...finish and two yields: %lu\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);
 

    return 0;
}

static int test_do_finish_yield_no_dispatch(void)
{
    unsigned long t1, t2;
    unsigned int id_1, id_2;
    int i = 0;
   
    t1 = test_fipc_start_stopwatch();

    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
            DO_FINISH({
                    
                    ASYNC({
                        awe_mapper_create_id(&id_1);
                        THCYieldAndSaveNoDispatch(id_1);
                        awe_mapper_remove_id(id_1);
                    });             
                    ASYNC({
                        awe_mapper_create_id(&id_2);
                        THCYieldToIdAndSaveNoDispatch(id_1,id_2);
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

    printf("Average time per do_finish and two yields (no dispatch): %lu\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);


    return 0;
}



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
    
    printf("Average time per context switch: %lu\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);
                                        


    return 0;
}

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

    printf("Average time per context switch (no dispatch): %lu\n", 
          (t2 - t1)/NUM_SWITCH_MEASUREMENTS);


    return 0;
}


int main (void) {
    
    thc_init();

    //test_async();
   
    //test_async_yield();

    test_ctx_switch_no_dispatch();
    
    thc_done();

    return 0; 
}

