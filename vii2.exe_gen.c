#include <stdio.h>
#include <stdlib.h>
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
static Value* val_print(Value *v) { v = val_unwrap(v); if(!v) return val_none(); if(v->kind==VAL_NUM) printf(v->num==(long long)v->num?"%lld":"%g",(long long)v->num); else if(v->kind==VAL_STR) printf("%s",v->str); else if(v->kind==VAL_LIST){ printf("["); for(int i=0;i<v->item_count;i++){ if(i)printf(", "); val_print(v->items[i]); } printf("]"); } else if(v->kind==VAL_DICT){ printf("{"); bool f=1; for(int i=0;i<256;i++) for(Entry *e=v->fields->buckets[i];e;e=e->next){ if(!f)printf(", "); printf("%s: ", e->key); val_print(e->val); f=0; } printf("}"); } else if(v->kind==VAL_FUNC) printf("<func>"); else if(v->kind==VAL_BIT) printf("%lld",(long long)v->num); printf("\n"); fflush(stdout); return v; }
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
static Value* runtime_ask(Value* p) { p = val_unwrap(p); if(p&&p->kind==VAL_STR){ FILE* f=fopen(p->str,"rb"); if(!f) return val_str(""); fseek(f,0,SEEK_END); long l=ftell(f); fseek(f,0,SEEK_SET); char* b=malloc(l+1); fread(b,1,l,f); b[l]=0; fclose(f); Value* v=val_str(b); free(b); return v; } char buf[1024]; if(!fgets(buf,1024,stdin)) return val_str(""); buf[strcspn(buf,"\n")]=0; return val_str(buf); }
static Value* runtime_len(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR) return val_num(strlen(v->str)); if(v->kind==VAL_LIST) return val_num(v->item_count); if(v->kind==VAL_DICT){ int c=0; for(int i=0;i<256;i++) for(Entry *e=v->fields->buckets[i];e;e=e->next) c++; return val_num(c); } return val_num(0); }
static Value* runtime_ord(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR&&v->str[0]) return val_num((unsigned char)v->str[0]); return val_num(0); }
static Value* runtime_chr(Value* v) { v = val_unwrap(v); char b[2]={v->num,0}; return val_str(b); }
static Value* runtime_tonum(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR){ char*e; double d=strtod(v->str,&e); return val_num(*e==0&&e!=v->str?d:0); } if(v->kind==VAL_NUM) return v; return val_num(0); }
static Value* runtime_str_concat(const char* a, const char* b) { char buf[2048]; snprintf(buf, 2048, "%s%s", a, b); return val_str(buf); }
static Value* runtime_put(Value* p, Value* d, int a) { p = val_unwrap(p); d = val_unwrap(d); if(p->kind!=VAL_STR) return val_none(); FILE* f; if(p->str[0]=='\0') f=stdout; else f=fopen(p->str, a?"a":"w"); if(!f) return val_none(); if(d->kind==VAL_STR) fprintf(f,"%s",d->str); else { if(d->kind==VAL_NUM) fprintf(f,d->num==(long long)d->num?"%lld":"%g",(long long)d->num); } if(f!=stdout) fclose(f); return val_none(); }
static Value* runtime_tostr(Value* v) { v = val_unwrap(v); char b[64]; if(v->kind==VAL_NUM){ if(v->num==(long long)v->num) snprintf(b,64,"%lld",(long long)v->num); else snprintf(b,64,"%g",v->num); return val_str(b); } if(v->kind==VAL_STR) return v; return val_str(""); }
static Value* runtime_slice(Value* v, Value* sv, Value* ev) { v = val_unwrap(v); sv = val_unwrap(sv); ev = val_unwrap(ev); int s=(int)sv->num, e=(int)ev->num; if(v->kind==VAL_STR){ int l=strlen(v->str); if(s<0)s+=l; if(e<0)e+=l; if(s<0)s=0; if(e>l)e=l; if(s>=e)return val_str(""); int n=e-s; char *b=malloc(n+1); memcpy(b,v->str+s,n); b[n]=0; Value *res=val_str(b); free(b); return res; } if(v->kind==VAL_LIST){ int l=v->item_count; if(s<0)s+=l; if(e<0)e+=l; if(s<0)s=0; if(e>l)e=l; Value *res=val_list(); if(s>=e)return res; for(int i=s;i<e;i++){ if(res->item_count>=res->item_cap){ res->item_cap*=2; res->items=realloc(res->items,res->item_cap*sizeof(Value*)); } res->items[res->item_count++]=v->items[i]; } return res; } return val_none(); }
static Value* runtime_type(Value* v) { v = val_unwrap(v); switch(v->kind){ case VAL_NUM: return val_str("num"); case VAL_STR: return val_str("str"); case VAL_LIST: return val_str("list"); case VAL_DICT: return val_str("dict"); case VAL_BIT: return val_str("bit"); default: return val_str("none"); } }
static Value* runtime_sys(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_STR) return val_num(-1); return val_num((double)system(v->str)); }
static Value* runtime_split(Value* s, Value* d) { char *st = strdup(s->str); Value *res = val_list(); char *t = strtok(st, d->str); while(t){ if(res->item_count>=res->item_cap){ res->item_cap*=2; res->items=realloc(res->items, res->item_cap*sizeof(Value*)); } res->items[res->item_count++]=val_str(t); t=strtok(NULL, d->str); } free(st); return res; }
static Value* runtime_trim(Value* v) { char *s = v->str; while(isspace(*s)) s++; if(!*s) return val_str(""); char *e = s+strlen(s)-1; while(e>s && isspace(*e)) e--; *(e+1)=0; return val_str(s); }
static Value* runtime_replace(Value* v, Value* t, Value* r) { char b[4096]={0}; char *p=v->str; char *o=b; size_t tl=strlen(t->str); while(*p){ char *f=strstr(p,t->str); if(f==p){ strcpy(o,r->str); o+=strlen(r->str); p+=tl; } else *o++=*p++; } return val_str(b); }
static Value* runtime_safe(Value* v) { return v; }
static Value* runtime_env(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_STR) return val_str(""); char* e=getenv(v->str); return e?val_str(e):val_str(""); }
static Value* io_func_enable_ansi_colors(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  val_str("");
  return val_none();
}

static Value* io_func_read_file(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "path", argv[0]);
  return val_out(runtime_ask(runtime_var_get(env, "path")));
  return val_none();
}

static Value* io_func_report_error(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "filename", argv[0]);
  if(argc > 1) table_set(env, "src", argv[1]);
  if(argc > 2) table_set(env, "pos", argv[2]);
  if(argc > 3) table_set(env, "line", argv[3]);
  if(argc > 4) table_set(env, "fmt", argv[4]);
  val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("x1b[1;31mError in "))->str, runtime_tostr(runtime_var_get(env, "filename"))->str), val_str(" on line ")), runtime_tostr(runtime_var_get(env, "line"))), val_str(": x1b[0m")), runtime_var_get(env, "fmt"))); printf("\n");
  table_set(env, "start", runtime_var_get(env, "pos"));
  while (val_truthy(runtime_binop(48, runtime_var_get(env, "start"), val_num(0)))) {
    table_set(env, "c", runtime_at(runtime_var_get(env, "src"), runtime_binop(42, runtime_var_get(env, "start"), val_num(1))));
if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "c"), val_str("\n")), runtime_var_get(env, "c")), val_str("r")))) {
  break;
}
table_set(env, "start", runtime_binop(42, runtime_var_get(env, "start"), val_num(1)));
  }
  table_set(env, "end", runtime_var_get(env, "pos"));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "end"), runtime_len(runtime_var_get(env, "src"))))) {
    table_set(env, "c", runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "end")));
if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "c"), val_str("\n")), runtime_var_get(env, "c")), val_str("r")))) {
  break;
}
table_set(env, "end", runtime_binop(41, runtime_var_get(env, "end"), val_num(1)));
  }
  table_set(env, "lstr", runtime_tostr(runtime_var_get(env, "line")));
  while (val_truthy(runtime_binop(47, runtime_len(runtime_var_get(env, "lstr")), val_num(5)))) {
    table_set(env, "lstr", runtime_str_concat(runtime_tostr(val_str(" "))->str, runtime_tostr(runtime_var_get(env, "lstr"))->str));
  }
  val_print(runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str(" "))->str, runtime_tostr(runtime_var_get(env, "lstr"))->str), val_str(" | ")), runtime_slice(runtime_var_get(env, "src"), runtime_var_get(env, "start"), runtime_var_get(env, "end")))); printf("\n");
  table_set(env, "caret_line", val_str("       | "));
  table_set(env, "i", runtime_var_get(env, "start"));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_var_get(env, "pos")))) {
    if (val_truthy(runtime_binop(51, runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "i")), val_str("r")))) {
  table_set(env, "caret_line", runtime_binop(41, runtime_var_get(env, "caret_line"), val_str(" ")));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  val_print(runtime_binop(41, runtime_var_get(env, "caret_line"), val_str("x1b[1;32m^x1b[0m"))); printf("\n");
  exit((int)(val_num(1))->num);
  return val_none();
}

static Value* io_func_is_digit(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "c", argv[0]);
  table_set(env, "o", runtime_ord(runtime_var_get(env, "c")));
  return val_out(runtime_binop(49, runtime_binop(52, runtime_binop(50, runtime_var_get(env, "o"), val_num(48)), runtime_var_get(env, "o")), val_num(57)));
  return val_none();
}

static Value* io_func_is_alpha(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "c", argv[0]);
  table_set(env, "o", runtime_ord(runtime_var_get(env, "c")));
  return val_out(runtime_binop(53, runtime_binop(53, runtime_binop(49, runtime_binop(52, runtime_binop(50, runtime_var_get(env, "o"), val_num(65)), runtime_var_get(env, "o")), val_num(90)), runtime_binop(49, runtime_binop(52, runtime_binop(50, runtime_var_get(env, "o"), val_num(97)), runtime_var_get(env, "o")), val_num(122))), runtime_binop(40, runtime_var_get(env, "o"), val_num(95))));
  return val_none();
}

static Value* io_func_is_alnum(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "c", argv[0]);
  return val_out(runtime_binop(53, runtime_call(table_get(env, "is_alpha"), env, 1, (Value*[]){runtime_var_get(env, "c")}), runtime_call(table_get(env, "is_digit"), env, 1, (Value*[]){runtime_var_get(env, "c")})));
  return val_none();
}

static Value* io_func_get_kw_kind(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "text", argv[0]);
  table_set(env, "kind", runtime_at(runtime_var_get(env, "KW_MAP"), runtime_var_get(env, "text")));
  if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "kind")), val_str("none")))) {
    val_print(runtime_var_get(env, "TOK_IDENT")); printf("\n");
  } else {
    val_print(runtime_var_get(env, "kind")); printf("\n");
  }
  return val_none();
}

static Value* io_func_lex(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "lexer_ctx", argv[0]);
  table_set(env, "tokens", val_list());
  table_set(env, "pos", runtime_at(runtime_var_get(env, "lexer_ctx"), val_str("pos")));
  table_set(env, "line", runtime_at(runtime_var_get(env, "lexer_ctx"), val_str("line")));
  table_set(env, "boot_debug", runtime_at(runtime_var_get(env, "lexer_ctx"), val_str("boot_debug")));
  table_set(env, "at_start", val_num(1));
  table_set(env, "indent_stack", val_list());
  runtime_set(runtime_var_get(env, "indent_stack"), val_num(0), val_num(0));
  table_set(env, "src", runtime_at(runtime_var_get(env, "lexer_ctx"), val_str("src")));
  table_set(env, "source_len", runtime_len(runtime_var_get(env, "src")));
  table_set(env, "filename", runtime_at(runtime_var_get(env, "lexer_ctx"), val_str("filename")));
  table_set(env, "last_heartbeat", val_num(0 - (val_num(1))->num));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "pos"), runtime_var_get(env, "source_len")))) {
    table_set(env, "t_count", runtime_len(runtime_var_get(env, "tokens")));
if (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(52, runtime_var_get(env, "boot_debug"), runtime_binop(40, runtime_binop(45, runtime_var_get(env, "t_count"), val_num(1000)), val_num(0))), runtime_var_get(env, "t_count")), runtime_var_get(env, "last_heartbeat")))) {
  val_print(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Lexer heartbeat: "))->str, runtime_tostr(runtime_tostr(runtime_var_get(env, "t_count")))->str), val_str(" tokens generated..."))); printf("\n");
table_set(env, "last_heartbeat", runtime_var_get(env, "t_count"));
}
table_set(env, "char", runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos")));
if (val_truthy(runtime_binop(52, runtime_binop(40, runtime_var_get(env, "at_start"), val_num(0)), runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "char"), val_str(" ")), runtime_var_get(env, "char")), val_str("\t")), runtime_var_get(env, "char")), val_str("r"))))) {
  table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
} else {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("\n")))) {
    if (val_truthy(runtime_binop(51, runtime_binop(53, runtime_binop(40, runtime_len(runtime_var_get(env, "tokens")), val_num(0)), runtime_at(runtime_at(runtime_var_get(env, "tokens"), runtime_binop(42, runtime_len(runtime_var_get(env, "tokens")), val_num(1))), val_str("kind"))), runtime_var_get(env, "TOK_NEWLINE")))) {
  table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "TOK_NEWLINE"));
runtime_key(runtime_var_get(env, "t"), val_str("filename"), runtime_var_get(env, "filename"));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "pos"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
}
table_set(env, "line", runtime_binop(41, runtime_var_get(env, "line"), val_num(1)));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
table_set(env, "at_start", val_num(1));
  } else {
    if (val_truthy(runtime_var_get(env, "at_start"))) {
      table_set(env, "start_pos", runtime_var_get(env, "pos"));
table_set(env, "spaces", val_num(0));
while (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(47, runtime_var_get(env, "pos"), runtime_var_get(env, "source_len")), runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos"))), val_str(" ")))) {
  table_set(env, "spaces", runtime_binop(41, runtime_var_get(env, "spaces"), val_num(1)));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
}
table_set(env, "next", runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos")));
table_set(env, "at_start", val_num(0));
if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "next"), val_str("\n")), runtime_var_get(env, "next")), val_str("r")), runtime_var_get(env, "next")), val_str("#")), runtime_var_get(env, "next")), val_str("")), runtime_type(runtime_var_get(env, "next"))), val_str("none")))) {
  val_print(val_str("")); printf("\n");
} else {
  table_set(env, "top", runtime_at(runtime_var_get(env, "indent_stack"), runtime_binop(42, runtime_len(runtime_var_get(env, "indent_stack")), val_num(1))));
if (val_truthy(runtime_binop(48, runtime_var_get(env, "spaces"), runtime_var_get(env, "top")))) {
  runtime_set(runtime_var_get(env, "indent_stack"), runtime_len(runtime_var_get(env, "indent_stack")), runtime_var_get(env, "spaces"));
table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "TOK_INDENT"));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "start_pos"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
}
while (val_truthy(runtime_binop(47, runtime_var_get(env, "spaces"), runtime_at(runtime_var_get(env, "indent_stack"), runtime_binop(42, runtime_len(runtime_var_get(env, "indent_stack")), val_num(1)))))) {
  table_set(env, "indent_stack", runtime_slice(runtime_var_get(env, "indent_stack"), val_num(0), runtime_binop(42, runtime_len(runtime_var_get(env, "indent_stack")), val_num(1))));
table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "TOK_DEDENT"));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "start_pos"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
}
}
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("#")))) {
        if (val_truthy(runtime_binop(40, runtime_slice(runtime_var_get(env, "src"), runtime_var_get(env, "pos"), runtime_binop(41, runtime_var_get(env, "pos"), val_num(7))), val_str("#! ONCE")))) {
  table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "TOK_SHEBANG"));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "pos"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(7)));
} else {
  while (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(47, runtime_var_get(env, "pos"), runtime_var_get(env, "source_len")), runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos"))), val_str("\n")))) {
  table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
}
}
      } else {
        if (val_truthy(runtime_call(table_get(env, "is_digit"), env, 1, (Value*[]){runtime_var_get(env, "char")}))) {
          table_set(env, "start", runtime_var_get(env, "pos"));
while (val_truthy(runtime_binop(52, runtime_binop(47, runtime_var_get(env, "pos"), runtime_var_get(env, "source_len")), runtime_call(table_get(env, "is_digit"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos"))})))) {
  table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos")), val_str(".")))) {
  table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
while (val_truthy(runtime_binop(52, runtime_binop(47, runtime_var_get(env, "pos"), runtime_var_get(env, "source_len")), runtime_call(table_get(env, "is_digit"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos"))})))) {
  table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
}
}
table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "TOK_NUM"));
runtime_key(runtime_var_get(env, "t"), val_str("text"), runtime_slice(runtime_var_get(env, "src"), runtime_var_get(env, "start"), runtime_var_get(env, "pos")));
runtime_key(runtime_var_get(env, "t"), val_str("num"), runtime_tonum(runtime_slice(runtime_var_get(env, "src"), runtime_var_get(env, "start"), runtime_var_get(env, "pos"))));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "start"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("\"")))) {
            table_set(env, "start_pos", runtime_var_get(env, "pos"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
table_set(env, "start", runtime_var_get(env, "pos"));
while (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(47, runtime_var_get(env, "pos"), runtime_var_get(env, "source_len")), runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos"))), val_str("\"")))) {
  if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos")), val_str("\\")))) {
  table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
}
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
}
table_set(env, "raw", runtime_slice(runtime_var_get(env, "src"), runtime_var_get(env, "start"), runtime_var_get(env, "pos")));
table_set(env, "unescaped", val_str(""));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "raw"))))) {
  table_set(env, "c", runtime_at(runtime_var_get(env, "raw"), runtime_var_get(env, "i")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "c"), val_str("\\")))) {
  table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
table_set(env, "nc", runtime_at(runtime_var_get(env, "raw"), runtime_var_get(env, "i")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "nc"), val_str("n")))) {
  table_set(env, "unescaped", runtime_binop(41, runtime_var_get(env, "unescaped"), runtime_chr(val_num(10))));
} else {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "nc"), val_str("t")))) {
    table_set(env, "unescaped", runtime_binop(41, runtime_var_get(env, "unescaped"), runtime_chr(val_num(9))));
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "nc"), val_str("\\")))) {
      table_set(env, "unescaped", runtime_binop(41, runtime_var_get(env, "unescaped"), val_str("\\")));
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "nc"), val_str("\"")))) {
        table_set(env, "unescaped", runtime_binop(41, runtime_var_get(env, "unescaped"), val_str("\"")));
      } else {
        table_set(env, "unescaped", runtime_binop(41, runtime_var_get(env, "unescaped"), runtime_var_get(env, "nc")));
      }
    }
  }
}
} else {
  table_set(env, "unescaped", runtime_binop(41, runtime_var_get(env, "unescaped"), runtime_var_get(env, "c")));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "TOK_STR"));
runtime_key(runtime_var_get(env, "t"), val_str("text"), runtime_var_get(env, "unescaped"));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "start_pos"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
          } else {
            if (val_truthy(runtime_call(table_get(env, "is_alpha"), env, 1, (Value*[]){runtime_var_get(env, "char")}))) {
              table_set(env, "start", runtime_var_get(env, "pos"));
while (val_truthy(runtime_binop(52, runtime_binop(47, runtime_var_get(env, "pos"), runtime_var_get(env, "source_len")), runtime_call(table_get(env, "is_alnum"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "src"), runtime_var_get(env, "pos"))})))) {
  table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
}
table_set(env, "text", runtime_slice(runtime_var_get(env, "src"), runtime_var_get(env, "start"), runtime_var_get(env, "pos")));
table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_call(table_get(env, "get_kw_kind"), env, 1, (Value*[]){runtime_var_get(env, "text")}));
runtime_key(runtime_var_get(env, "t"), val_str("text"), runtime_var_get(env, "text"));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "start"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
            } else {
              table_set(env, "op_kind", val_num(0 - (val_num(1))->num));
table_set(env, "op_text", val_str(""));
table_set(env, "start_pos", runtime_var_get(env, "pos"));
table_set(env, "next", runtime_at(runtime_var_get(env, "src"), runtime_binop(41, runtime_var_get(env, "pos"), val_num(1))));
if (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(40, runtime_var_get(env, "char"), val_str("=")), runtime_var_get(env, "next")), val_str("=")))) {
  table_set(env, "op_kind", runtime_var_get(env, "TOK_EQEQ"));
table_set(env, "op_text", val_str("=="));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(2)));
} else {
  if (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(40, runtime_var_get(env, "char"), val_str("-")), runtime_var_get(env, "next")), val_str(">")))) {
    table_set(env, "op_kind", runtime_var_get(env, "TOK_ARROW"));
table_set(env, "op_text", val_str("->"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(2)));
  } else {
    if (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(40, runtime_var_get(env, "char"), val_str("<")), runtime_var_get(env, "next")), val_str("=")))) {
      table_set(env, "op_kind", runtime_var_get(env, "TOK_LTE"));
table_set(env, "op_text", val_str("<="));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(2)));
    } else {
      if (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(40, runtime_var_get(env, "char"), val_str(">")), runtime_var_get(env, "next")), val_str("=")))) {
        table_set(env, "op_kind", runtime_var_get(env, "TOK_GTE"));
table_set(env, "op_text", val_str(">="));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(2)));
      } else {
        if (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(40, runtime_var_get(env, "char"), val_str("!")), runtime_var_get(env, "next")), val_str("=")))) {
          table_set(env, "op_kind", runtime_var_get(env, "TOK_NE"));
table_set(env, "op_text", val_str("!="));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(2)));
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("=")))) {
            table_set(env, "op_kind", runtime_var_get(env, "TOK_EQ"));
table_set(env, "op_text", val_str("="));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
          } else {
            if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("+")))) {
              table_set(env, "op_kind", runtime_var_get(env, "TOK_PLUS"));
table_set(env, "op_text", val_str("+"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
            } else {
              if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("-")))) {
                table_set(env, "op_kind", runtime_var_get(env, "TOK_MINUS"));
table_set(env, "op_text", val_str("-"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
              } else {
                if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("*")))) {
                  table_set(env, "op_kind", runtime_var_get(env, "TOK_STAR"));
table_set(env, "op_text", val_str("*"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
                } else {
                  if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("/")))) {
                    table_set(env, "op_kind", runtime_var_get(env, "TOK_SLASH"));
table_set(env, "op_text", val_str("/"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
                  } else {
                    if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("%")))) {
                      table_set(env, "op_kind", runtime_var_get(env, "TOK_PCT"));
table_set(env, "op_text", val_str("%"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
                    } else {
                      if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("<")))) {
                        table_set(env, "op_kind", runtime_var_get(env, "TOK_LT"));
table_set(env, "op_text", val_str("<"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
                      } else {
                        if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str(">")))) {
                          table_set(env, "op_kind", runtime_var_get(env, "TOK_GT"));
table_set(env, "op_text", val_str(">"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
                        } else {
                          if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str("(")))) {
                            table_set(env, "op_kind", runtime_var_get(env, "TOK_LPAREN"));
table_set(env, "op_text", val_str("("));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
                          } else {
                            if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str(")")))) {
                              table_set(env, "op_kind", runtime_var_get(env, "TOK_RPAREN"));
table_set(env, "op_text", val_str(")"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
                            } else {
                              if (val_truthy(runtime_binop(40, runtime_var_get(env, "char"), val_str(";")))) {
                                table_set(env, "op_kind", runtime_var_get(env, "TOK_SEMICOLON"));
table_set(env, "op_text", val_str(";"));
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
if (val_truthy(runtime_binop(51, runtime_var_get(env, "op_kind"), val_num(0 - (val_num(1))->num)))) {
  table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "op_kind"));
runtime_key(runtime_var_get(env, "t"), val_str("text"), runtime_var_get(env, "op_text"));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "start_pos"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op_kind"), val_num(0 - (val_num(1))->num)))) {
  val_print(runtime_call(table_get(env, "report_error"), env, 5, (Value*[]){runtime_at(runtime_var_get(env, "lexer_ctx"), val_str("filename")), runtime_var_get(env, "src"), runtime_var_get(env, "pos"), runtime_var_get(env, "line"), runtime_str_concat(runtime_tostr(val_str("Unexpected character: "))->str, runtime_tostr(runtime_var_get(env, "char"))->str)})); printf("\n");
table_set(env, "pos", runtime_binop(41, runtime_var_get(env, "pos"), val_num(1)));
}
            }
          }
        }
      }
    }
  }
}
  }
  while (val_truthy(runtime_binop(48, runtime_len(runtime_var_get(env, "indent_stack")), val_num(1)))) {
    table_set(env, "indent_stack", runtime_slice(runtime_var_get(env, "indent_stack"), val_num(0), runtime_binop(42, runtime_len(runtime_var_get(env, "indent_stack")), val_num(1))));
table_set(env, "t", val_dict());
runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "TOK_DEDENT"));
runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "pos"));
runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
  }
  table_set(env, "t", val_dict());
  runtime_key(runtime_var_get(env, "t"), val_str("kind"), runtime_var_get(env, "TOK_EOF"));
  runtime_key(runtime_var_get(env, "t"), val_str("line"), runtime_var_get(env, "line"));
  runtime_key(runtime_var_get(env, "t"), val_str("pos"), runtime_var_get(env, "pos"));
  runtime_set(runtime_var_get(env, "tokens"), runtime_len(runtime_var_get(env, "tokens")), runtime_var_get(env, "t"));
  return val_out(runtime_var_get(env, "tokens"));
  return val_none();
}

static Value* io_func_nd_new(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "kind", argv[0]);
  if(argc > 1) table_set(env, "t", argv[1]);
  table_set(env, "n", val_dict());
  runtime_key(runtime_var_get(env, "n"), val_str("kind"), runtime_var_get(env, "kind"));
  runtime_key(runtime_var_get(env, "n"), val_str("body"), val_list());
  runtime_key(runtime_var_get(env, "n"), val_str("line"), runtime_at(runtime_var_get(env, "t"), val_str("line")));
  runtime_key(runtime_var_get(env, "n"), val_str("filename"), runtime_at(runtime_var_get(env, "t"), val_str("filename")));
  runtime_key(runtime_var_get(env, "n"), val_str("pos"), runtime_at(runtime_var_get(env, "t"), val_str("pos")));
  return val_out(runtime_var_get(env, "n"));
  return val_none();
}

static Value* io_func_nd_push(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "node", argv[0]);
  if(argc > 1) table_set(env, "child", argv[1]);
  table_set(env, "body", runtime_at(runtime_var_get(env, "node"), val_str("body")));
  return val_out(runtime_set(runtime_var_get(env, "body"), runtime_len(runtime_var_get(env, "body")), runtime_var_get(env, "child")));
  return val_none();
}

static Value* io_func_is_all_caps(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "s", argv[0]);
  if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(51, runtime_type(runtime_var_get(env, "s")), val_str("str")), runtime_len(runtime_var_get(env, "s"))), val_num(0)))) {
    return val_out(val_num(0));
  }
  table_set(env, "has_upper", val_num(0));
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "s"))))) {
    table_set(env, "c", runtime_at(runtime_var_get(env, "s"), runtime_var_get(env, "i")));
table_set(env, "o", runtime_ord(runtime_var_get(env, "c")));
if (val_truthy(runtime_binop(49, runtime_binop(52, runtime_binop(50, runtime_var_get(env, "o"), val_num(97)), runtime_var_get(env, "o")), val_num(122)))) {
  return val_out(val_num(0));
}
if (val_truthy(runtime_binop(49, runtime_binop(52, runtime_binop(50, runtime_var_get(env, "o"), val_num(65)), runtime_var_get(env, "o")), val_num(90)))) {
  table_set(env, "has_upper", val_num(1));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  return val_out(runtime_var_get(env, "has_upper"));
  return val_none();
}

static Value* io_func_track_constant(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  if(argc > 1) table_set(env, "name", argv[1]);
  if(argc > 2) table_set(env, "pos", argv[2]);
  if(argc > 3) table_set(env, "line", argv[3]);
  if (val_truthy(runtime_binop(40, runtime_type(runtime_at(runtime_var_get(env, "p"), val_str("constants"))), val_str("none")))) {
    runtime_key(runtime_var_get(env, "p"), val_str("constants"), val_list());
  }
  table_set(env, "constants", runtime_at(runtime_var_get(env, "p"), val_str("constants")));
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "constants"))))) {
    if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "constants"), runtime_var_get(env, "i")), runtime_var_get(env, "name")))) {
  val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Error in "))->str, runtime_tostr(runtime_at(runtime_var_get(env, "p"), val_str("filename")))->str), val_str(" on line ")), runtime_tostr(runtime_var_get(env, "line"))), val_str(": cannot reassign constant '")), runtime_var_get(env, "name")), val_str("'"))); printf("\n");
exit((int)(val_num(1))->num);
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  return val_out(runtime_set(runtime_var_get(env, "constants"), runtime_len(runtime_var_get(env, "constants")), runtime_var_get(env, "name")));
  return val_none();
}

static Value* io_func_peek(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "tokens", runtime_at(runtime_var_get(env, "p"), val_str("tokens")));
  table_set(env, "pos", runtime_at(runtime_var_get(env, "p"), val_str("pos")));
  return val_out(runtime_at(runtime_var_get(env, "tokens"), runtime_var_get(env, "pos")));
  return val_none();
}

static Value* io_func_advance(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  runtime_key(runtime_var_get(env, "p"), val_str("pos"), runtime_binop(41, runtime_at(runtime_var_get(env, "p"), val_str("pos")), val_num(1)));
  return val_out(runtime_var_get(env, "t"));
  return val_none();
}

static Value* io_func_expect(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  if(argc > 1) table_set(env, "kind", argv[1]);
  table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  if (val_truthy(runtime_binop(51, runtime_at(runtime_var_get(env, "t"), val_str("kind")), runtime_var_get(env, "kind")))) {
    val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Error in "))->str, runtime_tostr(runtime_at(runtime_var_get(env, "p"), val_str("filename")))->str), val_str(" on line ")), runtime_tostr(runtime_at(runtime_var_get(env, "t"), val_str("line")))), val_str(": expected token kind ")), runtime_tostr(runtime_var_get(env, "kind"))), val_str(", got ")), runtime_tostr(runtime_at(runtime_var_get(env, "t"), val_str("kind"))))); printf("\n");
exit((int)(val_num(1))->num);
  }
  return val_out(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  return val_none();
}

static Value* io_func_skip_newlines(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  while (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_at(runtime_var_get(env, "t"), val_str("kind")), runtime_var_get(env, "TOK_NEWLINE")), runtime_at(runtime_var_get(env, "t"), val_str("kind"))), runtime_var_get(env, "TOK_SEMICOLON")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  }
  return val_none();
}

static Value* io_func_infer_node_type(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "n", argv[0]);
  if(argc > 1) table_set(env, "fn_ctx", argv[1]);
  if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "n")), val_str("none")))) {
    return val_out(val_str("none"));
  }
  table_set(env, "k", runtime_at(runtime_var_get(env, "n"), val_str("kind")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_NUM")))) {
    return val_out(val_str("num"));
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_STR")))) {
      return val_out(val_str("str"));
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_VAR")))) {
        if (val_truthy(runtime_binop(51, runtime_type(runtime_at(runtime_var_get(env, "n"), val_str("type_tag"))), val_str("none")))) {
  return val_out(runtime_at(runtime_var_get(env, "n"), val_str("type_tag")));
}
if (val_truthy(runtime_binop(51, runtime_type(runtime_var_get(env, "fn_ctx")), val_str("none")))) {
  table_set(env, "params", runtime_at(runtime_var_get(env, "fn_ctx"), val_str("body")));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "params"))))) {
  table_set(env, "param", runtime_at(runtime_var_get(env, "params"), runtime_var_get(env, "i")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "param"), val_str("name")), runtime_at(runtime_var_get(env, "n"), val_str("name"))))) {
  table_set(env, "tt", runtime_at(runtime_var_get(env, "param"), val_str("type_tag")));
if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "tt")), val_str("none")))) {
  return val_out(val_str("unknown"));
}
return val_out(runtime_var_get(env, "tt"));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
}
return val_out(val_str("unknown"));
      } else {
        if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BINOP")))) {
          table_set(env, "op", runtime_at(runtime_var_get(env, "n"), val_str("op")));
if (val_truthy(runtime_binop(49, runtime_binop(52, runtime_binop(50, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_LT")), runtime_var_get(env, "op")), runtime_var_get(env, "TOK_OR")))) {
  return val_out(val_str("bit"));
}
return val_out(val_str("num"));
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_NOT")))) {
            return val_out(val_str("bit"));
          } else {
            if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_LEN")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_ORD")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_TONUM")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_TIME")))) {
              val_print(val_str("num")); printf("\n");
            } else {
              if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_CHR")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_TOSTR")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_TRIM")))) {
                val_print(val_str("str")); printf("\n");
              } else {
                if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_LIST")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_SPLIT")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_KEYS")))) {
                  val_print(val_str("list")); printf("\n");
                } else {
                  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_DICT")))) {
                    val_print(val_str("dict")); printf("\n");
                  } else {
                    val_print(val_str("unknown")); printf("\n");
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return val_none();
}

static Value* io_func_parse_primary(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  table_set(env, "k", runtime_at(runtime_var_get(env, "t"), val_str("kind")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_LPAREN")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_RPAREN")})); printf("\n");
val_print(runtime_var_get(env, "n")); printf("\n");
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_MINUS")))) {
      val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_UMINUS"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_NOT")))) {
        val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_NOT"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
      } else {
        if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_NUM")))) {
          val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_NUM"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("num"), runtime_at(runtime_var_get(env, "t"), val_str("num")));
val_print(runtime_var_get(env, "n")); printf("\n");
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_STR")))) {
            val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_STR"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("str"), runtime_at(runtime_var_get(env, "t"), val_str("text")));
val_print(runtime_var_get(env, "n")); printf("\n");
          } else {
            if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_ASK")))) {
              val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_ASK"), runtime_var_get(env, "t")})); printf("\n");
            } else {
              if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_ARG")))) {
                val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_ARG"), runtime_var_get(env, "t")})); printf("\n");
              } else {
                if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_KEYS")))) {
                  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_KEYS"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                } else {
                  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_LEN")))) {
                    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_LEN"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                  } else {
                    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_ORD")))) {
                      val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_ORD"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                    } else {
                      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_CHR")))) {
                        val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_CHR"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                      } else {
                        if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_TONUM")))) {
                          val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_TONUM"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                        } else {
                          if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_TOSTR")))) {
                            val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_TOSTR"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                          } else {
                            if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_SPLIT")))) {
                              val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_SPLIT"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
runtime_key(runtime_var_get(env, "n"), val_str("right"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                            } else {
                              if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_TRIM")))) {
                                val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_TRIM"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                              } else {
                                if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_REPLACE")))) {
                                  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_REPLACE"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
val_print(runtime_var_get(env, "n")); printf("\n");
                                } else {
                                  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_SAFE")))) {
                                    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_SAFE"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                                  } else {
                                    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_SLICE")))) {
                                      val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_SLICE"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
val_print(runtime_var_get(env, "n")); printf("\n");
                                    } else {
                                      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_TYPE")))) {
                                        val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_TYPE"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                                      } else {
                                        if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_TIME")))) {
                                          val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_TIME"), runtime_var_get(env, "t")})); printf("\n");
                                        } else {
                                          if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_SYS")))) {
                                            val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_SYS"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                                          } else {
                                            if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_REF")))) {
                                              val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_REF"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                                            } else {
                                              if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_ENV")))) {
                                                val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_ENV"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                                              } else {
                                                if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_EXIT")))) {
                                                  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_EXIT"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_var_get(env, "n")); printf("\n");
                                                } else {
                                                  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_LIST")))) {
                                                    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_LIST"), runtime_var_get(env, "t")})); printf("\n");
                                                  } else {
                                                    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_DICT")))) {
                                                      val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_DICT"), runtime_var_get(env, "t")})); printf("\n");
                                                    } else {
                                                      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_DO")))) {
                                                        table_set(env, "do_tok", runtime_var_get(env, "t"));
val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_DO"), runtime_var_get(env, "do_tok")}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_ARROW")))) {
  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
runtime_key(runtime_var_get(env, "n"), val_str("mod_tag"), runtime_at(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("text")));
}
runtime_key(runtime_var_get(env, "n"), val_str("name"), runtime_at(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("text")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_ARROW")))) {
  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
runtime_key(runtime_var_get(env, "n"), val_str("type_tag"), runtime_at(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("text")));
}
while (val_truthy(runtime_binop(40, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_IDENT")))) {
  table_set(env, "param", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_VAR"), runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")})}));
table_set(env, "param_name", runtime_at(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("text")));
runtime_key(runtime_var_get(env, "param"), val_str("name"), runtime_var_get(env, "param_name"));
if (val_truthy(runtime_call(table_get(env, "is_all_caps"), env, 1, (Value*[]){runtime_var_get(env, "param_name")}))) {
  val_print(runtime_call(table_get(env, "track_constant"), env, 4, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "param_name"), runtime_at(runtime_var_get(env, "t"), val_str("pos")), runtime_at(runtime_var_get(env, "t"), val_str("line"))})); printf("\n");
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_ARROW")))) {
  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
runtime_key(runtime_var_get(env, "param"), val_str("type_tag"), runtime_at(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("text")));
}
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_var_get(env, "param")})); printf("\n");
}
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_INDENT")})); printf("\n");
runtime_key(runtime_var_get(env, "p"), val_str("in_func"), runtime_binop(41, runtime_at(runtime_var_get(env, "p"), val_str("in_func")), val_num(1)));
table_set(env, "old_func", runtime_at(runtime_var_get(env, "p"), val_str("current_func")));
runtime_key(runtime_var_get(env, "p"), val_str("current_func"), runtime_var_get(env, "n"));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_block"), env, 2, (Value*[]){runtime_var_get(env, "p"), val_num(1)}));
if (val_truthy(runtime_binop(51, runtime_type(runtime_at(runtime_var_get(env, "n"), val_str("type_tag"))), val_str("none")))) {
  table_set(env, "block", runtime_at(runtime_var_get(env, "n"), val_str("left")));
if (val_truthy(runtime_binop(48, runtime_len(runtime_at(runtime_var_get(env, "block"), val_str("body"))), val_num(0)))) {
  table_set(env, "last", runtime_at(runtime_at(runtime_var_get(env, "block"), val_str("body")), runtime_binop(42, runtime_len(runtime_at(runtime_var_get(env, "block"), val_str("body"))), val_num(1))));
table_set(env, "actual", runtime_call(table_get(env, "infer_node_type"), env, 2, (Value*[]){runtime_var_get(env, "last"), runtime_var_get(env, "n")}));
table_set(env, "expected", runtime_at(runtime_var_get(env, "n"), val_str("type_tag")));
if (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_var_get(env, "actual"), val_str("unknown")), runtime_var_get(env, "actual")), runtime_var_get(env, "expected")))) {
  val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Error in "))->str, runtime_tostr(runtime_at(runtime_var_get(env, "p"), val_str("filename")))->str), val_str(" on line ")), runtime_tostr(runtime_at(runtime_var_get(env, "do_tok"), val_str("line")))), val_str(": return type mismatch in function '")), runtime_at(runtime_var_get(env, "n"), val_str("name"))), val_str("': expected '")), runtime_var_get(env, "expected")), val_str("', found '")), runtime_var_get(env, "actual")), val_str("'"))); printf("\n");
exit((int)(val_num(1))->num);
}
}
}
runtime_key(runtime_var_get(env, "p"), val_str("current_func"), runtime_var_get(env, "old_func"));
runtime_key(runtime_var_get(env, "p"), val_str("in_func"), runtime_binop(42, runtime_at(runtime_var_get(env, "p"), val_str("in_func")), val_num(1)));
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_DEDENT")})); printf("\n");
val_print(runtime_var_get(env, "n")); printf("\n");
                                                      } else {
                                                        if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_IDENT")))) {
                                                          val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_VAR"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("name"), runtime_at(runtime_var_get(env, "t"), val_str("text")));
val_print(runtime_var_get(env, "n")); printf("\n");
                                                        } else {
                                                          val_print(runtime_str_concat(runtime_tostr(val_str("Unexpected token: "))->str, runtime_tostr(runtime_at(runtime_var_get(env, "t"), val_str("text")))->str)); printf("\n");
exit((int)(val_num(1))->num);
                                                        }
                                                      }
                                                    }
                                                  }
                                                }
                                              }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return val_none();
}

static Value* io_func_parse_postfix(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "expr", runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  while (val_truthy(val_num(1))) {
    table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
table_set(env, "k", runtime_at(runtime_var_get(env, "t"), val_str("kind")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_AT")))) {
  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_AT"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "expr"));
runtime_key(runtime_var_get(env, "n"), val_str("right"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
table_set(env, "expr", runtime_var_get(env, "n"));
} else {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_SET")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_SET"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "expr"));
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
table_set(env, "expr", runtime_var_get(env, "n"));
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_KEY")))) {
      val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_KEY"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "expr"));
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
table_set(env, "expr", runtime_var_get(env, "n"));
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_PUT")))) {
        val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_PUT"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "expr"));
runtime_key(runtime_var_get(env, "n"), val_str("right"), runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
table_set(env, "expr", runtime_var_get(env, "n"));
      } else {
        if (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_ASK")), runtime_at(runtime_var_get(env, "expr"), val_str("kind"))), runtime_var_get(env, "ND_ASK")))) {
          val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_ASKFILE"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "expr"));
table_set(env, "expr", runtime_var_get(env, "n"));
        } else {
          if (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(47, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_EQ")), runtime_var_get(env, "k")), runtime_var_get(env, "TOK_NEWLINE")), runtime_var_get(env, "k")), runtime_var_get(env, "TOK_SEMICOLON")), runtime_var_get(env, "k")), runtime_var_get(env, "TOK_DEDENT")), runtime_var_get(env, "k")), runtime_var_get(env, "TOK_EOF")))) {
            if (val_truthy(runtime_binop(51, runtime_at(runtime_var_get(env, "expr"), val_str("kind")), runtime_var_get(env, "ND_VAR")))) {
  break;
}
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_CALL"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "expr"));
while (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_NEWLINE")), runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind"))), runtime_var_get(env, "TOK_SEMICOLON")), runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind"))), runtime_var_get(env, "TOK_DEDENT")), runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind"))), runtime_var_get(env, "TOK_EOF")))) {
  val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_primary"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
}
table_set(env, "expr", runtime_var_get(env, "n"));
          } else {
            break;
          }
        }
      }
    }
  }
}
  }
  return val_out(runtime_var_get(env, "expr"));
  return val_none();
}

static Value* io_func_parse_expr(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "left", runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  table_set(env, "k", runtime_at(runtime_var_get(env, "t"), val_str("kind")));
  while (val_truthy(runtime_binop(49, runtime_binop(52, runtime_binop(50, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_EQEQ")), runtime_var_get(env, "k")), runtime_var_get(env, "TOK_OR")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "right", runtime_call(table_get(env, "parse_postfix"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_BINOP"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("op"), runtime_var_get(env, "k"));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "left"));
runtime_key(runtime_var_get(env, "n"), val_str("right"), runtime_var_get(env, "right"));
table_set(env, "left", runtime_var_get(env, "n"));
table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
table_set(env, "k", runtime_at(runtime_var_get(env, "t"), val_str("kind")));
  }
  table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  table_set(env, "op_tok", runtime_var_get(env, "t"));
  if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_EQ")), runtime_var_get(env, "k")), runtime_var_get(env, "TOK_IDENT")))) {
    if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "left"), val_str("kind")), runtime_var_get(env, "ND_VAR")))) {
  table_set(env, "name", runtime_at(runtime_var_get(env, "left"), val_str("name")));
if (val_truthy(runtime_call(table_get(env, "is_all_caps"), env, 1, (Value*[]){runtime_var_get(env, "name")}))) {
  val_print(runtime_call(table_get(env, "track_constant"), env, 4, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "name"), runtime_at(runtime_var_get(env, "t"), val_str("pos")), runtime_at(runtime_var_get(env, "t"), val_str("line"))})); printf("\n");
}
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_IDENT")))) {
  table_set(env, "next", runtime_at(runtime_at(runtime_var_get(env, "p"), val_str("tokens")), runtime_binop(41, runtime_at(runtime_var_get(env, "p"), val_str("pos")), val_num(1))));
if (val_truthy(runtime_binop(51, runtime_at(runtime_var_get(env, "next"), val_str("kind")), runtime_var_get(env, "TOK_EQ")))) {
  return val_out(runtime_var_get(env, "left"));
}
runtime_key(runtime_var_get(env, "left"), val_str("type_tag"), runtime_at(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("text")));
}
val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_ASSIGN"), runtime_var_get(env, "op_tok")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "left"));
runtime_key(runtime_var_get(env, "n"), val_str("right"), runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
table_set(env, "left", runtime_var_get(env, "n"));
if (val_truthy(runtime_binop(51, runtime_type(runtime_at(runtime_var_get(env, "left"), val_str("type_tag"))), val_str("none")))) {
  table_set(env, "actual", runtime_call(table_get(env, "infer_node_type"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right")), runtime_at(runtime_var_get(env, "p"), val_str("current_func"))}));
table_set(env, "expected", runtime_at(runtime_var_get(env, "left"), val_str("type_tag")));
if (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_var_get(env, "actual"), val_str("unknown")), runtime_var_get(env, "actual")), runtime_var_get(env, "expected")))) {
  val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Error in "))->str, runtime_tostr(runtime_at(runtime_var_get(env, "p"), val_str("filename")))->str), val_str(" on line ")), runtime_tostr(runtime_at(runtime_var_get(env, "op_tok"), val_str("line")))), val_str(": type mismatch in assignment to '")), runtime_at(runtime_var_get(env, "left"), val_str("name"))), val_str("': expected '")), runtime_var_get(env, "expected")), val_str("', found '")), runtime_var_get(env, "actual")), val_str("'"))); printf("\n");
exit((int)(val_num(1))->num);
}
}
  }
  return val_out(runtime_var_get(env, "left"));
  return val_none();
}

static Value* io_func_skip_block_body(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "depth", val_num(0));
  while (val_truthy(runtime_binop(51, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_EOF")))) {
    table_set(env, "k", runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_INDENT")))) {
  table_set(env, "depth", runtime_binop(41, runtime_var_get(env, "depth"), val_num(1)));
} else {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_DEDENT")))) {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "depth"), val_num(0)))) {
  break;
}
table_set(env, "depth", runtime_binop(42, runtime_var_get(env, "depth"), val_num(1)));
  }
}
val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
  }
  return val_none();
}

static Value* io_func_resolve_condition(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  if(argc > 1) table_set(env, "name", argv[1]);
  table_set(env, "defines", runtime_at(runtime_var_get(env, "p"), val_str("defines")));
  if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "defines")), val_str("none")))) {
    return val_out(val_num(0));
  }
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "defines"))))) {
    if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "defines"), runtime_var_get(env, "i")), runtime_var_get(env, "name")))) {
  return val_out(val_num(1));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  return val_out(val_num(0));
  return val_none();
}

static Value* io_func_parse_when_stmt(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "result", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_BLOCK"), runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")})}));
  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
  while (val_truthy(val_num(1))) {
    table_set(env, "cond", runtime_at(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("text")));
table_set(env, "taken", runtime_call(table_get(env, "resolve_condition"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "cond")}));
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_INDENT")})); printf("\n");
if (val_truthy(runtime_var_get(env, "taken"))) {
  table_set(env, "body", runtime_call(table_get(env, "parse_block"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_binop(48, runtime_at(runtime_var_get(env, "p"), val_str("in_func")), val_num(0))}));
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_DEDENT")})); printf("\n");
break;
}
val_print(runtime_call(table_get(env, "skip_block_body"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_DEDENT")})); printf("\n");
  }
  return val_out(runtime_var_get(env, "result"));
  return val_none();
}

static Value* io_func_parse_stmt(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
  table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  table_set(env, "k", runtime_at(runtime_var_get(env, "t"), val_str("kind")));
  table_set(env, "text", runtime_at(runtime_var_get(env, "t"), val_str("text")));
  if (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_IDENT")), runtime_var_get(env, "text")), val_str("IF")))) {
    return val_out(runtime_call(table_get(env, "parse_when_stmt"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_PASTE")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "t", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
if (val_truthy(runtime_binop(51, runtime_at(runtime_var_get(env, "t"), val_str("kind")), runtime_var_get(env, "TOK_STR")))) {
  val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Error in "))->str, runtime_tostr(runtime_at(runtime_var_get(env, "p"), val_str("filename")))->str), val_str(" on line ")), runtime_tostr(runtime_at(runtime_var_get(env, "t"), val_str("line")))), val_str(": paste requires a string filename"))); printf("\n");
exit((int)(val_num(1))->num);
}
table_set(env, "filename", runtime_at(runtime_var_get(env, "t"), val_str("text")));
val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "p_files", runtime_at(runtime_var_get(env, "p"), val_str("pasted_files")));
table_set(env, "already_done", val_num(0));
table_set(env, "i", val_num(0));
if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "p_files")), val_str("none")))) {
  runtime_key(runtime_var_get(env, "p"), val_str("pasted_files"), val_list());
table_set(env, "p_files", runtime_at(runtime_var_get(env, "p"), val_str("pasted_files")));
}
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "p_files"))))) {
  if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "p_files"), runtime_var_get(env, "i")), runtime_var_get(env, "filename")))) {
  table_set(env, "already_done", val_num(1));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
if (val_truthy(runtime_var_get(env, "already_done"))) {
  return val_out(runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_BLOCK"), runtime_var_get(env, "t")}));
}
if (val_truthy(runtime_at(runtime_var_get(env, "p"), val_str("boot_debug")))) {
  val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Paste trace: inclusion of '"))->str, runtime_tostr(runtime_var_get(env, "filename"))->str), val_str("' from '")), runtime_at(runtime_var_get(env, "p"), val_str("filename"))), val_str("'"))); printf("\n");
}
runtime_set(runtime_var_get(env, "p_files"), runtime_len(runtime_var_get(env, "p_files")), runtime_var_get(env, "filename"));
table_set(env, "src", runtime_ask(runtime_var_get(env, "filename")));
table_set(env, "l_ctx", val_dict());
runtime_key(runtime_var_get(env, "l_ctx"), val_str("src"), runtime_var_get(env, "src"));
runtime_key(runtime_var_get(env, "l_ctx"), val_str("pos"), val_num(0));
runtime_key(runtime_var_get(env, "l_ctx"), val_str("line"), val_num(1));
runtime_key(runtime_var_get(env, "l_ctx"), val_str("filename"), runtime_var_get(env, "filename"));
table_set(env, "sub_tokens", runtime_call(table_get(env, "lex"), env, 1, (Value*[]){runtime_var_get(env, "l_ctx")}));
table_set(env, "sub_p", val_dict());
runtime_key(runtime_var_get(env, "sub_p"), val_str("tokens"), runtime_var_get(env, "sub_tokens"));
runtime_key(runtime_var_get(env, "sub_p"), val_str("pos"), val_num(0));
runtime_key(runtime_var_get(env, "sub_p"), val_str("filename"), runtime_var_get(env, "filename"));
runtime_key(runtime_var_get(env, "sub_p"), val_str("src"), runtime_var_get(env, "src"));
runtime_key(runtime_var_get(env, "sub_p"), val_str("in_func"), runtime_at(runtime_var_get(env, "p"), val_str("in_func")));
runtime_key(runtime_var_get(env, "sub_p"), val_str("current_func"), runtime_at(runtime_var_get(env, "p"), val_str("current_func")));
runtime_key(runtime_var_get(env, "sub_p"), val_str("constants"), runtime_at(runtime_var_get(env, "p"), val_str("constants")));
runtime_key(runtime_var_get(env, "sub_p"), val_str("pasted_files"), runtime_var_get(env, "p_files"));
return val_out(runtime_call(table_get(env, "parse_program"), env, 1, (Value*[]){runtime_var_get(env, "sub_p")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_FOR")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_FOR"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("name"), runtime_at(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("text")));
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_IN")})); printf("\n");
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_INDENT")})); printf("\n");
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_block"), env, 2, (Value*[]){runtime_var_get(env, "p"), val_num(0)})})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_DEDENT")})); printf("\n");
return val_out(runtime_var_get(env, "n"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_IF")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_IF"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_INDENT")})); printf("\n");
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_block"), env, 2, (Value*[]){runtime_var_get(env, "p"), val_num(0)})})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_DEDENT")})); printf("\n");
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
if (val_truthy(runtime_binop(40, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_ELSE")))) {
  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
if (val_truthy(runtime_binop(40, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_IF")))) {
  runtime_key(runtime_var_get(env, "n"), val_str("right"), runtime_call(table_get(env, "parse_stmt"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
} else {
  val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_INDENT")})); printf("\n");
runtime_key(runtime_var_get(env, "n"), val_str("right"), runtime_call(table_get(env, "parse_block"), env, 2, (Value*[]){runtime_var_get(env, "p"), val_num(0)}));
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_DEDENT")})); printf("\n");
}
}
return val_out(runtime_var_get(env, "n"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_WHILE")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_WHILE"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_INDENT")})); printf("\n");
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_call(table_get(env, "parse_block"), env, 2, (Value*[]){runtime_var_get(env, "p"), val_num(0)})})); printf("\n");
val_print(runtime_call(table_get(env, "expect"), env, 2, (Value*[]){runtime_var_get(env, "p"), runtime_var_get(env, "TOK_DEDENT")})); printf("\n");
return val_out(runtime_var_get(env, "n"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_BREAK")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
return val_out(runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_BREAK"), runtime_var_get(env, "t")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_SKIP")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
return val_out(runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_SKIP"), runtime_var_get(env, "t")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_OUT")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_OUT"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
return val_out(runtime_var_get(env, "n"));
  }
  table_set(env, "expr", runtime_call(table_get(env, "parse_expr"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  table_set(env, "ek", runtime_at(runtime_var_get(env, "expr"), val_str("kind")));
  if (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_var_get(env, "ek"), runtime_var_get(env, "ND_ASSIGN")), runtime_var_get(env, "ek")), runtime_var_get(env, "ND_DO")), runtime_var_get(env, "ek")), runtime_var_get(env, "ND_SET")), runtime_var_get(env, "ek")), runtime_var_get(env, "ND_KEY")), runtime_var_get(env, "ek")), runtime_var_get(env, "ND_PUT")), runtime_var_get(env, "ek")), runtime_var_get(env, "ND_EXIT")))) {
    table_set(env, "n", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_PRINT"), runtime_var_get(env, "t")}));
runtime_key(runtime_var_get(env, "n"), val_str("left"), runtime_var_get(env, "expr"));
return val_out(runtime_var_get(env, "n"));
  }
  return val_out(runtime_var_get(env, "expr"));
  return val_none();
}

static Value* io_func_parse_block(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  if(argc > 1) table_set(env, "is_func", argv[1]);
  table_set(env, "block", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_BLOCK"), runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")})}));
  while (val_truthy(runtime_binop(51, runtime_binop(52, runtime_binop(51, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_DEDENT")), runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind"))), runtime_var_get(env, "TOK_EOF")))) {
    if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_NEWLINE")), runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind"))), runtime_var_get(env, "TOK_SEMICOLON")))) {
  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
} else {
  table_set(env, "stmt", runtime_call(table_get(env, "parse_stmt"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "block"), runtime_var_get(env, "stmt")})); printf("\n");
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
}
  }
  if (val_truthy(runtime_var_get(env, "is_func"))) {
    table_set(env, "body", runtime_at(runtime_var_get(env, "block"), val_str("body")));
table_set(env, "blen", runtime_len(runtime_var_get(env, "body")));
if (val_truthy(runtime_binop(48, runtime_var_get(env, "blen"), val_num(0)))) {
  table_set(env, "last_idx", runtime_binop(42, runtime_var_get(env, "blen"), val_num(1)));
table_set(env, "last", runtime_at(runtime_var_get(env, "body"), runtime_var_get(env, "last_idx")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "last"), val_str("kind")), runtime_var_get(env, "ND_PRINT")))) {
  runtime_set(runtime_var_get(env, "body"), runtime_var_get(env, "last_idx"), runtime_at(runtime_var_get(env, "last"), val_str("left")));
}
}
  }
  return val_out(runtime_var_get(env, "block"));
  return val_none();
}

static Value* io_func_parse_program(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "p", argv[0]);
  table_set(env, "prog", runtime_call(table_get(env, "nd_new"), env, 2, (Value*[]){runtime_var_get(env, "ND_BLOCK"), runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")})}));
  table_set(env, "t1", runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}));
  if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "t1"), val_str("kind")), runtime_var_get(env, "TOK_SHEBANG")))) {
    val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
table_set(env, "once_registry", runtime_at(runtime_var_get(env, "p"), val_str("once_registry")));
table_set(env, "fname", runtime_at(runtime_var_get(env, "p"), val_str("filename")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "once_registry"), runtime_var_get(env, "fname")), val_num(1)))) {
  return val_out(runtime_var_get(env, "prog"));
}
runtime_key(runtime_var_get(env, "once_registry"), runtime_var_get(env, "fname"), val_num(1));
  }
  while (val_truthy(runtime_binop(51, runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")), runtime_var_get(env, "TOK_EOF")))) {
    table_set(env, "k", runtime_at(runtime_call(table_get(env, "peek"), env, 1, (Value*[]){runtime_var_get(env, "p")}), val_str("kind")));
if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "TOK_NEWLINE")), runtime_var_get(env, "k")), runtime_var_get(env, "TOK_SEMICOLON")))) {
  val_print(runtime_call(table_get(env, "advance"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
} else {
  val_print(runtime_call(table_get(env, "nd_push"), env, 2, (Value*[]){runtime_var_get(env, "prog"), runtime_call(table_get(env, "parse_stmt"), env, 1, (Value*[]){runtime_var_get(env, "p")})})); printf("\n");
val_print(runtime_call(table_get(env, "skip_newlines"), env, 1, (Value*[]){runtime_var_get(env, "p")})); printf("\n");
}
  }
  return val_out(runtime_var_get(env, "prog"));
  return val_none();
}

static Value* io_func_val_num(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "n", argv[0]);
  table_set(env, "v", val_dict());
  runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_NUM"));
  runtime_key(runtime_var_get(env, "v"), val_str("num"), runtime_var_get(env, "n"));
  return val_out(runtime_var_get(env, "v"));
  return val_none();
}

static Value* io_func_val_str(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "s", argv[0]);
  table_set(env, "res", runtime_at(runtime_var_get(env, "STRING_CACHE"), runtime_var_get(env, "s")));
  if (val_truthy(runtime_binop(51, runtime_type(runtime_var_get(env, "res")), val_str("none")))) {
    return val_out(runtime_var_get(env, "res"));
  }
  table_set(env, "v", val_dict());
  runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_STR"));
  runtime_key(runtime_var_get(env, "v"), val_str("str"), runtime_var_get(env, "s"));
  runtime_key(runtime_var_get(env, "STRING_CACHE"), runtime_var_get(env, "s"), runtime_var_get(env, "v"));
  return val_out(runtime_var_get(env, "v"));
  return val_none();
}

static Value* io_func_val_none(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  table_set(env, "v", val_dict());
  runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_NONE"));
  return val_out(runtime_var_get(env, "v"));
  return val_none();
}

static Value* io_func_val_break(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  table_set(env, "v", val_dict());
  runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_BREAK"));
  return val_out(runtime_var_get(env, "v"));
  return val_none();
}

static Value* io_func_val_skip(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  table_set(env, "v", val_dict());
  runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_SKIP"));
  return val_out(runtime_var_get(env, "v"));
  return val_none();
}

static Value* io_func_val_out(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "val", argv[0]);
  table_set(env, "v", val_dict());
  runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_OUT"));
  runtime_key(runtime_var_get(env, "v"), val_str("inner"), runtime_var_get(env, "val"));
  return val_out(runtime_var_get(env, "v"));
  return val_none();
}

static Value* io_func_val_bit(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "n", argv[0]);
  table_set(env, "v", val_dict());
  runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_BIT"));
  runtime_key(runtime_var_get(env, "v"), val_str("num"), runtime_var_get(env, "n"));
  return val_out(runtime_var_get(env, "v"));
  return val_none();
}

static Value* io_func_val_print(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "v", argv[0]);
  table_set(env, "k", runtime_at(runtime_var_get(env, "v"), val_str("kind")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_NUM")))) {
    return val_out(runtime_tostr(runtime_at(runtime_var_get(env, "v"), val_str("num"))));
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_STR")))) {
      return val_out(runtime_at(runtime_var_get(env, "v"), val_str("str")));
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_BIT")))) {
        return val_out(runtime_tostr(runtime_at(runtime_var_get(env, "v"), val_str("num"))));
      } else {
        if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_NONE")))) {
          return val_out(val_str("none"));
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_LIST")))) {
            table_set(env, "items", runtime_at(runtime_var_get(env, "v"), val_str("items")));
table_set(env, "res_str", val_str("["));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "items"))))) {
  table_set(env, "res_str", runtime_binop(41, runtime_var_get(env, "res_str"), runtime_call(table_get(env, "val_print"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "items"), runtime_var_get(env, "i"))})));
if (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_binop(42, runtime_len(runtime_var_get(env, "items")), val_num(1))))) {
  table_set(env, "res_str", runtime_binop(41, runtime_var_get(env, "res_str"), val_str(", ")));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
return val_out(runtime_binop(41, runtime_var_get(env, "res_str"), val_str("]")));
          } else {
            if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_DICT")))) {
              return val_out(val_str("{dict}"));
            }
          }
        }
      }
    }
  }
  return val_none();
}

static Value* io_func_val_truthy(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "v", argv[0]);
  table_set(env, "k", runtime_at(runtime_var_get(env, "v"), val_str("kind")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_NUM")))) {
    return val_out(runtime_binop(51, runtime_at(runtime_var_get(env, "v"), val_str("num")), val_num(0)));
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_STR")))) {
      return val_out(runtime_binop(48, runtime_len(runtime_at(runtime_var_get(env, "v"), val_str("str"))), val_num(0)));
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_LIST")))) {
        return val_out(runtime_binop(48, runtime_len(runtime_at(runtime_var_get(env, "v"), val_str("items"))), val_num(0)));
      } else {
        if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_BIT")))) {
          return val_out(runtime_at(runtime_var_get(env, "v"), val_str("num")));
        }
      }
    }
  }
  return val_out(val_num(0));
  return val_none();
}

static Value* io_func_val_unwrap(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "v", argv[0]);
  table_set(env, "curr", runtime_var_get(env, "v"));
  table_set(env, "depth", val_num(0));
  while (val_truthy(runtime_binop(51, runtime_type(runtime_var_get(env, "curr")), val_str("none")))) {
    table_set(env, "k", runtime_at(runtime_var_get(env, "curr"), val_str("kind")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_REF")))) {
  table_set(env, "curr", runtime_at(runtime_var_get(env, "curr"), val_str("target")));
} else {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_OUT")))) {
    table_set(env, "curr", runtime_at(runtime_var_get(env, "curr"), val_str("inner")));
  } else {
    break;
  }
}
table_set(env, "depth", runtime_binop(41, runtime_var_get(env, "depth"), val_num(1)));
if (val_truthy(runtime_binop(48, runtime_var_get(env, "depth"), val_num(100)))) {
  val_print(val_str("Runtime error: circular reference in unwrap")); printf("\n");
exit((int)(val_num(1))->num);
}
  }
  return val_out(runtime_var_get(env, "curr"));
  return val_none();
}

static Value* io_func_table_get(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "env_", argv[0]);
  if(argc > 1) table_set(env, "name", argv[1]);
  table_set(env, "curr", runtime_var_get(env, "env_"));
  while (val_truthy(runtime_binop(51, runtime_type(runtime_var_get(env, "curr")), val_str("none")))) {
    table_set(env, "val", runtime_at(runtime_var_get(env, "curr"), runtime_var_get(env, "name")));
if (val_truthy(runtime_binop(51, runtime_type(runtime_var_get(env, "val")), val_str("none")))) {
  return val_out(runtime_var_get(env, "val"));
}
table_set(env, "curr", runtime_at(runtime_var_get(env, "curr"), val_str("parent")));
  }
  return val_out(runtime_var_get(env, "val_none"));
  return val_none();
}

static Value* io_func_table_set(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "env_", argv[0]);
  if(argc > 1) table_set(env, "name", argv[1]);
  if(argc > 2) table_set(env, "val", argv[2]);
  if (val_truthy(runtime_call(table_get(env, "is_all_caps"), env, 1, (Value*[]){runtime_var_get(env, "name")}))) {
    table_set(env, "existing", runtime_at(runtime_var_get(env, "env_"), runtime_var_get(env, "name")));
if (val_truthy(runtime_binop(51, runtime_type(runtime_var_get(env, "existing")), val_str("none")))) {
  val_print(runtime_str_concat(runtime_tostr(val_str("Runtime error: cannot reassign constant "))->str, runtime_tostr(runtime_var_get(env, "name"))->str)); printf("\n");
exit((int)(val_num(1))->num);
}
  }
  return val_out(runtime_key(runtime_var_get(env, "env_"), runtime_var_get(env, "name"), runtime_var_get(env, "val")));
  return val_none();
}

static Value* io_func_eval_block(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "block", argv[0]);
  if(argc > 1) table_set(env, "env_", argv[1]);
  table_set(env, "last", runtime_var_get(env, "val_none"));
  table_set(env, "body", runtime_at(runtime_var_get(env, "block"), val_str("body")));
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "body"))))) {
    table_set(env, "last", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "body"), runtime_var_get(env, "i")), runtime_var_get(env, "env_")}));
table_set(env, "k", runtime_at(runtime_var_get(env, "last"), val_str("kind")));
if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "VAL_BREAK")), runtime_var_get(env, "k")), runtime_var_get(env, "VAL_OUT")), runtime_var_get(env, "k")), runtime_var_get(env, "VAL_SKIP")))) {
  return val_out(runtime_var_get(env, "last"));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  return val_out(runtime_var_get(env, "last"));
  return val_none();
}

static Value* io_func_eval(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "n", argv[0]);
  if(argc > 1) table_set(env, "env_", argv[1]);
  if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "n")), val_str("none")))) {
    return val_out(runtime_var_get(env, "val_none"));
  }
  table_set(env, "interp_steps", runtime_binop(41, runtime_var_get(env, "interp_steps"), val_num(1)));
  table_set(env, "db_flag", runtime_call(table_get(env, "table_get"), env, 2, (Value*[]){runtime_var_get(env, "env_"), val_str("BOOT_DEBUG")}));
  if (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(40, runtime_at(runtime_var_get(env, "db_flag"), val_str("kind")), runtime_var_get(env, "VAL_BIT")), runtime_at(runtime_var_get(env, "db_flag"), val_str("num"))), val_num(1)))) {
    if (val_truthy(runtime_binop(40, runtime_binop(45, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "interp_steps"), val_num(1)), runtime_var_get(env, "interp_steps")), val_num(500)), val_num(0)))) {
  table_set(env, "msg", runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("[INTERP] Step "))->str, runtime_tostr(runtime_tostr(runtime_var_get(env, "interp_steps")))->str), val_str(": Node ")), runtime_tostr(runtime_at(runtime_var_get(env, "n"), val_str("kind")))), val_str(" at ")), runtime_at(runtime_var_get(env, "n"), val_str("filename"))), val_str(":")), runtime_tostr(runtime_at(runtime_var_get(env, "n"), val_str("line")))), val_str("\n")));
runtime_put(val_str(""), runtime_var_get(env, "msg"), 0);
}
  }
  table_set(env, "k", runtime_at(runtime_var_get(env, "n"), val_str("kind")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_NUM")))) {
    return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("num"))}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_STR")))) {
    return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("str"))}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_UMINUS")))) {
    table_set(env, "v", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){val_num(0 - (runtime_at(runtime_var_get(env, "v"), val_str("num")))->num)}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_VAR")))) {
    table_set(env, "res", runtime_call(table_get(env, "table_get"), env, 2, (Value*[]){runtime_var_get(env, "env_"), runtime_at(runtime_var_get(env, "n"), val_str("name"))}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_NONE")))) {
  table_set(env, "fn", runtime_at(runtime_var_get(env, "FUNCS"), runtime_at(runtime_var_get(env, "n"), val_str("name"))));
if (val_truthy(runtime_binop(51, runtime_type(runtime_var_get(env, "fn")), val_str("none")))) {
  table_set(env, "scope", val_dict());
runtime_key(runtime_var_get(env, "scope"), val_str("parent"), runtime_var_get(env, "env_"));
table_set(env, "res", runtime_call(table_get(env, "eval_block"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "fn"), val_str("left")), runtime_var_get(env, "scope")}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_OUT")))) {
  return val_out(runtime_at(runtime_var_get(env, "res"), val_str("inner")));
}
return val_out(runtime_var_get(env, "res"));
}
val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Runtime error: undefined variable "))->str, runtime_tostr(runtime_at(runtime_var_get(env, "n"), val_str("name")))->str), val_str(" in ")), runtime_at(runtime_var_get(env, "n"), val_str("filename"))), val_str(" on line ")), runtime_tostr(runtime_at(runtime_var_get(env, "n"), val_str("line"))))); printf("\n");
exit((int)(val_num(1))->num);
}
return val_out(runtime_var_get(env, "res"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_SKIP")))) {
    return val_out(runtime_var_get(env, "val_skip"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_ASSIGN")))) {
    table_set(env, "val", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right")), runtime_var_get(env, "env_")}));
table_set(env, "left", runtime_at(runtime_var_get(env, "n"), val_str("left")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "left"), val_str("kind")), runtime_var_get(env, "ND_VAR")))) {
  val_print(runtime_call(table_get(env, "table_set"), env, 3, (Value*[]){runtime_var_get(env, "env_"), runtime_at(runtime_var_get(env, "left"), val_str("name")), runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_var_get(env, "val")})})); printf("\n");
} else {
  if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "left"), val_str("kind")), runtime_var_get(env, "ND_AT")))) {
    table_set(env, "target", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "left"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "idx", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "left"), val_str("right")), runtime_var_get(env, "env_")})}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "target"), val_str("kind")), runtime_var_get(env, "VAL_LIST")))) {
  runtime_set(runtime_at(runtime_var_get(env, "target"), val_str("items")), runtime_at(runtime_var_get(env, "idx"), val_str("num")), runtime_var_get(env, "val"));
} else {
  if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "target"), val_str("kind")), runtime_var_get(env, "VAL_DICT")))) {
    if (val_truthy(runtime_binop(51, runtime_at(runtime_var_get(env, "idx"), val_str("kind")), runtime_var_get(env, "VAL_STR")))) {
  val_print(val_str("Runtime error: dict key must be string")); printf("\n");
exit((int)(val_num(1))->num);
}
runtime_key(runtime_at(runtime_var_get(env, "target"), val_str("fields")), runtime_at(runtime_var_get(env, "idx"), val_str("str")), runtime_var_get(env, "val"));
  }
}
  }
}
return val_out(runtime_var_get(env, "val"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BINOP")))) {
    table_set(env, "op", runtime_at(runtime_var_get(env, "n"), val_str("op")));
table_set(env, "l", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_AND")))) {
  if (val_truthy(val_bit(!val_truthy(runtime_call(table_get(env, "val_truthy"), env, 1, (Value*[]){runtime_var_get(env, "l")}))))) {
  return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){val_num(0)}));
}
return val_out(runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right")), runtime_var_get(env, "env_")}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_OR")))) {
  if (val_truthy(runtime_call(table_get(env, "val_truthy"), env, 1, (Value*[]){runtime_var_get(env, "l")}))) {
  return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){val_num(1)}));
}
return val_out(runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right")), runtime_var_get(env, "env_")}));
}
table_set(env, "r", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right")), runtime_var_get(env, "env_")})}));
if (val_truthy(runtime_binop(52, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_at(runtime_var_get(env, "l"), val_str("kind")), runtime_var_get(env, "VAL_NUM")), runtime_at(runtime_var_get(env, "l"), val_str("kind"))), runtime_var_get(env, "VAL_BIT")), runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_at(runtime_var_get(env, "r"), val_str("kind")), runtime_var_get(env, "VAL_NUM")), runtime_at(runtime_var_get(env, "r"), val_str("kind"))), runtime_var_get(env, "VAL_BIT"))))) {
  table_set(env, "lv", runtime_at(runtime_var_get(env, "l"), val_str("num")));
table_set(env, "rv", runtime_at(runtime_var_get(env, "r"), val_str("num")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_PLUS")))) {
  return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){runtime_binop(41, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_MINUS")))) {
  return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){runtime_binop(42, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_STAR")))) {
  return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){runtime_binop(43, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_SLASH")))) {
  return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){runtime_binop(44, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_LT")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(47, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_GT")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(48, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_LTE")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(49, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_GTE")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(50, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_EQEQ")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(40, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_NE")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(51, runtime_var_get(env, "lv"), runtime_var_get(env, "rv"))}));
}
}
if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_at(runtime_var_get(env, "l"), val_str("kind")), runtime_var_get(env, "VAL_STR")), runtime_at(runtime_var_get(env, "r"), val_str("kind"))), runtime_var_get(env, "VAL_STR")))) {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_PLUS")))) {
  table_set(env, "ls", val_str(""));
table_set(env, "rs", val_str(""));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "l"), val_str("kind")), runtime_var_get(env, "VAL_STR")))) {
  table_set(env, "ls", runtime_at(runtime_var_get(env, "l"), val_str("str")));
} else {
  table_set(env, "ls", runtime_tostr(runtime_at(runtime_var_get(env, "l"), val_str("num"))));
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "r"), val_str("kind")), runtime_var_get(env, "VAL_STR")))) {
  table_set(env, "rs", runtime_at(runtime_var_get(env, "r"), val_str("str")));
} else {
  table_set(env, "rs", runtime_tostr(runtime_at(runtime_var_get(env, "r"), val_str("num"))));
}
return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){runtime_binop(41, runtime_var_get(env, "ls"), runtime_var_get(env, "rs"))}));
}
if (val_truthy(runtime_binop(40, runtime_binop(52, runtime_binop(40, runtime_at(runtime_var_get(env, "l"), val_str("kind")), runtime_var_get(env, "VAL_STR")), runtime_at(runtime_var_get(env, "r"), val_str("kind"))), runtime_var_get(env, "VAL_STR")))) {
  table_set(env, "ls", runtime_at(runtime_var_get(env, "l"), val_str("str")));
table_set(env, "rs", runtime_at(runtime_var_get(env, "r"), val_str("str")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_LT")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(47, runtime_var_get(env, "ls"), runtime_var_get(env, "rs"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_GT")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(48, runtime_var_get(env, "ls"), runtime_var_get(env, "rs"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_LTE")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(49, runtime_var_get(env, "ls"), runtime_var_get(env, "rs"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_GTE")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(50, runtime_var_get(env, "ls"), runtime_var_get(env, "rs"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_EQEQ")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(40, runtime_var_get(env, "ls"), runtime_var_get(env, "rs"))}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "op"), runtime_var_get(env, "TOK_NE")))) {
  return val_out(runtime_call(table_get(env, "val_bit"), env, 1, (Value*[]){runtime_binop(51, runtime_var_get(env, "ls"), runtime_var_get(env, "rs"))}));
}
}
}
return val_out(runtime_var_get(env, "val_none"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_IF")))) {
    table_set(env, "cond", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
if (val_truthy(runtime_call(table_get(env, "val_truthy"), env, 1, (Value*[]){runtime_var_get(env, "cond")}))) {
  return val_out(runtime_call(table_get(env, "eval_block"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_var_get(env, "env_")}));
} else {
  if (val_truthy(runtime_binop(51, runtime_type(runtime_at(runtime_var_get(env, "n"), val_str("right"))), val_str("none")))) {
    table_set(env, "right", runtime_at(runtime_var_get(env, "n"), val_str("right")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "right"), val_str("kind")), runtime_var_get(env, "ND_BLOCK")))) {
  return val_out(runtime_call(table_get(env, "eval_block"), env, 2, (Value*[]){runtime_var_get(env, "right"), runtime_var_get(env, "env_")}));
}
return val_out(runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_var_get(env, "right"), runtime_var_get(env, "env_")}));
  }
}
return val_out(runtime_var_get(env, "val_none"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_WHILE")))) {
    while (val_truthy(runtime_call(table_get(env, "val_truthy"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}))) {
  table_set(env, "res", runtime_call(table_get(env, "eval_block"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0)), runtime_var_get(env, "env_")}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_BREAK")))) {
  break;
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_SKIP")))) {
  val_print(runtime_var_get(env, "skip")); printf("\n");
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_OUT")))) {
  return val_out(runtime_var_get(env, "res"));
}
}
return val_out(runtime_var_get(env, "val_none"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_FOR")))) {
    table_set(env, "lst", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "items", runtime_at(runtime_var_get(env, "lst"), val_str("items")));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "items"))))) {
  val_print(runtime_call(table_get(env, "table_set"), env, 3, (Value*[]){runtime_var_get(env, "env_"), runtime_at(runtime_var_get(env, "n"), val_str("name")), runtime_at(runtime_var_get(env, "items"), runtime_var_get(env, "i"))})); printf("\n");
table_set(env, "res", runtime_call(table_get(env, "eval_block"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0)), runtime_var_get(env, "env_")}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_BREAK")))) {
  break;
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_SKIP")))) {
  table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
val_print(runtime_var_get(env, "skip")); printf("\n");
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_OUT")))) {
  return val_out(runtime_var_get(env, "res"));
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
return val_out(runtime_var_get(env, "val_none"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_DO")))) {
    runtime_key(runtime_var_get(env, "FUNCS"), runtime_at(runtime_var_get(env, "n"), val_str("name")), runtime_var_get(env, "n"));
return val_out(runtime_var_get(env, "val_none"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_CALL")))) {
    table_set(env, "fn", runtime_at(runtime_var_get(env, "FUNCS"), runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("left")), val_str("name"))));
if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "fn")), val_str("none")))) {
  val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Runtime error: undefined function "))->str, runtime_tostr(runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("left")), val_str("name")))->str), val_str(" in ")), runtime_at(runtime_var_get(env, "n"), val_str("filename"))), val_str(" on line ")), runtime_tostr(runtime_at(runtime_var_get(env, "n"), val_str("line"))))); printf("\n");
exit((int)(val_num(1))->num);
}
table_set(env, "args", runtime_at(runtime_var_get(env, "n"), val_str("body")));
table_set(env, "params", runtime_at(runtime_var_get(env, "fn"), val_str("body")));
table_set(env, "scope", val_dict());
runtime_key(runtime_var_get(env, "scope"), val_str("parent"), runtime_var_get(env, "env_"));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "args"))))) {
  table_set(env, "val", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "args"), runtime_var_get(env, "i")), runtime_var_get(env, "env_")}));
val_print(runtime_call(table_get(env, "table_set"), env, 3, (Value*[]){runtime_var_get(env, "scope"), runtime_at(runtime_at(runtime_var_get(env, "params"), runtime_var_get(env, "i")), val_str("name")), runtime_var_get(env, "val")})); printf("\n");
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
table_set(env, "res", runtime_call(table_get(env, "eval_block"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "fn"), val_str("left")), runtime_var_get(env, "scope")}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "res"), val_str("kind")), runtime_var_get(env, "VAL_OUT")))) {
  return val_out(runtime_at(runtime_var_get(env, "res"), val_str("inner")));
}
return val_out(runtime_var_get(env, "res"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_SPLIT")))) {
    table_set(env, "s", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "d", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right")), runtime_var_get(env, "env_")})}));
table_set(env, "res", val_dict());
runtime_key(runtime_var_get(env, "res"), val_str("kind"), runtime_var_get(env, "VAL_LIST"));
runtime_key(runtime_var_get(env, "res"), val_str("items"), runtime_split(runtime_at(runtime_var_get(env, "s"), val_str("str")), runtime_at(runtime_var_get(env, "d"), val_str("str"))));
return val_out(runtime_var_get(env, "res"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_TRIM")))) {
    table_set(env, "v", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){runtime_trim(runtime_at(runtime_var_get(env, "v"), val_str("str")))}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_REPLACE")))) {
    table_set(env, "v", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "target", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0)), runtime_var_get(env, "env_")})}));
table_set(env, "repl", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(1)), runtime_var_get(env, "env_")})}));
return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){val_none()}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_SAFE")))) {
    return val_out(runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_OUT")))) {
    return val_out(runtime_call(table_get(env, "val_out"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BREAK")))) {
    return val_out(runtime_var_get(env, "val_break"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_PRINT")))) {
    table_set(env, "v_print", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
runtime_put(val_str(""), runtime_binop(41, runtime_call(table_get(env, "val_print"), env, 1, (Value*[]){runtime_var_get(env, "v_print")}), val_str("\n")), 0);
val_print(runtime_var_get(env, "v_print")); printf("\n");
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BLOCK")))) {
    return val_out(runtime_call(table_get(env, "eval_block"), env, 2, (Value*[]){runtime_var_get(env, "n"), runtime_var_get(env, "env_")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_ASK")))) {
    return val_out(runtime_ask(runtime_var_get(env, "val_str")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_ASKFILE")))) {
    table_set(env, "path", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
table_set(env, "content", runtime_ask(runtime_at(runtime_var_get(env, "path"), val_str("str"))));
return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){runtime_var_get(env, "content")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_LEN")))) {
    table_set(env, "v", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "v"), val_str("kind")), runtime_var_get(env, "VAL_STR")))) {
  return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){runtime_len(runtime_at(runtime_var_get(env, "v"), val_str("str")))}));
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "v"), val_str("kind")), runtime_var_get(env, "VAL_LIST")))) {
  return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){runtime_len(runtime_at(runtime_var_get(env, "v"), val_str("items")))}));
}
return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){val_num(0)}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_LIST")))) {
    table_set(env, "v", val_dict());
runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_LIST"));
runtime_key(runtime_var_get(env, "v"), val_str("items"), val_list());
return val_out(runtime_var_get(env, "v"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_DICT")))) {
    table_set(env, "v", val_dict());
runtime_key(runtime_var_get(env, "v"), val_str("kind"), runtime_var_get(env, "VAL_DICT"));
runtime_key(runtime_var_get(env, "v"), val_str("fields"), val_dict());
return val_out(runtime_var_get(env, "v"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_KEY")))) {
    table_set(env, "target", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "key_val", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0)), runtime_var_get(env, "env_")})}));
table_set(env, "val", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(1)), runtime_var_get(env, "env_")})}));
if (val_truthy(runtime_binop(51, runtime_at(runtime_var_get(env, "key_val"), val_str("kind")), runtime_var_get(env, "VAL_STR")))) {
  val_print(val_str("Runtime error: dict key must be string")); printf("\n");
exit((int)(val_num(1))->num);
}
runtime_key(runtime_at(runtime_var_get(env, "target"), val_str("fields")), runtime_at(runtime_var_get(env, "key_val"), val_str("str")), runtime_var_get(env, "val"));
return val_out(runtime_var_get(env, "val"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_SET")))) {
    table_set(env, "target", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "idx", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0)), runtime_var_get(env, "env_")})}));
table_set(env, "val", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(1)), runtime_var_get(env, "env_")}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "target"), val_str("kind")), runtime_var_get(env, "VAL_LIST")))) {
  table_set(env, "items", runtime_at(runtime_var_get(env, "target"), val_str("items")));
runtime_set(runtime_var_get(env, "items"), runtime_at(runtime_var_get(env, "idx"), val_str("num")), runtime_var_get(env, "val"));
}
return val_out(runtime_var_get(env, "val"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_KEYS")))) {
    table_set(env, "d", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "fields", runtime_at(runtime_var_get(env, "d"), val_str("fields")));
table_set(env, "ks", runtime_keys(runtime_var_get(env, "fields")));
table_set(env, "res", val_dict());
runtime_key(runtime_var_get(env, "res"), val_str("kind"), runtime_var_get(env, "VAL_LIST"));
runtime_key(runtime_var_get(env, "res"), val_str("items"), runtime_var_get(env, "ks"));
return val_out(runtime_var_get(env, "res"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_AT")))) {
    table_set(env, "target", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "idx", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right")), runtime_var_get(env, "env_")})}));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "target"), val_str("kind")), runtime_var_get(env, "VAL_LIST")))) {
  return val_out(runtime_at(runtime_at(runtime_var_get(env, "target"), val_str("items")), runtime_at(runtime_var_get(env, "idx"), val_str("num"))));
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "target"), val_str("kind")), runtime_var_get(env, "VAL_DICT")))) {
  table_set(env, "res", runtime_at(runtime_at(runtime_var_get(env, "target"), val_str("fields")), runtime_at(runtime_var_get(env, "idx"), val_str("str"))));
if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "res")), val_str("none")))) {
  return val_out(runtime_var_get(env, "val_none"));
}
return val_out(runtime_var_get(env, "res"));
}
return val_out(runtime_var_get(env, "val_none"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_SLICE")))) {
    table_set(env, "target", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")})}));
table_set(env, "start", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0)), runtime_var_get(env, "env_")})}));
table_set(env, "end", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(1)), runtime_var_get(env, "env_")})}));
table_set(env, "s", runtime_at(runtime_var_get(env, "start"), val_str("num")));
table_set(env, "e", runtime_at(runtime_var_get(env, "end"), val_str("num")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "target"), val_str("kind")), runtime_var_get(env, "VAL_STR")))) {
  return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){runtime_slice(runtime_at(runtime_var_get(env, "target"), val_str("str")), runtime_var_get(env, "s"), runtime_var_get(env, "e"))}));
}
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "target"), val_str("kind")), runtime_var_get(env, "VAL_LIST")))) {
  table_set(env, "res", val_dict());
runtime_key(runtime_var_get(env, "res"), val_str("kind"), runtime_var_get(env, "VAL_LIST"));
runtime_key(runtime_var_get(env, "res"), val_str("items"), runtime_slice(runtime_at(runtime_var_get(env, "target"), val_str("items")), runtime_var_get(env, "s"), runtime_var_get(env, "e")));
return val_out(runtime_var_get(env, "res"));
}
return val_out(runtime_var_get(env, "val_none"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_PUT")))) {
    table_set(env, "path", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
table_set(env, "data", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right")), runtime_var_get(env, "env_")}));
runtime_put(runtime_at(runtime_var_get(env, "path"), val_str("str")), runtime_at(runtime_var_get(env, "data"), val_str("str")), 0);
return val_out(runtime_var_get(env, "val_none"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_SYS")))) {
    table_set(env, "cmd", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){runtime_sys(runtime_at(runtime_var_get(env, "cmd"), val_str("str")))}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_ENV")))) {
    table_set(env, "var", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){runtime_call(table_get(env, "env_"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "var"), val_str("str"))})}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_ARG")))) {
    return val_out(runtime_call(table_get(env, "table_get"), env, 2, (Value*[]){runtime_var_get(env, "env_"), val_str("arg")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_TIME")))) {
    return val_out(runtime_call(table_get(env, "val_num"), env, 1, (Value*[]){val_num((double)time(NULL))}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_TYPE")))) {
    table_set(env, "v", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
table_set(env, "tk", runtime_at(runtime_var_get(env, "v"), val_str("kind")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "tk"), runtime_var_get(env, "VAL_NUM")))) {
  return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){val_str("num")}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "tk"), runtime_var_get(env, "VAL_STR")))) {
  return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){val_str("str")}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "tk"), runtime_var_get(env, "VAL_LIST")))) {
  return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){val_str("list")}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "tk"), runtime_var_get(env, "VAL_DICT")))) {
  return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){val_str("dict")}));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "tk"), runtime_var_get(env, "VAL_BIT")))) {
  return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){val_str("bit")}));
}
return val_out(runtime_call(table_get(env, "val_str"), env, 1, (Value*[]){val_str("none")}));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_REF")))) {
    table_set(env, "target", runtime_call(table_get(env, "table_get"), env, 2, (Value*[]){runtime_var_get(env, "env_"), runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("left")), val_str("name"))}));
table_set(env, "res", val_dict());
runtime_key(runtime_var_get(env, "res"), val_str("kind"), runtime_var_get(env, "VAL_REF"));
runtime_key(runtime_var_get(env, "res"), val_str("target"), runtime_var_get(env, "target"));
return val_out(runtime_var_get(env, "res"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_EXIT")))) {
    table_set(env, "v", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left")), runtime_var_get(env, "env_")}));
exit((int)(runtime_at(runtime_var_get(env, "v"), val_str("num")))->num);
  }
  return val_out(runtime_var_get(env, "val_none"));
  return val_none();
}

static Value* io_func_init_indent_cache(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), val_num(32)))) {
    table_set(env, "s", val_str(""));
table_set(env, "j", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "j"), runtime_var_get(env, "i")))) {
  table_set(env, "s", runtime_binop(41, runtime_var_get(env, "s"), val_str("  ")));
table_set(env, "j", runtime_binop(41, runtime_var_get(env, "j"), val_num(1)));
}
runtime_key(runtime_var_get(env, "indent_cache"), runtime_tostr(runtime_var_get(env, "i")), runtime_var_get(env, "s"));
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  return val_none();
}

static Value* io_func_emit(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "line", argv[0]);
  table_set(env, "indent_str", runtime_at(runtime_var_get(env, "indent_cache"), runtime_tostr(runtime_var_get(env, "indent_lvl"))));
  if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "indent_str")), val_str("none")))) {
    table_set(env, "indent_str", val_str(""));
  }
  return val_out(runtime_set(runtime_var_get(env, "c_lines"), runtime_len(runtime_var_get(env, "c_lines")), runtime_binop(41, runtime_var_get(env, "indent_str"), runtime_var_get(env, "line"))));
  return val_none();
}

static Value* io_func_hoist_vars(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "node", argv[0]);
  if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "node")), val_str("none")))) {
    return val_out(val_num(0));
  }
  table_set(env, "k", runtime_at(runtime_var_get(env, "node"), val_str("kind")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_ASSIGN")))) {
    table_set(env, "left", runtime_at(runtime_var_get(env, "node"), val_str("left")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "left"), val_str("kind")), runtime_var_get(env, "ND_VAR")))) {
  runtime_key(runtime_var_get(env, "declared_vars"), runtime_at(runtime_var_get(env, "left"), val_str("name")), val_num(1));
}
val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "node"), val_str("right"))})); printf("\n");
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BINOP")))) {
      val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "node"), val_str("left"))})); printf("\n");
val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "node"), val_str("right"))})); printf("\n");
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_IF")))) {
        val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "node"), val_str("left"))})); printf("\n");
table_set(env, "body", runtime_at(runtime_var_get(env, "node"), val_str("body")));
if (val_truthy(runtime_binop(48, runtime_len(runtime_var_get(env, "body")), val_num(0)))) {
  val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "body"), val_num(0))})); printf("\n");
}
if (val_truthy(runtime_binop(51, runtime_type(runtime_at(runtime_var_get(env, "node"), val_str("right"))), val_str("none")))) {
  val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "node"), val_str("right"))})); printf("\n");
}
      } else {
        if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_WHILE")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_FOR")))) {
          val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "node"), val_str("left"))})); printf("\n");
table_set(env, "body", runtime_at(runtime_var_get(env, "node"), val_str("body")));
if (val_truthy(runtime_binop(48, runtime_len(runtime_var_get(env, "body")), val_num(0)))) {
  val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "body"), val_num(0))})); printf("\n");
}
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BLOCK")))) {
            table_set(env, "body", runtime_at(runtime_var_get(env, "node"), val_str("body")));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "body"))))) {
  val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "body"), runtime_var_get(env, "i"))})); printf("\n");
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
          } else {
            if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_OUT")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_PRINT")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_LEN")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_TONUM")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_TOSTR")))) {
              val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "node"), val_str("left"))})); printf("\n");
            }
          }
        }
      }
    }
  }
  return val_none();
}

static Value* io_func_emit_expr(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "n", argv[0]);
  if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "n")), val_str("none")))) {
    return val_out(val_str("val_none()"));
  }
  table_set(env, "k", runtime_at(runtime_var_get(env, "n"), val_str("kind")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_NUM")))) {
    return val_out(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_num("))->str, runtime_tostr(runtime_tostr(runtime_at(runtime_var_get(env, "n"), val_str("num"))))->str), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_STR")))) {
    return val_out(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_str(\""))->str, runtime_tostr(runtime_at(runtime_var_get(env, "n"), val_str("str")))->str), val_str("\")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_VAR")))) {
    return val_out(runtime_at(runtime_var_get(env, "n"), val_str("name")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_NOT")))) {
    return val_out(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_not("))->str, runtime_tostr(runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))}))->str), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_LEN")))) {
    return val_out(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_len("))->str, runtime_tostr(runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))}))->str), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_TONUM")))) {
    return val_out(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_tonum("))->str, runtime_tostr(runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))}))->str), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_TOSTR")))) {
    return val_out(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_tostr("))->str, runtime_tostr(runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))}))->str), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_TYPE")))) {
    return val_out(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_type("))->str, runtime_tostr(runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))}))->str), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_LIST")))) {
    return val_out(val_str("val_list()"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_DICT")))) {
    return val_out(val_str("val_dict()"));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_AT")))) {
    return val_out(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_at("))->str, runtime_tostr(runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))}))->str), val_str(", ")), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right"))})), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_SLICE")))) {
    table_set(env, "body", runtime_at(runtime_var_get(env, "n"), val_str("body")));
return val_out(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_slice("))->str, runtime_tostr(runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))}))->str), val_str(", ")), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "body"), val_num(0))})), val_str(", ")), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "body"), val_num(1))})), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BINOP")))) {
    table_set(env, "l", runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))}));
table_set(env, "r", runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right"))}));
table_set(env, "op", runtime_at(runtime_var_get(env, "n"), val_str("op")));
return val_out(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("runtime_binop("))->str, runtime_tostr(runtime_var_get(env, "l"))->str), val_str(", ")), runtime_tostr(runtime_var_get(env, "op"))), val_str(", ")), runtime_var_get(env, "r")), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_CALL")))) {
    table_set(env, "name", runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("left")), val_str("name")));
table_set(env, "args", runtime_at(runtime_var_get(env, "n"), val_str("body")));
table_set(env, "s", runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("val_call("))->str, runtime_tostr(runtime_var_get(env, "name"))->str), val_str(", ")), runtime_tostr(runtime_len(runtime_var_get(env, "args")))));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "args"))))) {
  table_set(env, "s", runtime_binop(41, runtime_binop(41, runtime_var_get(env, "s"), val_str(", ")), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "args"), runtime_var_get(env, "i"))})));
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
return val_out(runtime_binop(41, runtime_var_get(env, "s"), val_str(")")));
  }
  if (val_truthy(runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_binop(53, runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_IF")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_WHILE")), runtime_var_get(env, "k")), runtime_var_get(env, "ND_BLOCK")))) {
    table_set(env, "old_lines", runtime_var_get(env, "c_lines"));
table_set(env, "c_lines", val_list());
table_set(env, "IND_OLD", runtime_var_get(env, "indent_lvl"));
table_set(env, "indent_lvl", val_num(0));
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("({ Value* _res = val_none(); ")})); printf("\n");
table_set(env, "indent_lvl", val_num(1));
val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_var_get(env, "n")})); printf("\n");
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("_res; ")})); printf("\n");
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("})")})); printf("\n");
table_set(env, "block_str", val_str(""));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "c_lines"))))) {
  table_set(env, "block_str", runtime_binop(41, runtime_binop(41, runtime_var_get(env, "block_str"), runtime_at(runtime_var_get(env, "c_lines"), runtime_var_get(env, "i"))), val_str(" ")));
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
table_set(env, "c_lines", runtime_var_get(env, "old_lines"));
table_set(env, "indent_lvl", runtime_var_get(env, "IND_OLD"));
return val_out(runtime_var_get(env, "block_str"));
  }
  return val_out(val_str("val_none()"));
  return val_none();
}

static Value* io_func_emit_stmt(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "n", argv[0]);
  if (val_truthy(runtime_binop(40, runtime_type(runtime_var_get(env, "n")), val_str("none")))) {
    return val_out(runtime_var_get(env, "val_none"));
  }
  table_set(env, "k", runtime_at(runtime_var_get(env, "n"), val_str("kind")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BLOCK")))) {
    table_set(env, "body", runtime_at(runtime_var_get(env, "n"), val_str("body")));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "body"))))) {
  if (val_truthy(runtime_binop(48, runtime_binop(52, runtime_binop(40, runtime_var_get(env, "i"), runtime_binop(42, runtime_len(runtime_var_get(env, "body")), val_num(1))), runtime_var_get(env, "indent_lvl")), val_num(0)))) {
  val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("_res = ")}), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "body"), runtime_var_get(env, "i"))})), val_str(";"))); printf("\n");
} else {
  val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "body"), runtime_var_get(env, "i"))})); printf("\n");
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_ASSIGN")))) {
      table_set(env, "left", runtime_at(runtime_var_get(env, "n"), val_str("left")));
val_print(runtime_binop(41, runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "left"), val_str("name"))}), val_str(" = ")), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right"))})), val_str(";"))); printf("\n");
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_IF")))) {
        val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("if (val_truthy(")}), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))})), val_str(")) {"))); printf("\n");
table_set(env, "indent_lvl", runtime_binop(41, runtime_var_get(env, "indent_lvl"), val_num(1)));
val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0))})); printf("\n");
table_set(env, "indent_lvl", runtime_binop(42, runtime_var_get(env, "indent_lvl"), val_num(1)));
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("}")})); printf("\n");
if (val_truthy(runtime_binop(51, runtime_type(runtime_at(runtime_var_get(env, "n"), val_str("right"))), val_str("none")))) {
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("else {")})); printf("\n");
table_set(env, "indent_lvl", runtime_binop(41, runtime_var_get(env, "indent_lvl"), val_num(1)));
val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("right"))})); printf("\n");
table_set(env, "indent_lvl", runtime_binop(42, runtime_var_get(env, "indent_lvl"), val_num(1)));
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("}")})); printf("\n");
}
      } else {
        if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_WHILE")))) {
          val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("while (val_truthy(")}), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))})), val_str(")) {"))); printf("\n");
table_set(env, "indent_lvl", runtime_binop(41, runtime_var_get(env, "indent_lvl"), val_num(1)));
val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0))})); printf("\n");
table_set(env, "indent_lvl", runtime_binop(42, runtime_var_get(env, "indent_lvl"), val_num(1)));
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("}")})); printf("\n");
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_FOR")))) {
            val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("{ Value* _lst = val_unwrap(")}), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))})), val_str(");"))); printf("\n");
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("  for (int _i = 0; _i < _lst->item_count; _i++) {")})); printf("\n");
val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("    table_set(env, \"")}), runtime_at(runtime_var_get(env, "n"), val_str("name"))), val_str("\", _lst->items[_i]);"))); printf("\n");
val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_at(runtime_at(runtime_var_get(env, "n"), val_str("body")), val_num(0))})); printf("\n");
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("  }}")})); printf("\n");
          } else {
            if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_BREAK")))) {
              val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("break;")})); printf("\n");
            } else {
              if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_SKIP")))) {
                val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("continue;")})); printf("\n");
              } else {
                if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_PRINT")))) {
                  val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("val_print(")}), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))})), val_str("); printf(\"\\n\");"))); printf("\n");
                } else {
                  if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_OUT")))) {
                    val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("return ")}), runtime_call(table_get(env, "emit_expr"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))})), val_str(";"))); printf("\n");
                  } else {
                    if (val_truthy(runtime_binop(40, runtime_var_get(env, "k"), runtime_var_get(env, "ND_DO")))) {
                      table_set(env, "OLD_VARS", runtime_var_get(env, "declared_vars"));
table_set(env, "declared_vars", val_dict());
table_set(env, "params", runtime_at(runtime_var_get(env, "n"), val_str("body")));
table_set(env, "j", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "j"), runtime_len(runtime_var_get(env, "params"))))) {
  runtime_key(runtime_var_get(env, "declared_vars"), runtime_at(runtime_at(runtime_var_get(env, "params"), runtime_var_get(env, "j")), val_str("name")), val_num(1));
table_set(env, "j", runtime_binop(41, runtime_var_get(env, "j"), val_num(1)));
}
val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))})); printf("\n");
val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("Value* ")}), runtime_at(runtime_var_get(env, "n"), val_str("name"))), val_str("() {"))); printf("\n");
table_set(env, "indent_lvl", runtime_binop(41, runtime_var_get(env, "indent_lvl"), val_num(1)));
table_set(env, "vnames", runtime_keys(runtime_var_get(env, "declared_vars")));
table_set(env, "i", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "vnames"))))) {
  table_set(env, "is_param", val_num(0));
table_set(env, "p_check", val_num(0));
while (val_truthy(runtime_binop(47, runtime_var_get(env, "p_check"), runtime_len(runtime_var_get(env, "params"))))) {
  if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "vnames"), runtime_var_get(env, "i")), runtime_at(runtime_at(runtime_var_get(env, "params"), runtime_var_get(env, "p_check")), val_str("name"))))) {
  table_set(env, "is_param", val_num(1));
}
table_set(env, "p_check", runtime_binop(41, runtime_var_get(env, "p_check"), val_num(1)));
}
if (val_truthy(runtime_binop(40, runtime_var_get(env, "is_param"), val_num(0)))) {
  val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("Value* ")}), runtime_at(runtime_var_get(env, "vnames"), runtime_var_get(env, "i"))), val_str(" = val_none();"))); printf("\n");
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
}
val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_at(runtime_var_get(env, "n"), val_str("left"))})); printf("\n");
table_set(env, "indent_lvl", runtime_binop(42, runtime_var_get(env, "indent_lvl"), val_num(1)));
val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("}")})); printf("\n");
table_set(env, "declared_vars", runtime_var_get(env, "OLD_VARS"));
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return val_none();
}

static Value* io_func_compile_to_c(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "prog", argv[0]);
  table_set(env, "c_lines", val_list());
  val_print(runtime_var_get(env, "init_indent_cache")); printf("\n");
  table_set(env, "indent_lvl", val_num(0));
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("#include <stdio.h>")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("#include \"src/vii.h\"")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("/* Generated by Vii Codegen */")})); printf("\n");
  table_set(env, "body", runtime_at(runtime_var_get(env, "prog"), val_str("body")));
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "body"))))) {
    table_set(env, "n", runtime_at(runtime_var_get(env, "body"), runtime_var_get(env, "i")));
if (val_truthy(runtime_binop(40, runtime_at(runtime_var_get(env, "n"), val_str("kind")), runtime_var_get(env, "ND_DO")))) {
  val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_var_get(env, "n")})); printf("\n");
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("int main(int argc, char** argv) {")})); printf("\n");
  table_set(env, "indent_lvl", runtime_binop(41, runtime_var_get(env, "indent_lvl"), val_num(1)));
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("global_arena = arena_create(512*1024*1024);")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("Value* arg = val_list();")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("for (int i = 1; i < argc; i++) {")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("  if (arg->item_count >= arg->item_cap) val_list_grow(arg);")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("  arg->items[arg->item_count++] = val_str(argv[i]);")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("}")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("Table* env = table_new(NULL);")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("table_set(env, \"arg\", arg);")})); printf("\n");
  table_set(env, "declared_vars", val_dict());
  val_print(runtime_call(table_get(env, "hoist_vars"), env, 1, (Value*[]){runtime_var_get(env, "prog")})); printf("\n");
  table_set(env, "vnames", runtime_keys(runtime_var_get(env, "declared_vars")));
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "vnames"))))) {
    val_print(runtime_binop(41, runtime_binop(41, runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("Value* ")}), runtime_at(runtime_var_get(env, "vnames"), runtime_var_get(env, "i"))), val_str(" = val_none();"))); printf("\n");
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  val_print(runtime_call(table_get(env, "emit_stmt"), env, 1, (Value*[]){runtime_var_get(env, "prog")})); printf("\n");
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("return 0;")})); printf("\n");
  table_set(env, "indent_lvl", runtime_binop(42, runtime_var_get(env, "indent_lvl"), val_num(1)));
  val_print(runtime_call(table_get(env, "emit"), env, 1, (Value*[]){val_str("}")})); printf("\n");
  table_set(env, "res", val_str(""));
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_len(runtime_var_get(env, "c_lines"))))) {
    table_set(env, "res", runtime_binop(41, runtime_binop(41, runtime_var_get(env, "res"), runtime_at(runtime_var_get(env, "c_lines"), runtime_var_get(env, "i"))), val_str("\n")));
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  return val_out(runtime_var_get(env, "res"));
  return val_none();
}

static Value* io_func_compile_to_bin(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "prog", argv[0]);
  if(argc > 1) table_set(env, "out_name", argv[1]);
  if(argc > 2) table_set(env, "keep_c", argv[2]);
  table_set(env, "c_src", runtime_call(table_get(env, "compile_to_c"), env, 1, (Value*[]){runtime_var_get(env, "prog")}));
  table_set(env, "c_fname", runtime_binop(41, runtime_var_get(env, "out_name"), val_str(".c")));
  runtime_put(runtime_var_get(env, "c_fname"), runtime_var_get(env, "c_src"), 0);
  table_set(env, "cmd", runtime_binop(41, runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("gcc -O3 -Isrc "))->str, runtime_tostr(runtime_var_get(env, "c_fname"))->str), val_str(" -o ")), runtime_var_get(env, "out_name")));
  val_print(runtime_binop(41, runtime_str_concat(runtime_tostr(val_str("Compiling "))->str, runtime_tostr(runtime_var_get(env, "out_name"))->str), val_str("..."))); printf("\n");
  val_print(runtime_sys(runtime_var_get(env, "cmd"))); printf("\n");
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "keep_c"), val_num(0)))) {
    val_print(val_str("")); printf("\n");
  }
  return val_none();
}

static Value* io_func_arena_create(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "size", argv[0]);
  val_print(val_str("DEBUG: Arena initialized")); printf("\n");
  return val_out(val_num(0));
  return val_none();
}

static Value* io_func_print_usage(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  val_print(val_str("vii - a minimalist programming language")); printf("\n");
  val_print(val_str("Usage: vii <file.vii> [-o program] [args...]")); printf("\n");
  val_print(val_str("       vii --version")); printf("\n");
  val_print(val_str("       vii --help")); printf("\n");
  val_print(val_str("       vii --debug <file.vii>")); printf("\n");
  val_print(val_str("")); printf("\n");
  val_print(val_str("Options:")); printf("\n");
  val_print(val_str("  -o <name>      Compile to executable")); printf("\n");
  val_print(val_str("  -k, --keep     Keep transpiled .c source")); printf("\n");
  val_print(val_str("  -D <name>      Define compile-time flag for IF macros")); printf("\n");
  exit((int)(val_num(1))->num);
  return val_none();
}

static Value* io_func_add_define(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "name", argv[0]);
  return val_out(runtime_set(runtime_var_get(env, "CLI_DEFINES"), runtime_len(runtime_var_get(env, "CLI_DEFINES")), runtime_var_get(env, "name")));
  return val_none();
}

static Value* io_func_str_contains(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  if(argc > 0) table_set(env, "s", argv[0]);
  if(argc > 1) table_set(env, "sub", argv[1]);
  table_set(env, "len_s", runtime_len(runtime_var_get(env, "s")));
  table_set(env, "len_sub", runtime_len(runtime_var_get(env, "sub")));
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "len_sub"), val_num(0)))) {
    return val_out(val_num(1));
  }
  table_set(env, "i", val_num(0));
  table_set(env, "first_char", runtime_slice(runtime_var_get(env, "sub"), val_num(0), val_num(1)));
  while (val_truthy(runtime_binop(49, runtime_var_get(env, "i"), runtime_binop(42, runtime_var_get(env, "len_s"), runtime_var_get(env, "len_sub"))))) {
    if (val_truthy(runtime_binop(40, runtime_slice(runtime_var_get(env, "s"), runtime_var_get(env, "i"), runtime_binop(41, runtime_var_get(env, "i"), val_num(1))), runtime_var_get(env, "first_char")))) {
  if (val_truthy(runtime_binop(40, runtime_slice(runtime_var_get(env, "s"), runtime_var_get(env, "i"), runtime_binop(41, runtime_var_get(env, "i"), runtime_var_get(env, "len_sub"))), runtime_var_get(env, "sub")))) {
  return val_out(val_num(1));
}
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  return val_out(val_num(0));
  return val_none();
}

static Value* io_func_main(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  table_set(env, "global_arena", runtime_call(table_get(env, "arena_create"), env, 1, (Value*[]){runtime_binop(43, val_num(512 * 1024), val_num(1024))}));
  table_set(env, "input_path", val_str("none"));
  table_set(env, "output_name", val_str("none"));
  table_set(env, "debug_ast", val_num(0));
  table_set(env, "boot_debug", val_num(0));
  table_set(env, "keep_c", val_num(0));
  table_set(env, "argc", runtime_len(io_args));
  if (val_truthy(runtime_binop(47, runtime_var_get(env, "argc"), val_num(1)))) {
    val_print(runtime_var_get(env, "print_usage")); printf("\n");
  }
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_var_get(env, "argc")))) {
    table_set(env, "current_arg", runtime_at(io_args, runtime_var_get(env, "i")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--debug")))) {
  table_set(env, "debug_ast", val_num(1));
} else {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--bootdebug")))) {
    table_set(env, "boot_debug", val_num(1));
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("-o")))) {
      table_set(env, "next_i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
if (val_truthy(runtime_binop(47, runtime_var_get(env, "next_i"), runtime_var_get(env, "argc")))) {
  table_set(env, "output_name", runtime_at(io_args, runtime_var_get(env, "next_i")));
table_set(env, "i", runtime_var_get(env, "next_i"));
} else {
  val_print(runtime_binop(41, runtime_binop(41, runtime_var_get(env, "color_red"), val_str("Error: -o requires a filename")), runtime_var_get(env, "color_rst"))); printf("\n");
exit((int)(val_num(1))->num);
}
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("-k")))) {
        table_set(env, "keep_c", val_num(1));
      } else {
        if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--keep")))) {
          table_set(env, "keep_c", val_num(1));
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("-D")))) {
            table_set(env, "next_i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
if (val_truthy(runtime_binop(47, runtime_var_get(env, "next_i"), runtime_var_get(env, "argc")))) {
  val_print(runtime_at(runtime_call(table_get(env, "add_define"), env, 1, (Value*[]){io_args}), runtime_var_get(env, "next_i"))); printf("\n");
table_set(env, "i", runtime_var_get(env, "next_i"));
} else {
  val_print(runtime_binop(41, runtime_binop(41, runtime_var_get(env, "color_red"), val_str("Error: -D requires a name")), runtime_var_get(env, "color_rst"))); printf("\n");
exit((int)(val_num(1))->num);
}
          } else {
            if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--define")))) {
              table_set(env, "next_i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
if (val_truthy(runtime_binop(47, runtime_var_get(env, "next_i"), runtime_var_get(env, "argc")))) {
  val_print(runtime_at(runtime_call(table_get(env, "add_define"), env, 1, (Value*[]){io_args}), runtime_var_get(env, "next_i"))); printf("\n");
table_set(env, "i", runtime_var_get(env, "next_i"));
} else {
  val_print(runtime_binop(41, runtime_binop(41, runtime_var_get(env, "color_red"), val_str("Error: -D requires a name")), runtime_var_get(env, "color_rst"))); printf("\n");
exit((int)(val_num(1))->num);
}
            } else {
              if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--version")))) {
                val_print(val_str("vii 1.2.6")); printf("\n");
exit((int)(val_num(0))->num);
              } else {
                if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--help")))) {
                  val_print(runtime_var_get(env, "print_usage")); printf("\n");
                } else {
                  if (val_truthy(runtime_call(table_get(env, "str_contains"), env, 2, (Value*[]){runtime_var_get(env, "current_arg"), val_str("main.vii")}))) {
                    val_print(val_str("")); printf("\n");
                  } else {
                    if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("vii")))) {
                      val_print(val_str("")); printf("\n");
                    } else {
                      if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("vii.exe")))) {
                        val_print(val_str("")); printf("\n");
                      } else {
                        table_set(env, "first_char", runtime_at(runtime_var_get(env, "current_arg"), val_num(0)));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "first_char"), val_str("-")))) {
  val_print(runtime_str_concat(runtime_tostr(val_str("Warning: Ignoring unknown flag: "))->str, runtime_tostr(runtime_var_get(env, "current_arg"))->str)); printf("\n");
} else {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "input_path"), val_str("none")))) {
    table_set(env, "input_path", runtime_var_get(env, "current_arg"));
  } else {
    val_print(runtime_str_concat(runtime_tostr(val_str("Warning: Ignoring unknown positional argument: "))->str, runtime_tostr(runtime_var_get(env, "current_arg"))->str)); printf("\n");
  }
}
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  }
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "input_path"), val_str("none")))) {
    val_print(runtime_var_get(env, "print_usage")); printf("\n");
  }
  if (val_truthy(runtime_binop(51, runtime_var_get(env, "output_name"), val_str("none")))) {
    table_set(env, "has_exe", runtime_call(table_get(env, "str_contains"), env, 2, (Value*[]){runtime_var_get(env, "output_name"), val_str(".exe")}));
table_set(env, "has_EXE", runtime_call(table_get(env, "str_contains"), env, 2, (Value*[]){runtime_var_get(env, "output_name"), val_str(".EXE")}));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "has_exe"), val_num(0)))) {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "has_EXE"), val_num(0)))) {
  table_set(env, "output_name", runtime_binop(41, runtime_var_get(env, "output_name"), val_str(".exe")));
}
}
  }
  table_set(env, "cli_args", val_list());
  table_set(env, "i", val_num(0));
  while (val_truthy(runtime_binop(47, runtime_var_get(env, "i"), runtime_var_get(env, "argc")))) {
    table_set(env, "current_arg", runtime_at(io_args, runtime_var_get(env, "i")));
if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--debug")))) {
  table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
} else {
  if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--bootdebug")))) {
    table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
  } else {
    if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("-o")))) {
      table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(2)));
    } else {
      if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("-k")))) {
        table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
      } else {
        if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--keep")))) {
          table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
        } else {
          if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("-D")))) {
            table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(2)));
          } else {
            if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("--define")))) {
              table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(2)));
            } else {
              if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), runtime_var_get(env, "input_path")))) {
                table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
              } else {
                if (val_truthy(runtime_call(table_get(env, "str_contains"), env, 2, (Value*[]){runtime_var_get(env, "current_arg"), val_str("main.vii")}))) {
                  table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
                } else {
                  if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("vii")))) {
                    table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
                  } else {
                    if (val_truthy(runtime_binop(40, runtime_var_get(env, "current_arg"), val_str("vii.exe")))) {
                      table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
                    } else {
                      runtime_set(runtime_var_get(env, "cli_args"), runtime_len(runtime_var_get(env, "cli_args")), runtime_var_get(env, "current_arg"));
table_set(env, "i", runtime_binop(41, runtime_var_get(env, "i"), val_num(1)));
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
  }
  table_set(env, "src_content", runtime_ask(runtime_var_get(env, "input_path")));
  table_set(env, "lexer", val_dict());
  runtime_key(runtime_var_get(env, "lexer"), val_str("src"), runtime_var_get(env, "src_content"));
  runtime_key(runtime_var_get(env, "lexer"), val_str("pos"), val_num(0));
  runtime_key(runtime_var_get(env, "lexer"), val_str("line"), val_num(1));
  runtime_key(runtime_var_get(env, "lexer"), val_str("filename"), runtime_var_get(env, "input_path"));
  runtime_key(runtime_var_get(env, "lexer"), val_str("boot_debug"), runtime_var_get(env, "boot_debug"));
  runtime_key(runtime_var_get(env, "lexer"), val_str("arena"), runtime_var_get(env, "global_arena"));
  table_set(env, "pasted_files", val_list());
  runtime_set(runtime_var_get(env, "pasted_files"), val_num(0), runtime_var_get(env, "input_path"));
  table_set(env, "tokens", runtime_call(table_get(env, "lex"), env, 1, (Value*[]){runtime_var_get(env, "lexer")}));
  table_set(env, "parser", val_dict());
  runtime_key(runtime_var_get(env, "parser"), val_str("tokens"), runtime_var_get(env, "tokens"));
  runtime_key(runtime_var_get(env, "parser"), val_str("pos"), val_num(0));
  runtime_key(runtime_var_get(env, "parser"), val_str("src"), runtime_var_get(env, "src_content"));
  runtime_key(runtime_var_get(env, "parser"), val_str("filename"), runtime_var_get(env, "input_path"));
  runtime_key(runtime_var_get(env, "parser"), val_str("arena"), runtime_var_get(env, "global_arena"));
  runtime_key(runtime_var_get(env, "parser"), val_str("defines"), runtime_var_get(env, "CLI_DEFINES"));
  runtime_key(runtime_var_get(env, "parser"), val_str("boot_debug"), runtime_var_get(env, "boot_debug"));
  runtime_key(runtime_var_get(env, "parser"), val_str("in_func"), val_num(0));
  runtime_key(runtime_var_get(env, "parser"), val_str("current_func"), val_str("none"));
  runtime_key(runtime_var_get(env, "parser"), val_str("constants"), val_list());
  runtime_key(runtime_var_get(env, "parser"), val_str("pasted_files"), runtime_var_get(env, "pasted_files"));
  runtime_key(runtime_var_get(env, "parser"), val_str("once_registry"), val_dict());
  table_set(env, "prog", runtime_call(table_get(env, "parse_program"), env, 1, (Value*[]){runtime_var_get(env, "parser")}));
  if (val_truthy(runtime_var_get(env, "debug_ast"))) {
    val_print(val_str("AST debug output not yet implemented in Vii main.vii")); printf("\n");
val_print(val_str("AST would be dumped to debug_ast.json")); printf("\n");
  }
  if (val_truthy(runtime_binop(51, runtime_var_get(env, "output_name"), val_str("none")))) {
    val_print(runtime_call(table_get(env, "compile_to_bin"), env, 3, (Value*[]){runtime_var_get(env, "prog"), runtime_var_get(env, "output_name"), runtime_var_get(env, "keep_c")})); printf("\n");
exit((int)(val_num(0))->num);
  }
  table_set(env, "global", val_dict());
  runtime_key(runtime_var_get(env, "global"), val_str("arg"), runtime_var_get(env, "cli_args"));
  runtime_key(runtime_var_get(env, "global"), val_str("BOOT_DEBUG"), runtime_var_get(env, "boot_debug"));
  table_set(env, "global_table", runtime_call(table_get(env, "table_new"), env, 1, (Value*[]){runtime_var_get(env, "global")}));
  table_set(env, "eval_result", runtime_call(table_get(env, "eval"), env, 2, (Value*[]){runtime_var_get(env, "prog"), runtime_var_get(env, "global_table")}));
  table_set(env, "res", runtime_call(table_get(env, "val_unwrap"), env, 1, (Value*[]){runtime_var_get(env, "eval_result")}));
  table_set(env, "res_kind", runtime_at(runtime_var_get(env, "res"), val_str("kind")));
  table_set(env, "is_none", runtime_binop(40, runtime_var_get(env, "res_kind"), runtime_var_get(env, "VAL_NONE")));
  table_set(env, "is_break", runtime_binop(40, runtime_var_get(env, "res_kind"), runtime_var_get(env, "VAL_BREAK")));
  if (val_truthy(runtime_var_get(env, "is_none"))) {
    val_print(val_str("")); printf("\n");
  } else {
    if (val_truthy(runtime_var_get(env, "is_break"))) {
      val_print(val_str("")); printf("\n");
    } else {
      table_set(env, "printed", runtime_call(table_get(env, "val_print"), env, 1, (Value*[]){runtime_var_get(env, "res")}));
table_set(env, "msg", runtime_binop(41, runtime_var_get(env, "printed"), val_str("\n")));
runtime_put(val_str(""), runtime_var_get(env, "msg"), 0);
    }
  }
  return val_out(runtime_var_get(env, "res"));
  return val_none();
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
  table_set(env, "enable_ansi_colors", val_func(io_func_enable_ansi_colors));
  table_set(env, "read_file", val_func(io_func_read_file));
  table_set(env, "report_error", val_func(io_func_report_error));
  table_set(env, "is_digit", val_func(io_func_is_digit));
  table_set(env, "is_alpha", val_func(io_func_is_alpha));
  table_set(env, "is_alnum", val_func(io_func_is_alnum));
  table_set(env, "get_kw_kind", val_func(io_func_get_kw_kind));
  table_set(env, "lex", val_func(io_func_lex));
  table_set(env, "nd_new", val_func(io_func_nd_new));
  table_set(env, "nd_push", val_func(io_func_nd_push));
  table_set(env, "is_all_caps", val_func(io_func_is_all_caps));
  table_set(env, "track_constant", val_func(io_func_track_constant));
  table_set(env, "peek", val_func(io_func_peek));
  table_set(env, "advance", val_func(io_func_advance));
  table_set(env, "expect", val_func(io_func_expect));
  table_set(env, "skip_newlines", val_func(io_func_skip_newlines));
  table_set(env, "infer_node_type", val_func(io_func_infer_node_type));
  table_set(env, "parse_primary", val_func(io_func_parse_primary));
  table_set(env, "parse_postfix", val_func(io_func_parse_postfix));
  table_set(env, "parse_expr", val_func(io_func_parse_expr));
  table_set(env, "skip_block_body", val_func(io_func_skip_block_body));
  table_set(env, "resolve_condition", val_func(io_func_resolve_condition));
  table_set(env, "parse_when_stmt", val_func(io_func_parse_when_stmt));
  table_set(env, "parse_stmt", val_func(io_func_parse_stmt));
  table_set(env, "parse_block", val_func(io_func_parse_block));
  table_set(env, "parse_program", val_func(io_func_parse_program));
  table_set(env, "val_num", val_func(io_func_val_num));
  table_set(env, "val_str", val_func(io_func_val_str));
  table_set(env, "val_none", val_func(io_func_val_none));
  table_set(env, "val_break", val_func(io_func_val_break));
  table_set(env, "val_skip", val_func(io_func_val_skip));
  table_set(env, "val_out", val_func(io_func_val_out));
  table_set(env, "val_bit", val_func(io_func_val_bit));
  table_set(env, "val_print", val_func(io_func_val_print));
  table_set(env, "val_truthy", val_func(io_func_val_truthy));
  table_set(env, "val_unwrap", val_func(io_func_val_unwrap));
  table_set(env, "table_get", val_func(io_func_table_get));
  table_set(env, "table_set", val_func(io_func_table_set));
  table_set(env, "eval_block", val_func(io_func_eval_block));
  table_set(env, "eval", val_func(io_func_eval));
  table_set(env, "init_indent_cache", val_func(io_func_init_indent_cache));
  table_set(env, "emit", val_func(io_func_emit));
  table_set(env, "hoist_vars", val_func(io_func_hoist_vars));
  table_set(env, "emit_expr", val_func(io_func_emit_expr));
  table_set(env, "emit_stmt", val_func(io_func_emit_stmt));
  table_set(env, "compile_to_c", val_func(io_func_compile_to_c));
  table_set(env, "compile_to_bin", val_func(io_func_compile_to_bin));
  table_set(env, "arena_create", val_func(io_func_arena_create));
  table_set(env, "print_usage", val_func(io_func_print_usage));
  table_set(env, "add_define", val_func(io_func_add_define));
  table_set(env, "str_contains", val_func(io_func_str_contains));
  table_set(env, "main", val_func(io_func_main));
  Value *program_result = io_func_main(env, 0, NULL);
  if (program_result->kind != VAL_NONE && program_result->kind != VAL_BREAK) {
    val_print(program_result);
    printf("\n");
  }
  return 0;
}
