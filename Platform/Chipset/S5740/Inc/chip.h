/* PLACEHOLDER - paste the original Chipset/S5740/Inc/chip.h, fixing only the following.
 * (ERROR_CODE enum has 0xDEADxxxx values; do not regenerate from memory - apply to your original)
 *   1) VCO_MODE_T enum first item  VCO_MODE0 = 0;  ->  VCO_MODE0 = 0,   (semicolon -> comma)
 *   2) extern unsigned char __bas_end[];  ->  __bss_end[];          (pairs with __bss_start)
 *   3) (verify) struct BUILD_INFO build_info;  is defined in the header without extern
 *      -> multiple-definition risk if several .c include it. Verify original intent (whether extern).
 *   CRLF -> LF.
 */
