/*
** $Id: lapi.c,v 1.136 2001/03/07 18:09:25 roberto Exp roberto $
** Lua API
** See Copyright Notice in lua.h
*/


#include <string.h>

#define LUA_PRIVATE
#include "lua.h"

#include "lapi.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


const l_char lua_ident[] = l_s("$Lua: ") l_s(LUA_VERSION) l_s(" ")
  l_s(LUA_COPYRIGHT) l_s(" $\n") l_s("$Authors: ") l_s(LUA_AUTHORS) l_s(" $");



#ifndef api_check
#define api_check(L, o)		/* nothing */
#endif

#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->ci->base))

#define api_incr_top(L)	incr_top



TObject *luaA_index (lua_State *L, int index) {
  if (index > 0) {
    api_check(L, index <= L->top - L->ci->base);
    return L->ci->base+index-1;
  }
  else {
    api_check(L, index != 0 && -index <= L->top - L->ci->base);
    return L->top+index;
  }
}


static TObject *luaA_indexAcceptable (lua_State *L, int index) {
  if (index > 0) {
    TObject *o = L->ci->base+(index-1);
    api_check(L, index <= L->stack_last - L->ci->base);
    if (o >= L->top) return NULL;
    else return o;
  }
  else {
    api_check(L, index != 0 && -index <= L->top - L->ci->base);
    return L->top+index;
  }
}


void luaA_pushobject (lua_State *L, const TObject *o) {
  setobj(L->top, o);
  incr_top;
}

LUA_API int lua_stackspace (lua_State *L) {
  int i;
  lua_lock(L);
  i = (L->stack_last - L->top);
  lua_unlock(L);
  return i;
}



/*
** basic stack manipulation
*/


LUA_API int lua_gettop (lua_State *L) {
  int i;
  lua_lock(L);
  i = (L->top - L->ci->base);
  lua_unlock(L);
  return i;
}


LUA_API void lua_settop (lua_State *L, int index) {
  lua_lock(L);
  if (index >= 0)
    luaD_adjusttop(L, L->ci->base, index);
  else {
    api_check(L, -(index+1) <= (L->top - L->ci->base));
    L->top = L->top+index+1;  /* index is negative */
  }
  lua_unlock(L);
}


LUA_API void lua_remove (lua_State *L, int index) {
  StkId p;
  lua_lock(L);
  p = luaA_index(L, index);
  while (++p < L->top) setobj(p-1, p);
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_insert (lua_State *L, int index) {
  StkId p;
  StkId q;
  lua_lock(L);
  p = luaA_index(L, index);
  for (q = L->top; q>p; q--) setobj(q, q-1);
  setobj(p, L->top);
  lua_unlock(L);
}


LUA_API void lua_pushvalue (lua_State *L, int index) {
  lua_lock(L);
  setobj(L->top, luaA_index(L, index));
  api_incr_top(L);
  lua_unlock(L);
}



/*
** access functions (stack -> C)
*/


LUA_API int lua_type (lua_State *L, int index) {
  StkId o;
  int i;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  i = (o == NULL) ? LUA_TNONE : ttype(o);
  lua_unlock(L);
  return i;
}


LUA_API const l_char *lua_typename (lua_State *L, int t) {
  const l_char *s;
  lua_lock(L);
  s = (t == LUA_TNONE) ? l_s("no value") : basictypename(G(L), t);
  lua_unlock(L);
  return s;
}


LUA_API const l_char *lua_xtype (lua_State *L, int index) {
  StkId o;
  const l_char *type;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  type = (o == NULL) ? l_s("no value") : luaT_typename(G(L), o);
  lua_unlock(L);
  return type;
}


LUA_API int lua_iscfunction (lua_State *L, int index) {
  StkId o;
  int i;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  i = (o == NULL) ? 0 : iscfunction(o);
  lua_unlock(L);
  return i;
}

LUA_API int lua_isnumber (lua_State *L, int index) {
  TObject *o;
  int i;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  i = (o == NULL) ? 0 : (tonumber(o) == 0);
  lua_unlock(L);
  return i;
}

LUA_API int lua_isstring (lua_State *L, int index) {
  int t = lua_type(L, index);
  return (t == LUA_TSTRING || t == LUA_TNUMBER);
}


LUA_API int lua_tag (lua_State *L, int index) {
  StkId o;
  int i;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  i = (o == NULL) ? LUA_NOTAG : luaT_tag(o);
  lua_unlock(L);
  return i;
}

LUA_API int lua_equal (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);
  o1 = luaA_indexAcceptable(L, index1);
  o2 = luaA_indexAcceptable(L, index2);
  i = (o1 == NULL || o2 == NULL) ? 0  /* index out-of-range */
                                 : luaO_equalObj(o1, o2);
  lua_unlock(L);
  return i;
}

LUA_API int lua_lessthan (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);
  o1 = luaA_indexAcceptable(L, index1);
  o2 = luaA_indexAcceptable(L, index2);
  i = (o1 == NULL || o2 == NULL) ? 0  /* index out-of-range */
                                 : luaV_lessthan(L, o1, o2);
  lua_unlock(L);
  return i;
}



LUA_API lua_Number lua_tonumber (lua_State *L, int index) {
  StkId o;
  lua_Number n;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  n = (o == NULL || tonumber(o)) ? 0 : nvalue(o);
  lua_unlock(L);
  return n;
}

LUA_API const l_char *lua_tostring (lua_State *L, int index) {
  StkId o;
  const l_char *s;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  s = (o == NULL || tostring(L, o)) ? NULL : svalue(o);
  lua_unlock(L);
  return s;
}

LUA_API size_t lua_strlen (lua_State *L, int index) {
  StkId o;
  size_t l;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  l = (o == NULL || tostring(L, o)) ? 0 : tsvalue(o)->len;
  lua_unlock(L);
  return l;
}

LUA_API lua_CFunction lua_tocfunction (lua_State *L, int index) {
  StkId o;
  lua_CFunction f;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  f = (o == NULL || !iscfunction(o)) ? NULL : clvalue(o)->f.c;
  lua_unlock(L);
  return f;
}

LUA_API void *lua_touserdata (lua_State *L, int index) {
  StkId o;
  void *p;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  p = (o == NULL || ttype(o) != LUA_TUSERDATA) ? NULL :
                                                    tsvalue(o)->u.d.value;
  lua_unlock(L);
  return p;
}

LUA_API const void *lua_topointer (lua_State *L, int index) {
  StkId o;
  const void *p;
  lua_lock(L);
  o = luaA_indexAcceptable(L, index);
  if (o == NULL) p = NULL;
  else {
    switch (ttype(o)) {
      case LUA_TTABLE: 
        p = hvalue(o);
        break;
      case LUA_TFUNCTION:
        p = clvalue(o);
        break;
      default:
        p = NULL;
        break;
    }
  }
  lua_unlock(L);
  return p;
}



/*
** push functions (C -> stack)
*/


LUA_API void lua_pushnil (lua_State *L) {
  lua_lock(L);
  setnilvalue(L->top);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushnumber (lua_State *L, lua_Number n) {
  lua_lock(L);
  setnvalue(L->top, n);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushlstring (lua_State *L, const l_char *s, size_t len) {
  lua_lock(L);
  setsvalue(L->top, luaS_newlstr(L, s, len));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushstring (lua_State *L, const l_char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, strlen(s));
}


LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  lua_lock(L);
  api_checknelems(L, n);
  luaV_Cclosure(L, fn, n);
  lua_unlock(L);
}


LUA_API int lua_pushuserdata (lua_State *L, void *u) {
  int isnew;
  lua_lock(L);
  isnew = luaS_createudata(L, u, L->top);
  api_incr_top(L);
  lua_unlock(L);
  return isnew;
}



/*
** get functions (Lua -> stack)
*/


LUA_API void lua_getglobal (lua_State *L, const l_char *name) {
  lua_lock(L);
  luaV_getglobal(L, luaS_new(L, name), L->top);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_gettable (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, index);
  luaV_gettable(L, t, L->top-1, L->top-1);
  lua_unlock(L);
}


LUA_API void lua_rawget (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  setobj(L->top - 1, luaH_get(hvalue(t), L->top - 1));
  lua_unlock(L);
}


LUA_API void lua_rawgeti (lua_State *L, int index, int n) {
  StkId o;
  lua_lock(L);
  o = luaA_index(L, index);
  api_check(L, ttype(o) == LUA_TTABLE);
  setobj(L->top, luaH_getnum(hvalue(o), n));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_getglobals (lua_State *L) {
  lua_lock(L);
  sethvalue(L->top, L->gt);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API int lua_getref (lua_State *L, int ref) {
  int status = 1;
  lua_lock(L);
  if (ref == LUA_REFNIL) {
    setnilvalue(L->top);
    api_incr_top(L);
  }
  else {
    api_check(L, 0 <= ref && ref < G(L)->nref);
    if (G(L)->refArray[ref].st != LOCK && G(L)->refArray[ref].st != HOLD)
      status = 0;
    else {
      setobj(L->top, &G(L)->refArray[ref].o);
      api_incr_top(L);
    }
  }
  lua_unlock(L);
  return status;
}


LUA_API void lua_newtable (lua_State *L) {
  lua_lock(L);
  sethvalue(L->top, luaH_new(L, 0));
  api_incr_top(L);
  lua_unlock(L);
}



/*
** set functions (stack -> Lua)
*/


LUA_API void lua_setglobal (lua_State *L, const l_char *name) {
  lua_lock(L);
  api_checknelems(L, 1);
  luaV_setglobal(L, luaS_new(L, name), L->top - 1);
  L->top--;  /* remove element from the top */
  lua_unlock(L);
}


LUA_API void lua_settable (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = luaA_index(L, index);
  luaV_settable(L, t, L->top - 2, L->top - 1);
  L->top -= 2;  /* pop index and value */
  lua_unlock(L);
}


LUA_API void lua_rawset (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  setobj(luaH_set(L, hvalue(t), L->top-2), (L->top-1));
  L->top -= 2;
  lua_unlock(L);
}


LUA_API void lua_rawseti (lua_State *L, int index, int n) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = luaA_index(L, index);
  api_check(L, ttype(o) == LUA_TTABLE);
  setobj(luaH_setnum(L, hvalue(o), n), (L->top-1));
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_setglobals (lua_State *L) {
  StkId newtable;
  lua_lock(L);
  api_checknelems(L, 1);
  newtable = --L->top;
  api_check(L, ttype(newtable) == LUA_TTABLE);
  L->gt = hvalue(newtable);
  lua_unlock(L);
}


LUA_API int lua_ref (lua_State *L,  int lock) {
  int ref;
  lua_lock(L);
  api_checknelems(L, 1);
  if (ttype(L->top-1) == LUA_TNIL)
    ref = LUA_REFNIL;
  else {
    if (G(L)->refFree != NONEXT) {  /* is there a free place? */
      ref = G(L)->refFree;
      G(L)->refFree = G(L)->refArray[ref].st;
    }
    else {  /* no more free places */
      luaM_growvector(L, G(L)->refArray, G(L)->nref, G(L)->sizeref, struct Ref,
                      MAX_INT, l_s("reference table overflow"));
      ref = G(L)->nref++;
    }
    setobj(&G(L)->refArray[ref].o, L->top-1);
    G(L)->refArray[ref].st = lock ? LOCK : HOLD;
  }
  L->top--;
  lua_unlock(L);
  return ref;
}


/*
** `do' functions (run Lua code)
** (most of them are in ldo.c)
*/

LUA_API void lua_rawcall (lua_State *L, int nargs, int nresults) {
  lua_lock(L);
  api_checknelems(L, nargs+1);
  luaD_call(L, L->top-(nargs+1), nresults);
  lua_unlock(L);
}


/*
** Garbage-collection functions
*/

/* GC values are expressed in Kbytes: #bytes/2^10 */
#define GCscale(x)		((int)((x)>>10))
#define GCunscale(x)		((lu_mem)(x)<<10)

LUA_API int lua_getgcthreshold (lua_State *L) {
  int threshold;
  lua_lock(L);
  threshold = GCscale(G(L)->GCthreshold);
  lua_unlock(L);
  return threshold;
}

LUA_API int lua_getgccount (lua_State *L) {
  int count;
  lua_lock(L);
  count = GCscale(G(L)->nblocks);
  lua_unlock(L);
  return count;
}

LUA_API void lua_setgcthreshold (lua_State *L, int newthreshold) {
  lua_lock(L);
  if (newthreshold > GCscale(ULONG_MAX))
    G(L)->GCthreshold = ULONG_MAX;
  else
    G(L)->GCthreshold = GCunscale(newthreshold);
  luaC_checkGC(L);
  lua_unlock(L);
}


/*
** miscellaneous functions
*/

LUA_API int lua_newtype (lua_State *L, const l_char *name, int basictype) {
  int tag;
  lua_lock(L);
  if (basictype != LUA_TNONE &&
      basictype != LUA_TTABLE &&
      basictype != LUA_TUSERDATA)
    luaO_verror(L, l_s("invalid basic type (%d) for new type"), basictype);
  tag = luaT_newtag(L, name, basictype);
  if (tag == LUA_TNONE)
    luaO_verror(L, l_s("type name '%.30s' already exists"), name);
  lua_unlock(L);
  return tag;
}


LUA_API int lua_type2tag (lua_State *L, const l_char *name) {
  int tag;
  const TObject *v;
  lua_lock(L);
  v = luaH_getstr(G(L)->type2tag, luaS_new(L, name));
  if (ttype(v) == LUA_TNIL)
    tag = LUA_TNONE;
  else {
    lua_assert(ttype(v) == LUA_TNUMBER);
    tag = (int)nvalue(v);
  }
  lua_unlock(L);
  return tag;
}


LUA_API void lua_settag (lua_State *L, int tag) {
  int basictype;
  lua_lock(L);
  api_checknelems(L, 1);
  if (tag < 0 || tag >= G(L)->ntag)
    luaO_verror(L, l_s("%d is not a valid tag"), tag);
  basictype = G(L)->TMtable[tag].basictype;
  if (basictype != LUA_TNONE && basictype != ttype(L->top-1))
    luaO_verror(L, l_s("tag %d can only be used for type '%.20s'"), tag,
                basictypename(G(L), basictype));
  switch (ttype(L->top-1)) {
    case LUA_TTABLE:
      hvalue(L->top-1)->htag = tag;
      break;
    case LUA_TUSERDATA:
      tsvalue(L->top-1)->u.d.tag = tag;
      break;
    default:
      luaO_verror(L, l_s("cannot change the tag of a %.20s"),
                  luaT_typename(G(L), L->top-1));
  }
  lua_unlock(L);
}


LUA_API void lua_error (lua_State *L, const l_char *s) {
  lua_lock(L);
  luaD_error(L, s);
  lua_unlock(L);
}


LUA_API void lua_unref (lua_State *L, int ref) {
  lua_lock(L);
  if (ref >= 0) {
    api_check(L, ref < G(L)->nref && G(L)->refArray[ref].st < 0);
    G(L)->refArray[ref].st = G(L)->refFree;
    G(L)->refFree = ref;
  }
  lua_unlock(L);
}


LUA_API int lua_next (lua_State *L, int index) {
  StkId t;
  Node *n;
  int more;
  lua_lock(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  n = luaH_next(L, hvalue(t), luaA_index(L, -1));
  if (n) {
    setkey2obj(L->top-1, n);
    setobj(L->top, val(n));
    api_incr_top(L);
    more = 1;
  }
  else {  /* no more elements */
    L->top -= 1;  /* remove key */
    more = 0;
  }
  lua_unlock(L);
  return more;
}


LUA_API int lua_getn (lua_State *L, int index) {
  Hash *h;
  const TObject *value;
  int n;
  lua_lock(L);
  h = hvalue(luaA_index(L, index));
  value = luaH_getstr(h, luaS_newliteral(L, l_s("n")));  /* = h.n */
  if (ttype(value) == LUA_TNUMBER)
    n = (int)nvalue(value);
  else {
    lua_Number max = 0;
    int i = h->size;
    Node *nd = h->node;
    while (i--) {
      if (ttype_key(nd) == LUA_TNUMBER &&
          ttype(val(nd)) != LUA_TNIL &&
          nvalue_key(nd) > max)
        max = nvalue_key(nd);
      nd++;
    }
    n = (int)max;
  }
  lua_unlock(L);
  return n;
}


LUA_API void lua_concat (lua_State *L, int n) {
  lua_lock(L);
  api_checknelems(L, n);
  if (n >= 2) {
    luaV_strconc(L, n, L->top);
    L->top -= (n-1);
    luaC_checkGC(L);
  }
  else if (n == 0) {  /* push null string */
    setsvalue(L->top, luaS_newlstr(L, NULL, 0));
    api_incr_top(L);
  }
  /* else n == 1; nothing to do */
  lua_unlock(L);
}


LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  TString *ts;
  void *p;
  lua_lock(L);
  if (size == 0) size = 1;
  ts = luaS_newudata(L, size, NULL);
  setuvalue(L->top, ts);
  api_incr_top(L);
  p = ts->u.d.value;
  lua_unlock(L);
  return p;
}

