#include "master.hh"

uint32_t DSMMaster::getClientID(uint64_t addr) {
//TODO: Addr->Client Hash Function
    return 0;
}

char* DSMMaster::fwdPageRequest(const uint32_t clientID, const uint64_t addr, const uint32_t operation, const uint32_t pageSize) {
	PageRequest request;
	request.set_pageaddr(addr);
    if (DEBUG){
        printf("fwdPageRequest:: Sending request to client = %d, addr = %ld\n", clientID, addr);
    }

	PageReply reply;

	ClientContext context;

    uint32_t bytesReceived = 0;

    char* page = new char[pageSize/sizeof(char)];

    std::unique_ptr<ClientReader<PageReply> > reader(stub_->fwdPageRequest(&context, request));
    while (reader->Read(&reply)) {
        if(reply.ack()){
            if(DEBUG_DATA){
                std::cout << "fwdPage:: Received data = " << reply.pagedata() << std::endl;
            }
            memcpy(page+bytesReceived, reply.pagedata().data(), reply.size());
            bytesReceived += reply.size(); 
        }
    }
    Status status = reader->Finish();
    // Act upon its status.
    if (!status.ok()) {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
    }
    if(DEBUG_DATA){
        std::cout << "fwdPage:: Final Received page = " << std::string(page, pageSize) << std::endl;
    }
    return page;
}

char* DSMMaster::invPage(const uint32_t clientID, const uint64_t addr, const uint32_t pageSize) {
	PageRequest request;
	request.set_pageaddr(addr);
    if (DEBUG){
        printf("invPage:: Sending request to client = %d, addr = %ld\n", clientID, addr);
    }
	PageReply reply;

	ClientContext context;
    
    uint32_t bytesReceived = 0;

    char* page = new char[pageSize/sizeof(char)];

    std::unique_ptr<ClientReader<PageReply> > reader(stub_->invPage(&context, request));
    while (reader->Read(&reply)) {
        if(reply.ack()){
            if (DEBUG_DATA){
                std::cout << "invPage:: Received data = " << reply.pagedata() << std::endl;
            }
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
    uint32_t clientID = request->clientid();

    assert (operation != 2);

    //uint32_t targetClientID = master->getClientID(pageAddr);
    if (DEBUG){
        printf("getPage:: Received pageAddr = %ld, received operation = %d, received clientID = %d\n", pageAddr, operation, clientID);
    }
    bool isData = true;

    /*if (targetClientID != clientID) {
        // Forward Request
        master->fwdPageRequest(clientID, pageAddr, operation, pageSize);
        return Status::OK;
    }*/

    PageReply reply;
    reply.set_ack(true);

    PageState *pageState = pageTable->getState(pageTable->get(request->pageaddr()));

    if(DEBUG){
        printf("---------------------------------\ngetPage:: Initial value for state\n");
        pageState->printState();
        pageTable->printTable();
        printf("getPage:: formed value = %u\n-------------------------------\n", pageTable->formValue(pageState));
    }

    char* page;
    if (pageState->st == INV){
        if (operation == 0) {
            pageState->st = SHARED;
        } 
        else {
            pageState->st = MODIFIED;
        }
        pageState->clientList.push_back(clientID);
        reply.set_size(0);
        isData = false;
    } 
    else if (pageState->st == SHARED) {
        // Page is shared. If read, share to new client, else invalidate and share
        if (operation == 0) {
            pageState->clientList.push_back(clientID);
        } 
        else if (operation == 1) {
            pageState->st = MODIFIED;
            for (int i = 0; i < pageState->clientList.size(); i++) {
                // TODO: Make asynchronous
                page = clients[pageState->clientList[i]].invPage(pageState->clientList[i], request->pageaddr(), pageSize);
            }
            pageState->clientList.clear();
            pageState->clientList.push_back(request->clientid());
        }
    } 
    else {
        // Page is in modified. Invalidate and share
        if (operation == 0) {
            pageState->st = SHARED;
            for (int i = 0; i < pageState->clientList.size(); i++) {
                // TODO: Make asynchronous
                page = clients[pageState->clientList[i]].fwdPageRequest(pageState->clientList[i], request->pageaddr(), request->pageoperation(), pageSize);
            }
            pageState->clientList.clear();
            pageState->clientList.push_back(request->clientid());
        } 
        else {
            for (int i = 0; i < pageState->clientList.size(); i++) {
                // TODO: Make asynchronous
                page = clients[pageState->clientList[i]].invPage(pageState->clientList[i], request->pageaddr(), pageSize);
            }
            pageState->clientList.clear();
            pageState->clientList.push_back(request->clientid());
        }
    }
    pageTable->put(request->pageaddr(), pageTable->formValue(pageState));
    if(DEBUG){
        printf("----------------------------\ngetPage:: updating value for state\n");
        pageState->printState();
        pageTable->printTable();
        printf("getPage:: formed value = %u\n--------------------------------\n", pageTable->formValue(pageState));
        if (DEBUG_DATA && isData){
            std::cout << "getPage:: Received page data from client = " << std::string(page, pageSize) << std::endl;
        }
    }
    // TODO: Send Page
    int bytesSent = 0;
    uint32_t writeSize = 2048;
    if (isData){
        while(bytesSent < pageSize) {
            // TODO: Access 1024 Bytes of address
            char pageChunk[writeSize];
            memcpy(pageChunk, page+ bytesSent, writeSize);
            reply.set_pagedata(std::string(pageChunk, writeSize));
            if (DEBUG_DATA){
                std::cout << "getPage:: Sending page data" << std::string(pageChunk, writeSize) << std::endl;
            }
            reply.set_ack(true);
            reply.set_size(writeSize);
            bytesSent += writeSize;
            if (!writer->Write(reply)) {
                printf("ERROR: Failed to send page to client\n");
            }
            //writer->WritesDone();
        }
        free(page);
     }
    else{
        if (!writer->Write(reply)) {
            printf("ERROR: Failed to send page to client\n");
        }
    }

    //this may need to initiate RPC calls to other client to invalidate or fetch stuff
    //Status status = returnPage(context, request, &reply, writer, page);
    return Status::OK;
}


Status ClientImpl::returnPage(ServerContext* context, const PageRequest* request, PageReply *reply, ServerWriter<PageReply>* writer, char* pageData) {
    //TODO: Return page in streams
    uint32_t bytesSent = 0;
    uint32_t writeSize = 2048;
    char page[pageSize];

    while(bytesSent < pageSize) {
    }

    return Status::OK;
}
