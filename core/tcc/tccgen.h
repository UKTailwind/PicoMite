/*
 *  TCC - Tiny C Compiler
 *
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 */

/* Internal declarations of the code generator, which is split into
     tccgen.c   - core: symbols, value stack, code generation, casts
     tcctype.c  - types: sizes, compatibility, attributes, struct layout, declarators
     tccstruct.c - enum/struct/union declarations (struct_decl)
     tccexpr.c  - expressions
     tccstmt.c  - statements, initializers, declarations
   With ONE_SOURCE all of them are included into one translation unit (libtcc.c)
   in this order and everything below is static, as it was in the single tccgen.c. */

#ifndef TCCGEN_H
#define TCCGEN_H

#define vstack (_vstack + 1)
#define unevalmask 0xffff /* unevaluated subexpression */
#define NODATA_WANTED (nocode_wanted > 0) /* no static data output wanted either */
#define STATIC_DATA_WANTED (nocode_wanted & 0xC0000000) /* only static data output */

/* automagical code suppression, see gind/gjmp_acs in tccgen.c */
#define CODE_OFF() (nocode_wanted |= 0x20000000)
#define CODE_ON() (nocode_wanted &= ~0x20000000)

#if PTR_SIZE == 4
#define VT_SIZE_T (VT_INT | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_INT
#elif LONG_SIZE == 4
#define VT_SIZE_T (VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_LLONG
#else
#define VT_SIZE_T (VT_LONG | VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T (VT_LONG | VT_LLONG)
#endif

/* returns true for two-word types */
#define USING_TWO_WORDS(t) (R2_RET(t) != VT_CONST)

/* compiling intel long double natively */
#if (defined __i386__ || defined __x86_64__) \
    && (defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64)
# define TCC_IS_NATIVE_387
#endif

#define EXPR_CONST 1
#define EXPR_ANY   2
#define MAX_TEMP_LOCAL_VARIABLE_NUMBER 8
#define precedence_parser

struct switch_t {
    struct case_t {
        int64_t v1, v2;
	int sym;
    } **p; int n; /* list of case ranges */
    int def_sym; /* default symbol */
    int *bsym;
    struct scope *scope;
    struct switch_t *prev;
    SValue sv;
};

struct temp_local_variable {
	int location; //offset on stack. Svalue.c.i
	short size;
	short align;
};

struct scope {
    struct scope *prev;
    struct { int loc, num; } vla;
    struct { Sym *s; int n; } cl;
    int *bsym, *csym;
    Sym *lstk, *llstk;
};

#if !ONE_SOURCE
extern Sym *all_cleanups, *pending_gotos;
extern int local_scope;
extern int in_sizeof;
extern int in_generic;
extern SValue _vstack[1 + VSTACK_SIZE];
extern int last_line_num, new_file, func_ind;
extern struct switch_t *cur_switch;
extern struct temp_local_variable arr_temp_local_vars[MAX_TEMP_LOCAL_VARIABLE_NUMBER];
extern short nb_temp_local_vars;
extern struct scope *cur_scope, *loop_scope, *root_scope;
#endif

ST_FUNC void PUT_R_RET(SValue *sv, int t);
ST_FUNC int R2_RET(int t);
ST_FUNC int RC_RET(int t);
ST_FUNC int RC_TYPE(int t);
ST_FUNC int adjust_bf(SValue *sv, int bit_pos, int bit_size);
ST_FUNC int asm_label_instr(void);
ST_FUNC void block(int is_expr);
ST_FUNC int btype_size(int bt);
ST_FUNC void cast_error(CType *st, CType *dt);
ST_FUNC void clear_temp_local_var_list();
ST_FUNC int compare_types(CType *type1, CType *type2, int unqualified);
ST_INLN void convert_parameter_type(CType *pt);
ST_FUNC void decl(int l);
ST_FUNC int decl0(int l, int is_for_loop_init, Sym *func_sym);
ST_FUNC void decl_initializer(CType *type, Section *sec, unsigned long c, int flags);
ST_FUNC void decl_initializer_alloc(CType *type, AttributeDef *ad, int r, int has_init, int v, int scope);
ST_FUNC void expr_const1(void);
ST_INLN int64_t expr_const64(void);
ST_FUNC void expr_eq(void);
ST_FUNC void expr_type(CType *type, void (*expr_fn)(void));
ST_FUNC Sym *external_sym(int v, CType *type, int r, AttributeDef *ad);
ST_FUNC Sym * find_field (CType *type, int v, int *cumofs);
ST_FUNC void force_charshort_cast(void);
ST_FUNC void free_inline_functions(TCCState *s);
ST_FUNC void gen_assign_cast(CType *dt);
ST_FUNC void gen_cast(CType *type);
ST_FUNC void gen_cast_s(int t);
ST_FUNC void gen_inline_functions(TCCState *s);
ST_FUNC void gen_test_zero(int op);
ST_FUNC int get_temp_local_var(int size,int align);
ST_FUNC void gfunc_param_typed(Sym *func, Sym *arg);
ST_FUNC int gind(void);
ST_FUNC int gjmp_acs(int t);
ST_FUNC void gjmp_addr_acs(int t);
ST_FUNC void gv_dup(void);
ST_FUNC int gvtst(int inv, int t);
ST_FUNC void gvtst_set(int inv, int t);
ST_FUNC void init_prec(void);
ST_FUNC void init_putv(CType *type, Section *sec, unsigned long c);
ST_FUNC int is_compatible_types(CType *type1, CType *type2);
ST_FUNC int is_compatible_unqualified_types(CType *type1, CType *type2);
ST_INLN int is_integer_btype(int bt);
ST_INLN int is_null_pointer(SValue *p);
ST_FUNC void merge_attr(AttributeDef *ad, AttributeDef *ad1);
ST_FUNC void merge_funcattr(struct FuncAttr *fa, struct FuncAttr *fa1);
ST_FUNC void merge_symattr(struct SymAttr *sa, struct SymAttr *sa1);
ST_FUNC void move_reg(int r, int s, int t);
ST_FUNC void parse_attribute(AttributeDef *ad);
ST_FUNC int parse_btype(CType *type, AttributeDef *ad);
ST_FUNC void parse_expr_type(CType *type);
ST_FUNC void parse_type(CType *type);
ST_FUNC void patch_storage(Sym *sym, AttributeDef *ad, CType *type);
ST_FUNC int pointed_size(CType *type);
ST_INLN CType *pointed_type(CType *type);
ST_FUNC void pop_local_syms(Sym **ptop, Sym *b, int keep, int ellipsis);
ST_FUNC void skip_or_save_block(TokenString **str);
ST_FUNC void store_packed_bf(int bit_pos, int bit_size);
ST_FUNC void struct_decl(CType *type, int u);
ST_FUNC void struct_layout(CType *type, AttributeDef *ad);
ST_FUNC CType *type_decl(CType *type, AttributeDef *ad, int *v, int td);
ST_FUNC void NORETURN type_description_error(CType *type, const char *fmt, int limit);
ST_FUNC void type_incompatibility_error(CType* st, CType* dt, const char* fmt);
ST_FUNC void type_incompatibility_warning(CType* st, CType* dt, const char* fmt);
ST_FUNC void vdup(void);
ST_FUNC void vla_runtime_type_size(CType *type, int *a);
ST_FUNC void vpush(CType *type);
ST_FUNC void vpush64(int ty, unsigned long long v);
ST_FUNC void vpush_ref(CType *type, Section *sec, unsigned long offset, unsigned long size);
ST_INLN void vpushll(long long v);
ST_FUNC void vpushs(addr_t v);
ST_INLN void vpushsym(CType *type, Sym *sym);
ST_FUNC void vsetc(CType *type, int r, CValue *vc);
ST_FUNC void vseti(int r, int v);

/* Set 'nocode_wanted' after unconditional jumps (tccgen.c defines it itself
   after gjmp_addr_acs/gjmp_acs, the redirection is undefined at the end of tccstmt.c) */
#ifndef TCCGEN_MAIN
#define gjmp_addr gjmp_addr_acs
#define gjmp gjmp_acs
#endif

#endif /* TCCGEN_H */
