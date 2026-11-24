cmake_minimum_required(VERSION 3.16)
project(enchbin_builder C)

# cmake — только обёртка: builder.c — единая точка сборки.
add_custom_target(builder ALL
  COMMAND ${CMAKE_C_COMPILER} -O2 -o ${CMAKE_BINARY_DIR}/builder
          ${CMAKE_CURRENT_SOURCE_DIR}/../builder.c
  COMMENT "building EnchBin builder")

add_custom_target(build
  COMMAND ${CMAKE_BINARY_DIR}/builder build
  DEPENDS builder
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_custom_target(run
  COMMAND ${CMAKE_BINARY_DIR}/builder run
  DEPENDS builder
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
