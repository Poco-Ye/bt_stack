
#ifndef ucos_osl_h
#define ucos_osl_h

/* This is really platform specific and not OS specific. */
#ifndef BWL_UC_TICKS_PER_SECOND
#define BWL_UC_TICKS_PER_SECOND OS_TICKS_PER_SEC
#endif

//#define OSL_MSEC_TO_TICKS(msec)  ((BWL_UC_TICKS_PER_SECOND * (msec)) / 1000)
#define OSL_TICKS_TO_MSEC(ticks) ((1000 * (ticks)) / BWL_UC_TICKS_PER_SECOND)

#define __FUNCTION__	__func__
#endif  /* ucos_osl_h  */
