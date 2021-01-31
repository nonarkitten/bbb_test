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

uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);

uint32_t *EMIT_line8(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 1000xxx011xxxxxx - DIVU */
    if ((opcode & 0xf1c0) == 0x80c0)
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 1000xxx10000xxxx - SBCD */
    else if ((opcode & 0xf1f0) == 0x8100)
    {
        *ptr++ = udf(opcode);
    }
    /* 1000xxx10100xxxx - PACK */
    else if ((opcode & 0xf1f0) == 0x8140)
    {
        *ptr++ = udf(opcode);
    }
    /* 1000xxx11000xxxx - UNPK */
    else if ((opcode & 0xf1f0) == 0x8180)
    {
        *ptr++ = udf(opcode);
    }
    /* 1000xxx111xxxxxx - DIVS */
    else if ((opcode & 0xf1c0) == 0x81c0)
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 1000xxxxxxxxxxxx - OR */
    else if ((opcode & 0xf000) == 0x8000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
        uint8_t ext_words = 0;
        uint8_t test_register = 0xff;

        if (direction == 0)
        {
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t src = 0xff;

            test_register = dest;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            switch (size)
            {
            case 4:
#ifdef __aarch64__
                *ptr++ = orr_reg(dest, dest, src, LSL, 0);
#else
                *ptr++ = orrs_reg(dest, dest, src, 0);
#endif
                break;
            case 2:
#ifdef __aarch64__
                *ptr++ = orr_reg(src, src, dest, LSL, 0);
#else
                *ptr++ = lsl_immed(src, src, 16);
                *ptr++ = orrs_reg(src, src, dest, 16);
                *ptr++ = lsr_immed(src, src, 16);
#endif
                *ptr++ = bfi(dest, src, 0, 16);
                break;
            case 1:
#ifdef __aarch64__
                *ptr++ = orr_reg(src, src, dest, LSL, 0);
#else
                *ptr++ = lsl_immed(src, src, 24);
                *ptr++ = orrs_reg(src, src, dest, 24);
                *ptr++ = lsr_immed(src, src, 24);
#endif
                *ptr++ = bfi(dest, src, 0, 8);
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

            test_register = tmp;

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
                *ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = orrs_reg(tmp, tmp, src, 0);
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
                *ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = lsl_immed(tmp, tmp, 16);
                *ptr++ = orrs_reg(tmp, tmp, src, 16);
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
                *ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = lsl_immed(tmp, tmp, 24);
                *ptr++ = orrs_reg(tmp, tmp, src, 24);
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
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
#ifdef __aarch64__
            switch(size)
            {
                case 4:
                    *ptr++ = cmn_reg(31, test_register, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, test_register, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, test_register, LSL, 24);
                    break;
            }
#endif
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }

        RA_FreeARMRegister(&ptr, test_register);
    }
    else
        *ptr++ = udf(opcode);

    return ptr;
}
