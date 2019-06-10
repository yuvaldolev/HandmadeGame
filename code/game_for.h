#if !defined(GAME_FOR_H)

///////////////////////////////
//       Array Foreach       //
///////////////////////////////
#define ArrayForeach_(Array) \
for (u32 Index = 0; Index < ArrayCount(Array); ++Index) \
if (auto It = &(Array)[Index])

///////////////////////////////
//         For Range         //
///////////////////////////////
#define DoArgJoin_(Arg1, Arg2) Arg1 ## Arg2
#define ArgJoin_(Arg1, Arg2) DoArgJoin_(Arg1, Arg2)

#define Range2_(Beg, End) for (auto ArgJoin_(I, __LINE__) = Beg; \
ArgJoin_(I, __LINE__) < End; \
++ArgJoin_(I, __LINE__))
#define Range1_(End) Range2_(0, End)
#define GetRange_(_1, _2, Name, ...) Name
#define Range(...) GetRange_(__VA_ARGS__, Range2_, Range1_)(__VA_ARGS__)


#define ClosedRange2_(Beg, End) for (auto ArgJoin_(I, __LINE__) = Beg; \
ArgJoin_(I, __LINE__) <= End; \
++ArgJoin_(I, __LINE__))
#define ClosedRange1_(End) ClosedRange2_(0, End)
#define GetClosedRange_(_1, _2, Name, ...) Name
#define ClosedRange(...) GetClosedRange_(__VA_ARGS__, ClosedRange2_, ClosedRange1_)(__VA_ARGS__)

#define ForRange_(Name, Range) Range \
for (b32 ArgJoin_(B, __LINE__) = true; \
ArgJoin_(B, __LINE__); \
ArgJoin_(B, __LINE__) = false) \
for (auto Name = ArgJoin_(I, __LINE__); \
ArgJoin_(B, __LINE__);\
ArgJoin_(B, __LINE__) = false)

///////////////////////////////
//         For Macro         //
///////////////////////////////
#define GetFor_(_1, _2, Name, ...) Name
#define For(...) GetFor_(__VA_ARGS__, ForRange_, ArrayForeach_)(__VA_ARGS__)

#define GAME_FOR_H
#endif