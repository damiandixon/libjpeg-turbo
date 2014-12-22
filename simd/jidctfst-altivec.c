/*
 * AltiVec optimizations for libjpeg-turbo
 *
 * Copyright (C) 2014, D. R. Commander.
 * All rights reserved.
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/* FAST INTEGER INVERSE DCT
 *
 * This is similar to the SSE2 implementation, except that we left-shift the
 * constants by 1 less bit (the -1 in CONST_SHIFT.)  This is because
 * vec_madds(arg1, arg2, arg3) generates the 16-bit saturated sum of:
 *   the elements in arg3 + the most significant 17 bits of
 *     (the elements in arg1 * the elements in arg2).
 */

#include "jsimd_altivec.h"


#define F_1_082 277              /* FIX(1.082392200) */
#define F_1_414 362              /* FIX(1.414213562) */
#define F_1_847 473              /* FIX(1.847759065) */
#define F_2_613 669              /* FIX(2.613125930) */
#define F_1_613 (F_2_613 - 256)  /* FIX(2.613125930) - FIX(1) */

#define CONST_BITS 8
#define PASS1_BITS 2
#define PRE_MULTIPLY_SCALE_BITS 2
#define CONST_SHIFT (16 - PRE_MULTIPLY_SCALE_BITS - CONST_BITS - 1)


#define DO_IDCT(in)  \
{  \
  /* Even part */  \
  \
  tmp10 = vec_add(in##0, in##4);  \
  tmp11 = vec_sub(in##0, in##4);  \
  tmp13 = vec_add(in##2, in##6);  \
  \
  tmp12 = vec_sub(in##2, in##6);  \
  tmp12 = vec_sl(tmp12, pre_multiply_scale_bits);  \
  tmp12 = vec_madds(tmp12, pw_F1414, zero);  \
  tmp12 = vec_sub(tmp12, tmp13);  \
  \
  tmp0 = vec_add(tmp10, tmp13);  \
  tmp3 = vec_sub(tmp10, tmp13);  \
  tmp1 = vec_add(tmp11, tmp12);  \
  tmp2 = vec_sub(tmp11, tmp12);  \
  \
  /* Odd part */  \
  \
  z13 = vec_add(in##5, in##3);  \
  z10 = vec_sub(in##5, in##3);  \
  z10s = vec_sl(z10, pre_multiply_scale_bits);  \
  z11 = vec_add(in##1, in##7);  \
  z12s = vec_sub(in##1, in##7);  \
  z12s = vec_sl(z12s, pre_multiply_scale_bits);  \
  \
  tmp11 = vec_sub(z11, z13);  \
  tmp11 = vec_sl(tmp11, pre_multiply_scale_bits);  \
  tmp11 = vec_madds(tmp11, pw_F1414, zero);  \
  \
  tmp7 = vec_add(z11, z13);  \
  \
  z5 = vec_add(z10s, z12s);  \
  z5 = vec_madds(z5, pw_F1847, zero);  \
  \
  tmp10 = vec_madds(z12s, pw_F1082, zero);  \
  tmp10 = vec_sub(tmp10, z5);  \
  tmp12 = vec_madds(z10s, pw_MF1613, z5);  \
  tmp12 = vec_sub(tmp12, z10);  \
  \
  tmp6 = vec_sub(tmp12, tmp7);  \
  tmp5 = vec_sub(tmp11, tmp6);  \
  tmp4 = vec_add(tmp10, tmp5);  \
  \
  out0 = vec_add(tmp0, tmp7);  \
  out1 = vec_add(tmp1, tmp6);  \
  out2 = vec_add(tmp2, tmp5);  \
  out3 = vec_sub(tmp3, tmp4);  \
  out4 = vec_add(tmp3, tmp4);  \
  out5 = vec_sub(tmp2, tmp5);  \
  out6 = vec_sub(tmp1, tmp6);  \
  out7 = vec_sub(tmp0, tmp7);  \
}


void
jsimd_idct_ifast_altivec (void * dct_table_, JCOEFPTR coef_block,
                          JSAMPARRAY output_buf, JDIMENSION output_col)
{
  short *dct_table = (short *)dct_table_;
  __vector short row0, row1, row2, row3, row4, row5, row6, row7,
    col0, col1, col2, col3, col4, col5, col6, col7,
    quant0, quant1, quant2, quant3, quant4, quant5, quant6, quant7,
    tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp10, tmp11, tmp12, tmp13,
    z5, z10, z10s, z11, z12s, z13,
    out0, out1, out2, out3, out4, out5, out6, out7;
  __vector signed char outb;
  long long *outptr, *outbptr = (long long *)(&outb);

  /* Constants */
  __vector short zero = { __8X(0) },
    pw_F1414 = { __8X(F_1_414 << CONST_SHIFT) },
    pw_F1847 = { __8X(F_1_847 << CONST_SHIFT) },
    pw_MF1613 = { __8X(-F_1_613 << CONST_SHIFT) },
    pw_F1082 = { __8X(F_1_082 << CONST_SHIFT) };
  __vector unsigned short
    pre_multiply_scale_bits = { __8X(PRE_MULTIPLY_SCALE_BITS) },
    pass1_bits3 = { __8X(PASS1_BITS + 3) };
  __vector signed char pb_centerjsamp = { __16X(CENTERJSAMPLE) };

  /* Pass 1: process columns */

  col0 = *(__vector short *)&coef_block[0];
  col1 = *(__vector short *)&coef_block[8];
  col2 = *(__vector short *)&coef_block[16];
  col3 = *(__vector short *)&coef_block[24];
  col4 = *(__vector short *)&coef_block[32];
  col5 = *(__vector short *)&coef_block[40];
  col6 = *(__vector short *)&coef_block[48];
  col7 = *(__vector short *)&coef_block[56];

  tmp1 = vec_or(col1, col2);
  tmp2 = vec_or(col3, col4);
  tmp1 = vec_or(tmp1, tmp2);
  tmp3 = vec_or(col5, col6);
  tmp3 = vec_or(tmp3, col7);
  tmp1 = vec_or(tmp1, tmp3);

  quant0 = *(__vector short *)&dct_table[0];
  col0 = vec_mladd(col0, quant0, zero);

  if (vec_all_eq(tmp1, zero)) {
    /* AC terms all zero */

    row0 = vec_splat(col0, 0);
    row1 = vec_splat(col0, 1);
    row2 = vec_splat(col0, 2);
    row3 = vec_splat(col0, 3);
    row4 = vec_splat(col0, 4);
    row5 = vec_splat(col0, 5);
    row6 = vec_splat(col0, 6);
    row7 = vec_splat(col0, 7);

  } else {

    quant1 = *(__vector short *)&dct_table[8];
    quant2 = *(__vector short *)&dct_table[16];
    quant3 = *(__vector short *)&dct_table[24];
    quant4 = *(__vector short *)&dct_table[32];
    quant5 = *(__vector short *)&dct_table[40];
    quant6 = *(__vector short *)&dct_table[48];
    quant7 = *(__vector short *)&dct_table[56];

    col1 = vec_mladd(col1, quant1, zero);
    col2 = vec_mladd(col2, quant2, zero);
    col3 = vec_mladd(col3, quant3, zero);
    col4 = vec_mladd(col4, quant4, zero);
    col5 = vec_mladd(col5, quant5, zero);
    col6 = vec_mladd(col6, quant6, zero);
    col7 = vec_mladd(col7, quant7, zero);

    DO_IDCT(col);

    TRANSPOSE(out, row);
  }

  /* Pass 2: process rows */

  DO_IDCT(row);

  out0 = vec_sra(out0, pass1_bits3);
  out1 = vec_sra(out1, pass1_bits3);
  out2 = vec_sra(out2, pass1_bits3);
  out3 = vec_sra(out3, pass1_bits3);
  out4 = vec_sra(out4, pass1_bits3);
  out5 = vec_sra(out5, pass1_bits3);
  out6 = vec_sra(out6, pass1_bits3);
  out7 = vec_sra(out7, pass1_bits3);

  TRANSPOSE(out, col);

  outb = vec_packs(col0, col1);
  outb = vec_add(outb, pb_centerjsamp);
  outptr = (long long *)(output_buf[0] + output_col);
  *outptr = outbptr[0];
  outptr = (long long *)(output_buf[1] + output_col);
  *outptr = outbptr[1];

  outb = vec_packs(col2, col3);
  outb = vec_add(outb, pb_centerjsamp);
  outptr = (long long *)(output_buf[2] + output_col);
  *outptr = outbptr[0];
  outptr = (long long *)(output_buf[3] + output_col);
  *outptr = outbptr[1];

  outb = vec_packs(col4, col5);
  outb = vec_add(outb, pb_centerjsamp);
  outptr = (long long *)(output_buf[4] + output_col);
  *outptr = outbptr[0];
  outptr = (long long *)(output_buf[5] + output_col);
  *outptr = outbptr[1];

  outb = vec_packs(col6, col7);
  outb = vec_add(outb, pb_centerjsamp);
  outptr = (long long *)(output_buf[6] + output_col);
  *outptr = outbptr[0];
  outptr = (long long *)(output_buf[7] + output_col);
  *outptr = outbptr[1];
}
