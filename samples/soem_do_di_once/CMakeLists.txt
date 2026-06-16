add_executable(soem_do_di_once soem_do_di_once.c)
target_link_libraries(soem_do_di_once soem)
install(TARGETS soem_do_di_once DESTINATION bin)
