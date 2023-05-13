#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <mutex>

#include <grpcpp/grpcpp.h>

#include "directory.grpc.pb.h"
#include"table.h"
#define DEBUG 0
#define DEBUG_DATA 0
#define TOTAL_LOCKS 100

using grpc::Channel;
using grpc::ClientContext;
using grpc::ServerContext;
using grpc::Status;
using directory::Client;
using directory::Master;
using directory::PageRequest;
using directory::PageReply;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ClientReader;
using grpc::ClientWriter;


class DSMMaster {
    public:
        //DSMMaster(uint32_t numClients)
	    //{
        //    stub_.resize(numClients);
        //}
        DSMMaster(std::shared_ptr<Channel> channel)
	    : stub_(Master::NewStub(channel)) {}

    //void initChannel(std::shared_ptr<Channel> channel, uint32_t clientNum);
    uint32_t getClientID(uint64_t addr);

	char* fwdPageRequest(const uint32_t clientID, const uint64_t addr, const uint32_t operation, const uint32_t pageSize);

	char* invPage(const uint32_t clientID, const uint64_t addr, const uint32_t pageSize, bool sendPage);

    Status sendPage(ClientContext* context, PageRequest* request, PageReply* reply, uint32_t clientNum);

    private:
	//std::vector<std::unique_ptr<Master::Stub>> stub_;
	std::unique_ptr<Master::Stub> stub_;

};


class ClientImpl final : public Client::Service {

    public:
        ClientImpl(uint32_t p_masterID, uint64_t p_startAddress,  uint32_t p_numPages, uint32_t p_numClients) :
            masterID(p_masterID),
            startAddress(p_startAddress),
            numPages(p_numPages),
            numClients(p_numClients) {

            std::map<int, std::string> clientIPs;
            clientIPs[0] = "10.10.1.1:50051";
            clientIPs[1] = "10.10.1.2:50051";
            clientIPs[2] = "10.10.1.3:50051";
            for (int i = 0; i < numClients; i++){
                 clients.push_back(std::move(DSMMaster(grpc::CreateChannel(clientIPs[i], grpc::InsecureChannelCredentials()))));
            }
            pageTable = new PageTable();
        }

        ~ClientImpl() {
            delete pageTable;
        }

        PageTable *pageTable;
        int getClientID(uint64_t addr);
        Status getPage(ServerContext* context, const PageRequest* request, ServerWriter<PageReply>* writer) override;
        Status returnPage(ServerContext* context, const PageRequest* request, PageReply *reply, ServerWriter<PageReply>* writer, char* pageData);

    private:
        uint64_t startAddress;
        uint32_t numPages;
        uint32_t pageSize = 4096;
        uint32_t numClients;
        ///FIXME the master to client address are not setup, not sure what is stub resizing
        std::vector<DSMMaster> clients;
        uint32_t masterID;
        std::mutex dsmLock;//[TOTAL_LOCKS];

};
