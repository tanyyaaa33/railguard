# CMake generated Testfile for 
# Source directory: /Users/tanya/Documents/railguard
# Build directory: /Users/tanya/Documents/railguard/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(tarjan "/Users/tanya/Documents/railguard/build/test_tarjan")
set_tests_properties(tarjan PROPERTIES  _BACKTRACE_TRIPLES "/Users/tanya/Documents/railguard/CMakeLists.txt;44;add_test;/Users/tanya/Documents/railguard/CMakeLists.txt;0;")
add_test(dsu_rollback "/Users/tanya/Documents/railguard/build/test_dsu")
set_tests_properties(dsu_rollback PROPERTIES  _BACKTRACE_TRIPLES "/Users/tanya/Documents/railguard/CMakeLists.txt;45;add_test;/Users/tanya/Documents/railguard/CMakeLists.txt;0;")
add_test(cascade "/Users/tanya/Documents/railguard/build/test_cascade")
set_tests_properties(cascade PROPERTIES  ENVIRONMENT "RAILGUARD_DATA=/Users/tanya/Documents/railguard/data" _BACKTRACE_TRIPLES "/Users/tanya/Documents/railguard/CMakeLists.txt;46;add_test;/Users/tanya/Documents/railguard/CMakeLists.txt;0;")
add_test(sprague "/Users/tanya/Documents/railguard/build/test_sprague")
set_tests_properties(sprague PROPERTIES  _BACKTRACE_TRIPLES "/Users/tanya/Documents/railguard/CMakeLists.txt;48;add_test;/Users/tanya/Documents/railguard/CMakeLists.txt;0;")
