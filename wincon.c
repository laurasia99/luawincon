/* wincon.h - Lua interface to Windows Console API
 * (c) Laurasia Consulting Pty Ltd
 * Refer bottom of file for license (MIT)
 *
 * https://docs.microsoft.com/en-us/windows/console/console-reference
 *  - Assumes Win32 console application (AllocConsole not supported)
 *  - Assumes single process connected to the console (no process groups)
 *  - Console aliases not supported
 *  - Console control handlers (^C, ^Break) not supported
 *  - Console fonts not supported
 *  - Console history lists not supported
 *  - Pseudo consoles not supported
 */


#include "lua.h"
#include "lauxlib.h"

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>

#ifdef UNICODE
#error "Unicode not supported"
#endif

#define STRINGIFY(X) #X
#define TOSTRING(X) STRINGIFY(X)

/* Diagnostic */

static void tabledump(lua_State *L,int i);
static void elementdump(lua_State *L,int i) {
  int t=lua_type(L,i);
  switch(t){
  case LUA_TSTRING:printf("'%s'",lua_tostring(L,i));break;
  case LUA_TBOOLEAN:printf(lua_toboolean(L,i)?"true":"false");break;
  case LUA_TNUMBER:printf("%g",lua_tonumber(L,i));break;
  case LUA_TTABLE:tabledump(L,i);break;
  default:printf("%s",lua_typename(L,t));break;
  }
}
static void tabledump(lua_State *L,int i) {
  if(i<0)i=lua_gettop(L)+1+i;
  lua_pushnil(L);
  printf("{");
  while(lua_next(L,i)!=0){
    elementdump(L,-2); printf(":"); elementdump(L,-1); printf(",");
    lua_pop(L,1);
  }
  printf("}");
}

static void stackdump(lua_State *L) {
  int i;
  int top=lua_gettop(L);
  for(i=1;i<=top;++i){
    int t=lua_type(L,i);
    switch(t){
    case LUA_TSTRING:printf("'%s'",lua_tostring(L,i));break;
    case LUA_TBOOLEAN:printf(lua_toboolean(L,i)?"true":"false");break;
    case LUA_TNUMBER:printf("%g",lua_tonumber(L,i));break;
    case LUA_TTABLE:tabledump(L,i);break;
    default:printf("%s",lua_typename(L,t));break;
    }
    printf(" ");
  }
  printf("\n");
}

/* Error return */
static int lGetLastError(lua_State *L) {
  DWORD rv;
  LPTSTR lpBuffer;
  lua_Debug ar;
  lua_getstack(L,0,&ar);
  lua_getinfo(L,"n",&ar);
  lua_pushnil(L);
  rv = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                     NULL, GetLastError(), 0, (LPTSTR)&lpBuffer, 0, 0);
  if(0!=rv){
    lua_pushfstring(L,"%s():%s",ar.name,lpBuffer);
    LocalFree(lpBuffer);
  } else { /* FormatMessage() failed */
    lua_pushfstring(L,"%s():Win32 error %I",ar.name,(lua_Integer)GetLastError());
  }
  return 2;
}

/* Anchor unique keys for registry */
static char hin,hout;

static void setHandle(lua_State *L,DWORD n,void *key) {
  HANDLE h=GetStdHandle(n);
  if(INVALID_HANDLE_VALUE==h) {
    lGetLastError(L);
    lua_error(L);
  }
  lua_pushinteger(L,(lua_Integer)h);
  lua_rawsetp(L,LUA_REGISTRYINDEX,key);
}

static HANDLE getHandle(lua_State *L,void *key) {
  lua_rawgetp(L,LUA_REGISTRYINDEX,key);
  return (HANDLE)lua_tointeger(L,-1);
}

#define CONIN(L)  getHandle(L,&hin)
#define CONOUT(L) getHandle(L,&hout)

/* Core functionality required for virtual terminals */

static int lGetConsoleMode(lua_State *L,HANDLE h) {
  DWORD m;
  if(0==GetConsoleMode(h,&m)) return lGetLastError(L);
  lua_pushinteger(L,(lua_Integer)m);
  return 1;
}
static int lSetConsoleMode(lua_State *L,HANDLE h) {
  DWORD m=(DWORD)luaL_checkinteger(L,1);
  if(0==SetConsoleMode(h,m)) return lGetLastError(L);
  return 0;
}
static int lGetConsoleInputMode(lua_State *L) {
  return lGetConsoleMode(L,CONIN(L));
}
static int lGetConsoleOutputMode(lua_State *L) {
  return lGetConsoleMode(L,CONOUT(L));
}
static int lSetConsoleInputMode(lua_State *L) {
  return lSetConsoleMode(L,CONIN(L));
}
static int lSetConsoleOutputMode(lua_State *L) {
  return lSetConsoleMode(L,CONOUT(L));
}

/** Thin wrappers - deprecated Win32 interfaces */

/* Code pages */

static int lGetConsoleCP(lua_State *L) {
  lua_pushinteger(L,(lua_Integer)GetConsoleCP());
  return 1;
}
static int lGetConsoleOutputCP(lua_State *L) {
  lua_pushinteger(L,(lua_Integer)GetConsoleOutputCP());
  return 1;
}
static int lSetConsoleCP(lua_State *L) {
  UINT cp=(DWORD)luaL_checkinteger(L,1);
  if(0==SetConsoleCP(cp))
    return lGetLastError(L);
  return 0;
}
static int lSetConsoleOutputCP(lua_State *L) {
  UINT cp=(DWORD)luaL_checkinteger(L,1);
  if(0==SetConsoleOutputCP(cp))
    return lGetLastError(L);
  return 0;
}

/* Input */

/* INPUT_RECORD:Key/Mouse/Resize->{table}; pushed to stack */
static int convertKeyInput(lua_State *L,KEY_EVENT_RECORD *ev) {
  lua_createtable(L,0,5);
  lua_pushliteral(L,"key");                 lua_setfield(L,-2,"type");
  lua_pushboolean(L,ev->bKeyDown);          lua_setfield(L,-2,"keydown");
  lua_pushinteger(L,ev->wRepeatCount);      lua_setfield(L,-2,"count");
  lua_pushinteger(L,ev->wVirtualKeyCode);   lua_setfield(L,-2,"vkey");
  lua_pushinteger(L,ev->dwControlKeyState); lua_setfield(L,-2,"state");
  return 1;
}
static int convertMouseInput(lua_State *L,MOUSE_EVENT_RECORD *ev) {
  lua_createtable(L,0,6);
  lua_pushliteral(L,"mouse");               lua_setfield(L,-2,"type");
  lua_pushinteger(L,ev->dwMousePosition.X); lua_setfield(L,-2,"x");
  lua_pushinteger(L,ev->dwMousePosition.Y); lua_setfield(L,-2,"y");
  lua_pushinteger(L,ev->dwButtonState);     lua_setfield(L,-2,"buttons");
  lua_pushinteger(L,ev->dwControlKeyState); lua_setfield(L,-2,"state");
  lua_pushinteger(L,ev->dwEventFlags);      lua_setfield(L,-2,"event");
  return 1;
}
static int convertResizeInput(lua_State *L,WINDOW_BUFFER_SIZE_RECORD *ev) {
  lua_createtable(L,0,3);
  lua_pushliteral(L,"resize");     lua_setfield(L,-2,"type");
  lua_pushinteger(L,ev->dwSize.X); lua_setfield(L,-2,"columns");
  lua_pushinteger(L,ev->dwSize.Y); lua_setfield(L,-2,"rows");
  return 1;
}
static int convertInput(lua_State *L,INPUT_RECORD *ev) {
  switch(ev->EventType) {
  case KEY_EVENT:          
    return convertKeyInput(L,&(ev->Event.KeyEvent));
  case MOUSE_EVENT:
    return convertMouseInput(L,&(ev->Event.MouseEvent));
  case WINDOW_BUFFER_SIZE_EVENT:
    return convertResizeInput(L,&(ev->Event.WindowBufferSizeEvent));
  }
  return 0;  /* ignore other events */
}
static void convertInputs(lua_State *L,int n,INPUT_RECORD *ev) {
  int i,j;
  for(i=j=0;i<n;++i)
    if(convertInput(L,ev+i)) lua_rawseti(L,-2,++j);
}

static int lFlushConsoleInputBuffer(lua_State *L) {
  if(0==FlushConsoleInputBuffer(CONIN(L)))
    return lGetLastError(L);
  return 0;
}
#ifdef GET_CONSOLE_SELECTION_INFO
/* 7 June 22
 * I have not been able to get this function to work. Is there
 * some interaction with ENABLE_QUICK_EDIT_MODE that I don't know?
 * In any case, avoid publishing this.
 */
static int lGetConsoleSelectionInfo(lua_State *L) {
  CONSOLE_SELECTION_INFO cs;
  if(0==GetConsoleSelectionInfo(&cs))
    return lGetLastError(L);
  lua_createtable(L,0,7);
  lua_pushinteger(L,cs.dwFlags);             lua_setfield(L,-2,"flags");
  lua_pushinteger(L,cs.dwSelectionAnchor.X); lua_setfield(L,-2,"x");
  lua_pushinteger(L,cs.dwSelectionAnchor.Y); lua_setfield(L,-2,"y");
  lua_pushinteger(L,cs.srSelection.Left);    lua_setfield(L,-2,"left");
  lua_pushinteger(L,cs.srSelection.Top);     lua_setfield(L,-2,"top");
  lua_pushinteger(L,cs.srSelection.Right);   lua_setfield(L,-2,"right");
  lua_pushinteger(L,cs.srSelection.Bottom);  lua_setfield(L,-2,"bottom");
  return 1;
}
#endif
static int lGetNumberOfConsoleInputEvents(lua_State *L) {
  DWORD n;
  if(0==GetNumberOfConsoleInputEvents(CONIN(L),&n))
    return lGetLastError(L);
  lua_pushinteger(L,(lua_Integer)n);
  return 1;
}
static int lGetNumberOfConsoleMouseButtons(lua_State *L) {
  DWORD n;
  if(0==GetNumberOfConsoleMouseButtons(&n))
    return lGetLastError(L);
  lua_pushinteger(L,(lua_Integer)n);
  return 1;
}
static int lPeekConsoleInput(lua_State *L) {
  HANDLE h=CONIN(L);
  INPUT_RECORD *ip;
  DWORD n,m;
  if(0==GetNumberOfConsoleInputEvents(h,&n))
    return lGetLastError(L);
  lua_createtable(L,n,0);
  if(0<n) { /* Avoid blocking; no waiting for an event */
    if(0==PeekConsoleInput(h,ip=_alloca(n*sizeof(*ip)),n,&m))
      return lGetLastError(L);
    convertInputs(L,m,ip);
  }
  return 1;
}
static int lReadConsoleInput(lua_State *L) {
  HANDLE h=CONIN(L);
  INPUT_RECORD *ip;
  DWORD n,m;
  if(0==GetNumberOfConsoleInputEvents(h,&n))
    return lGetLastError(L);
  lua_createtable(L,n,0);
  if(0<n) { /* Avoid blocking; no waiting for an event */
    ip=_alloca(n*sizeof(*ip));
    if(0==ReadConsoleInput(h,ip=_alloca(n*sizeof(*ip)),n,&m))
      return lGetLastError(L);
    convertInputs(L,m,ip);
  }
  return 1;
}
static int lWaitForConsoleInput(lua_State *L) {
/* Return true if input signalled */
  DWORD rv;
  lua_Integer wait=INFINITE; /* optional argument; default is to block */
  if(1<=lua_gettop(L))
    wait=luaL_checkinteger(L,1);
  if(WAIT_FAILED==(rv=WaitForSingleObject(CONIN(L),wait)))
     return lGetLastError(L);
  lua_pushboolean(L,rv==WAIT_OBJECT_0);
  return 1;
}

/* Output */

static int lFillConsoleOutputAttribute(lua_State *L) {
/* x,y,attribute,count */
  DWORD m;
  COORD xy;
  lua_Integer attr,n;
  xy.X=luaL_checkinteger(L,1); xy.Y=luaL_checkinteger(L,2);
  attr=luaL_checkinteger(L,3); n=luaL_checkinteger(L,4);
  if(0==FillConsoleOutputAttribute(CONOUT(L),attr,n,xy,&m))
    return lGetLastError(L);
  return 0;
}
static int lFillConsoleOutputCharacter(lua_State *L) {
/* x,y,char,count */
  DWORD m;
  COORD xy;
  lua_Integer ch,n;
  xy.X=luaL_checkinteger(L,1); xy.Y=luaL_checkinteger(L,2);
  ch=luaL_checkinteger(L,3); n=luaL_checkinteger(L,4);
  if(0==FillConsoleOutputCharacter(CONOUT(L),ch,n,xy,&m))
    return lGetLastError(L);
  return 0;
}
static int lSetConsoleTextAttribute(lua_State *L) {
/* attr */
  lua_Integer attr;
  attr=luaL_checkinteger(L,1);
  if(0==SetConsoleTextAttribute(CONOUT(L),attr))
    return lGetLastError(L);
  return 0;
}
static int lWriteConsole(lua_State *L) {
/* text->#written */
  DWORD m;
  size_t n;
  const char *s=luaL_checklstring(L,1,&n);
  if(0==WriteConsole(CONOUT(L),s,n,&m,0))
    return lGetLastError(L);
  lua_pushinteger(L,(lua_Integer)m);
  return 1;
}
#ifdef XXX
/* WriteConsoleOutput() copies data from a raw buffer.
 * This creates a more complex Lua interface, and hence,
 * has not been supported.
 */
static int lWriteConsoleOutput(lua_State *L);
/* Microsoft recommends that these functions not be used, and
 * that alternatively the (virtual terminal) text formatting and
 * cursor positioning sequences be used.
 */
static int lWriteConsoleOutputAttribute(lua_State *L);
static int lWriteConsoleOutputCharacter(lua_State *L);
#endif

/* Cursor */

static int lGetConsoleCursorInfo(lua_State *L) {
/* (visible,size) */
  CONSOLE_CURSOR_INFO ci;
  if(0==GetConsoleCursorInfo(CONOUT(L),&ci))
    return lGetLastError(L);
  lua_pushboolean(L,ci.bVisible);
  lua_pushinteger(L,(lua_Integer)ci.dwSize);
  return 2;
}
static int lSetConsoleCursorInfo(lua_State *L) {
/* visible=TRUE,size=100 */
  CONSOLE_CURSOR_INFO ci={100,TRUE};
  if(1<=lua_gettop(L)) {
    luaL_checkany(L,1);
    ci.bVisible=lua_toboolean(L,1);
    if(2<=lua_gettop(L)) {
      ci.dwSize=luaL_checkinteger(L,2);
      if(ci.dwSize<1||ci.dwSize>100) luaL_error(L,"Size must be in the range 1..100 (had %d)",(int)ci.dwSize);
  } }
  if(0==SetConsoleCursorInfo(CONOUT(L),&ci))
    return lGetLastError(L);
  return 0;
}
static int lSetConsoleCursorPosition(lua_State *L) {
/* x=0,y=0 */
  COORD xy={0,0};
  if(1<=lua_gettop(L)) xy.X=luaL_checkinteger(L,1);
  if(2<=lua_gettop(L)) xy.Y=luaL_checkinteger(L,2);
  if(0==SetConsoleCursorPosition(CONOUT(L),xy))
    return lGetLastError(L);
  return 0;
}

/* Screen buffers */

static int lGetConsoleScreenBufferInfo(lua_State *L) {
  CONSOLE_SCREEN_BUFFER_INFO bi;
  if(0==GetConsoleScreenBufferInfo(CONOUT(L),&bi))
    return lGetLastError(L);
  lua_createtable(L,0,11);
  lua_pushinteger(L,bi.dwSize.X);           lua_setfield(L,-2,"columns");
  lua_pushinteger(L,bi.dwSize.Y);           lua_setfield(L,-2,"rows");
  lua_pushinteger(L,bi.dwCursorPosition.X); lua_setfield(L,-2,"cx");
  lua_pushinteger(L,bi.dwCursorPosition.Y); lua_setfield(L,-2,"cy");
  lua_pushinteger(L,bi.wAttributes);        lua_setfield(L,-2,"attributes");
  lua_pushinteger(L,bi.srWindow.Left);      lua_setfield(L,-2,"left");
  lua_pushinteger(L,bi.srWindow.Top);       lua_setfield(L,-2,"top");
  lua_pushinteger(L,bi.srWindow.Right);     lua_setfield(L,-2,"right");
  lua_pushinteger(L,bi.srWindow.Bottom);    lua_setfield(L,-2,"bottom");
  lua_pushinteger(L,bi.dwMaximumWindowSize.X); lua_setfield(L,-2,"maxcols");
  lua_pushinteger(L,bi.dwMaximumWindowSize.Y); lua_setfield(L,-2,"maxrows");
  return 1;
}

#ifdef XXX
static int lCreateConsoleScreenBuffer(lua_State *L);
static int lGetConsoleScreenBufferInfoEx(lua_State *L);
static int lGetLargestConsoleWindowSize(lua_State *L);
static int lScrollConsoleScreenBuffer(lua_State *L);
static int lSetConsoleActiveScreenBuffer(lua_State *L);
static int lSetConsoleScreenBufferInfoEx(lua_State *L);
static int lSetConsoleScreenBufferSize(lua_State *L);
static int lSetConsoleWindowInfo(lua_State *L);
#endif

/* Titles */

static int gettitle(lua_State *L,DWORD(*fn)(LPTSTR,DWORD)) {
  TCHAR buf[4*1024]; /* 4k is excessive */
  DWORD rv=(*fn)(buf,sizeof(buf)/sizeof(buf[0]));
  if(0==rv&&GetLastError()!=ERROR_SUCCESS) {
    return lGetLastError(L);
  } else if(0==rv) { /* buffer too small - really! */
    lua_pushnil(L);
    lua_pushstring(L,"Abnormal error " TOSTRING(__LINE__));
    return 2;
  } else {
    lua_pushlstring(L,buf,rv);
    return 1;
  }
}

static int lGetConsoleOriginalTitle(lua_State *L) {
  return gettitle(L,GetConsoleOriginalTitle);
}
static int lGetConsoleTitle(lua_State *L) {
  return gettitle(L,GetConsoleTitle);
}
static int lSetConsoleTitle(lua_State *L) {
  const char * s=luaL_checkstring(L,1);
  if(0==SetConsoleTitle(s))
    return lGetLastError(L);
  return 0;
}

#ifdef AVOID_UNSUPPORTED

/* Not likely to be supported - discards mouse/resize events */
static int lReadConsole(lua_State *L);

/* Not likely to be supported - "wrong way verb" */
static int lWriteConsoleInput(lua_State *L);

/* Unsupported - "Wrong way" actions reading raw console data */
static int lReadConsoleOutput(lua_State *L);
static int lReadConsoleOutputAttribute(lua_State *L);
static int lReadConsoleOutputCharacter(lua_State *L);

/* Unsupported - changing standard handles */
static int lSetStdHandle(lua_State *L);

/* Unsupported - access to underlying window handle */
static int lGetConsoleWindow(lua_State *L);

/* Unsupported - console fonts (deprecated?) */
static int lGetConsoleFontSize(lua_State *L);
static int lGetCurrentConsoleFont(lua_State *L);
static int lGetCurrentConsoleFontEx(lua_State *L);
static int lSetCurrentConsoleFontEx(lua_State *L);

/* Unsupported - display modes (deprecated?) */
static int lGetConsoleDisplayMode(lua_State *L);
  DWORD m;
  if(0==GetConsoleDisplayMode(&m)){
    return lGetLastError(L);
  } else {
    lua_pushinteger(L,(lua_Integer)ci.dwSize);
    return 1;
  }
}
static int lSetConsoleDisplayMode(lua_State *L);

#endif /* AVOID_UNSUPPORTED */

/*
 */

static const luaL_Reg winconlib[] = {
#define C(x) {#x,l##x}

/* Core functionality */
  C(GetConsoleInputMode),
  C(GetConsoleOutputMode),
  C(SetConsoleInputMode),
  C(SetConsoleOutputMode),

/* Code pages */
  C(GetConsoleCP),
  C(GetConsoleOutputCP),
  C(SetConsoleCP),
  C(SetConsoleOutputCP),

/* Input */
  C(FlushConsoleInputBuffer),
#ifdef GET_CONSOLE_SELECTION_INFO
  C(GetConsoleSelectionInfo),
#endif
  C(GetNumberOfConsoleInputEvents),
  C(GetNumberOfConsoleMouseButtons),
  C(PeekConsoleInput),
  C(ReadConsoleInput),
  C(WaitForConsoleInput),

/* Output */
  C(FillConsoleOutputAttribute),
  C(FillConsoleOutputCharacter),
  C(SetConsoleTextAttribute),
  C(WriteConsole),
  /*C(WriteConsoleOutput),*/
  /*C(WriteConsoleOutputAttribute),*/
  /*C(WriteConsoleOutputCharacter),*/

/* Cursor */
  C(GetConsoleCursorInfo),
  C(SetConsoleCursorInfo),
  C(SetConsoleCursorPosition),

/* Screen buffers */
  C(GetConsoleScreenBufferInfo),
#if 0
  C(CreateConsoleScreenBuffer),
  C(GetConsoleScreenBufferInfoEx),
  C(GetLargestConsoleWindowSize),
  C(ScrollConsoleScreenBuffer),
  C(SetConsoleActiveScreenBuffer),
  C(SetConsoleScreenBufferInfoEx),
  C(SetConsoleScreenBufferSize),
  C(SetConsoleWindowInfo),
#endif

/* Titles */
  C(GetConsoleOriginalTitle),
  C(GetConsoleTitle),
  C(SetConsoleTitle),

/* Unsupported */
#ifdef AVOID_UNSUPPORTED
/*C(SetStdHandle),*/
/*C(ReadConsole),*/
/*C(WriteConsoleInput),*/
/*C(ReadConsoleOutput),*/
/*C(ReadConsoleOutputAttribute),*/
/*C(ReadConsoleOutputCharacter),*/
/*C(GetConsoleWindow),*/
/*C(GetConsoleFontSize),*/
/*C(GetCurrentConsoleFont),*/
/*C(GetCurrentConsoleFontEx),*/
/*C(SetCurrentConsoleFontEx),*/
/*C(GetConsoleDisplayMode),*/
/*C(SetConsoleDisplayMode),*/
#endif

#undef C
  {NULL, NULL}
};

/*
** Open wincon library
*/
LUAMOD_API int luaopen_wincon(lua_State *L) {

  setHandle(L,STD_INPUT_HANDLE,&hin);
  setHandle(L,STD_OUTPUT_HANDLE,&hout);

  luaL_newlib(L, winconlib);

#define FLAG(m)    lua_pushinteger(L,m);lua_setfield(L,-2,#m)
#define FLAG2(m,n) lua_pushinteger(L,n);lua_setfield(L,-2,#m)

/* Console mode - Input flags */
  FLAG(ENABLE_ECHO_INPUT);
  FLAG(ENABLE_INSERT_MODE);
  FLAG(ENABLE_LINE_INPUT);
  FLAG(ENABLE_MOUSE_INPUT);
  FLAG(ENABLE_PROCESSED_INPUT);
  FLAG(ENABLE_QUICK_EDIT_MODE);
  FLAG(ENABLE_EXTENDED_FLAGS);
  FLAG(ENABLE_WINDOW_INPUT);
  FLAG(ENABLE_VIRTUAL_TERMINAL_INPUT);
/* Console mode - Output flags */
  FLAG(ENABLE_PROCESSED_OUTPUT);
  FLAG(ENABLE_WRAP_AT_EOL_OUTPUT);
  FLAG(ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  FLAG(DISABLE_NEWLINE_AUTO_RETURN);
  FLAG(ENABLE_LVB_GRID_WORLDWIDE);
/* Code pages */
  FLAG(CP_UTF8);
/* Waiting for console input */
  FLAG(INFINITE);
/* Console selection */
  FLAG(CONSOLE_MOUSE_DOWN);
  FLAG(CONSOLE_MOUSE_SELECTION);
  FLAG(CONSOLE_NO_SELECTION);
  FLAG(CONSOLE_SELECTION_IN_PROGRESS);
  FLAG(CONSOLE_SELECTION_NOT_EMPTY);
/* Control key state */
  FLAG(CAPSLOCK_ON);
  FLAG(ENHANCED_KEY);
  FLAG(LEFT_ALT_PRESSED);
  FLAG(LEFT_CTRL_PRESSED);
  FLAG(NUMLOCK_ON);
  FLAG(RIGHT_ALT_PRESSED);
  FLAG(RIGHT_CTRL_PRESSED);
  FLAG(SCROLLLOCK_ON);
  FLAG(SHIFT_PRESSED);
/* Mouse event */
  FLAG2(LEFTMOST,FROM_LEFT_1ST_BUTTON_PRESSED);
  FLAG2(RIGHTMOST,RIGHTMOST_BUTTON_PRESSED);
  FLAG2(DIRECTION,0xffff0000); /* High word */
  FLAG(DOUBLE_CLICK);
  FLAG(MOUSE_MOVED);
  FLAG(MOUSE_WHEELED);
  FLAG(MOUSE_HWHEELED);
/* Character attributes */
  FLAG(FOREGROUND_BLUE);
  FLAG(FOREGROUND_GREEN);
  FLAG(FOREGROUND_RED);
  FLAG(FOREGROUND_INTENSITY);
  FLAG(BACKGROUND_BLUE);
  FLAG(BACKGROUND_GREEN);
  FLAG(BACKGROUND_RED);
  FLAG(BACKGROUND_INTENSITY);
/* Character attributes - DBCS */
/*FLAG(COMMON_LVB_LEADING_BYTE);*/
/*FLAG(COMMON_LVB_TRAILING_BYTE);*/
/*FLAG(COMMON_LVB_GRID_HORIZONTAL);*/
/*FLAG(COMMON_LVB_GRID_LVERTICAL);*/
/*FLAG(COMMON_LVB_GRID_RVERTICAL);*/
/*FLAG(COMMON_LVB_REVERSE_VIDEO);*/
/*FLAG(COMMON_LVB_UNDERSCORE);*/

#undef FLAG

  return 1;
}


