#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum { VAL_NUM, VAL_STR, VAL_LIST, VAL_FUNC, VAL_NONE } ValKind;
struct Table;
struct Value;
typedef struct Value* (*IoFunc)(struct Table*, int, struct Value**);
typedef struct Value { ValKind kind; double num; char *str; struct Value **items; int item_count; int item_cap; IoFunc func; } Value;
typedef struct Entry { char *key; Value *val; struct Entry *next; } Entry;
typedef struct Table { Entry *buckets[256]; struct Table *parent; } Table;

static Value *val_num(double n) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NUM; v->num = n; return v; }
static Value *val_str(const char *s) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_STR; v->str = strdup(s); return v; }
static Value *val_list() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_LIST; v->item_cap = 8; v->items = calloc(8, sizeof(Value*)); return v; }
static Value *val_func(IoFunc f) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_FUNC; v->func = f; return v; }
static Value *val_none() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NONE; return v; }

static unsigned hash(const char *s) { unsigned h = 0; while(*s) h = h * 31 + (unsigned char)*s++; return h % 256; }
static Table* table_new(Table *p) { Table *t = calloc(1, sizeof(Table)); t->parent = p; return t; }
static Value* table_get(Table *t, const char *k) { for(Table *cur=t;cur;cur=cur->parent){ unsigned h=hash(k); for(Entry *e=cur->buckets[h];e;e=e->next) if(!strcmp(e->key,k)) return e->val; } return val_none(); }
static void table_set(Table *t, const char *k, Value *v) { unsigned h = hash(k); for(Entry *e=t->buckets[h];e;e=e->next) if(!strcmp(e->key,k)){e->val=v;return;} Entry *e=malloc(sizeof(Entry)); e->key=strdup(k); e->val=v; e->next=t->buckets[h]; t->buckets[h]=e; }

static bool val_truthy(Value *v) { if(!v) return false; if(v->kind==VAL_NUM) return v->num != 0; if(v->kind==VAL_STR) return v->str[0]!='\0'; return v->item_count > 0; }
static void val_print(Value *v) { if(!v) return; if(v->kind==VAL_NUM) printf(v->num==(long long)v->num?"%lld":"%g",(long long)v->num); else if(v->kind==VAL_STR) printf("%s",v->str); else if(v->kind==VAL_LIST){ printf("["); for(int i=0;i<v->item_count;i++){ if(i)printf(", "); val_print(v->items[i]); } printf("]"); } }
static Value* runtime_binop(int op, Value* l, Value* r) {
  if(op==21 && (l->kind==VAL_STR||r->kind==VAL_STR)){ char b[1024]; sprintf(b, "%s%s", l->kind==VAL_STR?l->str:"", r->kind==VAL_STR?r->str:""); return val_str(b); }
  if(op==20){ if(l->kind==VAL_STR&&r->kind==VAL_STR) return val_num(strcmp(l->str,r->str)==0?1:0); }
  double a=l->num, b=r->num; switch(op){
    case 21: return val_num(a+b); case 22: return val_num(a-b); case 23: return val_num(a*b); case 24: return val_num(b?(int)a/(int)b:0); case 25: return val_num(b?(int)a%(int)b:0);
    case 26: return val_num(a<b?1:0); case 27: return val_num(a>b?1:0); case 20: return val_num(a==b?1:0); default: return val_none();
  }
}
static Value* runtime_call(Value* f, Table* e, int c, Value** v) { if(f->kind==VAL_FUNC) return f->func(e, c, v); return val_none(); }
static Value* io_args;
static Value* runtime_var_get(Table* e, const char* k) { Value* v=table_get(e,k); if(v->kind==VAL_FUNC) return v->func(e,0,NULL); return v; }
static Value* runtime_at(Value* l, Value* r) { int i=(int)r->num; if(l->kind==VAL_STR){ if(i<0||i>=strlen(l->str)) return val_none(); char b[2]={l->str[i],0}; return val_str(b); } if(l->kind==VAL_LIST){ if(i<0||i>=l->item_count) return val_none(); return l->items[i]; } return val_none(); }
static void runtime_set(Value* l, Value* i, Value* v) { if(l->kind!=VAL_LIST) return; int idx=(int)i->num; if(idx==l->item_count){ if(l->item_count>=l->item_cap){ l->item_cap*=2; l->items=realloc(l->items,l->item_cap*sizeof(Value*)); } l->items[l->item_count++]=v; } else if(idx>=0 && idx<l->item_count) l->items[idx]=v; }
static Value* runtime_ask(Value* p) { if(p&&p->kind==VAL_STR){ FILE* f=fopen(p->str,"rb"); if(!f) return val_str(""); fseek(f,0,SEEK_END); long l=ftell(f); fseek(f,0,SEEK_SET); char* b=malloc(l+1); fread(b,1,l,f); b[l]=0; fclose(f); Value* v=val_str(b); free(b); return v; } char buf[1024]; if(!fgets(buf,1024,stdin)) return val_str(""); buf[strcspn(buf,"\n")]=0; return val_str(buf); }
static Value* runtime_len(Value* v) { if(v->kind==VAL_STR) return val_num(strlen(v->str)); if(v->kind==VAL_LIST) return val_num(v->item_count); return val_num(0); }
static Value* runtime_ord(Value* v) { if(v->kind==VAL_STR&&v->str[0]) return val_num((unsigned char)v->str[0]); return val_num(0); }
static Value* runtime_chr(Value* v) { char b[2]={v->num,0}; return val_str(b); }
static Value* runtime_tonum(Value* v) { if(v->kind==VAL_STR){ char*e; double d=strtod(v->str,&e); return val_num(*e==0&&e!=v->str?d:0); } if(v->kind==VAL_NUM) return v; return val_num(0); }
static Value* runtime_put(Value* p, Value* d) { if(p->kind!=VAL_STR) return val_none(); FILE* f=fopen(p->str,"w"); if(!f) return val_none(); if(d->kind==VAL_STR) fprintf(f,"%s",d->str); else { if(d->kind==VAL_NUM) fprintf(f,d->num==(long long)d->num?"%lld":"%g",(long long)d->num); } fclose(f); return val_none(); }
static Value* runtime_tostr(Value* v) { char b[64]; if(v->kind==VAL_NUM){ if(v->num==(long long)v->num) snprintf(b,64,"%%lld",(long long)v->num); else snprintf(b,64,"%%g",v->num); return val_str(b); } if(v->kind==VAL_STR) return v; return val_str(""); }
static Value* io_func_logger(Table* parent, int argc, Value** argv) {
  Table* env = table_new(parent);
  table_set(env, "cmd", runtime_at(io_args, val_num(0)));
if (val_truthy(runtime_binop(20, runtime_var_get(env, "cmd"), val_str("init")))) {
  runtime_put(val_str("logfile.log"), val_str(""));
val_print(val_str("Initialized logfile.log")); printf("\n");
} else {
  if (val_truthy(runtime_binop(20, runtime_var_get(env, "cmd"), val_str("log")))) {
    table_set(env, "message", runtime_at(io_args, val_num(1)));
table_set(env, "old", runtime_ask(val_str("logfile.log")));
table_set(env, "new", runtime_binop(21, runtime_binop(21, runtime_var_get(env, "old"), runtime_var_get(env, "message")), val_str("\n")));
runtime_put(val_str("logfile.log"), runtime_var_get(env, "new"));
val_print(runtime_binop(21, val_str("Logged: "), runtime_var_get(env, "message"))); printf("\n");
  } else {
    val_print(val_str("unknown command")); printf("\n");
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
  table_set(env, "logger", val_func(io_func_logger));
val_print(runtime_var_get(env, "logger")); printf("\n");
  return 0;
}
