#if !defined(GAME_ASSERT_H)

#if GAME_SLOW
// TODO(yuval, eran): Temporary
#include <stdio.h>

#define Assert_(expression, shouldCrash) \
if (!(expression)) \
{ \
    char message[256]; \
    snprintf(message, sizeof(message), "Assertion Failure: %s at %s:%s:%d", \
    #expression, __FILE__, __FUNCTION__, __LINE__); \
    platform.DisplayMessageBox("ASSERSION FAILURE", message); \
    if (shouldCrash) \
    { \
        *(volatile int*)0 = 0; \
    } \
}

#define Assert(expression) Assert_((expression), true)
#define SoftAssert(expression) Assert_((expression), false)

#else
#define Assert(expression)
#define SoftAssert(expression)
#endif

#define GAME_ASSERT_H
#endif