# This software is dual-licensed under GPLv3 and a commercial
# license. See the file LICENSE.md distributed with this software for
# full license information.

target_sources(soem PRIVATE
  osal/darwin/osal.c
  osal/darwin/osal_defs.h
  oshw/darwin/oshw.c
  oshw/darwin/oshw.h
  oshw/darwin/nicdrv.c
  oshw/darwin/nicdrv.h
)

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${SOEM_SOURCE_DIR}/osal/darwin>
  $<BUILD_INTERFACE:${SOEM_SOURCE_DIR}/oshw/darwin>
  $<INSTALL_INTERFACE:include/soem>
)

foreach(target IN ITEMS
    soem
    ec_sample
    eepromtool
    eni_test
    firm_update
    simple_ng
    slaveinfo)
  if (TARGET ${target})
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
    )
  endif()
endforeach()

target_link_libraries(soem PUBLIC pthread)

install(FILES
  osal/darwin/osal_defs.h
  oshw/darwin/nicdrv.h
  DESTINATION include/soem
)
