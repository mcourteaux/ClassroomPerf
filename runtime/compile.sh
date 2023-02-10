#!/bin/bash -eux

echo "Compiling..."

DIR=$1
cd $DIR
FLAGS="$(cat flags.txt)"
g++ benchmark.cpp -o benchmark $FLAGS > compile_stdout.log 2> compile_stderr.log

if [ $? != 0 ]; then
  echo "Compile failed"
  exit 1
fi

echo "Running..."

./benchmark > best_time.txt
BENCHMARK_RESULT=$?

if [ $BENCHMARK_RESULT != 0 ]; then
  echo "Benchmark unhappy (status $BENCHMARK_RESULT)"
  exit 2
fi
