
#include <ctime>

#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>

#include <fstream>
#include <glog/logging.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#define log(severity, msg) \
    LOG(severity) << msg;  \
    google::FlushLogFiles(google::severity);

#include "snsFollowSync.grpc.pb.h"

using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using snsFollowSync::SNSFollowSync;
using snsFollowSync::Relation;
using snsFollowSync::Users;
using snsFollowSync::Post;
using snsFollowSync::Reply;


using namespace std;

class SNSFollowSyncImpl final : public SNSFollowSync::Service {
    Status SyncUsers(ServerContext *context, const Users *users, Reply *reply) override {
        return Status::OK;
    }

    Status SyncRelations(ServerContext *context, const Relation *relation, Reply *reply) override {
        return Status::OK;
    }

    Status SyncTimeline(ServerContext *context, const Post *post, Reply *reply) override {
        return Status::OK;
    }

};
void RunServer(string ip, std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  SNSFollowSyncImpl server;
  string address = ip + ":"+port_no;
  ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&server);
  std::unique_ptr<grpc::Server> srv(builder.BuildAndStart());
  srv->Wait();
}

int main(int argc, char** argv) {
  
  std::string coordinatorIP = "localhost";
  std::string coordinatorPort = "";
  int id = 0;
  string type = "";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:cip:cp:id")) != -1){
    switch(opt) {
      case 'cip':
          coordinatorIP = optarg;
          break;
      case 'cp':
        coordinatorPort = optarg;
        break;
      case 'id':
        id = atoi(optarg);
        break;

      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(coordinatorIP, coordinatorPort);
  return 0;
}