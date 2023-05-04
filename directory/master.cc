#include "master.hh"

void DSMMaster::initChannel(std::shared_ptr<Channel> channel, uint32_t clientNum) {
    stub_[clientNum] = Master::NewStub(channel);
    return;
}

uint32_t DSMMaster::getClientID(uint64_t addr) {
//TODO: Addr->Client Hash Function
    return 0;
}

char* DSMMaster::fwdPageRequest(const uint32_t clientID, const uint64_t addr, const uint32_t operation, const uint32_t pageSize) {
	PageRequest request;
	request.set_pageaddr(addr);
	request.set_pageoperation(operation);

	PageReply reply;

	ClientContext context;

    uint32_t bytesReceived = 0;

    char* page = new char[pageSize/sizeof(char)];

    std::unique_ptr<ClientReader<PageReply> > reader(stub_[clientID]->fwdPageRequest(&context, request));
    while (reader->Read(&reply)) {
        if(reply.ack()){
            memcpy(page+bytesReceived, reply.pagedata().c_str(), reply.size());
            bytesReceived += reply.size(); 
        }
    }
    Status status = reader->Finish();
    // Act upon its status.
    if (!status.ok()) {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
    }
    return page;
}

char* DSMMaster::invPage(const uint32_t clientID, const uint64_t addr, const uint32_t pageSize) {
	PageRequest request;
	request.set_pageaddr(addr);
	request.set_pageoperation(2);

	PageReply reply;

	ClientContext context;
    
    uint32_t bytesReceived = 0;

    char* page = new char[pageSize/sizeof(char)];

    std::unique_ptr<ClientReader<PageReply> > reader(stub_[clientID]->invPage(&context, request));
    while (reader->Read(&reply)) {
        if(reply.ack()){
            memcpy(page+bytesReceived, reply.pagedata().c_str(), reply.size());
            bytesReceived += reply.size(); 
        }
    }
    Status status = reader->Finish();

    // Act upon its status.
    if (!status.ok()) {
      std::cout << status.error_code() << ": " << status.error_message()
               << std::endl;
    }
    return page;
}



Status ClientImpl::getPage(ServerContext* context, const PageRequest* request, ServerWriter<PageReply>* writer) {

    uint64_t pageAddr = request->pageaddr();
    uint32_t operation = request->pageoperation();

    assert (operation != 2);

    uint32_t targetClientID = master->getClientID(pageAddr);
    bool didFwd = false;

    if (targetClientID != clientID) {
        // Forward Request
        master->fwdPageRequest(clientID, pageAddr, operation, pageSize);
        return Status::OK;
    }

    PageReply reply;
    reply.set_ack(true);

    PageState *pageState = master->pageTable->getState(master->pageTable->get(request->pageaddr()));

    char* page;
    if (pageState->st == INV) {
        //Page is invalid. Send page and set state
        if (operation == 0) {
            pageState->st = SHARED;
        } else {
            pageState->st = MODIFIED;
        }
        pageState->clientList.push_back(clientID);
    } else if (pageState->st == SHARED) {
        // Page is shared. If read, share to new client, else invalidate and share
        if (operation == 0) {
           pageState->clientList.push_back(clientID);
        } else if (operation == 1) {
           pageState->st = MODIFIED;
           for (int i = 0; i < pageState->clientList.size(); i++) {
           // TODO: Make asynchronous
              page = master->invPage(pageState->clientList[i], request->pageaddr(), pageSize);
              didFwd = true;
           }
           pageState->clientList.clear();
           pageState->clientList.push_back(request->clientid());
        }
    } else {
    // Page is in modified. Invalidate and share
       if (operation == 0) {
           pageState->st = SHARED;
           for (int i = 0; i < pageState->clientList.size(); i++) {
           // TODO: Make asynchronous
              page = master->fwdPageRequest(pageState->clientList[i], request->pageaddr(), request->pageoperation(), pageSize);
              didFwd = true;
           }
           pageState->clientList.clear();
           pageState->clientList.push_back(request->clientid());
       } else {
           for (int i = 0; i < pageState->clientList.size(); i++) {
           // TODO: Make asynchronous
              page = master->invPage(pageState->clientList[i], request->pageaddr(), pageSize);
              didFwd = true;
           }
           pageState->clientList.clear();
           pageState->clientList.push_back(request->clientid());
       }
    }

    if (!didFwd) {
       memcpy(page, reinterpret_cast<void *> (request->pageaddr()), pageSize);
    }

    // TODO: Send Page
    int bytesSent = 0;
    while(bytesSent < pageSize) {
        // TODO: Access 1024 Bytes of address
        bytesSent += 1024;
    }

    master->pageTable->put(request->pageaddr(), master->pageTable->formValue(pageState));
    //this may need to initiate RPC calls to other client to invalidate or fetch stuff
    Status status = returnPage(context, request, &reply, writer, page);
    return status;
}


Status ClientImpl::returnPage(ServerContext* context, const PageRequest* request, PageReply *reply, ServerWriter<PageReply>* writer, char* pageData) {
    //TODO: Return page in streams
    uint32_t bytesSent = 0;
    uint32_t writeSize = 2048;
    char page[pageSize];

    while(bytesSent < pageSize) {
        memcpy(page, pageData + bytesSent, writeSize);
        reply->set_pagedata(std::string(page, writeSize));
        reply->set_ack(true);
        bytesSent += writeSize;
        if (!writer->Write(*reply)) {
            printf("ERROR: Failed to send page to client\n");
        }
        //writer->WritesDone();
    }

    return Status::OK;
}
