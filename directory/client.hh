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

class DSMClient {
    public:
        DSMClient(std::shared_ptr<Channel> channel)
	    : stub_(Client::NewStub(channel)) {}

	int pageRead(const uint64_t addr, const uint64_t pageSize = 4096) {
		PageRequest request;
		request.set_pageaddr(addr);
		request.set_pagesize(pageSize);
		request.set_pageoperation(0);

		PageReply reply;

		ClientContext context;

		Status status = stub_->pageRead(&context, request, &reply);

                // Act upon its status.
                if (status.ok()) {
                  return 0;
                } else {
                  std::cout << status.error_code() << ": " << status.error_message()
                            << std::endl;
                  return -1;
                }
	}

	int pageWrite(const uint64_t addr, const uint64_t pageSize = 4096) {
		PageRequest request;
		request.set_pageaddr(addr);
		request.set_pagesize(pageSize);
		request.set_pageoperation(1);

		PageReply reply;

		ClientContext context;

		Status status = stub_->pageWrite(&context, request, &reply);

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
	std::unique_ptr<Client::Stub> stub_;
};


class MasterImpl final : public Master::Service {

    Status fwdPageRequest(ServerContext* context, const PageRequest* request, PageReply *reply) override {
        reply->set_ack(true);
	return Status::OK;
    }

    Status invPage(ServerContext* context, const PageRequest* request, PageReply *reply) override {
        reply->set_ack(true);
	return Status::OK;
    }
};
