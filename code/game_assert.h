#if !defined(GAME_ASSERT_H)

#if GAME_SLOW

// TODO(yuval, eran): Temporary
#include <stdio.h>

#define Assert_(Expression, ShouldCrash) \
if (!(Expression)) \
{ \
    char Message[256]; \
    snprintf(Message, sizeof(Message), "Assertion Failure: %s at %s:%s:%d", \
    #Expression, __FILE__, __FUNCTION__, __LINE__); \
    Platform.DisplayMessageBox("ASSERSION FAILURE", Message); \
    if (ShouldCrash) \
    { \
        *(volatile int*)0 = 0; \
    } \
}

#define Assert(Expression) Assert_((Expression), true)
#define SoftAssert(Expression) Assert_((Expression), false)

#else
#define Assert(Expression)
#define SoftAssert(Expression)
#endif

#define GAME_ASSERT_H
#endif