#include <iostream>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include <thread>
#include <getopt.h>
#include "sns.grpc.pb.h"
#include "snsCoordinator.grpc.pb.h"
#include "client.h"
#include <glog/logging.h>
#define log(severity, msg) \
    LOG(severity) << msg;  \
    google::FlushLogFiles(google::severity);

using csce438::Message;
using csce438::Reply;
using csce438::Request;
using csce438::SNSService;
using snsCoordinator::ClusterId;
using snsCoordinator::FollowSyncs;
using snsCoordinator::Heartbeat;
using snsCoordinator::Server;
using snsCoordinator::ServerType;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::User;
using snsCoordinator::Users;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using namespace std;
class Client : public IClient
{
public:
    Client(const std::string &cip,
           const string &username,
           const std::string &cp)
        : cip(cip), username(username), cp(cp)
    {
    }

protected:
    virtual int connectTo();
    virtual IReply processCommand(std::string &input);
    virtual void processTimeline();

private:
    std::string cip;
    string username;
    std::string cp;

    // You can have an instance of the client stub
    // as a member variable.
    std::unique_ptr<SNSService::Stub> stub_;
    std::unique_ptr<SNSCoordinator::Stub> stub2_;
};

static struct option optlong[] ={
    {"cip", required_argument, 0, 'c'},
    {"cp", required_argument, 0, 'i'},
     {"p", required_argument, 0, 'p'},
    {"id", required_argument, 0, 'd'},
    {0, 0, 0, 0}};

int main(int argc, char **argv)
{

    int id = 0;
    string coordinatorPort = "3010";
    string coordinatorIP = "localhost";
    int idxx = 0;
    int opt = 0;
    for(int i = 0; i < argc; i++) {
     //   cout << argv[i] << endl;
    }
    while ((opt = getopt_long_only(argc, argv, "c:i:p:d:", optlong, &idxx)) != -1)
    {
   //     cout << opt << endl;
        switch (opt)
        {
        case 'c':
            coordinatorIP = optarg;
            break;
        case 'i':
            coordinatorPort = optarg;
         //   cout << optarg << endl;;
            break;
        case 'p':
            break;
        case 'd':
            id = atoi(optarg);
            break;
        default:
            std::cerr << "Invalid Command Line Argument\n";
        }
    }
    Client myc(coordinatorIP, to_string(id), coordinatorPort);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}



int Client::connectTo()
{
    // ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
    // ------------------------------------------------------------
    
    
    stub2_ = SNSCoordinator::NewStub(grpc::CreateChannel(cip + ":" + cp, grpc::InsecureChannelCredentials()));
    cout << (cip + ":" + cp) << endl;
    ClientContext context;
    User user;
    user.set_user_id(stoi(username));
    Server server;
    Status status = stub2_->GetServer(&context,user,&server);

    if(!status.ok()) return -1;    
     string info = server.server_ip() + ":" + server.port_num();
     cout << info << endl;
     stub_ = SNSService::NewStub(grpc::CreateChannel(info, grpc::InsecureChannelCredentials()));
     ClientContext context2;
     Request request;
     request.set_username(username);
     Reply reply;
     status = stub_->Login(&context2, request, &reply);
     if (status.ok() && reply.msg() == "SUCCESS")
     {
             return 1;
     }
    return -1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string &input)
{
    // ------------------------------------------------------------
    // GUIDE 1:
    // In this function, you are supposed to parse the given input
    // command and create your own message so that you call an
    // appropriate service method. The input command will be one
    // of the followings:
    //
    // FOLLOW <username>
    // UNFOLLOW <username>
    // LIST
    // TIMELINE
    //
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // GUIDE 2:
    // Then, you should create a variable of IReply structure
    // provided by the client.h and initialize it according to
    // the result. Finally you can finish this function by returning
    // the IReply.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Follow" service method for FOLLOW command,
    // IReply can be set as follow:
    //
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Follow(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //
    //      return ire;
    //
    // IMPORTANT:
    // For the command "LIST", you should set both "all_users" and
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------

    IReply ire;
    ClientContext context;
    Status status;
    Request request;
    request.add_arguments(username);
    Reply reply;
    if (input.substr(0, 4) == "LIST")
    {
        status = stub_->List(&context, request, &reply);
        ire.grpc_status = status;
    }
    else if (input.substr(0, 6) == "FOLLOW")
    {
        request.set_username(input.substr(7, input.size() - 7));
        status = stub_->Follow(&context, request, &reply);
        ire.grpc_status = status;
    }
    else if (input.substr(0, 8) == "UNFOLLOW")
    {
        request.set_username(input.substr(9, input.size() - 9));
        status = stub_->UnFollow(&context, request, &reply);
        ire.grpc_status = status;
    }
    else if (input.substr(0, 8) == "TIMELINE")
    {
        ire.comm_status = SUCCESS;
        ire.grpc_status = status;
    }
    else
    {
        ire.comm_status = FAILURE_INVALID;
    }
    ire.all_users = vector<string>(reply.all_users().begin(), reply.all_users().end());
    ire.following_users = vector<string>(reply.following_users().begin(), reply.following_users().end());
    if (status.ok())
    {
        // set respective status
        if (reply.msg() == "SUCCESS")
        {
            ire.comm_status = SUCCESS;
        }
        else if (reply.msg() == "FAILURE_ALREADY_EXISTS")
        {
            ire.comm_status = FAILURE_ALREADY_EXISTS;
        }
        else if (reply.msg() == "FAILURE_NOT_EXISTS")
        {
            ire.comm_status = FAILURE_NOT_EXISTS;
        }
        else if (reply.msg() == "FAILURE_INVALID_USERNAME")
        {
            ire.comm_status = FAILURE_INVALID_USERNAME;
        }
    }
    else
    {
        run_client();
        ire.comm_status = FAILURE_UNKNOWN;
    }
    return ire;
}

void process_input(std::shared_ptr<ClientReaderWriter<Message, Message>> stream, string &username)
{
    while (true)
    {
        string inp = getPostMessage();
        unsigned time = std::time(0);
        google::protobuf::Timestamp *stamp = new google::protobuf::Timestamp();
        stamp->set_seconds(time);
        Message message;
        message.set_username(username);
        message.set_msg(inp);
        message.set_allocated_timestamp(stamp);
        stream->Write(message);
    }
}

void Client::processTimeline()
{
    // ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
    // ------------------------------------------------------------
    ClientContext context;

    std::shared_ptr<ClientReaderWriter<Message, Message>> stream(
        stub_->Timeline(&context));
    Message m;
    m.set_username(username);
    stream->Write(m);

    Status status;

    thread tt(process_input, stream, std::ref(username));
    tt.detach();
    Message msg;
     auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context.set_deadline(deadline);
    while (stream->Read(&msg))
    {
        time_t t = static_cast<std::time_t>(msg.timestamp().seconds());
        displayPostMessage(msg.username(), msg.msg(), t);
    }
    run_client();
}
