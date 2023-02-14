#!/bin/bash -eux

echo "Compiling..."


DIR=$1
cd $DIR

# Syntax highlight source
highlight -O html --inline-css -f -o submitted_code.highlight.html submitted_code.hpp


# Compile
FLAGS="$(cat flags.txt)"
BINARY="benchmark"
g++ -g benchmark.cpp -o $BINARY -fdiagnostics-color=always $FLAGS > compile_stdout.log.ansi 2> compile_stderr.log.ansi
COMPILE_RESULT=$?
cat compile_stdout.log.ansi | aha --no-header > compile_stdout.log.html
cat compile_stderr.log.ansi | aha --no-header > compile_stderr.log.html

if [ $COMPILE_RESULT != 0 ]; then
  echo "Compile failed."
  echo "1" > exit_code
  exit 1
fi

# Get disassembly from function
OBJDUMP=$HOME/w/3rd/binutils/binutils/objdump
SYMBOL_NAME="student_atan"
SYMBOL=$($OBJDUMP -t $BINARY | grep ' g  ' | grep $SYMBOL_NAME | cut -d\  -f 6)
echo "Found symbol: $SYMBOL"
SYMBOL="_Z12student_atanff"
$OBJDUMP $BINARY --disassembler-color=extended-color --visualize-jumps=extended-color --disassemble=$SYMBOL --no-addresses --no-show-raw-insn > disassembly.ansi
$OBJDUMP $BINARY --disassembler-color=extended-color --visualize-jumps=extended-color --disassemble=$SYMBOL --no-addresses --no-show-raw-insn -S > disassembly_with_source.ansi
cat disassembly.ansi | aha --no-header > disassembly.html
cat disassembly_with_source.ansi | aha --no-header > disassembly_with_source.html

echo "Running..."

timeout 8 ./benchmark > best_time.txt 2> benchmark_output
BENCHMARK_RESULT=$?

if [ $BENCHMARK_RESULT == 124 ]; then
  echo "Timeout"
  echo "4" > exit_code
  exit 4
elif [ $BENCHMARK_RESULT != 0 ]; then
  echo "Benchmark unhappy (status $BENCHMARK_RESULT)"
  echo "2" > exit_code
  exit 2
fi

echo "Compile.sh completed succesfully"
echo "0" > exit_code
exit 0
