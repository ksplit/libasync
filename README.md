Note: If this is being used in the vanilla kernel, the following should be added to the end of the thread task struct in sched.h:
#ifdef CONFIG_LAZY_THC
         struct ptstate_t *ptstate;
#endif
