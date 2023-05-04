#pragma once

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "directory.grpc.pb.h"
#include"table.h"

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
        DSMMaster(uint32_t numClients)
	    {
            stub_.resize(numClients);
            pageTable = new PageTable();
        }

    void initChannel(std::shared_ptr<Channel> channel, uint32_t clientNum);
    uint32_t getClientID(uint64_t addr);

	char* fwdPageRequest(const uint32_t clientID, const uint64_t addr, const uint32_t operation, const uint32_t pageSize);

	char* invPage(const uint32_t clientID, const uint64_t addr, const uint32_t pageSize);

    Status sendPage(ClientContext* context, PageRequest* request, PageReply* reply, uint32_t clientNum);
    PageTable *pageTable;

    private:
	std::vector<std::unique_ptr<Master::Stub>> stub_;

};


class ClientImpl final : public Client::Service {

    public:
        ClientImpl(uint32_t p_clientID, uint64_t p_startAddress, uint64_t p_endAddress, uint32_t p_pageSize, uint32_t p_numClients) :
            clientID(p_clientID),
            startAddress(p_startAddress),
            pageSize(p_pageSize),
            numClients(p_numClients) {
                numPages = (p_endAddress - p_startAddress)/p_pageSize;
                master = new DSMMaster(p_numClients);
            }

        ~ClientImpl() {
            delete master;
        }

        int getClientID(uint64_t addr);
        Status getPage(ServerContext* context, const PageRequest* request, ServerWriter<PageReply>* writer) override;
        Status returnPage(ServerContext* context, const PageRequest* request, PageReply *reply, ServerWriter<PageReply>* writer, char* pageData);

    private:
        uint64_t startAddress;
        uint32_t numPages;
        uint32_t pageSize;
        uint32_t numClients;
        DSMMaster* master;
        uint32_t clientID;

    /*Status pageWrite(ServerContext* context, const PageRequest* request, PageReply *reply) override;
        reply->set_ack(true);
	return Status::OK;
    }*/
};
