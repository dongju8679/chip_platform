#source files for s5740

set(s5740_c_src
  ${chipset_dir}/Src/rom_app.c
  ${chipset_dir}/Src/rom_common.c
  ${chipset_dir}/Src/chip.c
  ${chipset_dir}/Src/usercode.c
  ${chipset_dir}/Src/mtvec.c
  ${chipset_dir}/Src/ipc.c
  ${chipset_dir}/Src/timeout.c
  ${chipset_dir}/Src/log_device.c
  ${chipset_dir}/Src/pspeedy.c
  ${chipset_dir}/Src/exceptions.c
  ${chipset_dir}/Src/interface.c
  ${chipset_dir}/Src/i2sr.c)

set(s5740_riscv_src
  ${chipset_dir}/Src/crt.S)

list(APPEND mcu_sources ${s5740_c_src})
list(APPEND mcu_sources ${s5740_riscv_src})
