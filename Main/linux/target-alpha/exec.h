/*
 *  Alpha emulation cpu run-time definitions for qemu.
 *
 *  Copyright (c) 2007 Jocelyn Mayer
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__ALPHA_EXEC_H__)
#define __ALPHA_EXEC_H__

#include "config.h"

#include "dyngen-exec.h"

#define TARGET_LONG_BITS 64

register struct CPUAlphaState *env asm(AREG0);

#define FP_STATUS (env->fp_status)

#include "cpu.h"
#include "exec-all.h"

#if !defined(CONFIG_USER_ONLY)
#include "softmmu_exec.h"
#endif /* !defined(CONFIG_USER_ONLY) */

static inline int cpu_has_work(CPUState *env)
{
    return (env->interrupt_request & CPU_INTERRUPT_HARD);
}

static inline void cpu_pc_from_tb(CPUState *env, TranslationBlock *tb)
{
    env->pc = tb->pc;
}

#endif /* !defined (__ALPHA_EXEC_H__) */
