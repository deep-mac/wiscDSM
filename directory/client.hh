#pragma once

#include <iostream>
#include <memory>
#include <string>
#include<signal.h>
#include<sys/mman.h>
#include<thread>
#include<unistd.h>

#include <grpcpp/grpcpp.h>

#include "directory.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#define DEBUG 1
#define DEBUG_DATA 0

using grpc::Server;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;
using grpc::ServerBuilder;
using directory::Client;
using directory::Master;
using directory::PageRequest;
using directory::PageReply;

struct sigaction sa;
int pageSize = 4096;
struct sigaction *sa_default;
enum OP {OP_READ, OP_WRITE};
int totalPages = 9;
int totalMasters = 3;
int clientID = 0;
uint64_t sharedAddrStart = (1 << 30);

void faultHandler(int sig, siginfo_t *info, void *ucontext);
int initShmem(uint64_t startAddress, int numPages);
void RunClient(std::string port);

class DSMClient {
    public:
        DSMClient(std::shared_ptr<Channel> channel)
	    : stub_(Client::NewStub(channel)) {}

	PageReply getPage(const uint64_t addr, const uint32_t operation);

    private:
	std::unique_ptr<Client::Stub> stub_;

    public:
    static uint32_t numSegFaults;

    static std::mutex dsmLock;
};


class MasterImpl final : public Master::Service {

    Status fwdPageRequest(ServerContext* context, const PageRequest* request, ServerWriter<PageReply> *writer) override;

    Status invPage(ServerContext* context, const PageRequest* request, ServerWriter<PageReply> *writer) override;

    public:
    static uint32_t numPageInvalidationsAcked;
    static uint32_t numPageInvalidationsNacked;
    static uint32_t numFwdReqsAcked;
    static uint32_t numFwdReqsNacked;
};

std::vector<DSMClient> masters;
