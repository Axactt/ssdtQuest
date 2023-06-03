#pragma once
#include<cstdio>


namespace logger
{

template<typename ... T>

inline void logA(const char* format, T const& ... args)

{

	printf(format, args ...);

}

template<typename ...T>

inline void logW(const wchar_t* format, T const& ... args)

{

	wprintf(format, args ...);

}

}