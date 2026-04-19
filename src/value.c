#include "io.h"

Value *val_num(double n) {
    Value *v = calloc(1, sizeof(Value));
    v->kind = VAL_NUM; v->num = n;
    return v;
}

Value *val_str(const char *s) {
    Value *v = calloc(1, sizeof(Value));
    v->kind = VAL_STR; v->str = strdup(s);
    return v;
}

Value *val_list(void) {
    Value *v = calloc(1, sizeof(Value));
    v->kind = VAL_LIST; v->item_cap = 8;
    v->items = calloc(v->item_cap, sizeof(Value*));
    return v;
}

Value *val_none(void) {
    Value *v = calloc(1, sizeof(Value));
    v->kind = VAL_NONE;
    return v;
}

bool val_truthy(Value *v) {
    if (!v) return false;
    switch (v->kind) {
        case VAL_NUM:  return v->num != 0;
        case VAL_STR:  return v->str[0] != '\0';
        case VAL_LIST: return v->item_count > 0;
        default:       return false;
    }
}

void val_print_to(Value *v, FILE *f) {
    if (!v) return;
    switch (v->kind) {
        case VAL_NUM:
            if (v->num == (long long)v->num)
                fprintf(f, "%lld", (long long)v->num);
            else
                fprintf(f, "%g", v->num);
            break;
        case VAL_STR:  fprintf(f, "%s", v->str); break;
        case VAL_LIST:
            fputc('[', f);
            for (int i = 0; i < v->item_count; i++) {
                if (i) fprintf(f, ", ");
                val_print_to(v->items[i], f);
            }
            fputc(']', f);
            break;
        case VAL_NONE: break;
    }
}

void val_print(Value *v) {
    val_print_to(v, stdout);
}
