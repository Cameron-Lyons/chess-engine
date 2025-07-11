#!/bin/bash

echo "Testing chess engine..."

# Test with a simple position
echo "Testing with starting position..."
echo -e "position startpos\ngo depth 3" | timeout 30s ./bin/chess_engine

echo "Test completed." 