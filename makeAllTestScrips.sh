#!bin/bash

# Remove Old Executables
echo "Removing Old Executables"
rm bin_test/*

# Compiling OSPRE
echo "Compiling OSPRE"
make -j --file=makeTestScripts bin_test/testBuilder
make -j --file=makeTestScripts bin_test/testByteBuffer
make -j --file=makeTestScripts bin_test/testCameraController
make -j --file=makeTestScripts bin_test/testCircularBuffer
make -j --file=makeTestScripts bin_test/testExternalBuilder
make -j --file=makeTestScripts bin_test/testExternalParser
make -j --file=makeTestScripts bin_test/testGNC
make -j --file=makeTestScripts bin_test/testImageProcessor
make -j --file=makeTestScripts bin_test/testParser
make -j --file=makeTestScripts bin_test/testScComms
make -j --file=makeTestScripts bin_test/testSpacecraft
make -j --file=makeTestScripts bin_test/testWatchDog

# Remvoing .o files
echo "Cleaning"
make --file=makeTestScripts clean
