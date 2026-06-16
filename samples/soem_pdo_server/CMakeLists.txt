add_executable(soem_pdo_server soem_pdo_server.c)
target_link_libraries(soem_pdo_server soem)
install(TARGETS soem_pdo_server DESTINATION bin)
