#!bin/sh

rm -rf bin
mkdir bin
cd bin

proj_name=App
proj_root_dir=$(pwd)/../

flags=(
	-std=gnu99 -w -pthread
)

# Include directories
inc=(
	-I ../third_party/include/			# Gunslinger includes
)

# Source files
src=(
	../source/main.c 
    ../third_party/include/flecs/flecs.c
)

libs=(
	-lopengl32
	-lkernel32 
	-luser32 
	-lshell32 
	-lgdi32 
    -lWinmm
	-lAdvapi32
    -lws2_32
)

# Build
gcc -O0 ${inc[*]} ${src[*]} ${flags[*]} ${libs[*]} -lm -o ${proj_name}

cd ..



