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

#include "snsCoordinator.grpc.pb.h"

using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
//using snsCoordinator::ActiveStatus;
using snsCoordinator::ClusterId;
using snsCoordinator::FollowSyncs;
using snsCoordinator::Heartbeat;
using snsCoordinator::Server;
using snsCoordinator::ServerType;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::User;
using snsCoordinator::Users;
using namespace std;
class SNSCoordinatorImpl final : public SNSCoordinator::Service {
    public:
    Status HandleHeartBeats(ServerContext *context, ServerReaderWriter<Heartbeat, Heartbeat>* stream) override {
      return Status::OK;
    }

    Status GetFollowSyncsForUsers(ServerContext *context, const Users *users, FollowSyncs *fsyncs) override {
      return Status::OK;
    }

    Status GetServer(ServerContext *context, const User *user, Server *server) override {
 //       log(INFO, "GetServer called for user " << user->user_id());

        int id = (user->user_id() % 3) + 1;

        if(clusters[id]->master->active()) {
          server->CopyFrom(*clusters[id]->master);
        } else {
          server->CopyFrom(*clusters[id]->slave);
        }

        // // return master if active, else return slave
        // if (clusters[clusterId][0]->active_status() == ActiveStatus::ACTIVE) {
        //     server->CopyFrom(clusters[clusterId][0]);
        // } else {
        //     server->CopyFrom(clusters[clusterId][1]);
        // }

        return Status::OK;
    }

    Status GetSlave(ServerContext *context, const ClusterId *cluster_id, Server *server) override {
     //   log(INFO, "GetSlave called for master " << cluster_id->cluster());

        server->CopyFrom(*clusters[cluster_id->cluster()]->slave);
        
        return Status::OK;
    }
    struct Cluster {
        Server* master;
        Server* slave;
        Server* sync;
    };

    unordered_map<int, Cluster*> clusters;
    

    //private:

};
void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  SNSCoordinatorImpl server;
  string address = "localhost:" + port_no;
  ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&server);
  std::unique_ptr<grpc::Server> srv(builder.BuildAndStart());
  srv->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
  return 0;
}