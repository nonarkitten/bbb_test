/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "support.h"
#include "m68k.h"
#include "registerallocator.h"

/* Line9 is one large SUBX/SUB/SUBA */

uint32_t *EMIT_line9(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* SUBA */
    if ((opcode & 0xf0c0) == 0x90c0)
    {
        uint8_t ext_words = 0;
        uint8_t size = (opcode & 0x0100) == 0x0100 ? 4 : 2;
        uint8_t reg = RA_MapM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);
        uint8_t tmp = 0xff;
        RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);

        if (size == 2)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
#ifdef __aarch64__
        if (size == 2)
            *ptr++ = sxth(tmp, tmp);

        *ptr++ = sub_reg(reg, reg, tmp, LSL, 0);
#else
        if (size == 2)
            *ptr++ = sxth(tmp, tmp, 0);

        *ptr++ = sub_reg(reg, reg, tmp, 0);
#endif
        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;
    }
    /* SUBX */
    else if ((opcode & 0xf130) == 0x9100)
    {
        /* Move negated C flag to ARM flags */
#ifdef __aarch64__
        uint8_t cc = RA_GetCC(&ptr);
        *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_X));
#else
        M68K_GetCC(&ptr);
        *ptr++ = tst_immed(REG_SR, SR_X);
#endif
        /* Register to register */
        if ((opcode & 0x0008) == 0)
        {
            uint8_t size = (opcode >> 6) & 3;
            uint8_t regx = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t regy = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t tmp = 0;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

            switch (size)
            {
                case 0: /* Byte */
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(tmp, tmp, regy, LSL, 24);
                    *ptr++ = subs_reg(tmp, tmp, regx, LSL, 24);
                    *ptr++ = bfxil(regy, tmp, 24, 8);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(tmp, regx, 24);
                    *ptr++ = sub_cc_immed(ARM_CC_NE, tmp, tmp, 0x401);
                    *ptr++ = rsbs_reg(tmp, tmp, regy, 24);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
                    *ptr++ = bfi(regy, tmp, 0, 8);
                    RA_FreeARMRegister(&ptr, tmp);
#endif
                    break;
                case 1: /* Word */
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(tmp, tmp, regy, LSL, 16);
                    *ptr++ = subs_reg(tmp, tmp, regx, LSL, 16);
                    *ptr++ = bfxil(regy, tmp, 16, 16);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(tmp, regx, 16);
                    *ptr++ = sub_cc_immed(ARM_CC_NE, tmp, tmp, 0x801);
                    *ptr++ = rsbs_reg(tmp, tmp, regy, 16);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
                    *ptr++ = bfi(regy, tmp, 0, 16);
                    RA_FreeARMRegister(&ptr, tmp);
#endif
                    break;
                case 2: /* Long */
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(regy, regy, tmp, LSL, 0);
                    *ptr++ = subs_reg(regy, regy, regx, LSL, 0);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = sub_cc_immed(ARM_CC_NE, regy, regy, 1);
                    *ptr++ = subs_reg(regy, regy, regx, 0);
#endif
                    break;
            }
        }
        /* memory to memory */
        else
        {
            uint8_t size = (opcode >> 6) & 3;
            uint8_t regx = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            uint8_t regy = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            uint8_t dest = RA_AllocARMRegister(&ptr);
            uint8_t src = RA_AllocARMRegister(&ptr);

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

            switch (size)
            {
                case 0: /* Byte */
                    *ptr++ = ldrb_offset_preindex(regx, src, (opcode & 7) == 7 ? -2 : -1);
                    *ptr++ = ldrb_offset_preindex(regy, dest, ((opcode >> 9) & 7) == 7 ? -2 : -1);
#ifdef __aarch64__
                    uint8_t tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(dest, tmp, dest, LSL, 24);
                    *ptr++ = subs_reg(dest, dest, src, LSL, 24);
                    *ptr++ = lsr(dest, dest, 24);
                    *ptr++ = strb_offset(regy, dest, 0);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = lsl_immed(src, src, 24);
                    *ptr++ = sub_cc_immed(ARM_CC_NE, dest, dest, 0x401);
                    *ptr++ = rsbs_reg(dest, src, dest, 24);
                    *ptr++ = lsr_immed(dest, dest, 24);
                    *ptr++ = strb_offset(regy, dest, 0);
#endif
                    break;
                case 1: /* Word */
                    *ptr++ = ldrh_offset_preindex(regx, src, -2);
                    *ptr++ = ldrh_offset_preindex(regy, dest, -2);
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(dest, tmp, dest, LSL, 16);
                    *ptr++ = subs_reg(dest, dest, src, LSL, 16);
                    *ptr++ = lsr(dest, dest, 16);
                    *ptr++ = strh_offset(regy, dest, 0);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = lsl_immed(src, src, 16);
                    *ptr++ = sub_cc_immed(ARM_CC_NE, dest, dest, 0x801);
                    *ptr++ = rsbs_reg(dest, src, dest, 16);
                    *ptr++ = lsr_immed(dest, dest, 16);
                    *ptr++ = strh_offset(regy, dest, 0);
#endif
                    break;
                case 2: /* Long */
                    *ptr++ = ldr_offset_preindex(regx, src, -4);
                    *ptr++ = ldr_offset_preindex(regy, dest, -4);
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(dest, tmp, dest, LSL, 0);
                    *ptr++ = subs_reg(dest, dest, src, LSL, 0);
                    *ptr++ = str_offset(regy, dest, 0);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = sub_cc_immed(ARM_CC_NE, dest, dest, 1);
                    *ptr++ = subs_reg(dest, dest, src, 0);
                    *ptr++ = str_offset(regy, dest, 0);
#endif
                    break;
            }

            RA_FreeARMRegister(&ptr, dest);
            RA_FreeARMRegister(&ptr, src);
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            if (update_mask & SR_X)
                ptr = EMIT_GetNZVnCX(ptr, cc, &update_mask);
            else
                ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CC);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
                else
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C | SR_X, ARM_CC_CC);
            }
        }
    }
    /* SUB */
    else if ((opcode & 0xf000) == 0x9000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
        uint8_t ext_words = 0;
#ifdef __aarch64__
        uint8_t tmp = 0xff;
#endif
        if (direction == 0)
        {
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t src = 0xff;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            if (size == 4)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            switch(size)
            {
                case 4:
#ifdef __aarch64__
                    *ptr++ = subs_reg(dest, dest, src, LSL, 0);
#else
                    *ptr++ = subs_reg(dest, dest, src, 0);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl(tmp, dest, 16);
                    *ptr++ = subs_reg(src, dest, src, LSL, 16);
                    *ptr++ = bfxil(dest, src, 16, 16);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = lsl_immed(src, src, 16);
                    *ptr++ = rsbs_reg(src, src, dest, 16);
                    *ptr++ = lsr_immed(src, src, 16);
                    *ptr++ = bfi(dest, src, 0, 16);
#endif
                    break;
                case 1:
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl(tmp, dest, 24);
                    *ptr++ = subs_reg(src, dest, src, LSL, 24);
                    *ptr++ = bfxil(dest, src, 24, 8);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = lsl_immed(src, src, 24);
                    *ptr++ = rsbs_reg(src, src, dest, 24);
                    *ptr++ = lsr_immed(src, src, 24);
                    *ptr++ = bfi(dest, src, 0, 8);
#endif
                    break;
            }

            RA_FreeARMRegister(&ptr, src);
        }
        else
        {
            uint8_t dest = 0xff;
            uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t mode = (opcode & 0x0038) >> 3;

            if (mode == 4 || mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

            /* Fetch data into temporary register, perform add, store it back */
            switch (size)
            {
            case 4:
                if (mode == 4)
                {
                    *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldr_offset(dest, tmp, 0);

                /* Perform calcualtion */
#ifdef __aarch64__
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = subs_reg(tmp, tmp, src, 0);
#endif
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = str_offset_postindex(dest, tmp, 4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = str_offset(dest, tmp, 0);
                break;
            case 2:
                if (mode == 4)
                {
                    *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrh_offset(dest, tmp, 0);
                /* Perform calcualtion */
#ifdef __aarch64__
                *ptr++ = lsl(tmp, tmp, 16);
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 16);
                *ptr++ = lsr(tmp, tmp, 16);
#else
                *ptr++ = lsl_immed(tmp, tmp, 16);
                *ptr++ = subs_reg(tmp, tmp, src, 16);
                *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strh_offset_postindex(dest, tmp, 2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strh_offset(dest, tmp, 0);
                break;
            case 1:
                if (mode == 4)
                {
                    *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrb_offset(dest, tmp, 0);

                /* Perform calcualtion */
#ifdef __aarch64__
                *ptr++ = lsl(tmp, tmp, 24);
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 24);
                *ptr++ = lsr(tmp, tmp, 24);
#else
                *ptr++ = lsl_immed(tmp, tmp, 24);
                *ptr++ = subs_reg(tmp, tmp, src, 24);
                *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, tmp, 0);
                break;
            }

            RA_FreeARMRegister(&ptr, dest);
            RA_FreeARMRegister(&ptr, tmp);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            if (update_mask & SR_X)
                ptr = EMIT_GetNZVnCX(ptr, cc, &update_mask);
            else
                ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CC);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
                else
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C | SR_X, ARM_CC_CC);
            }
        }
    }
    else
        *ptr++ = udf(opcode);


    return ptr;
}
