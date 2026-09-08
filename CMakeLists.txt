cmake_minimum_required(VERSION 3.20)
project(overcalc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(overcalc_core
  src/editor.cpp
  src/ast.cpp
  src/parser.cpp
  src/eval.cpp
  src/render.cpp
  src/latex.cpp
  src/format.cpp
  src/ast_dump.cpp
  src/symbols.cpp
  src/simplify.cpp
  src/derivative.cpp
)
target_include_directories(overcalc_core PUBLIC include)

add_executable(overcalc src/main.cpp)
target_link_libraries(overcalc PRIVATE overcalc_core)

include(CTest)
if(BUILD_TESTING)
  set(OVERCALC_TEST_FORMULA_FILE "${CMAKE_CURRENT_BINARY_DIR}/formula-input.txt")
  file(WRITE "${OVERCALC_TEST_FORMULA_FILE}" "sin(pi/2)+50%")
  set(OVERCALC_BATCH_FORMULA_FILE "${CMAKE_CURRENT_BINARY_DIR}/formula-batch.txt")
  file(WRITE "${OVERCALC_BATCH_FORMULA_FILE}" "2+2\n# comment\nsin(pi/2)+50%\n")
  set(OVERCALC_TUI_INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/tui-input.txt")
  file(WRITE "${OVERCALC_TUI_INPUT_FILE}" "x+0\n:derive x\nx^2\n:q\n")
  set(OVERCALC_TUI_PARTIAL_INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/tui-partial-input.txt")
  file(WRITE "${OVERCALC_TUI_PARTIAL_INPUT_FILE}" "1+\n\\frac{1}{\n:q\n")
  set(OVERCALC_TUI_MATH_ERROR_INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/tui-math-error-input.txt")
  file(WRITE "${OVERCALC_TUI_MATH_ERROR_INPUT_FILE}" "\\frac{\\sqrt{4}2}{2-2}\n:q\n")
  set(OVERCALC_SUGGESTION_CHECK "${CMAKE_CURRENT_BINARY_DIR}/check-latex-suggestion.cmake")
  file(WRITE "${OVERCALC_SUGGESTION_CHECK}" [=[
execute_process(
  COMMAND "${OVERCALC_EXE}" --no-color "\\alpa"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
string(CONCAT combined "${out}" "${err}")
if(NOT result EQUAL 1)
  message(FATAL_ERROR "expected parse failure, got ${result}")
endif()
if(NOT combined MATCHES "did you mean \\\\alpha")
  message(FATAL_ERROR "missing suggestion in output: ${combined}")
endif()
]=])
  set(OVERCALC_TUI_CHECK "${CMAKE_CURRENT_BINARY_DIR}/check-tui.cmake")
  file(WRITE "${OVERCALC_TUI_CHECK}" [=[
execute_process(
  COMMAND "${OVERCALC_EXE}" --no-color --tui
  INPUT_FILE "${TUI_INPUT_FILE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
string(CONCAT combined "${out}" "${err}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "tui failed: ${combined}")
endif()
if(NOT combined MATCHES "Simplified")
  message(FATAL_ERROR "missing simplified preview: ${combined}")
endif()
if(NOT combined MATCHES "Derivative d/dx")
  message(FATAL_ERROR "missing derivative preview: ${combined}")
endif()
]=])
  set(OVERCALC_TUI_PARTIAL_CHECK "${CMAKE_CURRENT_BINARY_DIR}/check-tui-partial.cmake")
  file(WRITE "${OVERCALC_TUI_PARTIAL_CHECK}" [=[
execute_process(
  COMMAND "${OVERCALC_EXE}" --no-color --tui
  INPUT_FILE "${TUI_PARTIAL_INPUT_FILE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
string(CONCAT combined "${out}" "${err}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "tui partial failed: ${combined}")
endif()
if(combined MATCHES "Error:")
  message(FATAL_ERROR "partial tui preview should not print Error: ${combined}")
endif()
if(NOT combined MATCHES "Editing  incomplete expression")
  message(FATAL_ERROR "missing editing fallback: ${combined}")
endif()
]=])
  set(OVERCALC_TUI_MATH_ERROR_CHECK "${CMAKE_CURRENT_BINARY_DIR}/check-tui-math-error.cmake")
  file(WRITE "${OVERCALC_TUI_MATH_ERROR_CHECK}" [=[
execute_process(
  COMMAND "${OVERCALC_EXE}" --no-color --tui
  INPUT_FILE "${TUI_MATH_ERROR_INPUT_FILE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
string(CONCAT combined "${out}" "${err}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "tui math-error failed: ${combined}")
endif()
if(NOT combined MATCHES "Math error  division by zero")
  message(FATAL_ERROR "missing math error: ${combined}")
endif()
if(combined MATCHES "Editing  incomplete expression")
  message(FATAL_ERROR "parseable math error should not be labeled incomplete: ${combined}")
endif()
]=])
  set(OVERCALC_BATCH_ERROR_CHECK "${CMAKE_CURRENT_BINARY_DIR}/check-batch-error.cmake")
  file(WRITE "${OVERCALC_BATCH_ERROR_CHECK}" [=[
file(WRITE "${BATCH_ERROR_FILE}" "2+2\n2+\n")
execute_process(
  COMMAND "${OVERCALC_EXE}" --no-color --batch --file "${BATCH_ERROR_FILE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
string(CONCAT combined "${out}" "${err}")
if(NOT result EQUAL 1)
  message(FATAL_ERROR "expected batch failure, got ${result}")
endif()
if(NOT combined MATCHES "2  2\\+  error: parse error")
  message(FATAL_ERROR "missing batch error in output: ${combined}")
endif()
]=])

  add_test(NAME help_mode COMMAND overcalc --help)
  set_tests_properties(help_mode PROPERTIES PASS_REGULAR_EXPRESSION "usage: overcalc")

  add_test(NAME version_mode COMMAND overcalc --version)
  set_tests_properties(version_mode PROPERTIES PASS_REGULAR_EXPRESSION "OverCalc [0-9]+\\.[0-9]+\\.[0-9]+")

  add_test(NAME list_symbols_mode COMMAND overcalc --no-color --list-symbols)
  set_tests_properties(list_symbols_mode PROPERTIES PASS_REGULAR_EXPRESSION "\\\\alpha.*alpha")

  add_test(NAME latex_command_suggestion COMMAND "${CMAKE_COMMAND}" -DOVERCALC_EXE=$<TARGET_FILE:overcalc> -P "${OVERCALC_SUGGESTION_CHECK}")

  add_test(NAME json_output COMMAND overcalc --json --steps "sin(pi/2)+50%")
  set_tests_properties(json_output PROPERTIES PASS_REGULAR_EXPRESSION "\"exact\": \"3/2\".*\"decimal\": 1\\.5.*\"steps\": \\[\"Start evaluation\"")

  add_test(NAME json_simplified_output COMMAND overcalc --json --steps "x+0")
  set_tests_properties(json_simplified_output PROPERTIES PASS_REGULAR_EXPRESSION "\"simplified\": \"x\".*\"simplified_latex\": \"x\".*\"Simplified: x\"")

  add_test(NAME json_timing_output COMMAND overcalc --json --timing "2+2")
  set_tests_properties(json_timing_output PROPERTIES PASS_REGULAR_EXPRESSION "\"timing_ms\": [0-9]+\\.[0-9]+")

  add_test(NAME timing_mode COMMAND overcalc --no-color --timing "2+2")
  set_tests_properties(timing_mode PROPERTIES PASS_REGULAR_EXPRESSION "timing_ms [0-9]+\\.[0-9]+")

  add_test(NAME tui_script_mode COMMAND "${CMAKE_COMMAND}" -DOVERCALC_EXE=$<TARGET_FILE:overcalc> -DTUI_INPUT_FILE=${OVERCALC_TUI_INPUT_FILE} -P "${OVERCALC_TUI_CHECK}")

  add_test(NAME tui_partial_mode COMMAND "${CMAKE_COMMAND}" -DOVERCALC_EXE=$<TARGET_FILE:overcalc> -DTUI_PARTIAL_INPUT_FILE=${OVERCALC_TUI_PARTIAL_INPUT_FILE} -P "${OVERCALC_TUI_PARTIAL_CHECK}")

  add_test(NAME tui_math_error_mode COMMAND "${CMAKE_COMMAND}" -DOVERCALC_EXE=$<TARGET_FILE:overcalc> -DTUI_MATH_ERROR_INPUT_FILE=${OVERCALC_TUI_MATH_ERROR_INPUT_FILE} -P "${OVERCALC_TUI_MATH_ERROR_CHECK}")

  add_test(NAME derivative_power COMMAND overcalc --no-color --derive x "x^2")
  set_tests_properties(derivative_power PROPERTIES PASS_REGULAR_EXPRESSION "Derivative d/dx.*Form     2 \\* x")

  add_test(NAME derivative_product COMMAND overcalc --no-color --derive x "x*x")
  set_tests_properties(derivative_product PROPERTIES PASS_REGULAR_EXPRESSION "Form     x \\+ x")

  add_test(NAME derivative_sin COMMAND overcalc --no-color --derive x "sin(x)")
  set_tests_properties(derivative_sin PROPERTIES PASS_REGULAR_EXPRESSION "Form     cos\\(x\\)")

  add_test(NAME derivative_json COMMAND overcalc --json --steps --derive x "x^2")
  set_tests_properties(derivative_json PROPERTIES PASS_REGULAR_EXPRESSION "\"derivative\": \"2 \\* x\".*\"Derivative d/dx: 2 \\* x\"")

  add_test(NAME batch_file_mode COMMAND overcalc --no-color --batch --file "${OVERCALC_BATCH_FORMULA_FILE}")
  set_tests_properties(batch_file_mode PROPERTIES PASS_REGULAR_EXPRESSION "1  2\\+2  = 4.*3  sin\\(pi/2\\)\\+50%  = 3/2 ~= 1\\.5")

  add_test(NAME batch_json_mode COMMAND overcalc --json --batch --file "${OVERCALC_BATCH_FORMULA_FILE}")
  set_tests_properties(batch_json_mode PROPERTIES PASS_REGULAR_EXPRESSION "\\[.*\"line\": 1.*\"exact\": \"4\".*\"line\": 3.*\"exact\": \"3/2\"")

  add_test(NAME batch_error_mode COMMAND "${CMAKE_COMMAND}" -DOVERCALC_EXE=$<TARGET_FILE:overcalc> -DBATCH_ERROR_FILE=${CMAKE_CURRENT_BINARY_DIR}/formula-batch-error.txt -P "${OVERCALC_BATCH_ERROR_CHECK}")

  add_test(NAME batch_derivative_mode COMMAND overcalc --no-color --batch --derive x --file "${OVERCALC_BATCH_FORMULA_FILE}")
  set_tests_properties(batch_derivative_mode PROPERTIES PASS_REGULAR_EXPRESSION "1  2\\+2  = 0.*3  sin\\(pi/2\\)\\+50%  = 0")

  add_test(NAME unquoted_expression_args COMMAND overcalc --no-color 2 + 2)
  set_tests_properties(unquoted_expression_args PROPERTIES PASS_REGULAR_EXPRESSION "Exact    4")

  add_test(NAME file_input COMMAND overcalc --no-color --file "${OVERCALC_TEST_FORMULA_FILE}")
  set_tests_properties(file_input PROPERTIES PASS_REGULAR_EXPRESSION "Exact    3/2")

  add_test(NAME eval_add COMMAND overcalc --no-color "2+2")
  set_tests_properties(eval_add PROPERTIES PASS_REGULAR_EXPRESSION "4")

  add_test(NAME eval_frac COMMAND overcalc --no-color "\\frac{1}{3}")
  set_tests_properties(eval_frac PROPERTIES PASS_REGULAR_EXPRESSION "1/3")

  add_test(NAME eval_pow COMMAND overcalc --no-color "2^5")
  set_tests_properties(eval_pow PROPERTIES PASS_REGULAR_EXPRESSION "32")

  add_test(NAME eval_sqrt COMMAND overcalc --no-color "\\sqrt{16}")
  set_tests_properties(eval_sqrt PROPERTIES PASS_REGULAR_EXPRESSION "4")

  add_test(NAME eval_plain_sqrt COMMAND overcalc --no-color "sqrt(16)")
  set_tests_properties(eval_plain_sqrt PROPERTIES PASS_REGULAR_EXPRESSION "Exact    4")

  add_test(NAME eval_indexed_root COMMAND overcalc --no-color "\\sqrt[3]{8}")
  set_tests_properties(eval_indexed_root PROPERTIES PASS_REGULAR_EXPRESSION "2")

  add_test(NAME eval_radical_expr COMMAND overcalc --no-color "\\frac{1}{\\sqrt{3}*2}")
  set_tests_properties(eval_radical_expr PROPERTIES PASS_REGULAR_EXPRESSION "0\\.288")

  add_test(NAME eval_decimal COMMAND overcalc --no-color "0.5+0.25")
  set_tests_properties(eval_decimal PROPERTIES PASS_REGULAR_EXPRESSION "Decimal  0\\.75")

  add_test(NAME eval_sin COMMAND overcalc --no-color "sin(pi/2)")
  set_tests_properties(eval_sin PROPERTIES PASS_REGULAR_EXPRESSION "Exact    1")

  add_test(NAME eval_latex_sin COMMAND overcalc --no-color "\\sin{\\pi/2}")
  set_tests_properties(eval_latex_sin PROPERTIES PASS_REGULAR_EXPRESSION "Exact    1")

  add_test(NAME eval_ln COMMAND overcalc --no-color "ln(e)")
  set_tests_properties(eval_ln PROPERTIES PASS_REGULAR_EXPRESSION "Exact    1")

  add_test(NAME eval_factorial COMMAND overcalc --no-color "5!")
  set_tests_properties(eval_factorial PROPERTIES PASS_REGULAR_EXPRESSION "Exact    120")

  add_test(NAME eval_abs COMMAND overcalc --no-color "abs(-3)")
  set_tests_properties(eval_abs PROPERTIES PASS_REGULAR_EXPRESSION "Exact    3")

  add_test(NAME eval_percent COMMAND overcalc --no-color "50%")
  set_tests_properties(eval_percent PROPERTIES PASS_REGULAR_EXPRESSION "Exact    1/2")

  add_test(NAME eval_percent_multiply COMMAND overcalc --no-color "20%*80")
  set_tests_properties(eval_percent_multiply PROPERTIES PASS_REGULAR_EXPRESSION "Exact    16")

  add_test(NAME eval_latex_cdot COMMAND overcalc --no-color "2\\cdot3")
  set_tests_properties(eval_latex_cdot PROPERTIES PASS_REGULAR_EXPRESSION "Exact    6")

  add_test(NAME eval_latex_times COMMAND overcalc --no-color "2\\times3")
  set_tests_properties(eval_latex_times PROPERTIES PASS_REGULAR_EXPRESSION "Exact    6")

  add_test(NAME eval_latex_div COMMAND overcalc --no-color "6\\div3")
  set_tests_properties(eval_latex_div PROPERTIES PASS_REGULAR_EXPRESSION "Exact    2")

  add_test(NAME eval_latex_left_right_group COMMAND overcalc --no-color "\\left(2+3\\right)^2")
  set_tests_properties(eval_latex_left_right_group PROPERTIES PASS_REGULAR_EXPRESSION "Exact    25")

  add_test(NAME eval_latex_left_right_abs COMMAND overcalc --no-color "\\left|-3\\right|")
  set_tests_properties(eval_latex_left_right_abs PROPERTIES PASS_REGULAR_EXPRESSION "Exact    3")

  add_test(NAME eval_latex_spacing_commands COMMAND overcalc --no-color "2\\,\\cdot\\quad3")
  set_tests_properties(eval_latex_spacing_commands PROPERTIES PASS_REGULAR_EXPRESSION "Exact    6")

  add_test(NAME simplify_add_zero COMMAND overcalc --no-color "x+0")
  set_tests_properties(simplify_add_zero PROPERTIES PASS_REGULAR_EXPRESSION "Simplified.*Form     x")

  add_test(NAME simplify_mul_one COMMAND overcalc --no-color "1*x")
  set_tests_properties(simplify_mul_one PROPERTIES PASS_REGULAR_EXPRESSION "Form     x")

  add_test(NAME simplify_same_subtract COMMAND overcalc --no-color "x-x")
  set_tests_properties(simplify_same_subtract PROPERTIES PASS_REGULAR_EXPRESSION "Exact    0")

  add_test(NAME simplify_zero_product_exact COMMAND overcalc --no-color "sqrt(16)+x*0")
  set_tests_properties(simplify_zero_product_exact PROPERTIES PASS_REGULAR_EXPRESSION "Exact    4")

  add_test(NAME simplify_steps COMMAND overcalc --no-color --steps "x+0")
  set_tests_properties(simplify_steps PROPERTIES PASS_REGULAR_EXPRESSION "Simplified: x")

  add_test(NAME eval_exact_sin_identity COMMAND overcalc --no-color "sin(pi/2)")
  set_tests_properties(eval_exact_sin_identity PROPERTIES PASS_REGULAR_EXPRESSION "Exact    1")

  add_test(NAME eval_exact_cos_identity COMMAND overcalc --no-color "cos(pi)")
  set_tests_properties(eval_exact_cos_identity PROPERTIES PASS_REGULAR_EXPRESSION "Exact    -1")

  add_test(NAME ascii_mode COMMAND overcalc --no-color --ascii "\\frac{1}{3}")
  set_tests_properties(ascii_mode PROPERTIES PASS_REGULAR_EXPRESSION "OverCalc / Input")

  add_test(NAME color_mode COMMAND overcalc "2+2")
  set_tests_properties(color_mode PROPERTIES PASS_REGULAR_EXPRESSION "\\[36m")

  add_test(NAME syntax_highlight_formula COMMAND overcalc "2+sin(pi)")
  set_tests_properties(syntax_highlight_formula PROPERTIES PASS_REGULAR_EXPRESSION "\\[32;1m2.*\\[33;1m\\+.*\\[36;1msin.*\\[35;1mpi")

  add_test(NAME width_mode COMMAND overcalc --no-color --width 52 "2+2")
  set_tests_properties(width_mode PROPERTIES PASS_REGULAR_EXPRESSION "OverCalc / Input")

  add_test(NAME ascii_subscript COMMAND overcalc --no-color --ascii "\\alpha_1")
  set_tests_properties(ascii_subscript PROPERTIES PASS_REGULAR_EXPRESSION "alpha_1")

  add_test(NAME latex_output_mode COMMAND overcalc --latex-output "\\frac{1}{2}")
  set_tests_properties(latex_output_mode PROPERTIES PASS_REGULAR_EXPRESSION "\\\\frac\\{1\\}\\{2\\} = 1/2")

  add_test(NAME latex_precedence COMMAND overcalc --latex-output "1/(2+3)")
  set_tests_properties(latex_precedence PROPERTIES PASS_REGULAR_EXPRESSION "1/\\(2\\+3\\) = 1/5")

  add_test(NAME latex_power_base_precedence COMMAND overcalc --latex-output "((2 + 3)^2 - 9) / 4")
  set_tests_properties(latex_power_base_precedence PROPERTIES PASS_REGULAR_EXPRESSION "\\(\\(2\\+3\\)\\^\\{2\\}-9\\)/4 = 4")

  add_test(NAME latex_indexed_root COMMAND overcalc --latex-output "\\sqrt[3]{8}")
  set_tests_properties(latex_indexed_root PROPERTIES PASS_REGULAR_EXPRESSION "\\\\sqrt\\[3\\]\\{8\\} = 2")

  add_test(NAME latex_multiply_operator COMMAND overcalc --latex-output "2*3")
  set_tests_properties(latex_multiply_operator PROPERTIES PASS_REGULAR_EXPRESSION "2\\\\cdot3 = 6")

  add_test(NAME render_precedence_ascii COMMAND overcalc --no-color --ascii "(1+2)*3")
  set_tests_properties(render_precedence_ascii PROPERTIES PASS_REGULAR_EXPRESSION "\\(1 \\+ 2\\) \\* 3")

  add_test(NAME render_power_base_precedence_ascii COMMAND overcalc --no-color --steps --ascii "((2 + 3)^2 - 9) / 4")
  set_tests_properties(render_power_base_precedence_ascii PROPERTIES PASS_REGULAR_EXPRESSION "---------------")

  add_test(NAME render_power_base_precedence_unicode COMMAND overcalc --no-color --steps "((2 + 3)^2 - 9) / 4")
  set_tests_properties(render_power_base_precedence_unicode PROPERTIES PASS_REGULAR_EXPRESSION "──────────────")

  add_test(NAME render_sum_unicode COMMAND overcalc --no-color "\\sum_i^3")
  set_tests_properties(render_sum_unicode PROPERTIES PASS_REGULAR_EXPRESSION " > ")

  add_test(NAME render_product_unicode COMMAND overcalc --no-color "\\prod_i^n")
  set_tests_properties(render_product_unicode PROPERTIES PASS_REGULAR_EXPRESSION "┬ ┬")

  add_test(NAME render_integral_unicode COMMAND overcalc --no-color "\\int_0^1")
  set_tests_properties(render_integral_unicode PROPERTIES PASS_REGULAR_EXPRESSION "⌠")

  add_test(NAME render_sqrt_overbar_unicode COMMAND overcalc --no-color "\\sqrt{1+2}")
  set_tests_properties(render_sqrt_overbar_unicode PROPERTIES PASS_REGULAR_EXPRESSION "───")

  add_test(NAME render_tall_paren_unicode COMMAND overcalc --no-color "(\\frac{1}{2})^2")
  set_tests_properties(render_tall_paren_unicode PROPERTIES PASS_REGULAR_EXPRESSION "⎛")

  add_test(NAME ast_json_mode COMMAND overcalc --ast-json "x^2")
  set_tests_properties(ast_json_mode PROPERTIES PASS_REGULAR_EXPRESSION "\"Superscript\"")

  add_test(NAME function_ast_json COMMAND overcalc --ast-json "sin(pi/2)")
  set_tests_properties(function_ast_json PROPERTIES PASS_REGULAR_EXPRESSION "\"FunctionCall\"")

  add_test(NAME decimal_ast_json COMMAND overcalc --ast-json "0.25")
  set_tests_properties(decimal_ast_json PROPERTIES PASS_REGULAR_EXPRESSION "\"Decimal\"")

  add_test(NAME factorial_ast_json COMMAND overcalc --ast-json "5!")
  set_tests_properties(factorial_ast_json PROPERTIES PASS_REGULAR_EXPRESSION "\"FunctionCall\".*\"fact\"")

  add_test(NAME percent_ast_json COMMAND overcalc --ast-json "50%")
  set_tests_properties(percent_ast_json PROPERTIES PASS_REGULAR_EXPRESSION "\"FunctionCall\".*\"percent\"")

  add_test(NAME subscript_ast COMMAND overcalc --ast-json "x_0")
  set_tests_properties(subscript_ast PROPERTIES PASS_REGULAR_EXPRESSION "\"Subscript\"")

  add_test(NAME latex_greek_ast COMMAND overcalc --ast-json "\\alpha_1")
  set_tests_properties(latex_greek_ast PROPERTIES PASS_REGULAR_EXPRESSION "\"Subscript\".*\"alpha\"")

  add_test(NAME indexed_root_ast COMMAND overcalc --ast-json "\\sqrt[3]{x}")
  set_tests_properties(indexed_root_ast PROPERTIES PASS_REGULAR_EXPRESSION "\"Sqrt\".*\"index\"")

  add_test(NAME latex_pi_eval COMMAND overcalc --no-color "\\pi+1")
  set_tests_properties(latex_pi_eval PROPERTIES PASS_REGULAR_EXPRESSION "4\\.14")

  add_test(NAME steps_mode COMMAND overcalc --no-color --steps "pi+1")
  set_tests_properties(steps_mode PROPERTIES PASS_REGULAR_EXPRESSION "Approx decimal")

  add_test(NAME implicit_mul COMMAND overcalc --no-color "2pi")
  set_tests_properties(implicit_mul PROPERTIES PASS_REGULAR_EXPRESSION "6\\.28")

  add_test(NAME unary_minus COMMAND overcalc --no-color "-3^2")
  set_tests_properties(unary_minus PROPERTIES PASS_REGULAR_EXPRESSION "9")
endif()

if(BUILD_TESTING)
  add_executable(overcalc_editor_test tests/editor_test.cpp)
  target_link_libraries(overcalc_editor_test PRIVATE overcalc_core)
  add_test(NAME structured_editor COMMAND overcalc_editor_test)
endif()
install(TARGETS overcalc RUNTIME DESTINATION bin)
if(BUILD_TESTING)
  add_test(NAME backend_contract COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:overcalc> -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend_test.cmake")
endif()
