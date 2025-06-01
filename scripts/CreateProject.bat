cd ..
cmake  -B build -S .    -DCMAKE_build_TYPE=Debug -G "Visual Studio 17 2022" -A x64 -DQJS_DEBUGGER_SUPPORT=ON
cmake  -B dummy_build -S .   -DCMAKE_EXPORT_COMPILE_COMMANDS=on -DCMAKE_build_TYPE=Debug -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DQJS_DEBUGGER_SUPPORT=ON
cp dummy_build/compile_commands.json .
cd scripts
pause