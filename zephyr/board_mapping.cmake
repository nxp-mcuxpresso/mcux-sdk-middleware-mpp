# Map Zephyr board names to MPP board directories
if(BOARD MATCHES "mimxrt1170_evk")
    set(MPP_BOARD "evkbmimxrt1170")
elseif(BOARD MATCHES "mimxrt1060_evk")
    set(MPP_BOARD "evkbmimxrt1060")
endif()