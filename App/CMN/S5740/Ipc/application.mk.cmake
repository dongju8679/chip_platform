#BMARK file for $bmark

set(s5720_ipc_c_src
${app_cmn_dir}/Ipc/Src/rfm_ipc_msg.c
${app_cmn_dir}/Ipc/Src/rfm_ipc_handler.c
)

list(APPEND mcu_sources ${s5720_ipc_c_src})
