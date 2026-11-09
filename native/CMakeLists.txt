cmake_minimum_required(VERSION 3.20)
project(reflexcore VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

function(reflex_strict_compile target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /fp:strict)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror
      -fno-fast-math -ffp-contract=off -frounding-math)
  else()
    message(FATAL_ERROR "Compiler floating-point behavior has not been configured")
  endif()
endfunction()

add_library(reflexcore SHARED src/scheduler.cpp src/velocity.cpp src/pose.cpp src/force.cpp src/matrix.cpp src/matrix_family.cpp src/callback.cpp src/inverse3.cpp src/factorization.cpp src/inverse4.cpp src/rotor_product4.cpp src/constraint_solve.cpp src/preparation.cpp src/mechanism_core.cpp src/bike_force_stage.cpp)
target_include_directories(reflexcore PUBLIC include)
target_compile_definitions(reflexcore PRIVATE REFLEX_CORE_BUILD)
set_target_properties(reflexcore PROPERTIES CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES VERSION ${PROJECT_VERSION} SOVERSION 1)
reflex_strict_compile(reflexcore)

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  option(REFLEX_VERIFY_CAPTURE_SOURCES "Revalidate private original capture files as well as exported fixtures" ON)
  add_executable(reflex_force_stage_tests tests/force_stage_tests.cpp)
  target_link_libraries(reflex_force_stage_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_force_stage_tests)
  add_test(NAME bike_force_stage_contract COMMAND reflex_force_stage_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/out/original-math-force-stage-active-branches/inputs.bin
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/out/original-math-force-stage-active-branches/original.bin)
  add_test(NAME bike_force_stage_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_force_stage_fixture.py
    --library $<TARGET_FILE:reflexcore>
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bike_force_stage_live)
  add_test(NAME bike_force_stage_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../tools/export_force_stage_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bike_force_stage_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME bike_force_stage_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/export_force_stage_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bike_force_stage_live)
  endif()
  add_test(NAME bike_force_stage_original COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_force_stage_original.py --library $<TARGET_FILE:reflexcore>)
  add_executable(reflex_core_tests tests/scheduler_tests.cpp tests/c_header_check.c)
  target_link_libraries(reflex_core_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_core_tests)
  add_test(NAME core_contract COMMAND reflex_core_tests)
  add_executable(reflex_velocity_tests tests/velocity_tests.cpp)
  target_link_libraries(reflex_velocity_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_velocity_tests)
  add_test(NAME velocity_contract COMMAND reflex_velocity_tests)
  add_executable(reflex_pose_tests tests/pose_tests.cpp)
  target_link_libraries(reflex_pose_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_pose_tests)
  add_test(NAME pose_contract COMMAND reflex_pose_tests)
  add_executable(reflex_force_tests tests/force_tests.cpp)
  target_link_libraries(reflex_force_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_force_tests)
  add_test(NAME force_contract COMMAND reflex_force_tests)
  add_executable(reflex_matrix_tests tests/matrix_tests.cpp)
  target_link_libraries(reflex_matrix_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_matrix_tests)
  add_test(NAME matrix_contract COMMAND reflex_matrix_tests)
  add_executable(reflex_matrix_family_tests tests/matrix_family_tests.cpp)
  target_link_libraries(reflex_matrix_family_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_matrix_family_tests)
  add_test(NAME matrix_family_contract COMMAND reflex_matrix_family_tests)
  add_executable(reflex_callback_tests tests/callback_tests.cpp)
  target_link_libraries(reflex_callback_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_callback_tests)
  add_test(NAME callback_contract COMMAND reflex_callback_tests)
  add_test(NAME callback_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_callback_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME callback_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_callback_live.py --library $<TARGET_FILE:reflexcore>
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/callback_live/vectors.tsv)
  add_test(NAME callback_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_callback_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/callback_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME callback_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_callback_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/callback_live)
  endif()
  add_executable(reflex_mechanism_core_tests tests/mechanism_core_tests.cpp)
  target_link_libraries(reflex_mechanism_core_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_mechanism_core_tests)
  add_test(NAME mechanism_core_contract COMMAND reflex_mechanism_core_tests)
  add_test(NAME mechanism_core_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mechanism_core_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME mechanism_core_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mechanism_core_live.py --library $<TARGET_FILE:reflexcore>
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mechanism_core_live/vectors.tsv)
  add_test(NAME mechanism_core_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_mechanism_core_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mechanism_core_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME mechanism_core_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_mechanism_core_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mechanism_core_live)
  endif()
  add_executable(reflex_preparation_tests tests/preparation_tests.cpp)
  target_link_libraries(reflex_preparation_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_preparation_tests)
  add_test(NAME preparation_contract COMMAND reflex_preparation_tests)
  add_test(NAME preparation_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_preparation_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME preparation_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_preparation_live.py --library $<TARGET_FILE:reflexcore>
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/preparation_live/vectors.tsv)
  add_test(NAME preparation_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_preparation_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/preparation_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME preparation_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_preparation_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/preparation_live)
  endif()
  add_executable(reflex_constraint_solve_tests tests/constraint_solve_tests.cpp)
  target_link_libraries(reflex_constraint_solve_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_constraint_solve_tests)
  add_test(NAME constraint_solve_contract COMMAND reflex_constraint_solve_tests)
  add_test(NAME constraint_solve_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_constraint_solve_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME constraint_solve_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_constraint_solve_live.py --library $<TARGET_FILE:reflexcore>
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/constraint_solve_live/vectors.tsv)
  add_test(NAME constraint_solve_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_constraint_solve_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/constraint_solve_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME constraint_solve_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_constraint_solve_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/constraint_solve_live)
  endif()
  add_executable(reflex_rotor_product4_tests tests/rotor_product4_tests.cpp)
  target_link_libraries(reflex_rotor_product4_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_rotor_product4_tests)
  add_test(NAME rotor_product4_contract COMMAND reflex_rotor_product4_tests)
  add_test(NAME rotor_product4_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_rotor_product4_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME rotor_product4_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_rotor_product4_live.py --library $<TARGET_FILE:reflexcore>
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/rotor_product4_live/vectors.tsv)
  add_test(NAME rotor_product4_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_rotor_product4_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/rotor_product4_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME rotor_product4_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_rotor_product4_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/rotor_product4_live)
  endif()
  add_executable(reflex_inverse4_tests tests/inverse4_tests.cpp)
  target_link_libraries(reflex_inverse4_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_inverse4_tests)
  add_test(NAME inverse4_contract COMMAND reflex_inverse4_tests)
  add_test(NAME inverse4_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inverse4_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME inverse4_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inverse4_live.py --library $<TARGET_FILE:reflexcore>
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inverse4_live/vectors.tsv)
  add_test(NAME inverse4_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_inverse4_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inverse4_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME inverse4_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_inverse4_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inverse4_live)
  endif()
  add_executable(reflex_factorization_tests tests/factorization_tests.cpp)
  target_link_libraries(reflex_factorization_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_factorization_tests)
  add_test(NAME factorization_contract COMMAND reflex_factorization_tests)
  add_test(NAME factorization_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_factorization_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME factorization_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_factorization_live.py --library $<TARGET_FILE:reflexcore>
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/factorization_live/vectors.tsv)
  add_test(NAME factorization_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_factorization_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/factorization_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME factorization_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_factorization_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/factorization_live)
  endif()
  add_executable(reflex_inverse3_tests tests/inverse3_tests.cpp)
  target_link_libraries(reflex_inverse3_tests PRIVATE reflexcore)
  reflex_strict_compile(reflex_inverse3_tests)
  add_test(NAME inverse3_contract COMMAND reflex_inverse3_tests)
  add_test(NAME inverse3_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inverse3_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME inverse3_live COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inverse3_live.py --library $<TARGET_FILE:reflexcore>
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inverse3_live/vectors.tsv)
  add_test(NAME inverse3_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_inverse3_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inverse3_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME inverse3_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_inverse3_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inverse3_live)
  endif()
  add_test(NAME matrix_family_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_matrix_family_oracle.py --library $<TARGET_FILE:reflexcore>)
  add_test(NAME matrix_family_live COMMAND reflex_matrix_family_tests
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_family_live/vectors.tsv)
  add_test(NAME matrix_family_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_matrix_family_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_family_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME matrix_family_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_matrix_family_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_family_live)
  endif()
  add_test(NAME matrix_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_matrix_oracle.py
    --library $<TARGET_FILE:reflexcore>)
  add_test(NAME matrix_live COMMAND reflex_matrix_tests
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_live/vectors.tsv)
  add_test(NAME matrix_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_matrix_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME matrix_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_matrix_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_live)
  endif()
  add_test(NAME force_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_force_oracle.py
    --library $<TARGET_FILE:reflexcore>)
  add_test(NAME force_live COMMAND reflex_force_tests
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/force_live/vectors.tsv)
  add_test(NAME force_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_force_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/force_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME force_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_force_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/force_live)
  endif()
  add_test(NAME pose_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pose_oracle.py
    --library $<TARGET_FILE:reflexcore>)
  add_test(NAME pose_live COMMAND reflex_pose_tests
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pose_live/vectors.tsv)
  add_test(NAME pose_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_pose_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pose_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME pose_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_pose_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pose_live)
  endif()
  add_test(NAME velocity_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_velocity_oracle.py
    --library $<TARGET_FILE:reflexcore>)
  add_test(NAME velocity_live COMMAND reflex_velocity_tests
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/velocity_live/vectors.tsv)
  add_test(NAME velocity_fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_velocity_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/velocity_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME velocity_fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_velocity_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/velocity_live)
  endif()
  add_test(NAME scheduler_live COMMAND reflex_core_tests
    --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scheduler_live/vectors.tsv)
  add_test(NAME fixture_manifest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_scheduler_fixtures.py
    --verify-vectors-only ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scheduler_live)
  if(REFLEX_VERIFY_CAPTURE_SOURCES)
    add_test(NAME fixture_provenance COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../_re/trace/export_scheduler_fixtures.py
      --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scheduler_live)
  endif()
  add_test(NAME scheduler_oracle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scheduler_oracle.py
    --library $<TARGET_FILE:reflexcore>)
endif()
