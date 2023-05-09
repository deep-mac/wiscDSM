#include<client.hh>

void faultHandler(int sig, siginfo_t *info, void *ctx){

    char *inputAddr = (char*)info->si_addr;
    long int pageNumber = ((long int)((inputAddr) - sharedAddrStart))/pageSize;
    int masterNum = pageNumber/totalMasters;
    ucontext_t *ucontext = (ucontext_t*)ctx;

    if(DEBUG){
        printf("faultHandler:: Triggered\n");
        printf("faultHandler:: pageNumber = %ld, masterNum= %d\n", pageNumber, masterNum);
    }


    if (pageNumber < 0 || masterNum > 2){
        //This means it is not a shared page, call default segfault 
        if (sa_default->sa_flags & SA_SIGINFO) {
            (*(sa_default->sa_sigaction))(sig, info, ucontext);
        }
        else {
            if(DEBUG)
                printf("faultHandler: Calling defualt handler\n");
            (*(sa_default->sa_handler))(sig);
        }
    }
    else{
        int operation = OP_READ;
        if (ucontext->uc_mcontext.gregs[REG_ERR] & 2){
            //Write access
            operation = OP_WRITE;
        }
        //default operation OP_READ
        PageReply reply = masters[masterNum].getPage(pageNumber,operation);
        if (reply.ack()){
            void *baseAddr = (void*)(sharedAddrStart+(pageNumber*pageSize));
            int status = mprotect( baseAddr, pageSize, (reply.sharedormodified() == false)?(PROT_WRITE | PROT_READ):PROT_READ);
            if (status != 0){
                printf("ERROR: faultHanlder:: mprotect failed\n");
            }
            if (reply.containspage()) 
                memcpy(baseAddr, reply.pagedata().c_str(), pageSize);
            //if (operation == OP_WRITE){
            //    status = mprotect( baseAddr, pageSize, PROT_WRITE);
            //}
            //else{
            //    status = mprotect( baseAddr, pageSize, PROT_READ);
            //}
            //if (status != 0){
            //    printf("ERROR: faultHanlder:: mprotect failed after writing new data\n");
            //}
        }
        else{
            //FIXME - ideally else call default action and just segfault
            printf("ERROR: faultHandler:: Returned NULL from master\n");
            if (sa_default->sa_flags & SA_SIGINFO) {
                (*(sa_default->sa_sigaction))(sig, info, ucontext);
            }
            else {
                if(DEBUG)
                    printf("faultHandler: Calling defualt handler\n");
                (*(sa_default->sa_handler))(sig);
            }
        }

    }

}
int initShmem(uint64_t startAddress, int numPages, int i_clientID){
    
    //initShmem ------------
    char* sharedAddr = (char*)mmap((void *)startAddress, numPages * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    int status = mprotect(sharedAddr, numPages*pageSize, PROT_NONE);
    printf("initShmem:: shared Addr set to = %x\n", sharedAddr);
    sa_default = (struct sigaction*)malloc(sizeof(struct sigaction));
    sa.sa_sigaction = faultHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    sigemptyset(&sa_default->sa_mask);
    sa_default->sa_handler = SIG_DFL;
    sa_default->sa_flags = 0;

    if (sigaction(SIGSEGV, &sa, sa_default)){
        perror("sigaction");
        exit(1);
    }

    sa_default->sa_handler = SIG_DFL;
    sa_default->sa_flags = 0;

    sharedAddrStart = startAddress;
    totalPages = numPages;
    clientID = i_clientID;


    //Start the listener process
    std::string clientPort = "10.10.1.1:50051";
    std::map<int, std::string> clientPorts;
    
    clientPorts[0] = "10.10.1.1:50051";
    clientPorts[1] = "10.10.1.1:50052";
    clientPorts[2] = "10.10.1.1:50053";
    std::thread clientThread (RunClient, clientPorts[clientID]);
    clientThread.detach();

    //Establish connection with masters
    std::map<int, std::string> masterIPs;
    
    masterIPs[0] = "10.10.1.1:2048";
    masterIPs[1] = "10.10.1.1:2049";
    masterIPs[2] = "10.10.1.1:2050";
    for (int i = 0; i < 3; i++){
         masters.push_back(std::move(DSMClient(grpc::CreateChannel(masterIPs[i], grpc::InsecureChannelCredentials()))));
    }

    return 0;

}

void RunClient(std::string port) {
    MasterImpl service;

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(port, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<Server> client(builder.BuildAndStart());
    std::cout << "Client listening on " << port << std::endl;

    // Wait for the client to shutdown. Note that some other thread must be
    // responsible for shutting down the client for this call to ever return.
    client->Wait();
}


Status MasterImpl::fwdPageRequest(ServerContext* context, const PageRequest* request, ServerWriter<PageReply> *writer) {
    PageReply reply;
    if (DEBUG){
        //Only address is important
        printf("fwdPageRequest:: pageAddress = %ld\n", request->pageaddr());
    }
    void *baseAddr = (void*)(sharedAddrStart+(request->pageaddr()*pageSize));
    char page[pageSize];
    int writeSize = 2048;
    int status = mprotect(baseAddr, pageSize, PROT_READ);
    if (status != 0){
        printf("ERROR: fwdPageRequest:: mprotect failed\n");
        reply.set_ack(false);
        if (!writer->Write(reply)){
            printf("ERROR: fwdPageReuqest:: writer nacked while sending false!!");
        }
        //writer->WritesDone();
    }
    else{
        int bytesSent = 0;
        while(bytesSent < pageSize){
            memcpy(page, (char*)baseAddr+bytesSent, writeSize);
            reply.set_pagedata(std::string(page, writeSize));
            if (DEBUG_DATA){
                std::cout << "fwdPage:: sent data " << std::string(page, writeSize) << std::endl;
            }
            reply.set_ack(true);
            reply.set_size(writeSize);
            bytesSent += writeSize;
            if (!writer->Write(reply)){
                printf("ERROR: fwdPageReuqest:: writer nacked!!");
            }
            //writer->WritesDone();
        }
    }
    //Status status = writer->Finish();
	return Status::OK;
}

Status MasterImpl::invPage(ServerContext* context, const PageRequest* request, ServerWriter<PageReply> *writer) {
    PageReply reply;
    if (DEBUG){
        //Only address is important
        printf("invPage:: pageAddress = %ld\n", request->pageaddr());
    }
    void *baseAddr = (void*)(sharedAddrStart+(request->pageaddr()*pageSize));
    char page[pageSize];
    int writeSize = 2048;
    bool sendPage = request->needpage();
    if(sendPage){
        int status = mprotect( baseAddr, pageSize, PROT_READ);
        if (status != 0){
            printf("ERROR: invPage:: mprotect failed\n");
            reply.set_ack(false);
            if (!writer->Write(reply)){
                printf("ERROR: invPage:: writer nacked while sending false!!");
            }
            //writer->WritesDone();
        }
        else{
            int bytesSent = 0;
            reply.set_containspage(true);
            while(bytesSent < pageSize){
                memcpy(page, (char*)baseAddr+bytesSent, writeSize);
                reply.set_pagedata(std::string(page, writeSize));
                if (DEBUG_DATA){
                    std::cout << "invPage:: sent data " << std::string(page, writeSize) << std::endl;
                }
                reply.set_ack(true);
                reply.set_size(writeSize);
                bytesSent += writeSize;
                if (!writer->Write(reply)){
                    printf("ERROR: invPage:: writer nacked!!");
                }
                    //writer->WritesDone();
            }
         }
    }
    else{
        reply.set_containspage(false);
        reply.set_ack(true);
        reply.set_size(0);
        if (!writer->Write(reply)){
            printf("ERROR: invPage:: writer nacked!!");
        }
    }
    int status = mprotect( baseAddr, pageSize, PROT_NONE);
    
	return Status::OK;
}

PageReply DSMClient::getPage(const uint64_t addr, const uint32_t operation)  {
    PageRequest request;
    request.set_pageaddr(addr);
    request.set_pageoperation(operation);
    request.set_clientid(clientID);

    PageReply reply;
    char page[pageSize];

    ClientContext context;

    std::unique_ptr<ClientReader<PageReply>> reader(stub_->getPage(&context, request));
    int pktNum = 0; 
    int sizeReceived = 0;
    while (reader->Read(&reply)) {
        if(reply.ack()){
            if (reply.containspage()) {
                memcpy(page+sizeReceived, reply.pagedata().c_str(), reply.size());
                sizeReceived += reply.size(); 
            }
        }
        pktNum++; 
    }
    Status status = reader->Finish();
    if (DEBUG){
        printf("DSMClient::getPage:: Received page from master\n");
        if (DEBUG_DATA){
            //printf("DSMClient::getPage:: Received data = %s\n", page);
            std::cout << "DSMClient::getPage :: Received data = " << std::string(page, pageSize)  << std::endl;
        }
    }

    // Act upon its status.
    if (status.ok()) {
        if (reply.containspage()) {
            reply.set_pagedata(std::string(page, pageSize));
        }
        reply.set_ack(true);
        return reply;
    } else {
        std::cout << status.error_code() << ": " << status.error_message() << std::endl;
        reply.set_ack(false);
        return reply;
    }
}

int main(int argc, char *argv[]){
    if (argc < 2){
        printf("ERROR GIVE CLIENT ID\n");
        exit(1);
    }
    initShmem((uint64_t)(1 << 30), 9, atoi(argv[1]));
    int *p;
    p = (int*)0x40000000 + (int)0x1;
    printf("p pointer = %x\n", p);
    printf("Value before assignment = %d\n", *p);
    *p = 1;
    printf("Value after assignment = %d\n", *p);
    sleep(10);
    printf("Woke up from sleep\n");
//    printf("Value after assignment = %d\n", *p);
    //*p = 2;
    //printf("Value after assignment = %d\n", *p);
    return 1;
}
