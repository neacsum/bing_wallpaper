#ifndef VERSION_H
#define VERSION_H

#define STR(X) #X
#define STRINGERIZE(X) STR(X)
#define DOT2(A,B) STRINGERIZE(A) "." STRINGERIZE(B) 
#define DOT3(A,B,C) STRINGERIZE(A) "." STRINGERIZE(B) "." STRINGERIZE(C) 
#define DOT4(A,B,C,D) STRINGERIZE(A) "." STRINGERIZE(B) "." STRINGERIZE(C) "." STRINGERIZE(D) 

//target windows version
#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#endif

#define FVERS_MAJOR     0
#define FVERS_MINOR     2


#define PVERS_MAJOR     FVERS_MAJOR
#define PVERS_MINOR     FVERS_MINOR

#define COMP_STRING     "Mircea Neacsu"
#define LEGAL_STRING    "Copyright (c) Mircea Neacsu 2025"

#define INTERNAL_NAME   "wallpaper"
#define ORIG_FILENAME   "wallpaper.exe"
#define FDESC_STRING    "Bing daily wallpaper"
#define FVERS_STRING    DOT4(PVERS_MAJOR, PVERS_MINOR, FVERS_MAJOR, FVERS_MINOR)
#endif
