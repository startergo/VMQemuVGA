#!/bin/bash
# Compile test programs natively on Snow Leopard

echo "🔨 Compiling test programs on Snow Leopard..."

# Compile test_selectors_simple
echo "Compiling test_selectors_simple..."
gcc -arch x86_64 -framework IOKit -framework CoreFoundation -o test_selectors_simple test_selectors_simple.c

if [ $? -eq 0 ]; then
    echo "✅ test_selectors_simple compiled successfully"
else
    echo "❌ Failed to compile test_selectors_simple"
    exit 1
fi

# Compile test_minimal
echo "Compiling test_minimal..."
gcc -arch x86_64 -framework IOKit -framework CoreFoundation -o test_minimal test_minimal.c

if [ $? -eq 0 ]; then
    echo "✅ test_minimal compiled successfully"
else
    echo "❌ Failed to compile test_minimal"
    exit 1
fi

echo "🎉 All tests compiled successfully on Snow Leopard"
echo "Run with: ./test_selectors_simple or ./test_minimal"
