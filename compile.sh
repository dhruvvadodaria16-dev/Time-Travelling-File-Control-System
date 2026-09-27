g++ -std=c++17 -O2 -Wall -o filesystem sol.cpp

if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    echo "Run './filesystem' to start the program."
else
    echo "Compilation failed."
fi
