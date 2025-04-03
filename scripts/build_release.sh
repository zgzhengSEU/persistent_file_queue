conan install . --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
find . -name 'compile_commands.json' -exec cp {} ./build/ \;