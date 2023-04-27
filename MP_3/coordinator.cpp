#include <ctime>
#include <mutex>
#include <condition_variable>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <thread>
#include <fstream>
#include <glog/logging.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <getopt.h>
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
using snsCoordinator::ClusterId;
using snsCoordinator::FollowSyncs;
using snsCoordinator::Heartbeat;
using snsCoordinator::Server;
using snsCoordinator::ServerType;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::User;
using snsCoordinator::Users;
using namespace std;


struct Cluster {
        Server* master;
        Server* slave;
        Server* sync;
        bool masterActive, slaveActive, syncActive;
};
unordered_map<int, Cluster*> clusters;


//unsigned time;
void assign(Heartbeat h, Server *s) {
      s->set_server_ip(h.server_ip());
      s->set_port_num(h.server_port());
      s->set_server_id(h.server_id());
}

class SNSCoordinatorImpl final : public SNSCoordinator::Service {
    public:
    Status HandleHeartBeats(ServerContext *context, ServerReaderWriter<Heartbeat, Heartbeat>* stream) override {
      
      Heartbeat h;
      int id = h.server_id();
      stream->Read(&h);
      Server* s = new Server();
      if(h.server_type() == ServerType::MASTER) {
        clusters[id]->master = s;
        s->set_server_type(h.server_type());
        assign(h,s);
        clusters[id]->masterActive = true;
      } else if(h.server_type() == ServerType::SLAVE) {
        clusters[id]->slave = s;
        s->set_server_type(h.server_type());
        assign(h,s);
        clusters[id]->slaveActive = true;
      } else {
        clusters[id]->sync = s;
        s->set_server_type(h.server_type());
        assign(h,s);
        clusters[id]->syncActive = true;
      }
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(20);
      
        std::mutex mutex;
        std::condition_variable cv;
        bool received_request = false;

        std::thread reader_thread([&]() {
            while (stream->Read(&h))
            {
              cout  << "heartbeat" << endl;

                std::unique_lock<std::mutex> lock(mutex);
                received_request = true;
                cv.notify_one();
                deadline = std::chrono::system_clock::now() + std::chrono::seconds(20);
            }
        });

        while (true)
        {
            std::unique_lock<std::mutex> lock(mutex);
            while (!received_request && std::chrono::system_clock::now() < deadline)
            {
                cv.wait_until(lock, deadline);
            }

            if (!received_request)
            {
              cout << "request not recieved" << endl;
          if(s->server_type() == ServerType::MASTER) {
            clusters[id]->masterActive = false;
            cout << "yo" << endl;
          } else if(s->server_type() == ServerType::SLAVE) {

            clusters[id]->slaveActive = false;
          } else {
            clusters[id]->syncActive = false;

      }
       std::this_thread::sleep_for(std::chrono::seconds(5));
             //   return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Receiving next request timed out");
            }

            received_request = false;
        }

      reader_thread.join();
      return Status::OK;
    }

    Status GetFollowSyncsForUsers(ServerContext *context, const Users *users, FollowSyncs *fsyncs) override {
      for (auto user : users->users()) {
        int id = (user % 3);
   //     if(id == 0) id = 3;
        Server* sync = clusters[id]->sync;
        fsyncs->add_users(user);
        fsyncs->add_follow_syncs(id);
        fsyncs->add_follow_sync_ip(sync->server_ip());
        fsyncs->add_port_nums(sync->port_num());
      }
      
      return Status::OK;
    }

    Status GetServer(ServerContext *context, const User *user, Server *server) override {
       
   //     cout << "checkpoint" << endl;
        int id = (user->user_id() % 3);
     //   if(id == 0) id = 3;
   //     LOG(INFO) << "Getting Server for user " << id << endl;
        if(clusters[id]->masterActive) {
          cout << "yo3" << endl;
          server->CopyFrom(*(clusters[id]->master));
        } else {
          server->CopyFrom(*(clusters[id]->slave));
        }

        return Status::OK;
    }

    Status GetSlave2 (ServerContext *context, const ClusterId *cluster_id, Server *server) override {
       cout << "get slave called" << endl;
      return Status::OK;
    }

    Status GetSlave(ServerContext *context, const ClusterId *cluster_id, Server *server) override {
     //   log(INFO, "GetSlave called for master " << cluster_id->cluster());
          cout << "get slave called" << endl;
//         int id = cluster_id->cluster();
//         cout << "Retrieving slave for master " << id << endl;

//         while(!clusters[id]->slaveActive) {
//           std::this_thread::sleep_for(std::chrono::seconds(1));
//         }
//         server->CopyFrom(*(clusters[id]->slave));
//  //       cout << "Port number for slave: " <<  server->port_num() << endl;
//         cout << "Status of master: " << clusters[id]->masterActive << endl;
         return Status::OK;
    }


  
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
  cout << address << endl;
  ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&server);
  std::unique_ptr<grpc::Server> srv(builder.BuildAndStart());
  srv->Wait();
 // cout << "hey" << endl;
}

int main(int argc, char** argv) {
  clusters[0] = new Cluster();
  clusters[1] = new Cluster();
  clusters[2] = new Cluster();
 // google::InitGoogleLogging("coordinator.txt");
  std::string port = "3010";
  int opt = 0;
  cout << "hi" << endl;
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