#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

typedef enum { VAL_NUM, VAL_STR, VAL_LIST, VAL_DICT, VAL_FUNC, VAL_BIT, VAL_REF, VAL_BREAK, VAL_SKIP, VAL_OUT, VAL_NONE } ValKind;
struct Table;
struct Value;
typedef struct Value* (*IoFunc)(struct Table*, int, struct Value**);
typedef struct Value { ValKind kind; double num; char *str; struct Value **items; int item_count; int item_cap; struct Table *fields; IoFunc func; struct Value *target; struct Value *inner; } Value;
typedef struct Entry { char *key; Value *val; bool is_constant; struct Entry *next; } Entry;
typedef struct Table { Entry *buckets[256]; struct Table *parent; } Table;

static unsigned hash(const char *s) { unsigned h = 0; while(*s) h = h * 31 + (unsigned char)*s++; return h % 256; }
static Table* table_new(Table *p) { Table *t = calloc(1, sizeof(Table)); t->parent = p; return t; }
static Value *val_num(double n) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NUM; v->num = n; return v; }
static Value *val_str(const char *s) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_STR; v->str = strdup(s); return v; }
static Value *val_list() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_LIST; v->item_cap = 8; v->items = calloc(8, sizeof(Value*)); return v; }
static Value *val_dict() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_DICT; v->fields = table_new(NULL); return v; }
static Value *val_func(IoFunc f) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_FUNC; v->func = f; return v; }
static Value *val_bit(bool b) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_BIT; v->num = b ? 1 : 0; return v; }
static Value *val_ref(Value *t) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_REF; v->target = t; return v; }
static Value *val_out(Value *i) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_OUT; v->inner = i; return v; }
static Value *val_none() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NONE; return v; }

static Value* table_get(Table *t, const char *k) { for(Table *cur=t;cur;cur=cur->parent){ unsigned h=hash(k); for(Entry *e=cur->buckets[h];e;e=e->next) if(!strcmp(e->key,k)) return e->val; } return val_none(); }
static bool is_all_caps(const char *s) { if(!s||!*s)return false; bool h=false; for(const char *c=s;*c;c++){ if(*c>='a'&&*c<='z') return false; if(*c>='A'&&*c<='Z') h=true; } return h; }
static void table_set(Table *t, const char *k, Value *v) { unsigned h = hash(k); for(Entry *e=t->buckets[h];e;e=e->next) if(!strcmp(e->key,k)){ if(e->is_constant){fprintf(stderr,"Runtime error: cannot reassign constant '%s'\n",k);exit(1);} if(e->val->kind == VAL_REF) { Value *trg = e->val->target; trg->kind=v->kind; trg->num=v->num; trg->str=v->str; trg->items=v->items; trg->item_count=v->item_count; trg->fields=v->fields; return; } e->val=v;return;} Entry *e=malloc(sizeof(Entry)); e->key=strdup(k); e->val=v; e->is_constant=is_all_caps(k); e->next=t->buckets[h]; t->buckets[h]=e; }

static Value* val_unwrap(Value* v) { while(v && (v->kind == VAL_REF || v->kind == VAL_OUT)) v = (v->kind == VAL_REF ? v->target : v->inner); return v; }
static bool val_truthy(Value *v) { v = val_unwrap(v); if(!v) return false; if(v->kind==VAL_NUM) return v->num != 0; if(v->kind==VAL_STR) return v->str[0]!='\0'; if(v->kind==VAL_LIST||v->kind==VAL_DICT) return v->item_count > 0 || (v->kind==VAL_DICT && v->fields); if(v->kind==VAL_BIT) return v->num != 0; return false; }
static Value* val_print(Value *v) { v = val_unwrap(v); if(!v) return val_none(); if(v->kind==VAL_NUM) printf(v->num==(long long)v->num?"%lld":"%g",(long long)v->num); else if(v->kind==VAL_STR) printf("%s",v->str); else if(v->kind==VAL_LIST){ printf("["); for(int i=0;i<v->item_count;i++){ if(i)printf(", "); val_print(v->items[i]); } printf("]"); } else if(v->kind==VAL_DICT){ printf("{"); bool f=1; for(int i=0;i<256;i++) for(Entry *e=v->fields->buckets[i];e;e=e->next){ if(!f)printf(", "); printf("%s: ", e->key); val_print(e->val); f=0; } printf("}"); } else if(v->kind==VAL_FUNC) printf("<func>"); else if(v->kind==VAL_BIT) printf("%lld",(long long)v->num); fflush(stdout); return v; }
static Value* runtime_binop(int op, Value* l, Value* r) {
  l = val_unwrap(l); r = val_unwrap(r);
  if(op==41 && (l->kind==VAL_STR||r->kind==VAL_STR)){ char b[2048]; sprintf(b, "%s%s", l->kind==VAL_STR?l->str:"", r->kind==VAL_STR?r->str:""); return val_str(b); }
  if(l->kind==VAL_STR && r->kind==VAL_STR) {
    int c=strcmp(l->str,r->str); switch(op){
      case 40: return val_bit(c==0); case 51: return val_bit(c!=0);
      case 47: return val_bit(c<0); case 48: return val_bit(c>0); case 49: return val_bit(c<=0); case 50: return val_bit(c>=0);
    }
  }
  double a=l->num, b=r->num; switch(op){
    case 41: return val_num(a+b); case 42: return val_num(a-b); case 43: return val_num(a*b); case 44: return val_num(b?a/b:0); case 45: return val_num(b?fmod(a,b):0);
    case 47: return val_bit(a<b); case 48: return val_bit(a>b); case 49: return val_bit(a<=b); case 50: return val_bit(a>=b); case 51: return val_bit(a!=b); case 40: return val_bit(a==b); case 52: return val_bit(val_truthy(l)&&val_truthy(r)); case 53: return val_bit(val_truthy(l)||val_truthy(r)); default: return val_none();
  }
}
static Value* runtime_call(Value* f, Table* e, int c, Value** v) { if(f->kind==VAL_FUNC) return f->func(e, c, v); return val_none(); }
static Value* io_args;
static Value* runtime_var_get(Table* e, const char* k) { Value* v=table_get(e,k); if(v->kind==VAL_FUNC) return v->func(e,0,NULL); return v; }
static Value* runtime_at(Value* l, Value* r) { l = val_unwrap(l); r = val_unwrap(r); if(l->kind==VAL_DICT) return table_get(l->fields, r->str); int i=(int)r->num; if(l->kind==VAL_STR){ int len=strlen(l->str); if(i<0)i+=len; if(i<0||i>=len) return val_none(); char b[2]={l->str[i],0}; return val_str(b); } if(l->kind==VAL_LIST){ if(i<0)i+=l->item_count; if(i<0||i>=l->item_count) return val_none(); return l->items[i]; } return val_none(); }
static Value* runtime_set(Value* l, Value* i, Value* v) { l = val_unwrap(l); i = val_unwrap(i); if(l->kind!=VAL_LIST) return v; int idx=(int)i->num; if(idx==l->item_count){ if(l->item_count>=l->item_cap){ l->item_cap*=2; l->items=realloc(l->items,l->item_cap*sizeof(Value*)); } l->items[l->item_count++]=v; } else if(idx>=0 && idx<l->item_count) l->items[idx]=v; return v; }
static Value* runtime_key(Value* d, Value* k, Value* v) { d = val_unwrap(d); k = val_unwrap(k); if(d->kind==VAL_DICT) table_set(d->fields, k->str, v); return v; }
static Value* runtime_keys(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_DICT) return val_list(); Value* r=val_list(); for(int i=0;i<256;i++) for(Entry *e=v->fields->buckets[i];e;e=e->next){ if(r->item_count>=r->item_cap){r->item_cap*=2;r->items=realloc(r->items,r->item_cap*sizeof(Value*));} r->items[r->item_count++]=val_str(e->key); } return r; }
static Value* runtime_ask(Value* p) { p = val_unwrap(p); if(p&&p->kind==VAL_STR){ FILE* f=fopen(p->str,"rb"); if(!f){ fprintf(stderr,"Error: cannot open file '%s'\n",p->str); return val_str(""); } fseek(f,0,SEEK_END); long l=ftell(f); fseek(f,0,SEEK_SET); char* b=malloc(l+1); fread(b,1,l,f); b[l]=0; fclose(f); Value* v=val_str(b); free(b); return v; } char buf[1024]; if(!fgets(buf,1024,stdin)) return val_str(""); buf[strcspn(buf,"\n")]=0; return val_str(buf); }
static Value* runtime_len(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR) return val_num(strlen(v->str)); if(v->kind==VAL_LIST) return val_num(v->item_count); if(v->kind==VAL_DICT){ int c=0; for(int i=0;i<256;i++) for(Entry *e=v->fields->buckets[i];e;e=e->next) c++; return val_num(c); } return val_num(0); }
static Value* runtime_ord(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR&&v->str[0]) return val_num((unsigned char)v->str[0]); return val_num(0); }
static Value* runtime_chr(Value* v) { v = val_unwrap(v); char b[2]={v->num,0}; return val_str(b); }
static Value* runtime_tonum(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR){ char*e; double d=strtod(v->str,&e); return val_num(*e==0&&e!=v->str?d:0); } if(v->kind==VAL_NUM) return v; return val_num(0); }
static Value* runtime_str_concat(const char* a, const char* b) { char buf[2048]; snprintf(buf, 2048, "%s%s", a, b); return val_str(buf); }
static Value* runtime_put(Value* p, Value* d, int a) { p = val_unwrap(p); d = val_unwrap(d); if(p->kind!=VAL_STR) return val_none(); FILE* f; if(p->str[0]=='\0') f=stdout; else f=fopen(p->str, a?"a":"w"); if(!f) return val_none(); if(d->kind==VAL_STR) fprintf(f,"%s",d->str); else if(d->kind==VAL_NUM) fprintf(f,d->num==(long long)d->num?"%lld":"%g",(long long)d->num); else if(d->kind==VAL_BIT) fprintf(f,"%s",d->num?"1":"0"); else if(d->kind==VAL_NONE) { /* print nothing */ } if(f!=stdout) fclose(f); return val_none(); }
static Value* runtime_tostr(Value* v) { v = val_unwrap(v); char b[64]; if(v->kind==VAL_NUM){ if(v->num==(long long)v->num) snprintf(b,64,"%lld",(long long)v->num); else snprintf(b,64,"%g",v->num); return val_str(b); } if(v->kind==VAL_STR) return v; return val_str(""); }
static Value* runtime_slice(Value* v, Value* sv, Value* ev) { v = val_unwrap(v); sv = val_unwrap(sv); ev = val_unwrap(ev); int s=(int)sv->num, e=(int)ev->num; if(v->kind==VAL_STR){ int l=strlen(v->str); if(s<0)s+=l; if(e<0)e+=l; if(s<0)s=0; if(e>l)e=l; if(s>=e)return val_str(""); int n=e-s; char *b=malloc(n+1); memcpy(b,v->str+s,n); b[n]=0; Value *res=val_str(b); free(b); return res; } if(v->kind==VAL_LIST){ int l=v->item_count; if(s<0)s+=l; if(e<0)e+=l; if(s<0)s=0; if(e>l)e=l; Value *res=val_list(); if(s>=e)return res; for(int i=s;i<e;i++){ if(res->item_count>=res->item_cap){ res->item_cap*=2; res->items=realloc(res->items,res->item_cap*sizeof(Value*)); } res->items[res->item_count++]=v->items[i]; } return res; } return val_none(); }
static Value* runtime_type(Value* v) { v = val_unwrap(v); switch(v->kind){ case VAL_NUM: return val_str("num"); case VAL_STR: return val_str("str"); case VAL_LIST: return val_str("list"); case VAL_DICT: return val_str("dict"); case VAL_BIT: return val_str("bit"); default: return val_str("none"); } }
static Value* runtime_sys(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_STR) return val_num(-1); return val_num((double)system(v->str)); }
static Value* runtime_split(Value* s, Value* d) { char *st = strdup(s->str); Value *res = val_list(); char *t = strtok(st, d->str); while(t){ if(res->item_count>=res->item_cap){ res->item_cap*=2; res->items=realloc(res->items, res->item_cap*sizeof(Value*)); } res->items[res->item_count++]=val_str(t); t=strtok(NULL, d->str); } free(st); return res; }
static Value* runtime_trim(Value* v) { char *s = v->str; while(isspace(*s)) s++; if(!*s) return val_str(""); char *e = s+strlen(s)-1; while(e>s && isspace(*e)) e--; *(e+1)=0; return val_str(s); }
static Value* runtime_replace(Value* v, Value* t, Value* r) { char b[4096]={0}; char *p=v->str; char *o=b; size_t tl=strlen(t->str); while(*p){ char *f=strstr(p,t->str); if(f==p){ strcpy(o,r->str); o+=strlen(r->str); p+=tl; } else *o++=*p++; } return val_str(b); }
static Value* runtime_safe(Value* v) { return v; }
static Value* runtime_env(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_STR) return val_str(""); char* e=getenv(v->str); return e?val_str(e):val_str(""); }
static Value* io_val_print(Table* t, int argc, Value** argv) { if(argc > 0) { val_print(argv[0]); printf("\n"); fflush(stdout); return argv[0]; } return val_none(); }
static Value* io_val_unwrap(Table* t, int argc, Value** argv) { if(argc > 0) return val_unwrap(argv[0]); return val_none(); }
static Value* io_table_new(Table* t, int argc, Value** argv) { Value *v = val_dict(); if(argc > 0) { Value *p = val_unwrap(argv[0]); if(p->kind == VAL_DICT) v->fields->parent = p->fields; } return v; }
typedef struct { size_t cap; size_t off; char* data; } Arena;

static Value* io_func_arena_create(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "size", argv[0]);
  Arena* a = calloc(1, sizeof(Arena));
  Value* sz = (argc > 0) ? argv[0] : val_num(64*1024*1024);
  a->cap = (size_t)(sz->kind==VAL_NUM ? sz->num : 64*1024*1024);
  a->data = calloc(1, a->cap);
  return val_out(val_num((double)(uintptr_t)a));
}


int main(int argc, char **argv) {
  io_args = val_list();
  for(int i=1; i<argc; i++) { 
    if(io_args->item_count >= io_args->item_cap) { io_args->item_cap*=2; io_args->items=realloc(io_args->items, io_args->item_cap*sizeof(Value*)); }
    io_args->items[io_args->item_count++] = val_str(argv[i]);
  }
  Table *env = table_new(NULL);
  table_set(env, "global", val_dict());
  table_set(env, "arg", io_args);
  table_set(env, "argv", io_args);
  table_set(env, "val_print", val_func(io_val_print));
  table_set(env, "val_unwrap", val_func(io_val_unwrap));
  table_set(env, "table_new", val_func(io_table_new));
  Value *program_result = io_func_main(env, 0, NULL);
  if (program_result->kind != VAL_NONE && program_result->kind != VAL_BREAK) {
    val_print(program_result);
    printf("\n");
  }
  return 0;
}
