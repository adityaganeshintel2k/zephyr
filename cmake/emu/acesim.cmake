# SPDX-License-Identifier: Apache-2.0

# The ACE_SIM_DIR environment variable specifies the directory
# where the simulator was put, it is $HOME/acesim by default
# if ACE_SIM_DIR is not assign.
if(DEFINED ENV{ACE_SIM_DIR})
  set(SIM_DIR $ENV{ACE_SIM_DIR})
else()
  set(SIM_DIR ${ZEPHYR_STD_SIM_MODULE_DIR})
endif()

find_program(
  ACESIM
  PATHS ${SIM_DIR}
  NO_DEFAULT_PATH
  NAMES acesim.py
  )

if(${CONFIG_SOC} MATCHES "ace[0-9][0-9]_.+")
  set(SOC "${CONFIG_SOC}")
else()
  set(SOC "${CONFIG_SOC}_${CONFIG_RIMAGE_TARGET}")
endif()

set(ACESIM_FLAGS
  --soc ${SOC}
  --rimage ${APPLICATION_BINARY_DIR}/zephyr/zephyr.ri
  )

add_custom_target(run_acesim
  COMMAND
  ${ACESIM}
  ${ACESIM_FLAGS}
  WORKING_DIRECTORY ${APPLICATION_BINARY_DIR}
  DEPENDS zephyr.ri
  USES_TERMINAL
  )

add_custom_target(debugserver_acesim
  COMMAND
  ${ACESIM}
  ${ACESIM_FLAGS}
  --debug-port 20000
  WORKING_DIRECTORY ${APPLICATION_BINARY_DIR}
  DEPENDS zephyr.ri
  USES_TERMINAL
  )

add_custom_target(trace_acesim
  COMMAND
  ${ACESIM}
  ${ACESIM_FLAGS}
  --trace
  WORKING_DIRECTORY ${APPLICATION_BINARY_DIR}
  DEPENDS zephyr.ri
  USES_TERMINAL
  )
