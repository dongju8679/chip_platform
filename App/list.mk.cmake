# Applications to be included in the binary

if(NOT DEFINED ATE)
        SET(ATE FALSE)
endif()

SET(RF_APPLICATION)
SET(CMN_APPLICATION)
SET(L1_APPLICATION)

if(NOT X86_SIMULATOR_ENABLE)
        SET(app_cmn_dir ${src_dir}/App/CMN/${TARGET})
        SET(app_l1_dir ${src_dir}/App/L1/${TARGET})
        SET(app_rf_dir ${src_dir}/App/RF/${TARGET})
else()
        SET(app_rf_dir ${src_dir}/App/RF/${TARGET})
        list(APPEND incs ${src_dir}/App/CMN/${TARGET}/Ipc/Inc)
        list(APPEND incs ${src_dir}/App/L1/${TARGET}/Ipc/Inc)
        list(APPEND incs ${src_dir}/App/L1/${TARGET}/Hal/Inc)
endif()

if(${ATE} STREQUAL TRUE)
        include(${app_rf_dir}/list.mk.cmake)
else()
        if (EXISTS ${app_cmn_dir})
                include(${app_cmn_dir}/list.mk.cmake)
                list(APPEND APPS ${CMN_APPLICATION})
        endif()
        if (EXISTS ${app_l1_dir})
                include(${app_l1_dir}/list.mk.cmake)
                list(APPEND APPS ${L1_APPLICATION})
        endif()
        if (EXISTS ${app_rf_dir})
                include(${app_rf_dir}/list.mk.cmake)
                list(APPEND APPS ${RF_APPLICATION})
        endif()
endif()

# Set custom DEFINES if necessary

if(ATE)
        list(APPEND CUSTOM_DEFINE -DATE)
endif()
