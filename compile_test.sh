#!/bin/bash
# Compile the virgl test on Snow Leopard
gcc -o test_virgl_clear test_virgl_clear.c -framework OpenGL -framework GLUT
echo "Compiled test_virgl_clear"
