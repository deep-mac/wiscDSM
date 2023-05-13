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

char* DSMMaster::invPage(const uint32_t clientID, const uint64_t addr, const uint32_t pageSize, bool send_page) {
	PageRequest request;
	request.set_pageaddr(addr);
    request.set_needpage(send_page);
    if (DEBUG){
        printf("invPage:: Sending request to client = %d, addr = %ld\n", clientID, addr);
    }
	PageReply reply;

	ClientContext context;
    
    uint32_t bytesReceived = 0;

    char* page;
    if (send_page)
       page = new char[pageSize/sizeof(char)];
    else
       page = NULL;

    std::unique_ptr<ClientReader<PageReply> > reader(stub_->invPage(&context, request));
    while (reader->Read(&reply)) {
        if(reply.ack()){
            if (DEBUG_DATA){
                std::cout << "invPage:: Received data = " << reply.pagedata() << std::endl;
            }
            if (send_page) {
                memcpy(page+bytesReceived, reply.pagedata().c_str(), reply.size());
                bytesReceived += reply.size(); 
            }
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

    if (DEBUG){
        printf("getPage:: Received pageAddr = %ld, received operation = %d, received clientID = %d\n", pageAddr, operation, clientID);
    }

    PageReply reply;
    reply.set_ack(true);

    dsmLock.lock();//[pageAddr%TOTAL_LOCKS].lock();

    PageState *pageState = pageTable->getState(pageTable->get(request->pageaddr()));

    if(DEBUG){
        printf("---------------------------------\ngetPage:: Initial value for state\n");
        pageState->printState();
        pageTable->printTable();
        printf("getPage:: formed value = %u\n-------------------------------\n", pageTable->formValue(pageState));
    }

    char* page;

    bool send_page = true;
    bool page_sent = false;
    if (pageState->st == INV){
        //if (operation == 0) {
        //    pageState->st = SHARED;
        //} 
        //else {
        //    pageState->st = MODIFIED;
        //}
        pageState->st = MODIFIED; //Added by Selvaraj
        pageState->clientList.push_back(clientID);
        reply.set_size(0);
        page_sent= false;
    } 
    else if (pageState->st == SHARED) {
        // Page is shared. If read, share to new client, else invalidate and share
        if (operation == 0) {
            pageState->clientList.push_back(clientID);
            page = clients[pageState->clientList[0]].fwdPageRequest(pageState->clientList[0], request->pageaddr(), request->pageoperation(), pageSize);
            page_sent = true;
        } 
        else if (operation == 1) {
            pageState->st = MODIFIED;

            for(int value : pageState->clientList)
                 if(clientID == value){
                    send_page = false;
                 }
 
            for (int i = 0; i < pageState->clientList.size(); i++) {
                if(clientID != pageState->clientList[i]){
                    if(send_page == false || page_sent == true)
                        char* null = clients[pageState->clientList[i]].invPage(pageState->clientList[i], request->pageaddr(), pageSize, false);
                    else{
                        page_sent = true;
                        page = clients[pageState->clientList[i]].invPage(pageState->clientList[i], request->pageaddr(), pageSize, true);
                    }
                }
            }
            pageState->clientList.clear();
            pageState->clientList.push_back(request->clientid());
        }
    } 
    else {
        // Page is in modified. Invalidate and share
        if (operation == 0) {
            pageState->st = SHARED;
            page = clients[pageState->clientList[0]].fwdPageRequest(pageState->clientList[0], request->pageaddr(), request->pageoperation(), pageSize);
            page_sent = true;
            pageState->clientList.push_back(request->clientid());
        } 
        else {
            page = clients[pageState->clientList[0]].invPage(pageState->clientList[0], request->pageaddr(), pageSize, true);
            page_sent = true;
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
        if (DEBUG_DATA && page_sent){
            std::cout << "getPage:: Received page data from client = " << std::string(page, pageSize) << std::endl;
        }
    }

    reply.set_containspage(page_sent);
    reply.set_sharedormodified((pageState->st == MODIFIED)?false:true);

    // TODO: Send Page
    int bytesSent = 0;
    uint32_t writeSize = 2048;
    if (page_sent){
        while(bytesSent < pageSize) {
            // TODO: Access 1024 Bytes of address
            //char pageChunk[writeSize];
            //memcpy(pageChunk, page+ bytesSent, writeSize);
            reply.set_pagedata(std::string(page + bytesSent, writeSize));
            if (DEBUG_DATA){
                std::cout << "getPage:: Sending page data" << std::string(page + bytesSent, writeSize) << std::endl;
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
     } else{
        if (!writer->Write(reply)) {
            printf("ERROR: Failed to send page to client\n");
        }
    }

    dsmLock.unlock();//[pageAddr%TOTAL_LOCKS].unlock();
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
