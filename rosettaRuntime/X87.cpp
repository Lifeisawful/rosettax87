#include "X87.h"

#include "Export.h"
#include "Log.h"
#include "SIMDGuard.h"
#include "X87State.h"

#include "openlibm_math.h"

#define X87_F2XM1
#define X87_FABS
#define X87_FADD_ST
#define X87_FADD_F32
#define X87_FADD_F64
#define X87_FBLD
#define X87_FBSTP
#define X87_FCHS
#define X87_FCMOV
#define X87_FCOM_ST
#define X87_FCOM_F32
#define X87_FCOM_F64
#define X87_FCOMI
#define X87_FCOS
#define X87_FDECSTP
#define X87_FDIV_ST
#define X87_FDIV_F32
#define X87_FDIV_F64
#define X87_FDIVR_ST
#define X87_FDIVR_F32
#define X87_FDIVR_F64
#define X87_FFREE
#define X87_FIADD
#define X87_FICOM
#define X87_FIDIV
#define X87_FIDIVR
#define X87_FILD
#define X87_FIMUL
#define X87_FINCSTP
#define X87_FIST_I16
#define X87_FIST_I32
#define X87_FIST_I64
#define X87_FISTT_I16
#define X87_FISTT_I32
#define X87_FISTT_I64
#define X87_FISUB
#define X87_FISUBR
#define X87_FLD_STI
#define X87_FLD_CONSTANT
#define X87_FLD_FP32
#define X87_FLD_FP64
#define X87_FLD_FP80
#define X87_FMUL_ST
#define X87_FMUL_F32
#define X87_FMUL_F64
#define X87_FPATAN
#define X87_FPREM
#define X87_FPREM1
#define X87_FPTAN
#define X87_FRNDINT
#define X87_FSCALE
#define X87_FSIN
#define X87_FSINCOS
#define X87_FSQRT
#define X87_FST_STI
#define X87_FST_FP32
#define X87_FST_FP64
#define X87_FST_FP80
#define X87_FSUB_ST
#define X87_FSUB_F32
#define X87_FSUB_F64
#define X87_FSUBR_ST
#define X87_FSUBR_F32
#define X87_FSUBR_F64
#define X87_FUCOM
#define X87_FUCOMI
#define X87_FXAM
#define X87_FXCH
#define X87_FXTRACT
#define X87_FYL2X
#define X87_FYL2XP1

#define X87_TRAMPOLINE(NAME, REGISTER)                                         \
  void __attribute__((naked, used)) NAME() {                                   \
    asm volatile("adrp " #REGISTER ", _orig_" #NAME "@PAGE\n"                  \
                 "ldr " #REGISTER ", [" #REGISTER ", _orig_" #NAME             \
                 "@PAGEOFF]\n"                                                 \
                 "br " #REGISTER);                                             \
  }

void *init_library(SymbolList const *a1, unsigned long long a2,
                   ThreadContextOffsets const *a3) {
  exports_init();

  simple_printf("RosettaRuntimex87 built %s"
                "\n",
                __DATE__ " " __TIME__);

  return orig_init_library(a1, a2, a3);
}

X87_TRAMPOLINE(register_runtime_routine_offsets, x8)
X87_TRAMPOLINE(translator_use_t8027_codegen, x8)
X87_TRAMPOLINE(translator_reset, x8)
X87_TRAMPOLINE(ir_create_bad_access, x8)
X87_TRAMPOLINE(ir_create, x8)
X87_TRAMPOLINE(module_free, x8)
X87_TRAMPOLINE(module_get_size, x8)
X87_TRAMPOLINE(module_is_bad_access, x8)
X87_TRAMPOLINE(module_print, x8)
X87_TRAMPOLINE(translator_translate, x8)
X87_TRAMPOLINE(translator_free, x8)
X87_TRAMPOLINE(translator_get_data, x8)
X87_TRAMPOLINE(translator_get_size, x8)
X87_TRAMPOLINE(translator_get_branch_slots_offset, x8)
X87_TRAMPOLINE(translator_get_branch_slots_count, x8)
X87_TRAMPOLINE(translator_get_branch_entries, x2)
X87_TRAMPOLINE(translator_get_instruction_offsets, x2)
X87_TRAMPOLINE(translator_apply_fixups, x8)

void x87_init(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_init\n", 9);

#if defined(X87_CONVERT_TO_FP80)
  orig_x87_init(a1);
#else
  *a1 = X87State();
#endif
}
void x87_state_from_x86_float_state(X87State *a1, X86FloatState64 const *a2) {
  MISSING(1, "x87_state_from_x86_float_state\n", 31);
  orig_x87_state_from_x86_float_state(a1, a2);
}
void x87_state_to_x86_float_state(X87State const *a1, X86FloatState64 *a2) {
  MISSING(1, "x87_state_to_x86_float_state\n", 29);
  orig_x87_state_to_x86_float_state(a1, a2);
}
void x87_pop_register_stack(X87State *a1) {
  MISSING(1, "x87_pop_register_stack\n", 23);
  orig_x87_pop_register_stack(a1);
}
// Computes the exponential value of 2 to the power of the source operand
// minus 1. The source operand is located in register ST(0) and the result is
// also stored in ST(0). The value of the source operand must lie in the range
// –1.0 to +1.0. If the source value is outside this range, the result is
// undefined.
void x87_f2xm1(X87State *state) {
  SIMDGuard simd_guard;

  LOG(1, "x87_f2xm1\n", 10);
#if defined(X87_F2XM1)
  // Get value from ST(0)
  auto x = state->get_st(0);

  // // Check range [-1.0, +1.0]
  if (x < -1.0f || x > 1.0f) {
    // Set to NaN for undefined result
    state->set_st(0, 0);
    return;
  }

  // Calculate 2^x - 1 using mmath::exp2
  auto result = exp2(x) - 1.0f;

  // Store result back in ST(0)
  state->set_st(0, result);
#else
  orig_x87_f2xm1(state);
#endif
}

// Clears the sign bit of ST(0) to create the absolute value of the operand. The
// following table shows the results obtained when creating the absolute value
// of various classes of numbers. C1 	Set to 0.
void x87_fabs(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fabs\n", 10);

#if defined(X87_FABS)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get value in ST(0)
  auto value = a1->get_st(0);

  // Set value in ST(0) to its absolute value
  a1->set_st(0, value < 0 ? -value : value);
#else
  orig_x87_fabs(a1);
#endif
}

void x87_fadd_ST(X87State *a1, unsigned int st_offset_1,
                 unsigned int st_offset_2, bool pop_stack) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fadd_ST\n", 13);
#if defined(X87_FADD_ST)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get register indices and values
  const auto val1 = a1->get_st(st_offset_1);
  const auto val2 = a1->get_st(st_offset_2);

  // Perform addition and store result in ST(idx1)
  a1->set_st(st_offset_1, val1 + val2);

  if (pop_stack) {
    a1->pop();
  }
#else
  orig_x87_fadd_ST(a1, st_offset_1, st_offset_2, pop_stack);
#endif
}

void x87_fadd_f32(X87State *a1, unsigned int fp32) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fadd_f32\n", 14);
#if defined(X87_FADD_F32)

  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<float *>(&fp32);
  auto st0 = a1->get_st(0);

  a1->set_st(0, st0 + value);
#else
  orig_x87_fadd_f32(a1, fp32);
#endif
}
void x87_fadd_f64(X87State *a1, unsigned long long a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fadd_f64\n", 14);
#if defined(X87_FADD_F64)

  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<double *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, st0 + value);
#else
  orig_x87_fadd_f64(a1, a2);
#endif
}

double BCD2Double(uint8_t bcd[10]) {
  uint64_t tmp = 0;
  uint64_t mult = 1;
  uint8_t piece;

  for (int i=0; i<9; ++i) {
      piece = bcd[i];
      tmp += mult * (piece & 0x0F);
      mult *= 10;
      tmp += mult * ((piece >> 4) & 0x0F);
      mult *= 10;
  }

  piece = bcd[9];
  tmp += mult * (piece & 0x0F);

  double value = static_cast<double>(tmp);

  if (piece & 0x80) {
      value = -value; 
  }

  return value;
}

void x87_fbld(X87State *a1, unsigned long long a2, unsigned long long a3) {
  SIMDGuard simd_guard;
  LOG(1, "x87_fbld\n", 10);

#if defined(X87_FBLD)
  // set C1 to 0
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  uint8_t bcd[10];
  memcpy(bcd, &a2, 8);           // Copy 8 bytes from a2
  memcpy(bcd + 8, &a3, 2);       // Copy 2 bytes from a3

  auto value = BCD2Double(bcd);

  //Add space on the stack and push the converted BCD
  a1->push();
  a1->set_st(0, value);
#else
  orig_x87_fbld(a1, a2, a3);
#endif
}

void x87_fbstp(X87State const *a1) {
  MISSING(1, "x87_fbstp\n", 11);
  orig_x87_fbstp(a1);
}
void x87_fchs(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fchs\n", 10);
#if defined(X87_FCHS)
  // set C1 to 0
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Negate value in ST(0)
  a1->set_st(0, -a1->get_st(0));
#else
  orig_x87_fchs(a1);
#endif
}

void x87_fcmov(X87State *state, unsigned int condition,
               unsigned int st_offset) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fcmov\n", 11);

#if defined(X87_FCMOV)
  // clear precision flag
  state->status_word &= ~X87StatusWordFlag::kConditionCode1;

  double value;

  auto st_tag_word = state->get_st_tag(st_offset);
  if (st_tag_word != X87TagState::kEmpty) {
    if (condition == 0) {
      return;
    }

    value = state->get_st(st_offset);
  } else {
    state->status_word |= 0x41; // Set invalid operation
    value = 0.0f;
  }

  state->set_st(0, value); // Perform the actual register move
#else
  orig_x87_fcmov(state, condition, st_offset);
#endif
}

void x87_fcom_ST(X87State *a1, unsigned int st_offset,
                 unsigned int number_of_pops) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fcom_ST\n", 13);

#if defined(X87_FCOM_ST)
  // Get values to compare
  auto st0 = a1->get_st(0);
  auto sti = a1->get_st(st_offset);

  // Clear condition code bits C0, C2, C3 (bits 8, 9, 14)
  a1->status_word &=
      ~(kConditionCode0 | kConditionCode1 | kConditionCode2 | kConditionCode3);

  // Set condition codes based on comparison
  if (isnan(st0) || isnan(sti)) {
    a1->status_word |=
        kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
  } else if (st0 > sti) {
    // Leave C0=C2=C3=0
  } else if (st0 < sti) {
    a1->status_word |= kConditionCode0; // Set C0=1
  } else {                              // st0 == sti
    a1->status_word |= kConditionCode3; // Set C3=1
  }

  // Handle pops if requested
  for (unsigned int i = 0; i < number_of_pops; i++) {
    a1->pop();
  }
#else
  orig_x87_fcom_ST(a1, st_offset, number_of_pops);
#endif
}

void x87_fcom_f32(X87State *a1, unsigned int fp32, bool pop) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fcom_f32\n", 14);
#if defined(X87_FCOM_F32)
  auto st0 = a1->get_st(0);
  auto value = *reinterpret_cast<float *>(&fp32);

  a1->status_word &=
      ~(kConditionCode0 | kConditionCode1 | kConditionCode2 | kConditionCode3);

  if (isnan(st0) || isnan(value)) {
    a1->status_word |=
        kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
  } else if (st0 > value) {
    // Leave C0=C2=C3=0
  } else if (st0 < value) {
    a1->status_word |= kConditionCode0; // Set C0=1
  } else {                              // st0 == value
    a1->status_word |= kConditionCode3; // Set C3=1
  }

  if (pop) {
    a1->pop();
  }
#else
  orig_x87_fcom_f32(a1, fp32, pop);
#endif
}
void x87_fcom_f64(X87State *a1, unsigned long long fp64, bool pop) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fcom_f64\n", 14);
#if defined(X87_FCOM_F64)
  auto st0 = a1->get_st(0);
  auto value = *reinterpret_cast<double *>(&fp64);

  a1->status_word &=
      ~(kConditionCode0 | kConditionCode1 | kConditionCode2 | kConditionCode3);

  if (isnan(st0) || isnan(value)) {
    a1->status_word |=
        kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
  } else if (st0 > value) {
    // Leave C0=C2=C3=0
  } else if (st0 < value) {
    a1->status_word |= kConditionCode0; // Set C0=1
  } else {                              // st0 == value
    a1->status_word |= kConditionCode3; // Set C3=1
  }

  if (pop) {
    a1->pop();
  }
#else
  orig_x87_fcom_f64(a1, fp64, pop);
#endif
}

uint32_t x87_fcomi(X87State *state, unsigned int st_offset, bool pop) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fcomi\n", 11);
#if defined(X87_FCOMI)
  state->status_word &= ~(kConditionCode0);

  auto st0_val = state->get_st(0);
  auto sti_val = state->get_st(st_offset);

  uint32_t flags = 0;
  /*
  Filters: fcomi
  Randomness seeded to: 3528984885
  x87_fcomi_less
  x87_fcomi result: 0x000000000000000
  x87_fcomi_greater
  x87_fcomi result: 0x000000020000000
  x87_fcomi_equal
  x87_fcomi result: 0x000000060000000
  */

  if (st0_val < sti_val) {
    flags = 0x000000000000000;
  } else if (st0_val > sti_val) {
    flags = 0x000000020000000;
  } else {
    flags = 0x000000060000000;
  }

  if (pop) {
    state->pop();
  }

  return flags;
#else
  uint32_t flags = orig_x87_fcomi(state, st_offset, pop);
  return flags;
#endif
}

void x87_fcos(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fcos\n", 10);
#if defined(X87_FCOS)
  a1->status_word &= ~(kConditionCode1 | kConditionCode2);
  // Get ST(0)
  auto value = a1->get_st(0);

  // Calculate cosine
  auto result = cos(value);

  // Store result back in ST(0)
  a1->set_st(0, result);
#else
  orig_x87_fcos(a1);
#endif
}
void x87_fdecstp(X87State *a1) {
  MISSING(1, "x87_fdecstp\n", 13);
  orig_x87_fdecstp(a1);
}

void x87_fdiv_ST(X87State *a1, unsigned int st_offset_1,
                 unsigned int st_offset_2, bool pop_stack) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fdiv_ST\n", 13);
#if defined(X87_FDIV_ST)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get register indices and values
  const auto val1 = a1->get_st(st_offset_1);
  const auto val2 = a1->get_st(st_offset_2);

  // Perform reversed division and store result
  a1->set_st(st_offset_1, val1 / val2);

  if (pop_stack) {
    a1->pop();
  }
#else
  orig_x87_fdiv_ST(a1, st_offset_1, st_offset_2, pop_stack);
#endif
}

void x87_fdiv_f32(X87State *a1, unsigned int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fdiv_f32\n", 14);
#if defined(X87_FDIV_F32)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<float *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, st0 / value);
#else
  orig_x87_fdiv_f32(a1, a2);
#endif
}

void x87_fdiv_f64(X87State *a1, unsigned long long a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fdiv_f64\n", 14);

#if defined(X87_FDIV_F64)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<double *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, st0 / value);
#else
  orig_x87_fdiv_f64(a1, a2);
#endif
}

void x87_fdivr_ST(X87State *a1, unsigned int st_offset_1,
                  unsigned int st_offset_2, bool pop_stack) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fdivr_ST\n", 14);
#if defined(X87_FDIVR_ST)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get register indices and values
  const auto val1 = a1->get_st(st_offset_1);
  const auto val2 = a1->get_st(st_offset_2);

  // Perform reversed division and store result
  a1->set_st(st_offset_1, val2 / val1);

  if (pop_stack) {
    a1->pop();
  }
#else
  orig_x87_fdivr_ST(a1, st_offset_1, st_offset_2, pop_stack);
#endif
}

void x87_fdivr_f32(X87State *a1, unsigned int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fdivr_f32\n", 15);
#if defined(X87_FDIVR_F32)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<float *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, value / st0);
#else
  orig_x87_fdivr_f32(a1, a2);
#endif
}

void x87_fdivr_f64(X87State *a1, unsigned long long a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fdivr_f64\n", 15);
#if defined(X87_FDIVR_F64)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<double *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, value / st0);
#else
  orig_x87_fdivr_f64(a1, a2);
#endif
}

void x87_ffree(X87State *a1, unsigned int a2) {
  LOG(1, "x87_ffree\n", 11);
  orig_x87_ffree(a1, a2);
}
void x87_fiadd(X87State *a1, int m32int) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fiadd\n", 11);
#if defined(X87_FIADD)
  // simple_printf("m32int: %d\n", m32int);

  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get value in ST(0)
  auto st0 = a1->get_st(0);

  // Add integer value
  st0 += m32int;

  // Store result back in ST(0)
  a1->set_st(0, st0);
#else
  orig_x87_fiadd(a1, m32int);
#endif
}

void x87_ficom(X87State *a1, int src, bool pop) {
  SIMDGuard simd_guard;
  LOG(1, "x87_ficom\n", 11);
#if defined(X87_FICOM)
  auto st0 = a1->get_st(0);

  // Clear condition code bits C0, C2, C3 (bits 8, 9, 14)
  a1->status_word &=
      ~(kConditionCode0 | kConditionCode1 | kConditionCode2 | kConditionCode3);

  // Set condition codes based on comparison
  if (isnan(st0) || isnan(src)) {
    a1->status_word |=
        kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
  } else if (st0 > src) {
    // Leave C0=C2=C3=0
  } else if (st0 < src) {
    a1->status_word |= kConditionCode0; // Set C0=1
  } else {                              // st0 == src
    a1->status_word |= kConditionCode3; // Set C3=1
  }

  // Handle pops if requested
  if (pop) {
    a1->pop();
  }
#else
  orig_x87_ficom(a1, src, pop);
#endif
}

void x87_fidiv(X87State *a1, int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fidiv\n", 11);
#if defined(X87_FIDIV)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get value in ST(0)
  auto value = a1->get_st(0);

  // Divide by integer value
  value /= a2;

  // Store result back in ST(0)
  a1->set_st(0, value);
#else
  orig_x87_fidiv(a1, a2);
#endif
}

void x87_fidivr(X87State *a1, int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fidivr\n", 12);
#if defined(X87_FIDIVR)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get value in ST(0)
  auto value = a1->get_st(0);

  // Divide integer value by value in ST(0)
  value = a2 / value;

  // Store result back in ST(0)
  a1->set_st(0, value);
#else
  orig_x87_fidivr(a1, a2);
#endif
}

X87Float80 doubleToX87Components(int64_t value) {

  uint64_t abs_value =
      value >= 0 ? static_cast<uint64_t>(value) : static_cast<uint64_t>(-value);

  uint16_t leading_zeros =
      abs_value ? static_cast<uint16_t>(__builtin_clzll(abs_value)) : 64;
  uint64_t mantissa =
      abs_value ? ((abs_value << leading_zeros) | 0x8000000000000000ULL) : 0;

  uint16_t exponent;
  if (abs_value == 0) {
    exponent = 0;
  } else {
    exponent =
        static_cast<uint16_t>((value < 0 ? 0x8000 : 0) - leading_zeros + 16446);
  }

  X87Float80 result;
  result.mantissa = mantissa;
  result.exponent = exponent;

  return result;
}

// Converts the signed-integer source operand into double extended-precision
// floating-point format and pushes the value onto the FPU register stack. The
// source operand can be a word, doubleword, or quadword integer. It is loaded
// without rounding errors. The sign of the source operand is preserved.
void x87_fild(X87State *a1, int64_t value) {
  SIMDGuard simd_guard;
  LOG(1, "x87_fild\n", 10);

#if defined(X87_FILD)
  a1->push();
  a1->set_st(0, static_cast<double>(value));
#else
  orig_x87_fild(a1, value);
#endif
}

void x87_fimul(X87State *a1, int a2) {
  SIMDGuard simd_guard;
  LOG(1, "x87_fimul\n", 11);
#if defined(X87_FIMUL)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get value in ST(0)
  auto value = a1->get_st(0);

  // Multiply by integer value
  value *= a2;

  // Store result back in ST(0)
  a1->set_st(0, value);
#else
  orig_x87_fimul(a1, a2);
#endif
}
void x87_fincstp(X87State *a1) {
  MISSING(1, "x87_fincstp\n", 13);
  orig_x87_fincstp(a1);
}

X87ResultStatusWord x87_fist_i16(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fist_i16\n", 14);
#if defined(X87_FIST_I16)
  auto [value, status_word] = a1->get_st_const(0);
  X87ResultStatusWord result{0, status_word};

  // Special case: value > INT16_MAX or infinity (changed from >=)
  if (value > static_cast<double>(INT16_MAX)) {
    result.signed_result = INT16_MIN; // 0x8000
    result.status_word |= X87StatusWordFlag::kConditionCode1;
    return result;
  }

  // Special case: value <= INT16_MIN
  if (value <= static_cast<double>(INT16_MIN)) {
    result.signed_result = INT16_MIN;
    result.status_word |= X87StatusWordFlag::kConditionCode1;
    return result;
  }

  // Normal case
  auto round_bits = a1->control_word & X87ControlWord::kRoundingControlMask;

  switch (round_bits) {
  case X87ControlWord::kRoundToNearest: {
    result.signed_result = static_cast<int16_t>(std::nearbyint(value));
  } break;

  case X87ControlWord::kRoundDown: {
    result.signed_result = static_cast<int16_t>(std::floor(value));
    return result;
  } break;
  case X87ControlWord::kRoundUp: {
    result.signed_result = static_cast<int16_t>(std::ceil(value));
    return result;
  } break;

  case X87ControlWord::kRoundToZero: {
    result.signed_result = static_cast<int16_t>(value);
    return result;
  } break;
  }

  return result;
#else
  return orig_x87_fist_i16(a1);
#endif
}

X87ResultStatusWord x87_fist_i32(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fist_i32\n", 14);
#if defined(X87_FIST_I32)
  auto [value, status_word] = a1->get_st_const(0);
  X87ResultStatusWord result{0, status_word};

  // Special case: value >= INT32_MAX or infinity
  if (value >= static_cast<double>(INT32_MAX)) {
    result.signed_result = INT32_MIN; // 0x80000000
    result.status_word |= X87StatusWordFlag::kConditionCode1;
    return result;
  }

  // Special case: value <= INT32_MIN
  if (value <= static_cast<double>(INT32_MIN)) {
    result.signed_result = INT32_MIN;
    result.status_word |= X87StatusWordFlag::kConditionCode1;
    return result;
  }

  auto round_bits = a1->control_word & X87ControlWord::kRoundingControlMask;

  switch (round_bits) {
  case X87ControlWord::kRoundToNearest: {
    result.signed_result = static_cast<int32_t>(std::nearbyint(value));
  } break;

  case X87ControlWord::kRoundDown: {
    result.signed_result = static_cast<int32_t>(std::floor(value));
    return result;
  } break;
  case X87ControlWord::kRoundUp: {
    result.signed_result = static_cast<int32_t>(std::ceil(value));
    return result;
  } break;

  case X87ControlWord::kRoundToZero: {
    result.signed_result = static_cast<int32_t>(value);
    return result;
  } break;
  }

  return result;
#else
  return orig_x87_fist_i32(a1);
#endif
}
X87ResultStatusWord x87_fist_i64(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fist_i64\n", 14);
#if defined(X87_FIST_I64)
  // Get value in ST(0)
  auto [value, status_word] = a1->get_st_const(0);

  X87ResultStatusWord result{0, status_word};

  // Special case: value >= INT64_MAX or infinity
  if (value >= static_cast<double>(INT64_MAX)) {
    result.signed_result = INT64_MIN; // 0x8000000000000000
    result.status_word |= X87StatusWordFlag::kConditionCode1;
    return result;
  }

  // Special case: value <= INT64_MIN
  if (value <= static_cast<double>(INT64_MIN)) {
    result.signed_result = INT64_MIN;
    result.status_word |= X87StatusWordFlag::kConditionCode1;
    return result;
  }

  // Normal case

  auto round_bits = a1->control_word & X87ControlWord::kRoundingControlMask;

  switch (round_bits) {
  case X87ControlWord::kRoundToNearest: {
    result.signed_result = static_cast<int64_t>(std::nearbyint(value));
  } break;

  case X87ControlWord::kRoundDown: {
    result.signed_result = static_cast<int64_t>(std::floor(value));
    return result;
  } break;
  case X87ControlWord::kRoundUp: {
    result.signed_result = static_cast<int64_t>(std::ceil(value));
    return result;
  } break;

  case X87ControlWord::kRoundToZero: {
    result.signed_result = static_cast<int64_t>(value);
    return result;
  } break;
  }

  return result;
#else
  return orig_x87_fist_i64(a1);
#endif
}

X87ResultStatusWord x87_fistt_i16(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fistt_i16\n", 15);
#if defined(X87_FISTT_I16)
  // Get value in ST(0)
  auto [value, status_word] = a1->get_st_const(0);

  return {.signed_result = static_cast<int16_t>(value), status_word};
#else
  return orig_x87_fistt_i16(a1);
#endif
}

X87ResultStatusWord x87_fistt_i32(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fistt_i32\n", 15);
#if defined(X87_FISTT_I32)
  // Get value in ST(0)
  auto [value, status_word] = a1->get_st_const(0);

  return {.signed_result = static_cast<int32_t>(value), status_word};
#else
  return orig_x87_fistt_i32(a1);
#endif
}

X87ResultStatusWord x87_fistt_i64(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fistt_i64\n", 15);
#if defined(X87_FISTT_I64)
  // Get value in ST(0)
  auto [value, status_word] = a1->get_st_const(0);

  return {.signed_result = static_cast<int64_t>(value), status_word};
#else
  return orig_x87_fistt_i64(a1);
#endif
}
void x87_fisub(X87State *a1, int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fisub\n", 11);
#if defined(X87_FISUB)
  // Clear condition code 1
  a1->status_word &= ~(X87StatusWordFlag::kConditionCode1);

  // Get value in ST(0)
  auto value = a1->get_st(0);

  // Subtract integer value
  value -= a2;

  // Store result back in ST(0)
  a1->set_st(0, value);
#else
  orig_x87_fisub(a1, a2);
#endif
}

void x87_fisubr(X87State *a1, int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fisubr\n", 12);

#if defined(X87_FISUBR)
  // Clear condition code 1
  a1->status_word &= ~(X87StatusWordFlag::kConditionCode1);

  // Get value in ST(0)
  auto value = a1->get_st(0);

  // Subtract integer value
  value = a2 - value;

  // Store result back in ST(0)
  a1->set_st(0, value);
#else
  orig_x87_fisubr(a1, a2);
#endif
}

// Push ST(i) onto the FPU register stack.
void x87_fld_STi(X87State *a1, unsigned int st_offset) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fld_STi\n", 13);
#if defined(X87_FLD_STI)
  a1->status_word &= ~0x200u;

  // Get index of ST(i) register
  const auto value = a1->get_st(st_offset);

  // make room for new value
  a1->push();

  // Copy value from ST(i) to ST(0)
  a1->set_st(0, value);
#else
  orig_x87_fld_STi(a1, st_offset);
#endif
}
void x87_fld_constant(X87State *a1, X87Constant a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fld_constant\n", 18);
  // simple_printf("x87_fld_constant %d\n", (int)a2);
#if defined(X87_FLD_CONSTANT)
  switch (a2) {
  case X87Constant::kOne: { // fld1
    a1->push();
    a1->set_st(0, 1.0);
  } break;

  case X87Constant::kZero: { // fldz
    a1->push();
    a1->set_st(0, 0.0);
  } break;

  case X87Constant::kPi: { // fldpi
    // store_x87_extended_value(a1, {.ieee754 = 3.141592741f});
    a1->push();
    a1->set_st(0, 3.141592741f);
  } break;

  case X87Constant::kLog2e: { // fldl2e
    // store_x87_extended_value(a1, {.ieee754 = 1.44269502f});
    a1->push();
    a1->set_st(0, 1.44269502f);
  } break;

  case X87Constant::kLoge2: { // fldln2
    // store_x87_extended_value(a1, {.ieee754 = 0.693147182f});
    a1->push();
    a1->set_st(0, 0.693147182f);
  } break;

  case X87Constant::kLog2t: { // fldl2t
    // store_x87_extended_value(a1, {.ieee754 = 3.321928f});
    a1->push();
    a1->set_st(0, 3.321928f);
  } break;

  case X87Constant::kLog102: { // fldl2e
    // store_x87_extended_value(a1, {.ieee754 = 0.301029987f});
    a1->push();
    a1->set_st(0, 0.301029987f);
  } break;

  default: {
    simple_printf("x87_fld_constant ERROR %d\n", (int)a2);
  } break;
  }
#else
  orig_x87_fld_constant(a1, a2);
#endif
}
void x87_fld_fp32(X87State *a1, unsigned int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fld_fp32\n", 14);

#if defined(X87_FLD_FP32)
  // Push new value onto stack, get reference to new top
  a1->push();

  a1->set_st(0, *reinterpret_cast<float *>(&a2));
#else
  orig_x87_fld_fp32(a1, a2);
#endif
}

void x87_fld_fp64(X87State *a1, unsigned long long a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fld_fp64\n", 14);

#if defined(X87_FLD_FP64)
  // Push new value onto stack, get reference to new top
  a1->push();

  a1->set_st(0, *reinterpret_cast<double *>(&a2));
#else
  orig_x87_fld_fp64(a1, a2);
#endif
}

void x87_fld_fp80(X87State *a1, X87Float80 a2) {
  LOG(1, "x87_fld_fp80\n", 14);

#if defined(X87_FLD_FP80)
  auto ieee754 = ConvertX87RegisterToFloat32(a2, &a1->status_word);

  // convert to ieee754 float
  a1->push();
  a1->set_st(0, *reinterpret_cast<float *>(&ieee754));
#else
  orig_x87_fld_fp80(a1, a2);
#endif
}

void x87_fmul_ST(X87State *a1, unsigned int st_offset_1,
                 unsigned int st_offset_2, bool pop_stack) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fmul_ST\n", 13);

#if defined(X87_FMUL_ST)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get register indices and values
  const auto val1 = a1->get_st(st_offset_1);
  const auto val2 = a1->get_st(st_offset_2);

  // Perform reversed division and store result
  a1->set_st(st_offset_1, val1 * val2);

  if (pop_stack) {
    a1->pop();
  }
#else
  orig_x87_fmul_ST(a1, st_offset_1, st_offset_2, pop_stack);
#endif
}

void x87_fmul_f32(X87State *a1, unsigned int fp32) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fmul_f32\n", 14);

#if defined(X87_FMUL_F32)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<float *>(&fp32);
  auto st0 = a1->get_st(0);

  a1->set_st(0, st0 * value);
#else
  orig_x87_fmul_f32(a1, fp32);
#endif
}

void x87_fmul_f64(X87State *a1, unsigned long long a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fmul_f64\n", 14);

#if defined(X87_FMUL_F64)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<double *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, st0 * value);
#else
  orig_x87_fmul_f64(a1, a2);
#endif
}

// Replace ST(1) with arctan(ST(1)/ST(0)) and pop the register stack.
void x87_fpatan(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fpatan\n", 12);

#if defined(X87_FPATAN)
  a1->status_word &= ~(X87StatusWordFlag::kConditionCode1);

  // Get values from ST(0) and ST(1)
  auto st0 = a1->get_st(0);
  auto st1 = a1->get_st(1);

  // Calculate arctan(ST(1)/ST(0))
  auto result = atan2(st1, st0);

  // Store result in ST(1) and pop the register stack
  a1->set_st(1, result);

  a1->pop();
#else
  orig_x87_fpatan(a1);
#endif
}

// Replace ST(0) with the remainder obtained from dividing ST(0) by ST(1).
// Computes the remainder obtained from dividing the value in the ST(0) register
// (the dividend) by the value in the ST(1) register (the divisor or modulus),
// and stores the result in ST(0). The remainder represents the following value:
// Remainder := ST(0) − (Q ∗ ST(1)) Here, Q is an integer value that is obtained
// by truncating the floating-point number quotient of [ST(0) / ST(1)] toward
// zero. The sign of the remainder is the same as the sign of the dividend. The
// magnitude of the remainder is less than that of the modulus, unless a partial
// remainder was computed (as described below). This instruction produces an
// exact result; the inexact-result exception does not occur and the rounding
// control has no effect. The following table shows the results obtained when
// computing the remainder of various classes of numbers, assuming that
// underflow does not occur.
void x87_fprem(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fprem\n", 11);

#if defined(X87_FPREM)
  // Clear condition code bits initially
  a1->status_word &= ~(
      X87StatusWordFlag::kConditionCode0 | X87StatusWordFlag::kConditionCode1 |
      X87StatusWordFlag::kConditionCode2 | X87StatusWordFlag::kConditionCode3);

  auto st0 = a1->get_st(0);
  auto st1 = a1->get_st(1);

  // simple_printf("ST0=%f ST1=%f\n", st0, st1);

  // Handle special cases
  if (isnan(st0) || isnan(st1) || isinf(st0) || st1 == 0.0) {
    a1->set_st(0, std::numeric_limits<double>::quiet_NaN());
    a1->status_word |= X87StatusWordFlag::kInvalidOperation;
    return;
  }

  if (isinf(st1)) {
    a1->set_st(0, st0);
    return;
  }

  // Calculate raw division
  auto raw_div = st0 / st1;
  // simple_printf("raw division=%f\n", raw_div);

  // Calculate quotient by truncating toward zero
  auto truncated = std::trunc(raw_div);
  int64_t quotient = static_cast<int64_t>(truncated);
  // simple_printf("truncated=%f quotient=%d\n", truncated, quotient);

  // Calculate remainder
  auto result = st0 - (static_cast<double>(quotient) * st1);
  // simple_printf("final result=%f\n", result);

  // Set condition code bits based on quotient least significant bits
  if (quotient & 1)
    a1->status_word |= X87StatusWordFlag::kConditionCode1;
  if (quotient & 2)
    a1->status_word |= X87StatusWordFlag::kConditionCode3;
  if (quotient & 4)
    a1->status_word |= X87StatusWordFlag::kConditionCode0;

  // C2=0 indicates complete remainder
  // Convert to unsigned for comparison
  uint64_t abs_quotient = (quotient >= 0) ? quotient : -quotient;
  if (abs_quotient < (1ULL << 63)) { // Use 63 bits to avoid overflow
    a1->status_word &= ~X87StatusWordFlag::kConditionCode2;
  } else {
    a1->status_word |= X87StatusWordFlag::kConditionCode2;
  }
  // simple_printf("final result=%f\n", result);

  a1->set_st(0, result);

#else
  orig_x87_fprem(a1);
#endif
}
void x87_fprem1(X87State *a1) {
  MISSING(1, "x87_fprem1\n", 12);
  orig_x87_fprem1(a1);
}
// Computes the approximate tangent of the source operand in register ST(0),
// stores the result in ST(0), and pushes a 1.0 onto the FPU register stack. The
// source operand must be given in radians and must be less than ±263. The
// following table shows the unmasked results obtained when computing the
// partial tangent of various classes of numbers, assuming that underflow does
// not occur.
void x87_fptan(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fptan\n", 11);

#if defined(X87_FPTAN)
  a1->status_word &= ~(X87StatusWordFlag::kConditionCode1 |
                       X87StatusWordFlag::kConditionCode2);

  // Get value from ST(0)
  const auto value = a1->get_st(0);

  // Calculate tangent
  auto tan_value = tan(value);

  // Store result in ST(0)
  a1->set_st(0, tan_value);

  // Push 1.0 onto the FPU register stack
  a1->push();
  a1->set_st(0, 1.0);
#else
  orig_x87_fptan(a1);
#endif
}

// Rounds the source value in the ST(0) register to the nearest integral value,
// depending on the current rounding mode (setting of the RC field of the FPU
// control word), and stores the result in ST(0).
void x87_frndint(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_frndint\n", 13);

#if defined(X87_FRNDINT)
  a1->status_word &= ~(X87StatusWordFlag::kConditionCode1);

  // Get current value and round it
  double value = a1->get_st(0);
  double rounded;
  auto round_bits = a1->control_word & X87ControlWord::kRoundingControlMask;

  switch (round_bits) {
  case X87ControlWord::kRoundToNearest: {
    rounded = std::nearbyint(value);
  } break;

  case X87ControlWord::kRoundDown: {
    rounded = std::floor(value);
  } break;
  case X87ControlWord::kRoundUp: {
    rounded = std::ceil(value);
  } break;

  case X87ControlWord::kRoundToZero: {
    rounded = std::trunc(value);
  } break;
  }

  // Store rounded value and update tag
  a1->set_st(0, rounded);
#else
  orig_x87_frndint(a1);
#endif
}

// Truncates the value in the source operand (toward 0) to an integral value and
// adds that value to the exponent of the destination operand. The destination
// and source operands are floating-point values located in registers ST(0) and
// ST(1), respectively. This instruction provides rapid multiplication or
// division by integral powers of 2. The following table shows the results
// obtained when scaling various classes of numbers, assuming that neither
// overflow nor underflow occurs.
void x87_fscale(X87State *state) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fscale\n", 12);

#if defined(X87_FSCALE)
  state->status_word &= ~(X87StatusWordFlag::kConditionCode1);

  // Get values from ST(0) and ST(1)
  double st0 = state->get_st(0);
  double st1 = state->get_st(1);

  // Round ST(1) to nearest integer
  int scale = static_cast<int>(st1);

  // Scale ST(0) by 2^scale using bit manipulation for integer powers
  int32_t exponent = scale + 1023; // IEEE-754 bias
  uint64_t scaleFactor = static_cast<uint64_t>(exponent) << 52;
  double factor = *reinterpret_cast<double *>(&scaleFactor);

  // Multiply ST(0) by scale factor
  double result = st0 * factor;

  // Store result back in ST(0)
  state->set_st(0, result);
#else
  orig_x87_fscale(state);
#endif
}

void x87_fsin(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsin\n", 10);

#if defined(X87_FSIN)
  a1->status_word &= ~(X87StatusWordFlag::kConditionCode1 |
                       X87StatusWordFlag::kConditionCode2);

  // Get current value from top register
  const double value = a1->get_st(0);

  // Convert to NEON vector and calculate sin

  // Store result and update tag
  a1->set_st(0, sin(value));
#else
  orig_x87_fsin(a1);
#endif
}

// Compute the sine and cosine of ST(0); replace ST(0) with the approximate
// sine, and push the approximate cosine onto the register stack.
/*
IF ST(0) < 2^63
    THEN
        C2 := 0;
        TEMP := fcos(ST(0)); // approximation of cosine
        ST(0) := fsin(ST(0)); // approximation of sine
        TOP := TOP − 1;
        ST(0) := TEMP;
    ELSE (* Source operand out of range *)
        C2 := 1;
FI;
*/
void x87_fsincos(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsincos\n", 13);

#if defined(X87_FSINCOS)
  a1->status_word &= ~(X87StatusWordFlag::kConditionCode1 |
                       X87StatusWordFlag::kConditionCode2);

  // Get value from ST(0)
  const auto value = a1->get_st(0);

  // Calculate sine and cosine
  auto sin_value = sin(value);
  auto cos_value = cos(value);

  // Store sine in ST(0)
  a1->set_st(0, sin_value);

  // Push cosine onto the FPU register stack
  a1->push();
  a1->set_st(0, cos_value);

  // Clear C2 condition code bit
  a1->status_word &= ~X87StatusWordFlag::kConditionCode2;
#else
  orig_x87_fsincos(a1);
#endif
}
// Computes square root of ST(0) and stores the result in ST(0).
void x87_fsqrt(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsqrt\n", 11);

#if defined(X87_FSQRT)
  a1->status_word &= ~(X87StatusWordFlag::kConditionCode1);

  // Get current value and calculate sqrt
  const double value = a1->get_st(0);

  a1->status_word |= X87StatusWordFlag::kPrecision;

  // Store result and update tag
  a1->set_st(0, sqrt(value));
#else
  orig_x87_fsqrt(a1);
#endif
}

void x87_fst_STi(X87State *a1, unsigned int st_offset, bool pop) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fst_STi\n", 13);

#if defined(X87_FST_STI)
  // Clear C1 condition code (bit 9)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Copy ST(0) to ST(i)
  a1->set_st(st_offset, a1->get_st(0));

  // Pop if requested
  if (pop) {
    a1->pop();
  }
#else
  orig_x87_fst_STi(a1, st_offset, pop);
#endif
}

X87ResultStatusWord x87_fst_fp32(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fst_fp32\n", 14);

#if defined(X87_FST_FP32)
  auto [value, status_word] = a1->get_st_const32(0);
  float tmp = value;
  return {*reinterpret_cast<uint32_t *>(&tmp), status_word};
#else
  return orig_x87_fst_fp32(a1);
#endif
}

X87ResultStatusWord x87_fst_fp64(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fst_fp64\n", 14);

#if defined(X87_FST_FP64)
  // Create temporary double to ensure proper value representation
  auto [value, status_word] = a1->get_st_const(0);
  double tmp = value;
  return {*reinterpret_cast<uint64_t *>(&tmp), status_word};
#else
  return orig_x87_fst_fp64(a1);
#endif
}

X87Float80 x87_fst_fp80(X87State const *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fst_fp80\n", 14);

#if defined(X87_FST_FP80)
  // Get value from ST(0)
  auto [value, status_word] = a1->get_st_const(0);

  float tmp = value;
  uint32_t float32 = *reinterpret_cast<uint32_t *>(&tmp);
  ;

  // Extract components from float32
  uint32_t mantissa = float32 & 0x7FFFFF; // 23 bits
  uint8_t exp = (float32 >> 23) & 0xFF;   // 8 bits
  uint16_t sign = (float32 >> 31) << 15;  // Move sign to bit 15

  X87Float80 result;

  // Handle zero
  if (exp == 0 && mantissa == 0) {
    result.mantissa = 0;
    result.exponent = sign;
    return result;
  }

  // Handle subnormal numbers
  if (exp == 0) {
    // Set denormal flag

    // Count leading zeros to normalize
    int leading_zeros =
        __builtin_clz(mantissa) - 8; // -8 because mantissa is in upper 23 bits
    mantissa <<= leading_zeros;

    // Adjust exponent for normalization
    exp = 1 - leading_zeros;
  }
  // Handle infinity or NaN
  else if (exp == 255) {
    // Set invalid operation flag if NaN

    result.mantissa = (uint64_t)mantissa << 40 | 0x8000000000000000ULL;
    result.exponent = sign | 0x7FFF; // Maximum exponent
    return result;
  }

  // Normal numbers: Convert to x87 format
  // Shift 23-bit mantissa to 64 bits and set explicit integer bit
  result.mantissa = ((uint64_t)mantissa << 40) | 0x8000000000000000ULL;

  // Bias adjustment: IEEE 754 bias(127) to x87 bias(16383)
  result.exponent = sign | (exp + 16383 - 127);

  return result;
#else
  return orig_x87_fst_fp80(a1);
#endif
}
void x87_fsub_ST(X87State *a1, unsigned int st_offset1, unsigned int st_offset2,
                 bool pop) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsub_ST\n", 13);

#if defined(X87_FSUB_ST)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get register indices and values
  const auto val1 = a1->get_st(st_offset1);
  const auto val2 = a1->get_st(st_offset2);

  // Perform subtraction and store result in ST(st_offset1)
  a1->set_st(st_offset1, val1 - val2);

  if (pop) {
    a1->pop();
  }
#else
  orig_x87_fsub_ST(a1, st_offset1, st_offset2, pop);
#endif
}

void x87_fsub_f32(X87State *a1, unsigned int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsub_f32\n", 14);

#if defined(X87_FSUB_F32)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<float *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, st0 - value);
#else
  orig_x87_fsub_f32(a1, a2);
#endif
}

void x87_fsub_f64(X87State *a1, unsigned long long a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsub_f64\n", 14);

#if defined(X87_FSUB_F64)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<double *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, st0 - value);
#else
  orig_x87_fsub_f64(a1, a2);
#endif
}

void x87_fsubr_ST(X87State *a1, unsigned int st_offset1,
                  unsigned int st_offset2, bool pop) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsubr_ST\n", 14);

#if defined(X87_FSUBR_ST)
  // Clear condition code 1 and exception flags
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get register indices and values

  const auto val1 = a1->get_st(st_offset1);
  const auto val2 = a1->get_st(st_offset2);

  // Perform reversed subtraction and store result in ST(st_offset1)
  a1->set_st(st_offset1, val2 - val1);

  if (pop) {
    a1->pop();
  }
#else
  orig_x87_fsubr_ST(a1, st_offset1, st_offset2, pop);
#endif
}

void x87_fsubr_f32(X87State *a1, unsigned int a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsubr_f32\n", 15);

#if defined(X87_FSUBR_F32)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<float *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, value - st0);
#else
  orig_x87_fsubr_f32(a1, a2);
#endif
}

void x87_fsubr_f64(X87State *a1, unsigned long long a2) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fsubr_f64\n", 15);

#if defined(X87_FSUBR_F64)
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto value = *reinterpret_cast<double *>(&a2);
  auto st0 = a1->get_st(0);

  a1->set_st(0, value - st0);
#else
  orig_x87_fsubr_f64(a1, a2);
#endif
}

void x87_fucom(X87State *a1, unsigned int src, unsigned int pop) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fucom\n", 11);
#if defined(X87_FUCOM)
  auto st0 = a1->get_st(0);

  // Clear condition code bits C0, C2, C3 (bits 8, 9, 14)
  a1->status_word &=
      ~(kConditionCode0 | kConditionCode1 | kConditionCode2 | kConditionCode3);

  // Set condition codes based on comparison
  if (isnan(st0) || isnan(src)) {
    a1->status_word |=
        kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
  } else if (st0 > src) {
    // Leave C0=C2=C3=0
  } else if (st0 < src) {
    a1->status_word |= kConditionCode0; // Set C0=1
  } else {                              // st0 == src
    a1->status_word |= kConditionCode3; // Set C3=1
  }

  // Handle pops if requested
  if (pop) {
    a1->pop();
  }
#else
  orig_x87_fucom(a1, src, pop);
#endif
}
// Compare ST(0) with ST(i), check for ordered values, set status flags
// accordingly, and pop register stack.
uint32_t x87_fucomi(X87State *state, unsigned int st_offset, bool pop_stack) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fucomi\n", 12);

#if defined(X87_FUCOMI)
  state->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto st0_val = state->get_st(0);
  auto sti_val = state->get_st(st_offset);

  uint32_t flags = 0;
  /*
  Filters: fcomi
  Randomness seeded to: 3528984885
  x87_fcomi_less
  x87_fcomi result: 0x000000000000000
  x87_fcomi_greater
  x87_fcomi result: 0x000000020000000
  x87_fcomi_equal
  x87_fcomi result: 0x000000060000000
  */

  if (st0_val < sti_val) {
    flags = 0x000000000000000;
  } else if (st0_val > sti_val) {
    flags = 0x000000020000000;
  } else {
    flags = 0x000000060000000;
  }

  if (pop_stack) {
    state->pop();
  }

  return flags;
#else
  return orig_x87_fucomi(state, st_offset, pop_stack);
#endif
}

/*
C1 := sign bit of ST; (* 0 for positive, 1 for negative *)
CASE (class of value or number in ST(0)) OF
    Unsupported:C3, C2, C0 := 000;
    NaN:
        C3, C2, C0 := 001;
    Normal:
        C3, C2, C0 := 010;
    Infinity:
        C3, C2, C0 := 011;
    Zero:
        C3, C2, C0 := 100;
    Empty:
        C3, C2, C0 := 101;
    Denormal:
        C3, C2, C0 := 110;
ESAC;

*/
void x87_fxam(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fxam\n", 10);

#if defined(X87_FXAM)
  // Get tag state for ST(0)
  X87TagState tag = a1->get_st_tag(0);

  // simple_printf("tag: %d\n", tag);

  static_assert((X87StatusWordFlag::kConditionCode0 |
                 X87StatusWordFlag::kConditionCode1 |
                 X87StatusWordFlag::kConditionCode2 |
                 X87StatusWordFlag::kConditionCode3) == 0x4700);

  // Clear C3,C2,C1,C0 bits
  a1->status_word &= ~(
      X87StatusWordFlag::kConditionCode0 | X87StatusWordFlag::kConditionCode1 |
      X87StatusWordFlag::kConditionCode2 | X87StatusWordFlag::kConditionCode3);

  // Handle empty and zero based on tag word
  if (tag == X87TagState::kEmpty) {
    a1->status_word |= X87StatusWordFlag::kConditionCode3 |
                       X87StatusWordFlag::kConditionCode0; // C3=1, C0=1 (101)
    return;
  }
  if (tag == X87TagState::kZero) {
    a1->status_word |= X87StatusWordFlag::kConditionCode3; // C3=1 (100)
    return;
  }

  // Get actual value for other cases
  auto value = a1->get_st(0);

  // Set C1 based on sign
  if (signbit(value)) {
    a1->status_word |= X87StatusWordFlag::kConditionCode1;
  }

  // Set C3,C2,C0 based on value type
  if (isnan(value)) {
    a1->status_word |= X87StatusWordFlag::kConditionCode0; // 001
  } else if (isinf(value)) {
    a1->status_word |= X87StatusWordFlag::kConditionCode2 |
                       X87StatusWordFlag::kConditionCode0; // 011
  } else if (fpclassify(value) == FP_SUBNORMAL) {
    a1->status_word |= X87StatusWordFlag::kConditionCode3 |
                       X87StatusWordFlag::kConditionCode2; // 110
  } else {
    a1->status_word |= X87StatusWordFlag::kConditionCode2; // 010 (normal)
  }
#else
  orig_x87_fxam(a1);
#endif
}

void x87_fxch(X87State *a1, unsigned int st_offset) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fxch\n", 10);

#if defined(X87_FXCH)
  // Clear condition code 1
  a1->status_word &= ~X87StatusWordFlag::kConditionCode1;

  auto st0 = a1->get_st(0);
  auto sti = a1->get_st(st_offset);

  a1->set_st(0, sti);
  a1->set_st(st_offset, st0);
#else
  orig_x87_fxch(a1, st_offset);
#endif
}

//Notes: I believe this implementation is reasonable
void x87_fxtract(X87State *a1) {
  SIMDGuard simd_guard;

  LOG(1, "x87_fxtract\n", 13);

#if defined(X87_FXTRACT)
  // Get st0 value
  auto st0 = a1->get_st(0);

  //Get components of st0
  X87Float80 result = doubleToX87Components(st0);

  // Store exponent in ST(0)
  a1->set_st(0, result.exponent);

  // Make room on the stack and push the mantissa (significand)
  a1->push();
  a1->set_st(0, result.mantissa);

#else
  orig_x87_fxtract(a1);
#endif
}

void fyl2x_common(X87State *state, float constant){
  // Clear condition code 1
  state->status_word &= ~X87StatusWordFlag::kConditionCode1;

  // Get x from ST(0) and y from ST(1)
  auto st0 = state->get_st(0);
  auto st1 = state->get_st(1);

  // Calculate y * log2(x)
  auto result = st1 * (log2(st0+constant));

  // Pop ST(0)
  state->pop();

  // Store result in new ST(0)
  state->set_st(0, result);
}

// Replace ST(1) with (ST(1) ∗ log2ST(0)) and pop the register stack.
void x87_fyl2x(X87State *state) {
  SIMDGuard simd_guard;
  LOG(1, "x87_fyl2x\n", 12);

#if defined(X87_FYL2X)
  fyl2x_common(state, 0.0);
#else
  orig_x87_fyl2x(state);
#endif
}

// Replace ST(1) with (ST(1) ∗ log2ST(0 + 1.0)) and pop the register stack.
void x87_fyl2xp1(X87State *state) {
  SIMDGuard simd_guard;
  LOG(1, "x87_fyl2xp1\n", 14);

#if defined(X87_FYL2XP1)
  fyl2x_common(state, 1.0);
#else
  orig_x87_fyl2xp(state);
#endif
}

X87_TRAMPOLINE(sse_pcmpestri, x8)
X87_TRAMPOLINE(sse_pcmpestrm, x8)
X87_TRAMPOLINE(sse_pcmpistri, x8)
X87_TRAMPOLINE(sse_pcmpistrm, x8)
X87_TRAMPOLINE(is_ldt_initialized, x8)
X87_TRAMPOLINE(get_ldt, x8)
X87_TRAMPOLINE(set_ldt, x8)
X87_TRAMPOLINE(execution_mode_for_code_segment_selector, x8)
X87_TRAMPOLINE(mov_segment, x8)
X87_TRAMPOLINE(abi_for_address, x8)
X87_TRAMPOLINE(determine_state_recovery_action, x8)
X87_TRAMPOLINE(get_segment_limit, x8)
X87_TRAMPOLINE(translator_set_variant, x8)

X87_TRAMPOLINE(runtime_cpuid, x22)
X87_TRAMPOLINE(runtime_wide_udiv_64, x25)
X87_TRAMPOLINE(runtime_wide_sdiv_64, x22)