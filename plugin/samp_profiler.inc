#if defined _samp_profiler_included
    #endinput
#endif
#define _samp_profiler_included

native SP_Register(const name[]);
native SP_Begin(sectionid);
native SP_End(token);
native SP_SetEnabled(bool:enabled);
native SP_IsEnabled();
native SP_Reset();
// threshold is in microseconds; 0 prints every section with calls.
native SP_Dump(threshold = 0);
