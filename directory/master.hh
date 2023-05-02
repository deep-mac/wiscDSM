#pragma once

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "directory.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ServerContext;
using grpc::Status;
using directory::Client;
using directory::Master;
using directory::PageRequest;
using directory::PageReply;

class DSMMaster {
    public:
        DSMMaster(std::shared_ptr<Channel> channel)
	    : stub_(Master::NewStub(channel)) {}

	int fwdPageRequest(const uint64_t addr, const uint32_t operation, const uint64_t pageSize = 4096) {
		PageRequest request;
		request.set_pageaddr(addr);
		request.set_pagesize(pageSize);
		request.set_pageoperation(operation);

		PageReply reply;

		ClientContext context;

		Status status = stub_->fwdPageRequest(&context, request, &reply);

                // Act upon its status.
                if (status.ok()) {
                  return 0;
                } else {
                  std::cout << status.error_code() << ": " << status.error_message()
                            << std::endl;
                  return -1;
                }
	}

	int invPage(const uint64_t addr, const uint64_t pageSize = 4096) {
		PageRequest request;
		request.set_pageaddr(addr);
		request.set_pagesize(pageSize);
		request.set_pageoperation(2);

		PageReply reply;

		ClientContext context;

		Status status = stub_->invPage(&context, request, &reply);

                // Act upon its status.
                if (status.ok()) {
                  return 0;
                } else {
                  std::cout << status.error_code() << ": " << status.error_message()
                            << std::endl;
                  return -1;
                }
	}

    private:
	std::unique_ptr<Master::Stub> stub_;
};


class ClientImpl final : public Client::Service {

    Status pageRead(ServerContext* context, const PageRequest* request, PageReply *reply) override {
        reply->set_ack(true);
	return Status::OK;
    }

    Status pageWrite(ServerContext* context, const PageRequest* request, PageReply *reply) override {
        reply->set_ack(true);
	return Status::OK;
    }
};
