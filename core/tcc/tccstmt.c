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

/* statements, initializers, declarations (split out of tccgen.c) */

#define USING_GLOBALS
#include "tcc.h"
#include "tccgen.h"

/* ------------------------------------------------------------------------- */
/* return from function */

#ifndef TCC_TARGET_ARM64
static void gfunc_return(CType *func_type)
{
    if ((func_type->t & VT_BTYPE) == VT_STRUCT) {
        CType type, ret_type;
        int ret_align, ret_nregs, regsize;
        ret_nregs = gfunc_sret(func_type, func_var, &ret_type,
                               &ret_align, &regsize);
        if (ret_nregs < 0) {
#ifdef TCC_TARGET_RISCV64
            arch_transfer_ret_regs(0);
#endif
        } else if (0 == ret_nregs) {
            /* if returning structure, must copy it to implicit
               first pointer arg location */
            type = *func_type;
            mk_pointer(&type);
            vset(&type, VT_LOCAL | VT_LVAL, func_vc);
            indir();
            vswap();
            /* copy structure value to pointer */
            vstore();
        } else {
            /* returning structure packed into registers */
            int size, addr, align, rc;
            size = type_size(func_type,&align);
            if ((vtop->r != (VT_LOCAL | VT_LVAL) ||
                 (vtop->c.i & (ret_align-1)))
                && (align & (ret_align-1))) {
                loc = (loc - size) & -ret_align;
                addr = loc;
                type = *func_type;
                vset(&type, VT_LOCAL | VT_LVAL, addr);
                vswap();
                vstore();
                vpop();
                vset(&ret_type, VT_LOCAL | VT_LVAL, addr);
            }
            vtop->type = ret_type;
            rc = RC_RET(ret_type.t);
            if (ret_nregs == 1)
                gv(rc);
            else {
                for (;;) {
                    vdup();
                    gv(rc);
                    vpop();
                    if (--ret_nregs == 0)
                      break;
                    /* We assume that when a structure is returned in multiple
                       registers, their classes are consecutive values of the
                       suite s(n) = 2^n */
                    rc <<= 1;
                    vtop->c.i += regsize;
                }
            }
        }
    } else {
        gv(RC_RET(func_type->t));
    }
    vtop--; /* NOT vpop() because on x86 it would flush the fp stack */
}
#endif

static void check_func_return(void)
{
    if ((func_vt.t & VT_BTYPE) == VT_VOID)
        return;
    if (!strcmp (funcname, "main")
        && (func_vt.t & VT_BTYPE) == VT_INT) {
        /* main returns 0 by default */
        vpushi(0);
        gen_assign_cast(&func_vt);
        gfunc_return(&func_vt);
    } else {
        tcc_warning("function might return no value: '%s'", funcname);
    }
}

/* ------------------------------------------------------------------------- */
/* switch/case */

static int case_cmp(const void *pa, const void *pb)
{
    int64_t a = (*(struct case_t**) pa)->v1;
    int64_t b = (*(struct case_t**) pb)->v1;
    return a < b ? -1 : a > b;
}

static void gtst_addr(int t, int a)
{
    gsym_addr(gvtst(0, t), a);
}

static void gcase(struct case_t **base, int len, int *bsym)
{
    struct case_t *p;
    int e;
    int ll = (vtop->type.t & VT_BTYPE) == VT_LLONG;
    while (len > 8) {
        /* binary search */
        p = base[len/2];
        vdup();
	if (ll)
	    vpushll(p->v2);
	else
	    vpushi(p->v2);
        gen_op(TOK_LE);
        e = gvtst(1, 0);
        vdup();
	if (ll)
	    vpushll(p->v1);
	else
	    vpushi(p->v1);
        gen_op(TOK_GE);
        gtst_addr(0, p->sym); /* v1 <= x <= v2 */
        /* x < v1 */
        gcase(base, len/2, bsym);
        /* x > v2 */
        gsym(e);
        e = len/2 + 1;
        base += e; len -= e;
    }
    /* linear scan */
    while (len--) {
        p = *base++;
        vdup();
	if (ll)
	    vpushll(p->v2);
	else
	    vpushi(p->v2);
        if (p->v1 == p->v2) {
            gen_op(TOK_EQ);
            gtst_addr(0, p->sym);
        } else {
            gen_op(TOK_LE);
            e = gvtst(1, 0);
            vdup();
	    if (ll)
	        vpushll(p->v1);
	    else
	        vpushi(p->v1);
            gen_op(TOK_GE);
            gtst_addr(0, p->sym);
            gsym(e);
        }
    }
    *bsym = gjmp(*bsym);
}

/* ------------------------------------------------------------------------- */
/* __attribute__((cleanup(fn))) */

static void try_call_scope_cleanup(Sym *stop)
{
    Sym *cls = cur_scope->cl.s;

    for (; cls != stop; cls = cls->ncl) {
	Sym *fs = cls->next;
	Sym *vs = cls->prev_tok;

	vpushsym(&fs->type, fs);
	vset(&vs->type, vs->r, vs->c);
	vtop->sym = vs;
        mk_pointer(&vtop->type);
	gaddrof();
	gfunc_call(1);
    }
}

static void try_call_cleanup_goto(Sym *cleanupstate)
{
    Sym *oc, *cc;
    int ocd, ccd;

    if (!cur_scope->cl.s)
	return;

    /* search NCA of both cleanup chains given parents and initial depth */
    ocd = cleanupstate ? cleanupstate->v & ~SYM_FIELD : 0;
    for (ccd = cur_scope->cl.n, oc = cleanupstate; ocd > ccd; --ocd, oc = oc->ncl)
      ;
    for (cc = cur_scope->cl.s; ccd > ocd; --ccd, cc = cc->ncl)
      ;
    for (; cc != oc; cc = cc->ncl, oc = oc->ncl, --ccd)
      ;

    try_call_scope_cleanup(cc);
}

/* call 'func' for each __attribute__((cleanup(func))) */
static void block_cleanup(struct scope *o)
{
    int jmp = 0;
    Sym *g, **pg;
    for (pg = &pending_gotos; (g = *pg) && g->c > o->cl.n;) {
        if (g->prev_tok->r & LABEL_FORWARD) {
            Sym *pcl = g->next;
            if (!jmp)
                jmp = gjmp(0);
            gsym(pcl->jnext);
            try_call_scope_cleanup(o->cl.s);
            pcl->jnext = gjmp(0);
            if (!o->cl.n)
                goto remove_pending;
            g->c = o->cl.n;
            pg = &g->prev;
        } else {
    remove_pending:
            *pg = g->prev;
            sym_free(g);
        }
    }
    gsym(jmp);
    try_call_scope_cleanup(o->cl.s);
}

/* ------------------------------------------------------------------------- */
/* VLA */

static void vla_restore(int loc)
{
    if (loc)
        gen_vla_sp_restore(loc);
}

static void vla_leave(struct scope *o)
{
    if (o->vla.num < cur_scope->vla.num)
        vla_restore(o->vla.loc);
}

/* ------------------------------------------------------------------------- */
/* local scopes */

void new_scope(struct scope *o)
{
    /* copy and link previous scope */
    *o = *cur_scope;
    o->prev = cur_scope;
    cur_scope = o;

    /* record local declaration stack position */
    o->lstk = local_stack;
    o->llstk = local_label_stack;

    ++local_scope;
}

void prev_scope(struct scope *o, int is_expr)
{
    vla_leave(o->prev);

    if (o->cl.s != o->prev->cl.s)
        block_cleanup(o->prev);

    /* pop locally defined labels */
    label_pop(&local_label_stack, o->llstk, is_expr);

    /* In the is_expr case (a statement expression is finished here),
       vtop might refer to symbols on the local_stack.  Either via the
       type or via vtop->sym.  We can't pop those nor any that in turn
       might be referred to.  To make it easier we don't roll back
       any symbols in that case; some upper level call to block() will
       do that.  We do have to remove such symbols from the lookup
       tables, though.  sym_pop will do that.  */

    /* pop locally defined symbols */
    pop_local_syms(&local_stack, o->lstk, is_expr, 0);

    cur_scope = o->prev;
    --local_scope;
}

/* leave a scope via break/continue(/goto) */
void leave_scope(struct scope *o)
{
    if (!o)
        return;
    try_call_scope_cleanup(o->cl.s);
    vla_leave(o);
}

/* ------------------------------------------------------------------------- */
/* call block from 'for do while' loops */

static void lblock(int *bsym, int *csym)
{
    struct scope *lo = loop_scope, *co = cur_scope;
    int *b = co->bsym, *c = co->csym;
    if (csym) {
        co->csym = csym;
        loop_scope = co;
    }
    co->bsym = bsym;
    block(0);
    co->bsym = b;
    if (csym) {
        co->csym = c;
        loop_scope = lo;
    }
}

ST_FUNC void block(int is_expr)
{
    int a, b, c, d, e, t;
    struct scope o;
    Sym *s;

    if (is_expr) {
        /* default return value is (void) */
        vpushi(0);
        vtop->type.t = VT_VOID;
    }

again:
    t = tok, next();

    if (t == TOK_IF) {
        /* An else-if chain is sequential, but recursively parsing each
           else retains a block frame for every preceding condition.
           Keep their end jumps in one chain instead.  Nested statements
           still use block(), preserving dangling-else association. */
        d = 0;
        for (;;) {
            skip('(');
            gexpr();
            skip(')');
            a = gvtst(1, 0);
            block(0);
            if (tok != TOK_ELSE) {
                gsym(a);
                break;
            }
            d = gjmp(d);
            gsym(a);
            next();
            if (tok != TOK_IF) {
                block(0);
                break;
            }
            next();
        }
        gsym(d);
    } else if (t == TOK_WHILE) {
        d = gind();
        skip('(');
        gexpr();
        skip(')');
        a = gvtst(1, 0);
        b = 0;
        lblock(&a, &b);
        gjmp_addr(d);
        gsym_addr(b, d);
        gsym(a);

    } else if (t == '{') {
        new_scope(&o);

        /* handle local labels declarations */
        while (tok == TOK_LABEL) {
            do {
                next();
                if (tok < TOK_UIDENT)
                    expect("label identifier");
                label_push(&local_label_stack, tok, LABEL_DECLARED);
                next();
            } while (tok == ',');
            skip(';');
        }

        while (tok != '}') {
	    decl(VT_LOCAL);
            if (tok != '}') {
                if (is_expr)
                    vpop();
                block(is_expr);
            }
        }

        prev_scope(&o, is_expr);
        if (local_scope)
            next();
        else if (!nocode_wanted)
            check_func_return();

    } else if (t == TOK_RETURN) {
        b = (func_vt.t & VT_BTYPE) != VT_VOID;
        if (tok != ';') {
            gexpr();
            if (b) {
                gen_assign_cast(&func_vt);
            } else {
                if (vtop->type.t != VT_VOID)
                    tcc_warning("void function returns a value");
                vtop--;
            }
        } else if (b) {
            tcc_warning("'return' with no value");
            b = 0;
        }
        leave_scope(root_scope);
        if (b)
            gfunc_return(&func_vt);
        skip(';');
        /* jump unless last stmt in top-level block */
        if (tok != '}' || local_scope != 1)
            rsym = gjmp(rsym);
        CODE_OFF();

    } else if (t == TOK_BREAK) {
        /* compute jump */
        if (!cur_scope->bsym)
            tcc_error("cannot break");
        if (cur_switch && cur_scope->bsym == cur_switch->bsym)
            leave_scope(cur_switch->scope);
        else
            leave_scope(loop_scope);
        *cur_scope->bsym = gjmp(*cur_scope->bsym);
        skip(';');

    } else if (t == TOK_CONTINUE) {
        /* compute jump */
        if (!cur_scope->csym)
            tcc_error("cannot continue");
        leave_scope(loop_scope);
        *cur_scope->csym = gjmp(*cur_scope->csym);
        skip(';');

    } else if (t == TOK_FOR) {
        new_scope(&o);

        skip('(');
        if (tok != ';') {
            /* c99 for-loop init decl? */
            if (!decl0(VT_LOCAL, 1, NULL)) {
                /* no, regular for-loop init expr */
                gexpr();
                vpop();
            }
        }
        skip(';');
        a = b = 0;
        c = d = gind();
        if (tok != ';') {
            gexpr();
            a = gvtst(1, 0);
        }
        skip(';');
        if (tok != ')') {
            e = gjmp(0);
            d = gind();
            gexpr();
            vpop();
            gjmp_addr(c);
            gsym(e);
        }
        skip(')');
        lblock(&a, &b);
        gjmp_addr(d);
        gsym_addr(b, d);
        gsym(a);
        prev_scope(&o, 0);

    } else if (t == TOK_DO) {
        a = b = 0;
        d = gind();
        lblock(&a, &b);
        gsym(b);
        skip(TOK_WHILE);
        skip('(');
	gexpr();
        skip(')');
        skip(';');
	c = gvtst(0, 0);
	gsym_addr(c, d);
        gsym(a);

    } else if (t == TOK_SWITCH) {
        struct switch_t *sw;

        sw = tcc_mallocz(sizeof *sw);
        sw->bsym = &a;
        sw->scope = cur_scope;
        sw->prev = cur_switch;
        cur_switch = sw;

        skip('(');
        gexpr();
        skip(')');
        sw->sv = *vtop--; /* save switch value */

        a = 0;
        b = gjmp(0); /* jump to first case */
        lblock(&a, NULL);
        a = gjmp(a); /* add implicit break */
        /* case lookup */
        gsym(b);

        qsort(sw->p, sw->n, sizeof(void*), case_cmp);
        for (b = 1; b < sw->n; b++)
            if (sw->p[b - 1]->v2 >= sw->p[b]->v1)
                tcc_error("duplicate case value");

        /* Our switch table sorting is signed, so the compared
           value needs to be as well when it's 64bit.  */
        vpushv(&sw->sv);
        if ((vtop->type.t & VT_BTYPE) == VT_LLONG)
            vtop->type.t &= ~VT_UNSIGNED;
        gv(RC_INT);
        d = 0, gcase(sw->p, sw->n, &d);
        vpop();
        if (sw->def_sym)
            gsym_addr(d, sw->def_sym);
        else
            gsym(d);
        /* break label */
        gsym(a);

        dynarray_reset(&sw->p, &sw->n);
        cur_switch = sw->prev;
        tcc_free(sw);

    } else if (t == TOK_CASE) {
        struct case_t *cr = tcc_malloc(sizeof(struct case_t));
        if (!cur_switch)
            expect("switch");
        cr->v1 = cr->v2 = expr_const64();
        if (gnu_ext && tok == TOK_DOTS) {
            next();
            cr->v2 = expr_const64();
            if (cr->v2 < cr->v1)
                tcc_warning("empty case range");
        }
        cr->sym = gind();
        dynarray_add(&cur_switch->p, &cur_switch->n, cr);
        skip(':');
        is_expr = 0;
        goto block_after_label;

    } else if (t == TOK_DEFAULT) {
        if (!cur_switch)
            expect("switch");
        if (cur_switch->def_sym)
            tcc_error("too many 'default'");
        cur_switch->def_sym = gind();
        skip(':');
        is_expr = 0;
        goto block_after_label;

    } else if (t == TOK_GOTO) {
        vla_restore(root_scope->vla.loc);
        if (tok == '*' && gnu_ext) {
            /* computed goto */
            next();
            gexpr();
            if ((vtop->type.t & VT_BTYPE) != VT_PTR)
                expect("pointer");
            ggoto();

        } else if (tok >= TOK_UIDENT) {
	    s = label_find(tok);
	    /* put forward definition if needed */
            if (!s)
              s = label_push(&global_label_stack, tok, LABEL_FORWARD);
            else if (s->r == LABEL_DECLARED)
              s->r = LABEL_FORWARD;

	    if (s->r & LABEL_FORWARD) {
		/* start new goto chain for cleanups, linked via label->next */
		if (cur_scope->cl.s && !nocode_wanted) {
                    sym_push2(&pending_gotos, SYM_FIELD, 0, cur_scope->cl.n);
                    pending_gotos->prev_tok = s;
                    s = sym_push2(&s->next, SYM_FIELD, 0, 0);
                    pending_gotos->next = s;
                }
		s->jnext = gjmp(s->jnext);
	    } else {
		try_call_cleanup_goto(s->cleanupstate);
		gjmp_addr(s->jnext);
	    }
	    next();

        } else {
            expect("label identifier");
        }
        skip(';');

    } else if (t == TOK_ASM1 || t == TOK_ASM2 || t == TOK_ASM3) {
        asm_instr();

    } else {
        if (tok == ':' && t >= TOK_UIDENT) {
            /* label case */
	    next();
            s = label_find(t);
            if (s) {
                if (s->r == LABEL_DEFINED)
                    tcc_error("duplicate label '%s'", get_tok_str(s->v, NULL));
                s->r = LABEL_DEFINED;
		if (s->next) {
		    Sym *pcl; /* pending cleanup goto */
		    for (pcl = s->next; pcl; pcl = pcl->prev)
		      gsym(pcl->jnext);
		    sym_pop(&s->next, NULL, 0);
		} else
		  gsym(s->jnext);
            } else {
                s = label_push(&global_label_stack, t, LABEL_DEFINED);
            }
            s->jnext = gind();
            s->cleanupstate = cur_scope->cl.s;

    block_after_label:
            vla_restore(cur_scope->vla.loc);
            /* we accept this, but it is a mistake */
            if (tok == '}') {
                tcc_warning("deprecated use of label at end of compound statement");
            } else {
                goto again;
            }

        } else {
            /* expression case */
            if (t != ';') {
                unget_tok(t);
                if (is_expr) {
                    vpop();
                    gexpr();
                } else {
                    gexpr();
                    vpop();
                }
                skip(';');
            }
        }
    }
}

/* This skips over a stream of tokens containing balanced {} and ()
   pairs, stopping at outer ',' ';' and '}' (or matching '}' if we started
   with a '{').  If STR then allocates and stores the skipped tokens
   in *STR.  This doesn't check if () and {} are nested correctly,
   i.e. "({)}" is accepted.  */
ST_FUNC void skip_or_save_block(TokenString **str)
{
    int braces = tok == '{';
    int level = 0;
    if (str)
      *str = tok_str_alloc();

    while ((level > 0 || (tok != '}' && tok != ',' && tok != ';' && tok != ')'))) {
	int t;
	if (tok == TOK_EOF) {
	     if (str || level > 0)
	       tcc_error("unexpected end of file");
	     else
	       break;
	}
	if (str)
	  tok_str_add_tok(*str);
	t = tok;
	next();
	if (t == '{' || t == '(') {
	    level++;
	} else if (t == '}' || t == ')') {
	    level--;
	    if (level == 0 && braces && t == '}')
	      break;
	}
    }
    if (str) {
	tok_str_add(*str, -1);
	tok_str_add(*str, 0);
    }
}


static void parse_init_elem(int expr_type)
{
    int saved_global_expr;
    switch(expr_type) {
    case EXPR_CONST:
        /* compound literals must be allocated globally in this case */
        saved_global_expr = global_expr;
        global_expr = 1;
        expr_const1();
        global_expr = saved_global_expr;
        /* NOTE: symbols are accepted, as well as lvalue for anon symbols
	   (compound literals).  */
        if (((vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST
	     && ((vtop->r & (VT_SYM|VT_LVAL)) != (VT_SYM|VT_LVAL)
		 || vtop->sym->v < SYM_FIRST_ANOM))
#ifdef TCC_TARGET_PE
                 || ((vtop->r & VT_SYM) && vtop->sym->a.dllimport)
#endif
            )
            tcc_error("initializer element is not constant");
        break;
    case EXPR_ANY:
        expr_eq();
        break;
    }
}

/* put zeros for variable based init */
static void init_putz(Section *sec, unsigned long c, int size)
{
    if (sec) {
        /* nothing to do because globals are already set to zero */
    } else {
        vpush_global_sym(&func_old_type, TOK_memset);
        vseti(VT_LOCAL, c);
#if defined(TCC_TARGET_ARM) || defined(TCC_TARGET_ARM_THUMB)
        vpushs(size);
        vpushi(0);
#else
        vpushi(0);
        vpushs(size);
#endif
        gfunc_call(3);
    }
}

#define DIF_FIRST     1
#define DIF_SIZE_ONLY 2
#define DIF_HAVE_ELEM 4

/* t is the array or struct type. c is the array or struct
   address. cur_field is the pointer to the current
   field, for arrays the 'c' member contains the current start
   index.  'flags' is as in decl_initializer.
   'al' contains the already initialized length of the
   current container (starting at c).  This returns the new length of that.  */
static int decl_designator(CType *type, Section *sec, unsigned long c,
                           Sym **cur_field, int flags, int al)
{
    Sym *s, *f;
    int index, index_last, align, l, nb_elems, elem_size;
    unsigned long corig = c;

    elem_size = 0;
    nb_elems = 1;

    if (flags & DIF_HAVE_ELEM)
        goto no_designator;

    if (gnu_ext && tok >= TOK_UIDENT) {
        l = tok, next();
        if (tok == ':')
            goto struct_field;
        unget_tok(l);
    }

    /* NOTE: we only support ranges for last designator */
    while (nb_elems == 1 && (tok == '[' || tok == '.')) {
        if (tok == '[') {
            if (!(type->t & VT_ARRAY))
                expect("array type");
            next();
            index = index_last = expr_const();
            if (tok == TOK_DOTS && gnu_ext) {
                next();
                index_last = expr_const();
            }
            skip(']');
            s = type->ref;
	    if (index < 0 || (s->c >= 0 && index_last >= s->c) ||
		index_last < index)
	        tcc_error("invalid index");
            if (cur_field)
		(*cur_field)->c = index_last;
            type = pointed_type(type);
            elem_size = type_size(type, &align);
            c += index * elem_size;
            nb_elems = index_last - index + 1;
        } else {
            int cumofs;
            next();
            l = tok;
        struct_field:
            next();
            if ((type->t & VT_BTYPE) != VT_STRUCT)
                expect("struct/union type");
            cumofs = 0;
	    f = find_field(type, l, &cumofs);
            if (!f)
                expect("field");
            if (cur_field)
                *cur_field = f;
	    type = &f->type;
            c += cumofs + f->c;
        }
        cur_field = NULL;
    }
    if (!cur_field) {
        if (tok == '=') {
            next();
        } else if (!gnu_ext) {
	    expect("=");
        }
    } else {
    no_designator:
        if (type->t & VT_ARRAY) {
	    index = (*cur_field)->c;
	    if (type->ref->c >= 0 && index >= type->ref->c)
	        tcc_error("index too large");
            type = pointed_type(type);
            c += index * type_size(type, &align);
        } else {
            f = *cur_field;
	    while (f && (f->v & SYM_FIRST_ANOM) && (f->type.t & VT_BITFIELD))
	        *cur_field = f = f->next;
            if (!f)
                tcc_error("too many field init");
	    type = &f->type;
            c += f->c;
        }
    }
    /* must put zero in holes (note that doing it that way
       ensures that it even works with designators) */
    if (!(flags & DIF_SIZE_ONLY) && c - corig > al)
	init_putz(sec, corig + al, c - corig - al);
    decl_initializer(type, sec, c, flags & ~DIF_FIRST);

    /* XXX: make it more general */
    if (!(flags & DIF_SIZE_ONLY) && nb_elems > 1) {
        unsigned long c_end;
        uint8_t *src, *dst;
        int i;

        if (!sec) {
	    vset(type, VT_LOCAL|VT_LVAL, c);
	    for (i = 1; i < nb_elems; i++) {
		vset(type, VT_LOCAL|VT_LVAL, c + elem_size * i);
		vswap();
		vstore();
	    }
	    vpop();
        } else if (!NODATA_WANTED) {
	    c_end = c + nb_elems * elem_size;
	    if (c_end > sec->data_allocated)
	        section_realloc(sec, c_end);
	    src = sec->data + c;
	    dst = src;
	    for(i = 1; i < nb_elems; i++) {
		dst += elem_size;
		memcpy(dst, src, elem_size);
	    }
	}
    }
    c += nb_elems * type_size(type, &align);
    if (c - corig > al)
      al = c - corig;
    return al;
}

/* store a value or an expression directly in global data or in local array */
ST_FUNC void init_putv(CType *type, Section *sec, unsigned long c)
{
    int bt;
    void *ptr;
    CType dtype;

    dtype = *type;
    dtype.t &= ~VT_CONSTANT; /* need to do that to avoid false warning */

    if (sec) {
	int size, align;
        /* XXX: not portable */
        /* XXX: generate error if incorrect relocation */
        gen_assign_cast(&dtype);
        bt = type->t & VT_BTYPE;

        if ((vtop->r & VT_SYM)
            && bt != VT_PTR
            && bt != VT_FUNC
            && (bt != (PTR_SIZE == 8 ? VT_LLONG : VT_INT)
                || (type->t & VT_BITFIELD))
            && !((vtop->r & VT_CONST) && vtop->sym->v >= SYM_FIRST_ANOM)
            )
            tcc_error("initializer element is not computable at load time");

        if (NODATA_WANTED) {
            vtop--;
            return;
        }

	size = type_size(type, &align);
	section_reserve(sec, c + size);
        ptr = sec->data + c;

        /* XXX: make code faster ? */
	if ((vtop->r & (VT_SYM|VT_CONST)) == (VT_SYM|VT_CONST) &&
	    vtop->sym->v >= SYM_FIRST_ANOM &&
	    /* XXX This rejects compound literals like
	       '(void *){ptr}'.  The problem is that '&sym' is
	       represented the same way, which would be ruled out
	       by the SYM_FIRST_ANOM check above, but also '"string"'
	       in 'char *p = "string"' is represented the same
	       with the type being VT_PTR and the symbol being an
	       anonymous one.  That is, there's no difference in vtop
	       between '(void *){x}' and '&(void *){x}'.  Ignore
	       pointer typed entities here.  Hopefully no real code
	       will every use compound literals with scalar type.  */
	    (vtop->type.t & VT_BTYPE) != VT_PTR) {
	    /* These come from compound literals, memcpy stuff over.  */
	    Section *ssec;
	    ElfSym *esym;
	    ElfW_Rel *rel;
	    esym = elfsym(vtop->sym);
	    ssec = tcc_state->sections[esym->st_shndx];
	    memmove (ptr, ssec->data + esym->st_value, size);
	    if (ssec->reloc) {
		/* We need to copy over all memory contents, and that
		   includes relocations.  Use the fact that relocs are
		   created it order, so look from the end of relocs
		   until we hit one before the copied region.  */
		int num_relocs = ssec->reloc->data_offset / sizeof(*rel);
		rel = (ElfW_Rel*)(ssec->reloc->data + ssec->reloc->data_offset);
		while (num_relocs--) {
		    rel--;
		    if (rel->r_offset >= esym->st_value + size)
		      continue;
		    if (rel->r_offset < esym->st_value)
		      break;
		    /* Note: if the same fields are initialized multiple
		       times (possible with designators) then we possibly
		       add multiple relocations for the same offset here.
		       That would lead to wrong code, the last reloc needs
		       to win.  We clean this up later after the whole
		       initializer is parsed.  */
		    put_elf_reloca(symtab_section, sec,
				   c + rel->r_offset - esym->st_value,
				   ELFW(R_TYPE)(rel->r_info),
				   ELFW(R_SYM)(rel->r_info),
#if PTR_SIZE == 8
				   rel->r_addend
#else
				   0
#endif
				  );
		}
	    }
	} else {
            if (type->t & VT_BITFIELD) {
                int bit_pos, bit_size, bits, n;
                unsigned char *p, v, m;
                bit_pos = BIT_POS(vtop->type.t);
                bit_size = BIT_SIZE(vtop->type.t);
                p = (unsigned char*)ptr + (bit_pos >> 3);
                bit_pos &= 7, bits = 0;
                while (bit_size) {
                    n = 8 - bit_pos;
                    if (n > bit_size)
                        n = bit_size;
                    v = vtop->c.i >> bits << bit_pos;
                    m = ((1 << n) - 1) << bit_pos;
                    *p = (*p & ~m) | (v & m);
                    bits += n, bit_size -= n, bit_pos = 0, ++p;
                }
            } else
            switch(bt) {
		/* XXX: when cross-compiling we assume that each type has the
		   same representation on host and target, which is likely to
		   be wrong in the case of long double */
	    case VT_BOOL:
		vtop->c.i = vtop->c.i != 0;
	    case VT_BYTE:
		*(char *)ptr |= vtop->c.i;
		break;
	    case VT_SHORT:
		*(short *)ptr |= vtop->c.i;
		break;
	    case VT_FLOAT:
		*(float*)ptr = vtop->c.f;
		break;
	    case VT_DOUBLE:
		*(double *)ptr = vtop->c.d;
		break;
	    case VT_LDOUBLE:
#if defined TCC_IS_NATIVE_387
                if (sizeof (long double) >= 10) /* zero pad ten-byte LD */
                    memcpy(ptr, &vtop->c.ld, 10);
#ifdef __TINYC__
                else if (sizeof (long double) == sizeof (double))
                    __asm__("fldl %1\nfstpt %0\n" : "=m" (*ptr) : "m" (vtop->c.ld));
#endif
                else if (vtop->c.ld == 0.0)
                    ;
                else
#endif
                if (sizeof(long double) == LDOUBLE_SIZE)
		    *(long double*)ptr = vtop->c.ld;
                else if (sizeof(double) == LDOUBLE_SIZE)
		    *(double *)ptr = (double)vtop->c.ld;
                else
                    tcc_error("can't cross compile long double constants");
		break;
#if PTR_SIZE != 8
	    case VT_LLONG:
		*(long long *)ptr |= vtop->c.i;
		break;
#else
	    case VT_LLONG:
#endif
	    case VT_PTR:
		{
		    addr_t val = vtop->c.i;
#if PTR_SIZE == 8
		    if (vtop->r & VT_SYM)
		      greloca(sec, vtop->sym, c, R_DATA_PTR, val);
		    else
		      *(addr_t *)ptr |= val;
#else
		    if (vtop->r & VT_SYM)
		      greloc(sec, vtop->sym, c, R_DATA_PTR);
		    *(addr_t *)ptr |= val;
#endif
		    break;
		}
	    default:
		{
		    int val = vtop->c.i;
#if PTR_SIZE == 8
		    if (vtop->r & VT_SYM)
		      greloca(sec, vtop->sym, c, R_DATA_PTR, val);
		    else
		      *(int *)ptr |= val;
#else
		    if (vtop->r & VT_SYM)
		      greloc(sec, vtop->sym, c, R_DATA_PTR);
		    *(int *)ptr |= val;
#endif
		    break;
		}
	    }
	}
        vtop--;
    } else {
        vset(&dtype, VT_LOCAL|VT_LVAL, c);
        vswap();
        vstore();
        vpop();
    }
}

/* 't' contains the type and storage info. 'c' is the offset of the
   object in section 'sec'. If 'sec' is NULL, it means stack based
   allocation. 'flags & DIF_FIRST' is true if array '{' must be read (multi
   dimension implicit array init handling). 'flags & DIF_SIZE_ONLY' is true if
   size only evaluation is wanted (only for arrays). */
ST_FUNC void decl_initializer(CType *type, Section *sec, unsigned long c, 
                             int flags)
{
    int len, n, no_oblock, nb, i;
    int size1, align1;
    Sym *s, *f;
    Sym indexsym;
    CType *t1;

    if (!(flags & DIF_HAVE_ELEM) && tok != '{' &&
	/* In case of strings we have special handling for arrays, so
	   don't consume them as initializer value (which would commit them
	   to some anonymous symbol).  */
	tok != TOK_LSTR && tok != TOK_STR &&
	!(flags & DIF_SIZE_ONLY)) {
	parse_init_elem(!sec ? EXPR_ANY : EXPR_CONST);
        flags |= DIF_HAVE_ELEM;
    }

    if ((flags & DIF_HAVE_ELEM) &&
	!(type->t & VT_ARRAY) &&
	/* Use i_c_parameter_t, to strip toplevel qualifiers.
	   The source type might have VT_CONSTANT set, which is
	   of course assignable to non-const elements.  */
	is_compatible_unqualified_types(type, &vtop->type)) {
        init_putv(type, sec, c);
    } else if (type->t & VT_ARRAY) {
        s = type->ref;
        n = s->c;
        t1 = pointed_type(type);
        size1 = type_size(t1, &align1);

        no_oblock = 1;
        if (((flags & DIF_FIRST) && tok != TOK_LSTR && tok != TOK_STR) ||
            tok == '{') {
            if (tok != '{')
                tcc_error("character array initializer must be a literal,"
                    " optionally enclosed in braces");
            skip('{');
            no_oblock = 0;
        }

        /* only parse strings here if correct type (otherwise: handle
           them as ((w)char *) expressions */
        if ((tok == TOK_LSTR && 
#ifdef TCC_TARGET_PE
             (t1->t & VT_BTYPE) == VT_SHORT && (t1->t & VT_UNSIGNED)
#else
             (t1->t & VT_BTYPE) == VT_INT
#endif
            ) || (tok == TOK_STR && (t1->t & VT_BTYPE) == VT_BYTE)) {
	    len = 0;
            while (tok == TOK_STR || tok == TOK_LSTR) {
                int cstr_len, ch;

                /* compute maximum number of chars wanted */
                if (tok == TOK_STR)
                    cstr_len = tokc.str.size;
                else
                    cstr_len = tokc.str.size / sizeof(nwchar_t);
                cstr_len--;
                nb = cstr_len;
                if (n >= 0 && nb > (n - len))
                    nb = n - len;
                if (!(flags & DIF_SIZE_ONLY)) {
                    if (cstr_len > nb)
                        tcc_warning("initializer-string for array is too long");
                    /* in order to go faster for common case (char
                       string in global variable, we handle it
                       specifically */
                    if (sec && tok == TOK_STR && size1 == 1) {
                        if (!NODATA_WANTED)
                            memcpy(sec->data + c + len, tokc.str.data, nb);
                    } else {
                        for(i=0;i<nb;i++) {
                            if (tok == TOK_STR)
                                ch = ((unsigned char *)tokc.str.data)[i];
                            else
                                ch = ((nwchar_t *)tokc.str.data)[i];
			    vpushi(ch);
                            init_putv(t1, sec, c + (len + i) * size1);
                        }
                    }
                }
                len += nb;
                next();
            }
            /* only add trailing zero if enough storage (no
               warning in this case since it is standard) */
            if (n < 0 || len < n) {
                if (!(flags & DIF_SIZE_ONLY)) {
		    vpushi(0);
                    init_putv(t1, sec, c + (len * size1));
                }
                len++;
            }
	    len *= size1;
        } else {
	    indexsym.c = 0;
	    f = &indexsym;

          do_init_list:
	    len = 0;
	    while (tok != '}' || (flags & DIF_HAVE_ELEM)) {
		len = decl_designator(type, sec, c, &f, flags, len);
		flags &= ~DIF_HAVE_ELEM;
		if (type->t & VT_ARRAY) {
		    ++indexsym.c;
		    /* special test for multi dimensional arrays (may not
		       be strictly correct if designators are used at the
		       same time) */
		    if (no_oblock && len >= n*size1)
		        break;
		} else {
		    if (s->type.t == VT_UNION)
		        f = NULL;
		    else
		        f = f->next;
		    if (no_oblock && f == NULL)
		        break;
		}

		if (tok == '}')
		    break;
		skip(',');
	    }
        }
        /* put zeros at the end */
	if (!(flags & DIF_SIZE_ONLY) && len < n*size1)
	    init_putz(sec, c + len, n*size1 - len);
        if (!no_oblock)
            skip('}');
        /* patch type size if needed, which happens only for array types */
        if (n < 0)
            s->c = size1 == 1 ? len : ((len + size1 - 1)/size1);
    } else if ((type->t & VT_BTYPE) == VT_STRUCT) {
	size1 = 1;
        no_oblock = 1;
        if ((flags & DIF_FIRST) || tok == '{') {
            skip('{');
            no_oblock = 0;
        }
        s = type->ref;
        f = s->next;
        n = s->c;
	goto do_init_list;
    } else if (tok == '{') {
        if (flags & DIF_HAVE_ELEM)
          skip(';');
        next();
        decl_initializer(type, sec, c, flags & ~DIF_HAVE_ELEM);
        skip('}');
    } else if ((flags & DIF_SIZE_ONLY)) {
	/* If we supported only ISO C we wouldn't have to accept calling
	   this on anything than an array if DIF_SIZE_ONLY (and even then
	   only on the outermost level, so no recursion would be needed),
	   because initializing a flex array member isn't supported.
	   But GNU C supports it, so we need to recurse even into
	   subfields of structs and arrays when DIF_SIZE_ONLY is set.  */
        /* just skip expression */
        skip_or_save_block(NULL);
    } else {
	if (!(flags & DIF_HAVE_ELEM)) {
	    /* This should happen only when we haven't parsed
	       the init element above for fear of committing a
	       string constant to memory too early.  */
	    if (tok != TOK_STR && tok != TOK_LSTR)
	      expect("string constant");
	    parse_init_elem(!sec ? EXPR_ANY : EXPR_CONST);
	}
        init_putv(type, sec, c);
    }
}

/* parse an initializer for type 't' if 'has_init' is non zero, and
   allocate space in local or global data space ('r' is either
   VT_LOCAL or VT_CONST). If 'v' is non zero, then an associated
   variable 'v' of scope 'scope' is declared before initializers
   are parsed. If 'v' is zero, then a reference to the new object
   is put in the value stack. If 'has_init' is 2, a special parsing
   is done to handle string constants. */
ST_FUNC void decl_initializer_alloc(CType *type, AttributeDef *ad, int r, 
                                   int has_init, int v, int scope)
{
    int size, align, addr;
    TokenString *init_str = NULL;

    Section *sec;
    Sym *flexible_array;
    Sym *sym = NULL;
    int saved_nocode_wanted = nocode_wanted;
#ifdef CONFIG_TCC_BCHECK
    int bcheck = tcc_state->do_bounds_check && !NODATA_WANTED;
#endif

    /* Always allocate static or global variables */
    if (v && (r & VT_VALMASK) == VT_CONST)
        nocode_wanted |= 0x80000000;

    flexible_array = NULL;
    if ((type->t & VT_BTYPE) == VT_STRUCT) {
        Sym *field = type->ref->next;
        if (field) {
            while (field->next)
                field = field->next;
            if (field->type.t & VT_ARRAY && field->type.ref->c < 0)
                flexible_array = field;
        }
    }

    size = type_size(type, &align);
    /* If unknown size, we must evaluate it before
       evaluating initializers because
       initializers can generate global data too
       (e.g. string pointers or ISOC99 compound
       literals). It also simplifies local
       initializers handling */
    if (size < 0 || (flexible_array && has_init)) {
        if (!has_init) 
            tcc_error("unknown type size");
        /* get all init string */
        if (has_init == 2) {
	    init_str = tok_str_alloc();
            /* only get strings */
            while (tok == TOK_STR || tok == TOK_LSTR) {
                tok_str_add_tok(init_str);
                next();
            }
	    tok_str_add(init_str, -1);
	    tok_str_add(init_str, 0);
        } else {
	    skip_or_save_block(&init_str);
        }
        unget_tok(0);

        /* compute size */
        begin_macro(init_str, 1);
        next();
        decl_initializer(type, NULL, 0, DIF_FIRST | DIF_SIZE_ONLY);
        /* prepare second initializer parsing */
        macro_ptr = init_str->str;
        next();
        
        /* if still unknown size, error */
        size = type_size(type, &align);
        if (size < 0) 
            tcc_error("unknown type size");
    }
    /* If there's a flex member and it was used in the initializer
       adjust size.  */
    if (flexible_array &&
	flexible_array->type.ref->c > 0)
        size += flexible_array->type.ref->c
	        * pointed_size(&flexible_array->type);
    /* take into account specified alignment if bigger */
    if (ad->a.aligned) {
	int speca = 1 << (ad->a.aligned - 1);
        if (speca > align)
            align = speca;
    } else if (ad->a.packed) {
        align = 1;
    }

    if (!v && NODATA_WANTED)
        size = 0, align = 1;

    if ((r & VT_VALMASK) == VT_LOCAL) {
        sec = NULL;
#ifdef CONFIG_TCC_BCHECK
        if (bcheck && v) {
            /* add padding between stack variables for bound checking */
            loc--;
        }
#endif
        loc = (loc - size) & -align;
        addr = loc;
#ifdef CONFIG_TCC_BCHECK
        if (bcheck && v) {
            /* add padding between stack variables for bound checking */
            loc--;
        }
#endif
        if (v) {
            /* local variable */
#ifdef CONFIG_TCC_ASM
	    if (ad->asm_label) {
		int reg = asm_parse_regvar(ad->asm_label);
		if (reg >= 0)
		    r = (r & ~VT_VALMASK) | reg;
	    }
#endif
            sym = sym_push(v, type, r, addr);
	    if (ad->cleanup_func) {
		Sym *cls = sym_push2(&all_cleanups,
                    SYM_FIELD | ++cur_scope->cl.n, 0, 0);
		cls->prev_tok = sym;
		cls->next = ad->cleanup_func;
		cls->ncl = cur_scope->cl.s;
		cur_scope->cl.s = cls;
	    }

            sym->a = ad->a;
        } else {
            /* push local reference */
            vset(type, r, addr);
        }
    } else {
        if (v && scope == VT_CONST) {
            /* see if the symbol was already defined */
            sym = sym_find(v);
            if (sym) {
                patch_storage(sym, ad, type);
                /* we accept several definitions of the same global variable. */
                if (!has_init && sym->c && elfsym(sym)->st_shndx != SHN_UNDEF)
                    goto no_alloc;
            }
        }

        /* allocate symbol in corresponding section */
        sec = ad->section;
        if (!sec) {
            if (has_init)
                sec = data_section;
            else if (tcc_state->nocommon)
                sec = bss_section;
        }

        if (sec) {
	    addr = section_add(sec, size, align);
#ifdef CONFIG_TCC_BCHECK
            /* add padding if bound check */
            if (bcheck)
                section_add(sec, 1, 1);
#endif
        } else {
            addr = align; /* SHN_COMMON is special, symbol value is align */
	    sec = common_section;
        }

        if (v) {
            if (!sym) {
                sym = sym_push(v, type, r | VT_SYM, 0);
                patch_storage(sym, ad, NULL);
            }
            /* update symbol definition */
	    put_extern_sym(sym, sec, addr, size);
        } else {
            /* push global reference */
            vpush_ref(type, sec, addr, size);
            sym = vtop->sym;
	    vtop->r |= r;
        }

#ifdef CONFIG_TCC_BCHECK
        /* handles bounds now because the symbol must be defined
           before for the relocation */
        if (bcheck) {
            addr_t *bounds_ptr;

            greloca(bounds_section, sym, bounds_section->data_offset, R_DATA_PTR, 0);
            /* then add global bound info */
            bounds_ptr = section_ptr_add(bounds_section, 2 * sizeof(addr_t));
            bounds_ptr[0] = 0; /* relocated */
            bounds_ptr[1] = size;
        }
#endif
    }

    if (type->t & VT_VLA) {
        int a;

        if (NODATA_WANTED)
            goto no_alloc;

        /* save current stack pointer */
        if (root_scope->vla.loc == 0) {
            struct scope *v = cur_scope;
            gen_vla_sp_save(loc -= PTR_SIZE);
            do v->vla.loc = loc; while ((v = v->prev));
        }

        vla_runtime_type_size(type, &a);
        gen_vla_alloc(type, a);
#if defined TCC_TARGET_PE && defined TCC_TARGET_X86_64
        /* on _WIN64, because of the function args scratch area, the
           result of alloca differs from RSP and is returned in RAX.  */
        gen_vla_result(addr), addr = (loc -= PTR_SIZE);
#endif
        gen_vla_sp_save(addr);
        cur_scope->vla.loc = addr;
        cur_scope->vla.num++;
    } else if (has_init) {
	size_t oldreloc_offset = 0;
	if (sec && sec->reloc)
	  oldreloc_offset = sec->reloc->data_offset;
        decl_initializer(type, sec, addr, DIF_FIRST);
	if (sec && sec->reloc)
	  squeeze_multi_relocs(sec, oldreloc_offset);
        /* patch flexible array member size back to -1, */
        /* for possible subsequent similar declarations */
        if (flexible_array)
            flexible_array->type.ref->c = -1;
    }

 no_alloc:
    /* restore parse state if needed */
    if (init_str) {
        end_macro();
        next();
    }

    nocode_wanted = saved_nocode_wanted;
}

/* parse a function defined by symbol 'sym' and generate its code in
   'cur_text_section' */
static void gen_function(Sym *sym)
{
    /* Initialize VLA state */
    struct scope f = { 0 };
    cur_scope = root_scope = &f;

    nocode_wanted = 0;
    ind = cur_text_section->data_offset;
    if (sym->a.aligned) {
	size_t newoff = section_add(cur_text_section, 0,
				    1 << (sym->a.aligned - 1));
	gen_fill_nops(newoff - ind);
    }
    /* NOTE: we patch the symbol size later */
#ifdef TCC_TARGET_ARM_THUMB
    /* ARM ELF function symbols carry the instruction-set bit. */
    put_extern_sym(sym, cur_text_section, ind | 1, 0);
#else
    put_extern_sym(sym, cur_text_section, ind, 0);
#endif
    if (sym->type.ref->f.func_ctor)
        add_array (tcc_state, ".init_array", sym->c);
    if (sym->type.ref->f.func_dtor)
        add_array (tcc_state, ".fini_array", sym->c);
    funcname = get_tok_str(sym->v, NULL);
    func_ind = ind;
    /* put debug symbol */
    tcc_debug_funcstart(tcc_state, sym);
    /* push a dummy symbol to enable local sym storage */
    sym_push2(&local_stack, SYM_FIELD, 0, 0);
    local_scope = 1; /* for function parameters */
    gfunc_prolog(sym);
    local_scope = 0;
    rsym = 0;
    clear_temp_local_var_list();
    MOS_TRACE("func %s begin\n", funcname);
    block(0);
    MOS_TRACE("func %s body done, code %d bytes\n", funcname, ind - func_ind);
    gsym(rsym);
    nocode_wanted = 0;
    /* reset local stack */
    pop_local_syms(&local_stack, NULL, 0,
                   sym->type.ref->f.func_type == FUNC_ELLIPSIS);
    gfunc_epilog();
    MOS_TRACE("func %s epilog done\n", funcname);
    cur_text_section->data_offset = ind;
    local_scope = 0;
    label_pop(&global_label_stack, NULL, 0);
    sym_pop(&all_cleanups, NULL, 0);
    /* patch symbol size */
    elfsym(sym)->st_size = ind - func_ind;
    /* end of function */
    tcc_debug_funcend(tcc_state, ind - func_ind);
    /* It's better to crash than to generate wrong code */
    cur_text_section = NULL;
    funcname = ""; /* for safety */
    func_vt.t = VT_VOID; /* for safety */
    func_var = 0; /* for safety */
    ind = 0; /* for safety */
    nocode_wanted = 0x80000000;
    check_vstack();
    MOS_TRACE("func end\n");
    /* do this after funcend debug info */
    next();
}

ST_FUNC void gen_inline_functions(TCCState *s)
{
    Sym *sym;
    int inline_generated, i;
    struct InlineFunc *fn;

    tcc_open_bf(s, ":inline:", 0);
    /* iterate while inline function are referenced */
    do {
        inline_generated = 0;
        for (i = 0; i < s->nb_inline_fns; ++i) {
            fn = s->inline_fns[i];
            sym = fn->sym;
            if (sym && (sym->c || !(sym->type.t & VT_INLINE))) {
                /* the function was used or forced (and then not internal):
                   generate its code and convert it to a normal function */
                fn->sym = NULL;
                tcc_debug_putfile(s, fn->filename);
                begin_macro(fn->func_str, 1);
                next();
                cur_text_section = text_section;
                gen_function(sym);
                end_macro();

                inline_generated = 1;
            }
        }
    } while (inline_generated);
    tcc_close();
}

ST_FUNC void free_inline_functions(TCCState *s)
{
    int i;
    /* free tokens of unused inline functions */
    for (i = 0; i < s->nb_inline_fns; ++i) {
        struct InlineFunc *fn = s->inline_fns[i];
        if (fn->sym)
            tok_str_free(fn->func_str);
    }
    dynarray_reset(&s->inline_fns, &s->nb_inline_fns);
}

/* 'l' is VT_LOCAL or VT_CONST to define default storage type, or VT_CMP
   if parsing old style parameter decl list (and FUNC_SYM is set then) */
ST_FUNC int decl0(int l, int is_for_loop_init, Sym *func_sym)
{
    int v, has_init, r;
    CType type, btype;
    Sym *sym;
    AttributeDef ad, adbase;

    while (1) {
	if (tok == TOK_STATIC_ASSERT) {
	    CString error_str;
	    int c;

	    next();
	    skip('(');
	    c = expr_const();

	    if (tok == ')') {
		if (!c)
		    tcc_error("_Static_assert fail");
		next();
		goto static_assert_out;
	    }

	    skip(',');
	    parse_mult_str(&error_str, "string constant");
	    if (c == 0)
		tcc_error("%s", error_str.data);
	    cstr_free(&error_str);
	    skip(')');
	  static_assert_out:
            skip(';');
	    continue;
	}
        if (!parse_btype(&btype, &adbase)) {
            if (is_for_loop_init)
                return 0;
            /* skip redundant ';' if not in old parameter decl scope */
            if (tok == ';' && l != VT_CMP) {
                next();
                continue;
            }
            if (l != VT_CONST)
                break;
            if (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3) {
                /* global asm block */
                asm_global_instr();
                continue;
            }
            if (tok >= TOK_UIDENT) {
               /* special test for old K&R protos without explicit int
                  type. Only accepted when defining global data */
                btype.t = VT_INT;
            } else {
                if (tok != TOK_EOF)
                    expect("declaration");
                break;
            }
        }
        if (tok == ';') {
	    if ((btype.t & VT_BTYPE) == VT_STRUCT) {
		int v = btype.ref->v;
		if (!(v & SYM_FIELD) && (v & ~SYM_STRUCT) >= SYM_FIRST_ANOM)
        	    tcc_warning("unnamed struct/union that defines no instances");
                next();
                continue;
	    }
            if (IS_ENUM(btype.t)) {
                next();
                continue;
            }
        }
        while (1) { /* iterate thru each declaration */
            type = btype;
	    /* If the base type itself was an array type of unspecified
	       size (like in 'typedef int arr[]; arr x = {1};') then
	       we will overwrite the unknown size by the real one for
	       this decl.  We need to unshare the ref symbol holding
	       that size.  */
	    if ((type.t & VT_ARRAY) && type.ref->c < 0) {
		type.ref = sym_push(SYM_FIELD, &type.ref->type, 0, type.ref->c);
	    }
	    ad = adbase;
            type_decl(&type, &ad, &v, TYPE_DIRECT);
#if 0
            {
                char buf[500];
                type_to_str(buf, sizeof(buf), &type, get_tok_str(v, NULL));
                printf("type = '%s'\n", buf);
            }
#endif
            if ((type.t & VT_BTYPE) == VT_FUNC) {
                if ((type.t & VT_STATIC) && (l == VT_LOCAL))
                    tcc_error("function without file scope cannot be static");
                /* if old style function prototype, we accept a
                   declaration list */
                sym = type.ref;
                if (sym->f.func_type == FUNC_OLD && l == VT_CONST)
                    decl0(VT_CMP, 0, sym);
                /* always compile 'extern inline' */
                if (type.t & VT_EXTERN)
                    type.t &= ~VT_INLINE;
            }

            if (gnu_ext && (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3)) {
                ad.asm_label = asm_label_instr();
                /* parse one last attribute list, after asm label */
                parse_attribute(&ad);
            #if 0
                /* gcc does not allow __asm__("label") with function definition,
                   but why not ... */
                if (tok == '{')
                    expect(";");
            #endif
            }

#ifdef TCC_TARGET_PE
            if (ad.a.dllimport || ad.a.dllexport) {
                if (type.t & VT_STATIC)
                    tcc_error("cannot have dll linkage with static");
                if (type.t & VT_TYPEDEF) {
                    tcc_warning("'%s' attribute ignored for typedef",
                        ad.a.dllimport ? (ad.a.dllimport = 0, "dllimport") :
                        (ad.a.dllexport = 0, "dllexport"));
                } else if (ad.a.dllimport) {
                    if ((type.t & VT_BTYPE) == VT_FUNC)
                        ad.a.dllimport = 0;
                    else
                        type.t |= VT_EXTERN;
                }
            }
#endif
            if (tok == '{') {
                if (l != VT_CONST)
                    tcc_error("cannot use local functions");
                if ((type.t & VT_BTYPE) != VT_FUNC)
                    expect("function definition");

                /* reject abstract declarators in function definition
                   make old style params without decl have int type */
                sym = type.ref;
                while ((sym = sym->next) != NULL) {
                    if (!(sym->v & ~SYM_FIELD))
                        expect("identifier");
                    if (sym->type.t == VT_VOID)
                        sym->type = int_type;
                }

                /* put function symbol */
                type.t &= ~VT_EXTERN;
                sym = external_sym(v, &type, 0, &ad);
                /* static inline functions are just recorded as a kind
                   of macro. Their code will be emitted at the end of
                   the compilation unit only if they are used */
                if (sym->type.t & VT_INLINE) {
                    struct InlineFunc *fn;
                    fn = tcc_malloc(sizeof *fn + strlen(file->filename));
                    strcpy(fn->filename, file->filename);
                    fn->sym = sym;
		    skip_or_save_block(&fn->func_str);
                    dynarray_add(&tcc_state->inline_fns,
				 &tcc_state->nb_inline_fns, fn);
                } else {
                    /* compute text section */
                    cur_text_section = ad.section;
                    if (!cur_text_section)
                        cur_text_section = text_section;
                    gen_function(sym);
                }
                break;
            } else {
		if (l == VT_CMP) {
		    /* find parameter in function parameter list */
		    for (sym = func_sym->next; sym; sym = sym->next)
			if ((sym->v & ~SYM_FIELD) == v)
			    goto found;
		    tcc_error("declaration for parameter '%s' but no such parameter",
			      get_tok_str(v, NULL));
found:
		    if (type.t & VT_STORAGE) /* 'register' is okay */
		        tcc_error("storage class specified for '%s'",
				  get_tok_str(v, NULL));
		    if (sym->type.t != VT_VOID)
		        tcc_error("redefinition of parameter '%s'",
				  get_tok_str(v, NULL));
		    convert_parameter_type(&type);
		    sym->type = type;
		} else if (type.t & VT_TYPEDEF) {
                    /* save typedefed type  */
                    /* XXX: test storage specifiers ? */
                    sym = sym_find(v);
                    if (sym && sym->sym_scope == local_scope) {
                        if (!is_compatible_types(&sym->type, &type)
                            || !(sym->type.t & VT_TYPEDEF))
                            tcc_error("incompatible redefinition of '%s'",
                                get_tok_str(v, NULL));
                        sym->type = type;
                    } else {
                        sym = sym_push(v, &type, 0, 0);
                    }
                    sym->a = ad.a;
                    sym->f = ad.f;
		} else if ((type.t & VT_BTYPE) == VT_VOID
			   && !(type.t & VT_EXTERN)) {
		    tcc_error("declaration of void object");
                } else {
                    r = 0;
                    if ((type.t & VT_BTYPE) == VT_FUNC) {
                        /* external function definition */
                        /* specific case for func_call attribute */
                        type.ref->f = ad.f;
                    } else if (!(type.t & VT_ARRAY)) {
                        /* not lvalue if array */
                        r |= VT_LVAL;
                    }
                    has_init = (tok == '=');
                    if (has_init && (type.t & VT_VLA))
                        tcc_error("variable length array cannot be initialized");
                    if (((type.t & VT_EXTERN) && (!has_init || l != VT_CONST))
		        || (type.t & VT_BTYPE) == VT_FUNC
                        /* as with GCC, uninitialized global arrays with no size
                           are considered extern: */
                        || ((type.t & VT_ARRAY) && !has_init
                            && l == VT_CONST && type.ref->c < 0)
                        ) {
                        /* external variable or function */
                        type.t |= VT_EXTERN;
                        sym = external_sym(v, &type, r, &ad);
                        if (ad.alias_target) {
                            ElfSym *esym;
                            Sym *alias_target;
                            alias_target = sym_find(ad.alias_target);
                            esym = elfsym(alias_target);
                            if (!esym)
                                tcc_error("unsupported forward __alias__ attribute");
                            put_extern_sym2(sym, esym->st_shndx, esym->st_value, esym->st_size, 0);
                        }
                    } else {
                        if (type.t & VT_STATIC)
                            r |= VT_CONST;
                        else
                            r |= l;
                        if (has_init)
                            next();
                        else if (l == VT_CONST)
                            /* uninitialized global variables may be overridden */
                            type.t |= VT_EXTERN;
                        decl_initializer_alloc(&type, &ad, r, has_init, v, l);
                    }
                }
                if (tok != ',') {
                    if (is_for_loop_init)
                        return 1;
                    skip(';');
                    break;
                }
                next();
            }
        }
    }
    return 0;
}

ST_FUNC void decl(int l)
{
    decl0(l, 0, NULL);
}

/* ------------------------------------------------------------------------- */
#undef gjmp_addr
#undef gjmp
/* ------------------------------------------------------------------------- */
