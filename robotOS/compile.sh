#!/bin/bash

cd /home/pi/robotOS/mainProgram
g++ main.cpp -o main `pkg-config --cflags --libs opencv4`

echo ""
echo "Press any key..."
read -rsn1
