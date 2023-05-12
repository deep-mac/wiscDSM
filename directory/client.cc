#include<client.hh>

// Test includes go here
//#include "tests/test_100_100000_0.h"
//#include "tests/test_100_100000_25.h"
//#include "tests/test_100_100000_50.h"
//#include "tests/test_100_100000_75.h"
//#include "tests/test_100_100000_100.h"
//#include "tests/test_100_100000_0_stride.h"
//#include "tests/test_100_100000_25_stride.h"
//#include "tests/test_100_100000_50_stride.h"
//#include "tests/test_100_100000_75_stride.h"
//#include "tests/test_100_100000_100_stride.h"
#include "tests/test_100_20000_100_stride.h"
#include "tests/test_50_20000_100_stride.h"
#include "tests/test_20_20000_100_stride.h"
#include "tests/test_10_20000_100_stride.h"
#include "tests/test_1_20000_100_stride.h"
#include "tests/test_100_20000_75_stride.h"
#include "tests/test_50_20000_75_stride.h"
#include "tests/test_20_20000_75_stride.h"
#include "tests/test_10_20000_75_stride.h"
#include "tests/test_1_20000_75_stride.h"
#include "tests/test_100_20000_50_stride.h"
#include "tests/test_50_20000_50_stride.h"
#include "tests/test_20_20000_50_stride.h"
#include "tests/test_10_20000_50_stride.h"
#include "tests/test_1_20000_50_stride.h"
#include "tests/test_100_20000_25_stride.h"
#include "tests/test_50_20000_25_stride.h"
#include "tests/test_20_20000_25_stride.h"
#include "tests/test_10_20000_25_stride.h"
#include "tests/test_1_20000_25_stride.h"
#include "tests/test_100_20000_0_stride.h"
#include "tests/test_50_20000_0_stride.h"
#include "tests/test_20_20000_0_stride.h"
#include "tests/test_10_20000_0_stride.h"
#include "tests/test_1_20000_0_stride.h"
//

uint32_t DSMClient::numSegFaults = 0;
uint32_t MasterImpl::numPageInvalidationsAcked = 0;
uint32_t MasterImpl::numPageInvalidationsNacked = 0;
uint32_t MasterImpl::numFwdReqsAcked = 0;
uint32_t MasterImpl::numFwdReqsNacked = 0;

std::mutex DSMClient::dsmLock;

void faultHandler(int sig, siginfo_t *info, void *ctx){

    char *inputAddr = (char*)info->si_addr;
    long int pageNumber = ((long int)((inputAddr) - sharedAddrStart))/pageSize;
    int pagesPerMaster = totalPages/totalMasters;
    int masterNum = pageNumber/pagesPerMaster;
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
        DSMClient::numSegFaults++;
        int operation = OP_READ;
        if (ucontext->uc_mcontext.gregs[REG_ERR] & 2){
            //Write access
            operation = OP_WRITE;
        }
        //default operation OP_READ
        PageReply reply = masters[masterNum].getPage(pageNumber,operation);
        if (reply.ack()){
            void *baseAddr = (void*)(sharedAddrStart+(pageNumber*pageSize));
            int status = mprotect( baseAddr, pageSize, PROT_WRITE);
            if (status != 0){
                printf("ERROR: faultHanlder:: mprotect failed\n");
            }
            if (reply.containspage()) 
                memcpy(baseAddr, reply.pagedata().c_str(), pageSize);
            status = mprotect( baseAddr, pageSize, (reply.sharedormodified() == false)?(PROT_WRITE | PROT_READ):PROT_READ);
            if (status != 0){
                printf("ERROR: faultHanlder:: mprotect failed\n");
            }
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
int initShmem(uint64_t startAddress, int numPages, int i_clientID, bool isRemote){
    
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
    
    if (isRemote) {
        clientPorts[0] = "10.10.1.1:50051";
        clientPorts[1] = "10.10.1.2:50051";
        clientPorts[2] = "10.10.1.3:50051";
    } else {
        clientPorts[0] = "10.10.1.1:50051";
        clientPorts[1] = "10.10.1.1:50052";
        clientPorts[2] = "10.10.1.1:50053";
    }
    std::thread clientThread (RunClient, clientPorts[clientID]);
    clientThread.detach();

    //Establish connection with masters
    std::map<int, std::string> masterIPs;
    
    if (isRemote) {
        masterIPs[0] = "10.10.1.1:2048";
        masterIPs[1] = "10.10.1.2:2048";
        masterIPs[2] = "10.10.1.3:2048";
    } else {
        masterIPs[0] = "10.10.1.1:2048";
        masterIPs[1] = "10.10.1.1:2049";
        masterIPs[2] = "10.10.1.1:2050";
    }
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

    //DSMClient::dsmLock.lock();
    int status = mprotect(baseAddr, pageSize, PROT_READ);
    if (status != 0){
        printf("ERROR: fwdPageRequest:: mprotect failed\n");
        reply.set_ack(false);
        MasterImpl::numFwdReqsNacked++;
        if (!writer->Write(reply)){
            printf("ERROR: fwdPageReuqest:: writer nacked while sending false!!");
        }
        //writer->WritesDone();
    }
    else{
        int bytesSent = 0;
        MasterImpl::numFwdReqsAcked++;
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
    //DSMClient::dsmLock.unlock();
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
    if (DEBUG) printf("Need page : %d\n", sendPage);
    //DSMClient::dsmLock.lock();
    if(sendPage){
        int status = mprotect( baseAddr, pageSize, PROT_READ);
        if (status != 0){
            printf("ERROR: invPage:: mprotect failed\n");
            reply.set_ack(false);
            MasterImpl::numPageInvalidationsNacked++;
            if (!writer->Write(reply)){
                printf("ERROR: invPage:: writer nacked while sending false!!");
            }
            //writer->WritesDone();
        }
        else{
            int bytesSent = 0;
            reply.set_containspage(true);
            MasterImpl::numPageInvalidationsAcked++;
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
        MasterImpl::numPageInvalidationsAcked++;
        reply.set_size(0);
        if (!writer->Write(reply)){
            printf("ERROR: invPage:: writer nacked!!");
        }
    }
    int status = mprotect( baseAddr, pageSize, PROT_NONE);
    //DSMClient::dsmLock.unlock();
    
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
    //DSMClient::dsmLock.lock();
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
    //DSMClient::dsmLock.unlock();
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
    bool isRemote = false;
    if (argc < 2){
        printf("ERROR GIVE CLIENT ID\n");
        exit(1);
    } else if (argc == 3) {
        isRemote = true;
    }
    initShmem((uint64_t)(1 << 30), 300, atoi(argv[1]), isRemote);
    volatile int *p;
    p = (int*)0x40000000 + (int)0x1;
    //printf("p pointer = %x\n", p);
    //printf("Value before assignment = %d\n", *p);
    //*p = 1;
    //printf("Value after assignment = %d\n", *p);
    //sleep(10);
    //printf("Woke up from sleep\n");

    //*p = 0;
    //sleep(10);
    //*p = 1;

    // Custom tests go here
    //test_100_20000_100_stride(3, atoi(argv[1]));
    //test_100_20000_75_stride(3, atoi(argv[1]));
    //test_100_20000_50_stride(3, atoi(argv[1]));
    //test_100_20000_25_stride(3, atoi(argv[1]));
    //test_100_20000_0_stride(3, atoi(argv[1]));

    //test_50_20000_100_stride(3, atoi(argv[1]));
    //test_50_20000_75_stride(3, atoi(argv[1]));
    //test_50_20000_50_stride(3, atoi(argv[1]));
    //test_50_20000_25_stride(3, atoi(argv[1]));
    //test_50_20000_0_stride(3, atoi(argv[1]));

    //test_20_20000_100_stride(3, atoi(argv[1]));
    //test_20_20000_75_stride(3, atoi(argv[1]));
    //test_20_20000_50_stride(3, atoi(argv[1]));
    //test_20_20000_25_stride(3, atoi(argv[1]));
    //test_20_20000_0_stride(3, atoi(argv[1]));

    //test_10_20000_100_stride(3, atoi(argv[1]));
    //test_10_20000_75_stride(3, atoi(argv[1]));
    //test_10_20000_50_stride(3, atoi(argv[1]));
    //test_10_20000_25_stride(3, atoi(argv[1]));
    //test_10_20000_0_stride(3, atoi(argv[1]));

    //test_1_20000_100_stride(3, atoi(argv[1]));
    //test_1_20000_75_stride(3, atoi(argv[1]));
    //test_1_20000_50_stride(3, atoi(argv[1]));
    //test_1_20000_25_stride(3, atoi(argv[1]));
    //test_1_20000_0_stride(3, atoi(argv[1]));

    //
    sleep(30);

    printf("Number of segmentation faults : %d\n", DSMClient::numSegFaults);
    printf("Number of Page Invalidations Acked : %d\n", MasterImpl::numPageInvalidationsAcked);
    printf("Number of Page Invalidations Nacked : %d\n", MasterImpl::numPageInvalidationsNacked);
    printf("Number of Fwd Page Requests Acked : %d\n", MasterImpl::numFwdReqsAcked);
    printf("Number of Fwd Page Requests Nacked : %d\n", MasterImpl::numFwdReqsNacked);

    DSMClient::numSegFaults = 0;
    MasterImpl::numPageInvalidationsAcked = 0;
    MasterImpl::numPageInvalidationsNacked = 0;
    MasterImpl::numFwdReqsAcked = 0;
    MasterImpl::numFwdReqsNacked = 0;

    return 1;
}
