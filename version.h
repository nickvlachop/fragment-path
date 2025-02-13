#define MultiThread
#define WINDOWS
#if defined(MultiThread)
#if defined(WINDOWS)
#include <windows.h>
#define mutex_var CRITICAL_SECTION
#define cond_var CONDITION_VARIABLE
#define cond_sleep(x,y) SleepConditionVariableCS(x,y,INFINITE)
#define cond_broadcast(x) WakeAllConditionVariable(x)
#define mutex_lock(x) EnterCriticalSection(x)
#define mutex_unlock(x) LeaveCriticalSection(x)
#define mutex_init(x) InitializeCriticalSection(x)
#define mutex_destroy(x) DeleteCriticalSection(x)
#define cond_init(x) InitializeConditionVariable(x)
#define cond_destroy(x)
#else
#include <pthread.h>
#define mutex_var pthread_mutex_t
#define cond_var pthread_cond_t
#define cond_sleep(x,y) pthread_cond_wait(x,y)
#define cond_broadcast(x) pthread_cond_broadcast(x)
#define mutex_lock(x) pthread_mutex_lock(x)
#define mutex_unlock(x) pthread_mutex_unlock(x)
#define mutex_init(x) pthread_mutex_init(x,NULL)
#define mutex_destroy(x) pthread_mutex_destroy(x)
#define cond_init(x) pthread_cond_init(x,NULL)
#define cond_destroy(x) pthread_cond_destroy(x)
#endif
#endif