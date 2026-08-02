# PLACEHOLDER - paste the original riscv.mk.cmake, fixing only this one thing:
#   9 source paths ${common_dir}/src/...  ->  ${common_dir}/Src/...  (lowercase src -> uppercase Src)
#   (only the boot.c line already uses Src. The other 9 use src, which fails on case-sensitive Linux)
#   CRLF -> LF.
