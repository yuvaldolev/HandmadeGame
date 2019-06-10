#if !defined(GAME_MATH_H)

///////////////////////////////
//     Scalar Operations     //
///////////////////////////////
inline f32
Square(f32 Value)
{
    f32 Result = Value * Value;
    return Result;
}

//////////////////////////////
//      v2 Operations       //
//////////////////////////////
inline v2
V2(f32 X, f32 Y)
{
    v2 Result;
    
    Result.X = X;
    Result.Y = Y;
    
    return Result;
}

inline v2
operator+(v2 A, v2 B)
{
    v2 Result;
    
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    
    return Result;
}

inline v2&
operator+=(v2& A, v2 B)
{
    A = A + B;
    return A;
}

inline v2
operator-(v2 A)
{
    v2 Result;
    
    Result.X = -A.X;
    Result.Y = -A.Y;
    
    return Result;
}

inline v2
operator-(v2 A, v2 B)
{
    v2 Result = A + (-B);
    return Result;
}

inline v2&
operator-=(v2& A, v2& B)
{
    A = A - B;
    return A;
}

inline v2
operator*(f32 A, v2 B)
{
    v2 Result;
    
    Result.X = A * B.X;
    Result.Y = A * B.Y;
    
    return Result;
}

inline v2
operator*(v2 B, f32 A)
{
    v2 Result = A * B;
    return Result;
}

inline v2&
operator*=(v2& B, f32 A)
{
    B = A * B;
    return B;
}

inline f32
Inner(v2 A, v2 B)
{
    f32 Result = A.X * B.X + A.Y * B.Y;
    return Result;
}

inline f32
LengthSq(v2 A)
{
    f32 Result = Inner(A, A);
    return Result;
}

#define GAME_MATH_H
#endif