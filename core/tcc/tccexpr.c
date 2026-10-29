/*
 *  TCC - Tiny C Compiler
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* expressions (split out of tccgen.c) */

#define USING_GLOBALS
#include "tcc.h"
#include "tccgen.h"

static void parse_builtin_params(int nc, const char *args)
{
    char c, sep = '(';
    CType t;
    if (nc)
        nocode_wanted++;
    next();
    while ((c = *args++)) {
	skip(sep);
	sep = ',';
	switch (c) {
	    case 'e': expr_eq(); continue;
	    case 't': parse_type(&t); vpush(&t); continue;
	    default: tcc_error("internal error"); break;
	}
    }
    skip(')');
    if (nc)
        nocode_wanted--;
}

/* Keep call-only temporaries out of every recursive unary expression. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void unary_func_call(void)
{
    int size, align, r, t;
    Sym *s;
    SValue ret;
    Sym *sa;
    int nb_args, ret_nregs, ret_align, regsize, variadic;

    /* function call  */
    if ((vtop->type.t & VT_BTYPE) != VT_FUNC) {
        /* pointer test (no array accepted) */
        if ((vtop->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR) {
            vtop->type = *pointed_type(&vtop->type);
            if ((vtop->type.t & VT_BTYPE) != VT_FUNC)
                goto error_func;
        } else {
        error_func:
            expect("function pointer");
        }
    } else {
        vtop->r &= ~VT_LVAL; /* no lvalue */
    }
    /* get return type */
    s = vtop->type.ref;
    next();
    sa = s->next; /* first parameter */
    nb_args = regsize = 0;
    ret.r2 = VT_CONST;
    /* compute first implicit argument if a structure is returned */
    if ((s->type.t & VT_BTYPE) == VT_STRUCT) {
        variadic = (s->f.func_type == FUNC_ELLIPSIS);
        ret_nregs = gfunc_sret(&s->type, variadic, &ret.type,
                               &ret_align, &regsize);
        if (ret_nregs <= 0) {
            /* get some space for the returned structure */
            size = type_size(&s->type, &align);
    #ifdef TCC_TARGET_ARM64
        /* On arm64, a small struct is return in registers.
           It is much easier to write it to memory if we know
           that we are allowed to write some extra bytes, so
           round the allocated space up to a power of 2: */
        if (size < 16)
            while (size & (size - 1))
                size = (size | (size - 1)) + 1;
    #endif
            loc = (loc - size) & -align;
            ret.type = s->type;
            ret.r = VT_LOCAL | VT_LVAL;
            /* pass it as 'int' to avoid structure arg passing
               problems */
            vseti(VT_LOCAL, loc);
            ret.c = vtop->c;
            if (ret_nregs < 0)
              vtop--;
            else
              nb_args++;
        }
    } else {
        ret_nregs = 1;
        ret.type = s->type;
    }

    if (ret_nregs > 0) {
        /* return in register */
        ret.c.i = 0;
        PUT_R_RET(&ret, ret.type.t);
    }
    if (tok != ')') {
        for(;;) {
            expr_eq();
            gfunc_param_typed(s, sa);
            nb_args++;
            if (sa)
                sa = sa->next;
            if (tok == ')')
                break;
            skip(',');
        }
    }
    if (sa)
        tcc_error("too few arguments to function");
    skip(')');
    gfunc_call(nb_args);

    if (ret_nregs < 0) {
        vsetc(&ret.type, ret.r, &ret.c);
    #ifdef TCC_TARGET_RISCV64
        arch_transfer_ret_regs(1);
    #endif
    } else {
        /* return value */
        for (r = ret.r + ret_nregs + !ret_nregs; r-- > ret.r;) {
            vsetc(&ret.type, r, &ret.c);
            vtop->r2 = ret.r2; /* Loop only happens when r2 is VT_CONST */
        }

        /* handle packed struct return */
        if (((s->type.t & VT_BTYPE) == VT_STRUCT) && ret_nregs) {
            int addr, offset;

            size = type_size(&s->type, &align);
            /* We're writing whole regs often, make sure there's enough
               space.  Assume register size is power of 2.  */
            if (regsize > align)
              align = regsize;
            loc = (loc - size) & -align;
            addr = loc;
            offset = 0;
            for (;;) {
                vset(&ret.type, VT_LOCAL | VT_LVAL, addr + offset);
                vswap();
                vstore();
                vtop--;
                if (--ret_nregs == 0)
                  break;
                offset += regsize;
            }
            vset(&s->type, VT_LOCAL | VT_LVAL, addr);
        }

        /* Promote char/short return values. This is matters only
           for calling function that were not compiled by TCC and
           only on some architectures.  For those where it doesn't
           matter we expect things to be already promoted to int,
           but not larger.  */
        t = s->type.t & VT_BTYPE;
        if (t == VT_BYTE || t == VT_SHORT || t == VT_BOOL) {
    #ifdef PROMOTE_RET
            vtop->r |= BFVAL(VT_MUSTCAST, 1);
    #else
            vtop->type.t = VT_INT;
    #endif
        }
    }
    if (s->f.func_noreturn)
        CODE_OFF();
}

/* _Generic has its own parser state; do not reserve it in every unary
   recursion, including ordinary parenthesized expressions. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void unary_generic(void)
{
	CType controlling_type;
	int has_default = 0;
	int has_match = 0;
	int learn = 0;
	TokenString *str = NULL;
	int saved_const_wanted = const_wanted;

	next();
	skip('(');
	const_wanted = 0;
	expr_type(&controlling_type, expr_eq);
	controlling_type.t &= ~(VT_CONSTANT | VT_VOLATILE | VT_ARRAY);
	if ((controlling_type.t & VT_BTYPE) == VT_FUNC)
	  mk_pointer(&controlling_type);
	const_wanted = saved_const_wanted;
	for (;;) {
	    learn = 0;
	    skip(',');
	    if (tok == TOK_DEFAULT) {
		if (has_default)
		    tcc_error("too many 'default'");
		has_default = 1;
		if (!has_match)
		    learn = 1;
		next();
	    } else {
	        AttributeDef ad_tmp;
		int itmp;
	        CType cur_type;

                in_generic++;
		parse_btype(&cur_type, &ad_tmp);
                in_generic--;

		type_decl(&cur_type, &ad_tmp, &itmp, TYPE_ABSTRACT);
		if (compare_types(&controlling_type, &cur_type, 0)) {
		    if (has_match) {
		      tcc_error("type match twice");
		    }
		    has_match = 1;
		    learn = 1;
		}
	    }
	    skip(':');
	    if (learn) {
		if (str)
		    tok_str_free(str);
		skip_or_save_block(&str);
	    } else {
		skip_or_save_block(NULL);
	    }
	    if (tok == ')')
		break;
	}
	if (!str) {
	    type_description_error(&controlling_type,
	        "type '%s' does not match any association", 60);
	}
	begin_macro(str, 1);
	next();
	expr_eq();
	if (tok != TOK_EOF)
	    expect(",");
	end_macro();
        next();
}

ST_FUNC void unary(void)
{
    int n, t, align, size, r, sizeof_caller;
    CType type;
    Sym *s;
    AttributeDef ad;

    /* generate line number info */
    if (tcc_state->do_debug)
        tcc_debug_line(tcc_state);

    sizeof_caller = in_sizeof;
    in_sizeof = 0;
    type.ref = NULL;
    /* XXX: GCC 2.95.3 does not generate a table although it should be
       better here */
 tok_next:
    switch(tok) {
    case TOK_EXTENSION:
        next();
        goto tok_next;
    case TOK_LCHAR:
#ifdef TCC_TARGET_PE
        t = VT_SHORT|VT_UNSIGNED;
        goto push_tokc;
#endif
    case TOK_CINT:
    case TOK_CCHAR: 
	t = VT_INT;
 push_tokc:
	type.t = t;
	vsetc(&type, VT_CONST, &tokc);
        next();
        break;
    case TOK_CUINT:
        t = VT_INT | VT_UNSIGNED;
        goto push_tokc;
    case TOK_CLLONG:
        t = VT_LLONG;
	goto push_tokc;
    case TOK_CULLONG:
        t = VT_LLONG | VT_UNSIGNED;
	goto push_tokc;
    case TOK_CFLOAT:
        t = VT_FLOAT;
	goto push_tokc;
    case TOK_CDOUBLE:
        t = VT_DOUBLE;
	goto push_tokc;
    case TOK_CLDOUBLE:
        t = VT_LDOUBLE;
	goto push_tokc;
    case TOK_CLONG:
        t = (LONG_SIZE == 8 ? VT_LLONG : VT_INT) | VT_LONG;
	goto push_tokc;
    case TOK_CULONG:
        t = (LONG_SIZE == 8 ? VT_LLONG : VT_INT) | VT_LONG | VT_UNSIGNED;
	goto push_tokc;
    case TOK___FUNCTION__:
        if (!gnu_ext)
            goto tok_identifier;
        /* fall thru */
    case TOK___FUNC__:
        {
            void *ptr;
            int len;
            /* special function name identifier */
            len = strlen(funcname) + 1;
            /* generate char[len] type */
            type.t = VT_BYTE;
            mk_pointer(&type);
            type.t |= VT_ARRAY;
            type.ref->c = len;
            vpush_ref(&type, data_section, data_section->data_offset, len);
            if (!NODATA_WANTED) {
                ptr = section_ptr_add(data_section, len);
                memcpy(ptr, funcname, len);
            }
            next();
        }
        break;
    case TOK_LSTR:
#ifdef TCC_TARGET_PE
        t = VT_SHORT | VT_UNSIGNED;
#else
        t = VT_INT;
#endif
        goto str_init;
    case TOK_STR:
        /* string parsing */
        t = VT_BYTE;
        if (tcc_state->char_is_unsigned)
            t = VT_BYTE | VT_UNSIGNED;
    str_init:
        if (tcc_state->warn_write_strings)
            t |= VT_CONSTANT;
        type.t = t;
        mk_pointer(&type);
        type.t |= VT_ARRAY;
        memset(&ad, 0, sizeof(AttributeDef));
        decl_initializer_alloc(&type, &ad, VT_CONST, 2, 0, 0);
        break;
    case '(':
        next();
        /* cast ? */
        if (parse_btype(&type, &ad)) {
            type_decl(&type, &ad, &n, TYPE_ABSTRACT);
            skip(')');
            /* check ISOC99 compound literal */
            if (tok == '{') {
                    /* data is allocated locally by default */
                if (global_expr)
                    r = VT_CONST;
                else
                    r = VT_LOCAL;
                /* all except arrays are lvalues */
                if (!(type.t & VT_ARRAY))
                    r |= VT_LVAL;
                memset(&ad, 0, sizeof(AttributeDef));
                decl_initializer_alloc(&type, &ad, r, 1, 0, 0);
            } else {
                if (sizeof_caller) {
                    vpush(&type);
                    return;
                }
                unary();
                gen_cast(&type);
            }
        } else if (tok == '{') {
	    int saved_nocode_wanted = nocode_wanted;
            if (const_wanted && !(nocode_wanted & unevalmask))
                tcc_error("expected constant");
            /* save all registers */
            save_regs(0);
            /* statement expression : we do not accept break/continue
               inside as GCC does.  We do retain the nocode_wanted state,
	       as statement expressions can't ever be entered from the
	       outside, so any reactivation of code emission (from labels
	       or loop heads) can be disabled again after the end of it. */
            block(1);
	    nocode_wanted = saved_nocode_wanted;
            skip(')');
        } else {
            gexpr();
            skip(')');
        }
        break;
    case '*':
        next();
        unary();
        indir();
        break;
    case '&':
        next();
        unary();
        /* functions names must be treated as function pointers,
           except for unary '&' and sizeof. Since we consider that
           functions are not lvalues, we only have to handle it
           there and in function calls. */
        /* arrays can also be used although they are not lvalues */
        if ((vtop->type.t & VT_BTYPE) != VT_FUNC &&
            !(vtop->type.t & VT_ARRAY))
            test_lvalue();
        if (vtop->sym)
          vtop->sym->a.addrtaken = 1;
        mk_pointer(&vtop->type);
        gaddrof();
        break;
    case '!':
        next();
        unary();
        gen_test_zero(TOK_EQ);
        break;
    case '~':
        next();
        unary();
        vpushi(-1);
        gen_op('^');
        break;
    case '+':
        next();
        unary();
        if ((vtop->type.t & VT_BTYPE) == VT_PTR)
            tcc_error("pointer not accepted for unary plus");
        /* In order to force cast, we add zero, except for floating point
	   where we really need an noop (otherwise -0.0 will be transformed
	   into +0.0).  */
	if (!is_float(vtop->type.t)) {
	    vpushi(0);
	    gen_op('+');
	}
        break;
    case TOK_SIZEOF:
    case TOK_ALIGNOF1:
    case TOK_ALIGNOF2:
    case TOK_ALIGNOF3:
        t = tok;
        next();
        in_sizeof++;
        expr_type(&type, unary); /* Perform a in_sizeof = 0; */
        s = NULL;
        if (vtop[1].r & VT_SYM)
            s = vtop[1].sym; /* hack: accessing previous vtop */
        size = type_size(&type, &align);
        if (s && s->a.aligned)
            align = 1 << (s->a.aligned - 1);
        if (t == TOK_SIZEOF) {
            if (!(type.t & VT_VLA)) {
                if (size < 0)
                    tcc_error("sizeof applied to an incomplete type");
                vpushs(size);
            } else {
                vla_runtime_type_size(&type, &align);
            }
        } else {
            vpushs(align);
        }
        vtop->type.t |= VT_UNSIGNED;
        break;

    case TOK_builtin_expect:
	/* __builtin_expect is a no-op for now */
	parse_builtin_params(0, "ee");
	vpop();
        break;
    case TOK_builtin_types_compatible_p:
	parse_builtin_params(0, "tt");
	vtop[-1].type.t &= ~(VT_CONSTANT | VT_VOLATILE);
	vtop[0].type.t &= ~(VT_CONSTANT | VT_VOLATILE);
	n = is_compatible_types(&vtop[-1].type, &vtop[0].type);
	vtop -= 2;
	vpushi(n);
        break;
    case TOK_builtin_choose_expr:
	{
	    int64_t c;
	    next();
	    skip('(');
	    c = expr_const64();
	    skip(',');
	    if (!c) {
		nocode_wanted++;
	    }
	    expr_eq();
	    if (!c) {
		vpop();
		nocode_wanted--;
	    }
	    skip(',');
	    if (c) {
		nocode_wanted++;
	    }
	    expr_eq();
	    if (c) {
		vpop();
		nocode_wanted--;
	    }
	    skip(')');
	}
        break;
    case TOK_builtin_constant_p:
	parse_builtin_params(1, "e");
	n = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	vtop--;
	vpushi(n);
        break;
    case TOK_builtin_frame_address:
    case TOK_builtin_return_address:
        {
            int tok1 = tok;
            int level;
            next();
            skip('(');
            if (tok != TOK_CINT) {
                tcc_error("%s only takes positive integers",
                          tok1 == TOK_builtin_return_address ?
                          "__builtin_return_address" :
                          "__builtin_frame_address");
            }
            level = (uint32_t)tokc.i;
            next();
            skip(')');
            type.t = VT_VOID;
            mk_pointer(&type);
            vset(&type, VT_LOCAL, 0);       /* local frame */
            while (level--) {
                mk_pointer(&vtop->type);
                indir();                    /* -> parent frame */
            }
            if (tok1 == TOK_builtin_return_address) {
                // assume return address is just above frame pointer on stack
                vpushi(PTR_SIZE);
                gen_op('+');
                mk_pointer(&vtop->type);
                indir();
            }
        }
        break;
#ifdef TCC_TARGET_RISCV64
    case TOK_builtin_va_start:
        parse_builtin_params(0, "ee");
        r = vtop->r & VT_VALMASK;
        if (r == VT_LLOCAL)
            r = VT_LOCAL;
        if (r != VT_LOCAL)
            tcc_error("__builtin_va_start expects a local variable");
        gen_va_start();
	vstore();
        break;
#endif
#ifdef TCC_TARGET_X86_64
#ifdef TCC_TARGET_PE
    case TOK_builtin_va_start:
	parse_builtin_params(0, "ee");
        r = vtop->r & VT_VALMASK;
        if (r == VT_LLOCAL)
            r = VT_LOCAL;
        if (r != VT_LOCAL)
            tcc_error("__builtin_va_start expects a local variable");
        vtop->r = r;
	vtop->type = char_pointer_type;
	vtop->c.i += 8;
	vstore();
        break;
#else
    case TOK_builtin_va_arg_types:
	parse_builtin_params(0, "t");
	vpushi(classify_x86_64_va_arg(&vtop->type));
	vswap();
	vpop();
        break;
#endif
#endif

#ifdef TCC_TARGET_ARM64
    case TOK___va_start: {
	parse_builtin_params(0, "ee");
        //xx check types
        gen_va_start();
        vpushi(0);
        vtop->type.t = VT_VOID;
        break;
    }
    case TOK___va_arg: {
	parse_builtin_params(0, "et");
	type = vtop->type;
	vpop();
        //xx check types
        gen_va_arg(&type);
        vtop->type = type;
        break;
    }
    case TOK___arm64_clear_cache: {
	parse_builtin_params(0, "ee");
        gen_clear_cache();
        vpushi(0);
        vtop->type.t = VT_VOID;
        break;
    }
#endif
    /* pre operations */
    case TOK_INC:
    case TOK_DEC:
        t = tok;
        next();
        unary();
        inc(0, t);
        break;
    case '-':
        next();
        unary();
        t = vtop->type.t & VT_BTYPE;
	if (is_float(t)) {
            /* In IEEE negate(x) isn't subtract(0,x), but rather
	       subtract(-0, x).  */
	    vpush(&vtop->type);
	    if (t == VT_FLOAT)
	        vtop->c.f = -1.0 * 0.0;
	    else if (t == VT_DOUBLE)
	        vtop->c.d = -1.0 * 0.0;
            else
	        vtop->c.ld = -1.0 * 0.0;
	} else
	    vpushi(0);
	vswap();
	gen_op('-');
        break;
    case TOK_LAND:
        if (!gnu_ext)
            goto tok_identifier;
        next();
        /* allow to take the address of a label */
        if (tok < TOK_UIDENT)
            expect("label identifier");
        s = label_find(tok);
        if (!s) {
            s = label_push(&global_label_stack, tok, LABEL_FORWARD);
        } else {
            if (s->r == LABEL_DECLARED)
                s->r = LABEL_FORWARD;
        }
        if (!s->type.t) {
            s->type.t = VT_VOID;
            mk_pointer(&s->type);
            s->type.t |= VT_STATIC;
        }
        vpushsym(&s->type, s);
        next();
        break;

    case TOK_GENERIC:
        unary_generic();
        break;
    // special qnan , snan and infinity values
    case TOK___NAN__:
        n = 0x7fc00000;
special_math_val:
	vpushi(n);
	vtop->type.t = VT_FLOAT;
        next();
        break;
    case TOK___SNAN__:
	n = 0x7f800001;
	goto special_math_val;
    case TOK___INF__:
	n = 0x7f800000;
	goto special_math_val;

    default:
    tok_identifier:
        t = tok;
        next();
        if (t < TOK_UIDENT)
            expect("identifier");
        s = sym_find(t);
        if (!s || IS_ASM_SYM(s)) {
            const char *name = get_tok_str(t, NULL);
            if (tok != '(')
                tcc_error("'%s' undeclared", name);
            /* for simple function calls, we tolerate undeclared
               external reference to int() function */
            if (tcc_state->warn_implicit_function_declaration
#ifdef TCC_TARGET_PE
                /* people must be warned about using undeclared WINAPI functions
                   (which usually start with uppercase letter) */
                || (name[0] >= 'A' && name[0] <= 'Z')
#endif
            )
                tcc_warning("implicit declaration of function '%s'", name);
            s = external_global_sym(t, &func_old_type);
        }

        r = s->r;
        /* A symbol that has a register is a local register variable,
           which starts out as VT_LOCAL value.  */
        if ((r & VT_VALMASK) < VT_CONST)
            r = (r & ~VT_VALMASK) | VT_LOCAL;

        vset(&s->type, r, s->c);
        /* Point to s as backpointer (even without r&VT_SYM).
	   Will be used by at least the x86 inline asm parser for
	   regvars.  */
	vtop->sym = s;

        if (r & VT_SYM) {
            vtop->c.i = 0;
        } else if (r == VT_CONST && IS_ENUM_VAL(s->type.t)) {
            vtop->c.i = s->enum_val;
        }
        break;
    }
    
    /* post operations */
    while (1) {
        if (tok == TOK_INC || tok == TOK_DEC) {
            inc(1, tok);
            next();
        } else if (tok == '.' || tok == TOK_ARROW || tok == TOK_CDOUBLE) {
            int qualifiers, cumofs = 0;
            /* field */ 
            if (tok == TOK_ARROW) 
                indir();
            qualifiers = vtop->type.t & (VT_CONSTANT | VT_VOLATILE);
            test_lvalue();
            gaddrof();
            /* expect pointer on structure */
            if ((vtop->type.t & VT_BTYPE) != VT_STRUCT) {
                type_description_error(&vtop->type,
                    "expected struct or union but not '%s'", 256);
            }
            if (tok == TOK_CDOUBLE)
                expect("field name");
            next();
            if (tok == TOK_CINT || tok == TOK_CUINT)
                expect("field name");
	    s = find_field(&vtop->type, tok, &cumofs);
            if (!s)
                tcc_error("field not found: %s",  get_tok_str(tok & ~SYM_FIELD, &tokc));
            /* add field offset to pointer */
            vtop->type = char_pointer_type; /* change type to 'char *' */
            vpushi(cumofs + s->c);
            gen_op('+');
            /* change type to field type, and set to lvalue */
            vtop->type = s->type;
            vtop->type.t |= qualifiers;
            /* an array is never an lvalue */
            if (!(vtop->type.t & VT_ARRAY)) {
                vtop->r |= VT_LVAL;
#ifdef CONFIG_TCC_BCHECK
                /* if bound checking, the referenced pointer must be checked */
                if (tcc_state->do_bounds_check)
                    vtop->r |= VT_MUSTBOUND;
#endif
            }
            next();
        } else if (tok == '[') {
            next();
            gexpr();
            gen_op('+');
            indir();
            skip(']');
        } else if (tok == '(') {
            unary_func_call();
        } else {
            break;
        }
    }
}

#ifndef precedence_parser /* original top-down parser */

static void expr_prod(void)
{
    int t;

    unary();
    while ((t = tok) == '*' || t == '/' || t == '%') {
        next();
        unary();
        gen_op(t);
    }
}

static void expr_sum(void)
{
    int t;

    expr_prod();
    while ((t = tok) == '+' || t == '-') {
        next();
        expr_prod();
        gen_op(t);
    }
}

static void expr_shift(void)
{
    int t;

    expr_sum();
    while ((t = tok) == TOK_SHL || t == TOK_SAR) {
        next();
        expr_sum();
        gen_op(t);
    }
}

static void expr_cmp(void)
{
    int t;

    expr_shift();
    while (((t = tok) >= TOK_ULE && t <= TOK_GT) ||
           t == TOK_ULT || t == TOK_UGE) {
        next();
        expr_shift();
        gen_op(t);
    }
}

static void expr_cmpeq(void)
{
    int t;

    expr_cmp();
    while ((t = tok) == TOK_EQ || t == TOK_NE) {
        next();
        expr_cmp();
        gen_op(t);
    }
}

static void expr_and(void)
{
    expr_cmpeq();
    while (tok == '&') {
        next();
        expr_cmpeq();
        gen_op('&');
    }
}

static void expr_xor(void)
{
    expr_and();
    while (tok == '^') {
        next();
        expr_and();
        gen_op('^');
    }
}

static void expr_or(void)
{
    expr_xor();
    while (tok == '|') {
        next();
        expr_xor();
        gen_op('|');
    }
}

static void expr_landor(int op);

static void expr_land(void)
{
    expr_or();
    if (tok == TOK_LAND)
        expr_landor(tok);
}

static void expr_lor(void)
{
    expr_land();
    if (tok == TOK_LOR)
        expr_landor(tok);
}

# define expr_landor_next(op) op == TOK_LAND ? expr_or() : expr_land()
#else /* defined precedence_parser */
# define expr_landor_next(op) unary(), expr_infix(precedence(op) + 1)
# define expr_lor() unary(), expr_infix(1)

static int precedence(int tok)
{
    switch (tok) {
        case TOK_LOR: return 1;
        case TOK_LAND: return 2;
	case '|': return 3;
	case '^': return 4;
	case '&': return 5;
	case TOK_EQ: case TOK_NE: return 6;
 relat: case TOK_ULT: case TOK_UGE: return 7;
	case TOK_SHL: case TOK_SAR: return 8;
	case '+': case '-': return 9;
	case '*': case '/': case '%': return 10;
	default:
	    if (tok >= TOK_ULE && tok <= TOK_GT)
	        goto relat;
	    return 0;
    }
}
static unsigned char prec[256];
ST_FUNC void init_prec(void)
{
    int i;
    for (i = 0; i < 256; i++)
	prec[i] = precedence(i);
}
#define precedence(i) ((unsigned)i < 256 ? prec[i] : 0)

static void expr_landor(int op);

static void expr_infix(int p)
{
    int t = tok, p2;
    while ((p2 = precedence(t)) >= p) {
        if (t == TOK_LOR || t == TOK_LAND) {
            expr_landor(t);
        } else {
            next();
            unary();
            if (precedence(tok) > p2)
              expr_infix(p2 + 1);
            gen_op(t);
        }
        t = tok;
    }
}
#endif

/* Assuming vtop is a value used in a conditional context
   (i.e. compared with zero) return 0 if it's false, 1 if
   true and -1 if it can't be statically determined.  */
static int condition_3way(void)
{
    int c = -1;
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
	(!(vtop->r & VT_SYM) || !vtop->sym->a.weak)) {
	vdup();
        gen_cast_s(VT_BOOL);
	c = vtop->c.i;
	vpop();
    }
    return c;
}

static void expr_landor(int op)
{
    int t = 0, cc = 1, f = 0, i = op == TOK_LAND, c;
    for(;;) {
        c = f ? i : condition_3way();
        if (c < 0)
            save_regs(1), cc = 0;
        else if (c != i)
            nocode_wanted++, f = 1;
        if (tok != op)
            break;
        if (c < 0)
            t = gvtst(i, t);
        else
            vpop();
        next();
        expr_landor_next(op);
    }
    if (cc || f) {
        vpop();
        vpushi(i ^ f);
        gsym(t);
        nocode_wanted -= f;
    } else {
        gvtst_set(i, t);
    }
}

static int is_cond_bool(SValue *sv)
{
    if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
        && (sv->type.t & VT_BTYPE) == VT_INT)
        return (unsigned)sv->c.i < 2;
    if (sv->r == VT_CMP)
        return 1;
    return 0;
}

static void expr_cond(void);

/* Reserve conditional-operator state only after encountering '?'. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void expr_cond_op(void)
{
    int tt, u, r1, r2, rc, t1, t2, bt1, bt2, islv, c, g;
    SValue sv;
    CType type, type1, type2;
    int ncw_prev;

        next();
	c = condition_3way();
        ncw_prev = nocode_wanted;
        g = (tok == ':' && gnu_ext);
        tt = 0;
        if (!g) {
            if (c < 0) {
                save_regs(1);
                tt = gvtst(1, 0);
            } else {
                vpop();
            }
        } else if (c < 0) {
            /* needed to avoid having different registers saved in
               each branch */
            save_regs(1);
            gv_dup();
            tt = gvtst(0, 0);
        }

        if (1) {
            if (c == 0)
                nocode_wanted++;
            if (!g)
                gexpr();

            if (c < 0 && vtop->r == VT_CMP) {
                t1 = gvtst(0, 0);
                vpushi(0);
                gvtst_set(0, t1);
            }

            if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
                mk_pointer(&vtop->type);
	    type1 = vtop->type;
            sv = *vtop; /* save value to handle it later */
            vtop--; /* no vpop so that FP stack is not flushed */

            if (g) {
                u = tt;
            } else if (c < 0) {
                u = gjmp(0);
                gsym(tt);
            } else
                u = 0;

            nocode_wanted = ncw_prev;
            if (c == 1)
                nocode_wanted++;
            skip(':');
            expr_cond();

            if (c < 0 && is_cond_bool(vtop) && is_cond_bool(&sv)) {
                if (sv.r == VT_CMP) {
                    t1 = sv.jtrue;
                    t2 = u;
                } else {
                    t1 = gvtst(0, 0);
                    t2 = gjmp(0);
                    gsym(u);
                    vpushv(&sv);
                }
                gvtst_set(0, t1);
                gvtst_set(1, t2);
                nocode_wanted = ncw_prev;
                //  tcc_warning("two conditions expr_cond");
                return;
            }

            if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
                mk_pointer(&vtop->type);
	    type2=vtop->type;
            t1 = type1.t;
            bt1 = t1 & VT_BTYPE;
            t2 = type2.t;
            bt2 = t2 & VT_BTYPE;
            type.ref = NULL;

            /* cast operands to correct type according to ISOC rules */
            if (bt1 == VT_VOID || bt2 == VT_VOID) {
                type.t = VT_VOID; /* NOTE: as an extension, we accept void on only one side */
            } else if (bt1 == VT_BOOL && bt2 == VT_BOOL) {
                type = type1;
            } else if (is_float(bt1) && is_integer_btype(bt2)) {
                type = type1;
            } else if (is_integer_btype(bt1) && is_float(bt2)) {
                type = type2;
            } else if (is_float(bt1) && is_float(bt2)) {
                if (bt1 == VT_LDOUBLE || bt2 == VT_LDOUBLE) {
                    type.t = VT_LDOUBLE;

                } else if (bt1 == VT_DOUBLE || bt2 == VT_DOUBLE) {
                    type.t = VT_DOUBLE;
                } else {
                    type.t = VT_FLOAT;
                }
            } else if ((bt1 == VT_LLONG && is_integer_btype(bt2)) ||
                       (bt2 == VT_LLONG && is_integer_btype(bt1))) {
                /* cast to biggest op */
                type.t = VT_LLONG | VT_LONG;
                if (bt1 == VT_LLONG)
                    type.t &= t1;
                if (bt2 == VT_LLONG)
                    type.t &= t2;
                /* convert to unsigned if it does not fit in a long long */
                if ((t1 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_LLONG | VT_UNSIGNED) ||
                    (t2 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_LLONG | VT_UNSIGNED))
                    type.t |= VT_UNSIGNED;
            }
            /* http://port70.net/~nsz/c/c99/n1256.html#6.5.15p6 */
            /* If one is a null ptr constant the result type
                is the other.  */
            else if (bt1 == VT_PTR && is_null_pointer(vtop)) {
                type = type1;
            } else if (is_null_pointer(&sv) && bt2 == VT_PTR) {
                type = type2;
            }  else if (bt1 == VT_PTR && is_integer_btype(bt2)) {
                type = type1;
                tcc_warning("pointer/integer mismatch");
            } else if (is_integer_btype(bt1) && bt2 == VT_PTR) {
                type = type2;
                tcc_warning("pointer/integer mismatch");
            } else if (bt1==VT_PTR && bt2 == VT_PTR) {
		CType *pt1 = pointed_type(&type1);
		CType *pt2 = pointed_type(&type2);
                int pbt1 = pt1->t & VT_BTYPE;
                int pbt2 = pt2->t & VT_BTYPE;
		int newquals, copied = 0;
		/* pointers to void get preferred, otherwise the
		   pointed to types minus qualifs should be compatible */
                type = (pbt1 == VT_VOID) ? type1 : type2;
                if (pbt1 != VT_VOID && pbt2 != VT_VOID && ! compare_types(pt1, pt2, 1/*unqualif*/)) {
                    type_incompatibility_warning(&type1, &type2, "incompatible pointer types");
                    // result is void*
                    type.t = VT_VOID;
                    mk_pointer(&type);
                }
                /* combine qualifs */
		newquals = ((pt1->t | pt2->t) & (VT_CONSTANT | VT_VOLATILE));
		if ((~pointed_type(&type)->t & (VT_CONSTANT | VT_VOLATILE))
			& newquals)
		{
			/* copy the pointer target symbol */
			type.ref = sym_push(SYM_FIELD, &type.ref->type,
					    0, type.ref->c);
			copied = 1;
			pointed_type(&type)->t |= newquals;
		}
                /* pointers to incomplete arrays get converted to
		   pointers to completed ones if possible */
		if (pt1->t & VT_ARRAY
			&& pt2->t & VT_ARRAY
			&& pointed_type(&type)->ref->c < 0
			&& (pt1->ref->c > 0 || pt2->ref->c > 0))
		{
			if (!copied)
			  type.ref = sym_push(SYM_FIELD, &type.ref->type,
					      0, type.ref->c);
			pointed_type(&type)->ref =
			    sym_push(SYM_FIELD, &pointed_type(&type)->ref->type,
				     0, pointed_type(&type)->ref->c);
                        pointed_type(&type)->ref->c =
			    0 < pt1->ref->c ? pt1->ref->c : pt2->ref->c;
		}
            } else if (bt1 == VT_STRUCT && bt2 == VT_STRUCT) {
                /* test structure compatibility */
                if (type1.ref != type2.ref) {
                    type_incompatibility_error(&type1, &type2,"different struct/union types '%s' vs. '%s'");
                } else {
                    type = bt1 == VT_STRUCT ? type1 : type2;
                }
            } else if (is_integer_btype(bt1) && is_integer_btype(bt2)) {
                /* integer operations */
                type.t = VT_INT | (VT_LONG & (t1 | t2));
                /* convert to unsigned if one of both is unsigned */
                if ((t1 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_INT | VT_UNSIGNED) ||
                    (t2 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_INT | VT_UNSIGNED))
                    type.t |= VT_UNSIGNED;
            } else {
                type_incompatibility_error(&type1, &type2, "type '%s' is incompatible to '%s'");
            }

            /* keep structs lvalue by transforming `(expr ? a : b)` to `*(expr ? &a : &b)` so
               that `(expr ? a : b).mem` does not error  with "lvalue expected" */
            islv = (vtop->r & VT_LVAL) && (sv.r & VT_LVAL) && VT_STRUCT == (type.t & VT_BTYPE);

            /* now we convert second operand */
            if (c != 1) {
                gen_cast(&type);
                if (islv) {
                    mk_pointer(&vtop->type);
                    gaddrof();
                } else if (VT_STRUCT == (vtop->type.t & VT_BTYPE))
                    gaddrof();
            }

            rc = RC_TYPE(type.t);
            /* for long longs, we use fixed registers to avoid having
               to handle a complicated move */
            if (USING_TWO_WORDS(type.t))
                rc = RC_RET(type.t);

            tt = r2 = 0;
            if (c < 0) {
                r2 = gv(rc);
                tt = gjmp(0);
            }
            gsym(u);
            nocode_wanted = ncw_prev;

            /* this is horrible, but we must also convert first
               operand */
            if (c != 0) {
                *vtop = sv;
                gen_cast(&type);
                if (islv) {
                    mk_pointer(&vtop->type);
                    gaddrof();
                } else if (VT_STRUCT == (vtop->type.t & VT_BTYPE))
                    gaddrof();
            }

            if (c < 0) {
                r1 = gv(rc);
                move_reg(r2, r1, islv ? VT_PTR : type.t);
                vtop->r = r2;
                gsym(tt);
            }

            if (islv)
                indir();
        }
}

static void expr_cond(void)
{
    expr_lor();
    if (tok == '?')
        expr_cond_op();
}

ST_FUNC void expr_eq(void)
{
    int t;
    
    expr_cond();
    if ((t = tok) == '='
        || (t >= TOK_A_MOD && t <= TOK_A_DIV)
        || t == TOK_A_XOR || t == TOK_A_OR
        || t == TOK_A_SHL || t == TOK_A_SAR) {
        test_lvalue();
        next();
        if (t == '=') {
            expr_eq();
        } else {
            vdup();
            expr_eq();
            gen_op(t & 0x7f);
        }
        vstore();
    }
}

ST_FUNC void gexpr(void)
{
    while (1) {
        expr_eq();
        if (tok != ',')
            break;
        vpop();
        next();
    }
}

/* parse a constant expression and return value in vtop.  */
ST_FUNC void expr_const1(void)
{
    const_wanted++;
    nocode_wanted += unevalmask + 1;
    expr_cond();
    nocode_wanted -= unevalmask + 1;
    const_wanted--;
}

/* parse an integer constant and return its value. */
ST_INLN int64_t expr_const64(void)
{
    int64_t c;
    expr_const1();
    if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
        expect("constant expression");
    c = vtop->c.i;
    vpop();
    return c;
}

/* parse an integer constant and return its value.
   Complain if it doesn't fit 32bit (signed or unsigned).  */
ST_FUNC int expr_const(void)
{
    int c;
    int64_t wc = expr_const64();
    c = wc;
    if (c != wc && (unsigned)c != wc)
        tcc_error("constant exceeds 32 bit");
    return c;
}

