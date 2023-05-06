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
    std::string masterPort = "10.10.1.1:2048";
    std::thread masterThread (RunMaster, 1, (uint64_t)(1 << 30), 9, 3, masterPort);
    masterThread.join();
    return 0;
}
