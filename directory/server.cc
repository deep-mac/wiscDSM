#include "master.hh"
//#include "client.hh"
#include <thread>
#include <iostream>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

using grpc::Server;
using grpc::ServerBuilder;

void RunMaster(uint32_t masterID, uint64_t startAddress, uint32_t numPages, uint32_t numClients, std::string port) {
  ClientImpl service(masterID, startAddress, numPages, numClients);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(port, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> master(builder.BuildAndStart());
  std::cout << "Client listening on " << port << std::endl;

  // Wait for the master to shutdown. Note that some other thread must be
  // responsible for shutting down the master for this call to ever return.
  master->Wait();
}

int main(int argc, char** argv) {
    std::map<int, std::string> masterPort;
    if (argc == 2) {
        masterPort[0] = "10.10.1.1:2048";
        masterPort[1] = "10.10.1.1:2049";
        masterPort[2] = "10.10.1.1:2050";
    } else {
        masterPort[0] = "10.10.1.1:2048";
        masterPort[1] = "10.10.1.2:2048";
        masterPort[2] = "10.10.1.3:2048";
    }
    std::thread masterThread (RunMaster, 1, (uint64_t)(1 << 30), 9, 3, masterPort[atoi(argv[1])]);
    masterThread.join();
    return 0;
}
