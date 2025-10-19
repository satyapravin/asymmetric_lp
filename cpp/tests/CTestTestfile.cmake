# CMake generated Testfile for 
# Source directory: /home/pravin/dev/asymmetric_lp/cpp/tests
# Build directory: /home/pravin/dev/asymmetric_lp/cpp/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_tests "/home/pravin/dev/asymmetric_lp/cpp/tests/run_tests")
set_tests_properties(unit_tests PROPERTIES  TIMEOUT "30" WORKING_DIRECTORY "/home/pravin/dev/asymmetric_lp/cpp/tests" _BACKTRACE_TRIPLES "/home/pravin/dev/asymmetric_lp/cpp/tests/CMakeLists.txt;93;add_test;/home/pravin/dev/asymmetric_lp/cpp/tests/CMakeLists.txt;0;")
subdirs("_deps/doctest-build")
