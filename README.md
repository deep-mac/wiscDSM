# wiscDSM


To generate tests (requirements - numpy)
cd tests/
python testGenerator.py <num variables> <num accesses> <ratio of reads and writes (0-1)> 1
This generates a file in tests/ - test_<num_variables>_<num_accesses>_<ratio>_stride.h
cd ../directory
Include this file in client.cc and invoke the function in main

To build (requirements - gRPC installed) :
mkdir -p cmake/build
cd cmake/build
cmake ../..
make
./server <nodeID> 1 (To start the server on nodeID)
./client <nodeID> 1 (To launch the client and run the generated test)
