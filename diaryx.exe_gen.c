#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

typedef enum { VAL_NUM, VAL_STR, VAL_LIST, VAL_DICT, VAL_FUNC, VAL_BIT, VAL_REF, VAL_NONE } ValKind;
struct Table;
struct Value;
typedef struct Value* (*IoFunc)(struct Table*, int, struct Value**);
typedef struct Value { ValKind kind; double num; char *str; struct Value **items; int item_count; int item_cap; struct Table *fields; IoFunc func; struct Value *target; } Value;
typedef struct Entry { char *key; Value *val; bool is_constant; struct Entry *next; } Entry;
typedef struct Table { Entry *buckets[256]; struct Table *parent; } Table;

static Value *val_num(double n) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NUM; v->num = n; return v; }
static Value *val_str(const char *s) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_STR; v->str = strdup(s); return v; }
static Value *val_list() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_LIST; v->item_cap = 8; v->items = calloc(8, sizeof(Value*)); return v; }
static Value *val_dict() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_DICT; v->fields = table_new(NULL); return v; }
static Value *val_func(IoFunc f) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_FUNC; v->func = f; return v; }
static Value *val_bit(bool b) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_BIT; v->num = b ? 1 : 0; return v; }
static Value *val_ref(Value *t) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_REF; v->target = t; return v; }
static Value *val_none() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NONE; return v; }

static unsigned hash(const char *s) { unsigned h = 0; while(*s) h = h * 31 + (unsigned char)*s++; return h % 256; }
static Table* table_new(Table *p) { Table *t = calloc(1, sizeof(Table)); t->parent = p; return t; }
static Value* table_get(Table *t, const char *k) { for(Table *cur=t;cur;cur=cur->parent){ unsigned h=hash(k); for(Entry *e=cur->buckets[h];e;e=e->next) if(!strcmp(e->key,k)) return e->val; } return val_none(); }
static bool is_all_caps(const char *s) { if(!s||!*s)return false; bool h=false; for(const char *c=s;*c;c++){ if(*c>='a'&&*c<='z') return false; if(*c>='A'&&*c<='Z') h=true; } return h; }
static void table_set(Table *t, const char *k, Value *v) { unsigned h = hash(k); for(Entry *e=t->buckets[h];e;e=e->next) if(!strcmp(e->key,k)){ if(e->is_constant){fprintf(stderr,"Runtime error: cannot reassign constant '%s'\n",k);exit(1);} if(e->val->kind == VAL_REF) { Value *trg = e->val->target; trg->kind=v->kind; trg->num=v->num; trg->str=v->str; trg->items=v->items; trg->item_count=v->item_count; trg->fields=v->fields; return; } e->val=v;return;} Entry *e=malloc(sizeof(Entry)); e->key=strdup(k); e->val=v; e->is_constant=is_all_caps(k); e->next=t->buckets[h]; t->buckets[h]=e; }

static Value* val_unwrap(Value* v) { while(v && v->kind == VAL_REF) v = v->target; return v; }
static bool val_truthy(Value *v) { v = val_unwrap(v); if(!v) return false; if(v->kind==VAL_NUM) return v->num != 0; if(v->kind==VAL_STR) return v->str[0]!='\0'; return v->item_count > 0; }
static void val_print(Value *v) { v = val_unwrap(v); if(!v) return; if(v->kind==VAL_NUM) printf(v->num==(long long)v->num?"%lld":"%g",(long long)v->num); else if(v->kind==VAL_STR) printf("%s",v->str); else if(v->kind==VAL_LIST){ printf("["); for(int i=0;i<v->item_count;i++){ if(i)printf(", "); val_print(v->items[i]); } printf("]"); } }
static Value* runtime_binop(int op, Value* l, Value* r) {
  l = val_unwrap(l); r = val_unwrap(r);
  if(op==34 && (l->kind==VAL_STR||r->kind==VAL_STR)){ char b[2048]; sprintf(b, "%s%s", l->kind==VAL_STR?l->str:"", r->kind==VAL_STR?r->str:""); return val_str(b); }
  if(l->kind==VAL_STR && r->kind==VAL_STR) {
    int c=strcmp(l->str,r->str); switch(op){
      case 33: return val_bit(c==0); case 44: return val_bit(c!=0);
      case 40: return val_bit(c<0); case 41: return val_bit(c>0); case 42: return val_bit(c<=0); case 43: return val_bit(c>=0);
    }
  }
  double a=l->num, b=r->num; switch(op){
    case 34: return val_num(a+b); case 35: return val_num(a-b); case 36: return val_num(a*b); case 37: return val_num(b?a/b:0); case 38: return val_num(b?fmod(a,b):0);
    case 40: return val_bit(a<b); case 41: return val_bit(a>b); case 42: return val_bit(a<=b); case 43: return val_bit(a>=b); case 44: return val_bit(a!=b); case 33: return val_bit(a==b); case 45: return val_bit(val_truthy(l)&&val_truthy(r)); case 46: return val_bit(val_truthy(l)||val_truthy(r)); default: return val_none();
  }
}
static Value* runtime_call(Value* f, Table* e, int c, Value** v) { if(f->kind==VAL_FUNC) return f->func(e, c, v); return val_none(); }
static Value* io_args;
static Value* runtime_var_get(Table* e, const char* k) { Value* v=table_get(e,k); if(v->kind==VAL_FUNC) return v->func(e,0,NULL); return v; }
static Value* runtime_at(Value* l, Value* r) { l = val_unwrap(l); r = val_unwrap(r); int i=(int)r->num; if(l->kind==VAL_STR){ if(i<0||i>=strlen(l->str)) return val_none(); char b[2]={l->str[i],0}; return val_str(b); } if(l->kind==VAL_LIST){ if(i<0||i>=l->item_count) return val_none(); return l->items[i]; } return val_none(); }
static void runtime_set(Value* l, Value* i, Value* v) { l = val_unwrap(l); i = val_unwrap(i); if(l->kind!=VAL_LIST) return; int idx=(int)i->num; if(idx==l->item_count){ if(l->item_count>=l->item_cap){ l->item_cap*=2; l->items=realloc(l->items,l->item_cap*sizeof(Value*)); } l->items[l->item_count++]=v; } else if(idx>=0 && idx<l->item_count) l->items[idx]=v; }
static Value* runtime_ask(Value* p) { p = val_unwrap(p); if(p&&p->kind==VAL_STR){ FILE* f=fopen(p->str,"rb"); if(!f) return val_str(""); fseek(f,0,SEEK_END); long l=ftell(f); fseek(f,0,SEEK_SET); char* b=malloc(l+1); fread(b,1,l,f); b[l]=0; fclose(f); Value* v=val_str(b); free(b); return v; } char buf[1024]; if(!fgets(buf,1024,stdin)) return val_str(""); buf[strcspn(buf,"\n")]=0; return val_str(buf); }
static Value* runtime_len(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR) return val_num(strlen(v->str)); if(v->kind==VAL_LIST) return val_num(v->item_count); if(v->kind==VAL_DICT){ int c=0; for(int i=0;i<256;i++) for(Entry *e=v->fields->buckets[i];e;e=e->next) c++; return val_num(c); } return val_num(0); }
static Value* runtime_ord(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR&&v->str[0]) return val_num((unsigned char)v->str[0]); return val_num(0); }
static Value* runtime_chr(Value* v) { v = val_unwrap(v); char b[2]={v->num,0}; return val_str(b); }
static Value* runtime_tonum(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR){ char*e; double d=strtod(v->str,&e); return val_num(*e==0&&e!=v->str?d:0); } if(v->kind==VAL_NUM) return v; return val_num(0); }
static Value* runtime_str_concat(const char* a, const char* b) { char buf[2048]; snprintf(buf, 2048, "%s%s", a, b); return val_str(buf); }
static Value* runtime_put(Value* p, Value* d, int a) { p = val_unwrap(p); d = val_unwrap(d); if(p->kind!=VAL_STR) return val_none(); FILE* f=fopen(p->str, a?"a":"w"); if(!f) return val_none(); if(d->kind==VAL_STR) fprintf(f,"%s",d->str); else { if(d->kind==VAL_NUM) fprintf(f,d->num==(long long)d->num?"%lld":"%g",(long long)d->num); } fclose(f); return val_none(); }
static Value* runtime_tostr(Value* v) { v = val_unwrap(v); char b[64]; if(v->kind==VAL_NUM){ if(v->num==(long long)v->num) snprintf(b,64,"%%lld",(long long)v->num); else snprintf(b,64,"%%g",v->num); return val_str(b); } if(v->kind==VAL_STR) return v; return val_str(""); }
static Value* runtime_slice(Value* v, Value* sv, Value* ev) { v = val_unwrap(v); sv = val_unwrap(sv); ev = val_unwrap(ev); int s=(int)sv->num, e=(int)ev->num; if(v->kind==VAL_STR){ int l=strlen(v->str); if(s<0)s+=l; if(e<0)e+=l; if(s<0)s=0; if(e>l)e=l; if(s>=e)return val_str(""); int n=e-s; char *b=malloc(n+1); memcpy(b,v->str+s,n); b[n]=0; Value *res=val_str(b); free(b); return res; } if(v->kind==VAL_LIST){ int l=v->item_count; if(s<0)s+=l; if(e<0)e+=l; if(s<0)s=0; if(e>l)e=l; Value *res=val_list(); if(s>=e)return res; for(int i=s;i<e;i++){ if(res->item_count>=res->item_cap){ res->item_cap*=2; res->items=realloc(res->items,res->item_cap*sizeof(Value*)); } res->items[res->item_count++]=v->items[i]; } return res; } return val_none(); }
static Value* runtime_type(Value* v) { v = val_unwrap(v); switch(v->kind){ case VAL_NUM: return val_str("num"); case VAL_STR: return val_str("str"); case VAL_LIST: return val_str("list"); case VAL_DICT: return val_str("dict"); case VAL_BIT: return val_str("bit"); default: return val_str("none"); } }
static Value* runtime_sys(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_STR) return val_num(-1); return val_num((double)system(v->str)); }
static Value* runtime_env(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_STR) return val_str(""); char* e=getenv(v->str); return e?val_str(e):val_str(""); }
static Value* io_func_init(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  val_str("Making new devlog file...\n");
  char* file = (val_str("devlog.md"))->str;
  return runtime_put(runtime_var_get(env, "file"), val_str("Devlog kept with diaryx. Written in Vii"));
  return val_none();
}

static Value* io_func_log(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  table_set(env, "message", runtime_at(io_args, val_num(1)));
  char* file = (val_str("devlog.md"))->str;
  runtime_put(runtime_var_get(env, "file"), runtime_binop(34, runtime_str_concat("

", (val_num((double)time(NULL)))->str), val_str("\n")));
  return runtime_put(runtime_var_get(env, "file"), runtime_var_get(env, "message"));
  return val_none();
}

static Value* io_func_main(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  char* cmd = (argc > 0 && argv[0]->kind == VAL_STR) ? argv[0]->str : "";
  if (val_truthy(runtime_binop(33, runtime_var_get(env, "cmd"), val_str("init")))) {
    runtime_var_get(env, "init");
  } else {
    if (val_truthy(runtime_binop(33, runtime_var_get(env, "cmd"), val_str("log")))) {
      runtime_var_get(env, "log");
    }
  }
  return val_none();
}


int main(int argc, char **argv) {
  io_args = val_list();
  for(int i=1; i<argc; i++) { 
    if(io_args->item_count >= io_args->item_cap) { io_args->item_cap*=2; io_args->items=realloc(io_args->items, io_args->item_cap*sizeof(Value*)); }
    io_args->items[io_args->item_count++] = val_str(argv[i]);
  }
  Table *env = table_new(NULL);
  table_set(env, "arg", io_args);
  table_set(env, "cmd", runtime_at(io_args, val_num(0)));
table_set(env, "init", val_func(io_func_init));
table_set(env, "log", val_func(io_func_log));
table_set(env, "main", val_func(io_func_main));
val_print(runtime_call(table_get(env, "main"), env, 1, (Value*[]){runtime_var_get(env, "cmd")})); printf("\n");
  return 0;
}
