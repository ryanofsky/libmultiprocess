NIX_ARGS=(
  --arg libcxx true
)
CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_CXX_COMPILER=clang++
  -DCMAKE_CXX_STANDARD_LIBRARY=libc++
  -DCMAKE_CXX_FLAGS="-Werror -Wall -Wextra -Wpedantic -Wthread-safety-analysis -Wno-unused-parameter"
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  -DMP_ENABLE_CLANG_TIDY=ON
  -DMP_ENABLE_IWYU=ON
)
