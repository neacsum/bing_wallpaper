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

#define PVERS_MAJOR     0
#define PVERS_MINOR     1
#define FVERS_MAJOR     0
#define FVERS_MINOR     1

#define COMP_STRING     "Mircea Neacsu"
#define LEGAL_STRING    "Copyright (c) Mircea Neacsu 2025"

#define INTERNAL_NAME   "wallpaper"
#define ORIG_FILENAME   "wallpaper.exe"
#define FDESC_STRING    "Bing daily wallpaper"
#define FVERS_STRING    DOT4(PVERS_MAJOR, PVERS_MINOR, FVERS_MAJOR, FVERS_MINOR)
#endif
