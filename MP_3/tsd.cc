#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
using snsCoordinator::ClusterId;
using snsCoordinator::FollowSyncs;
using snsCoordinator::Heartbeat;
using snsCoordinator::Server;
using snsCoordinator::ServerType;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::User;
using snsCoordinator::Users;
#include "sns.grpc.pb.h"

using namespace std;
using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

class SNSServiceImpl final : public SNSService::Service {
  public:
  void loadData() {
    ifstream in;
    in.open("datafile.txt");
    while(in.fail()) {
      sleep(1);
      in.open("datafile.txt");
    }
    int num_users;
    in >> num_users;

    for(int i = 0; i < num_users; i++) {
      string username;
      in >> username;
      users.push_back(username);
      following[username] = vector<string>();
      posts[username] = vector<Post>();
      int num_following;
      in >> num_following;
      for(int j = 0; j < num_following; j++) {
        string f_name;
        in >> f_name;
        following[username].push_back(f_name);
      }
    }

    int num_posts;
    in >> num_posts;
    for(int i = 0; i < num_posts; i++) {
      Post p;
      in >> p.username;
      in >> p.post_time;
      in >> p.contents;
      allPosts.push_back(p);
    }

    in.close();
  }

  void updateData() {

    ofstream out;
    out.open("datafile.txt");

    while(out.fail()) {
      sleep(1);
      out.open("datafile.txt");
    }
    out << users.size() << endl;
    for(auto user : users) {
      vector<string> follow = following[user];
      vector<Post> post = posts[user];
      out << user << endl;
      out << follow.size() << endl;
      for(auto f : follow) {
        out << f << endl;
      }

    }
    out << allPosts.size() << endl;

    for(Post p : allPosts) {
      out << p.username << endl;
      out << p.post_time << endl;
      out << p.contents << endl;
    }
    out.close();
  }
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    string client_name = *(request->arguments().begin());
    for(string s : users) {
      reply->add_all_users(s);
    }
    for(string s : following[client_name]) {
      reply->add_following_users(s);
    }
    reply->set_msg("SUCCESS");
    
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    
    string username = request->username();
    string client_name = *(request->arguments().begin());

    if(std::find(users.begin(), users.end(), username) == users.end()) {
      reply->set_msg("FAILURE_INVALID_USERNAME");
    } else if(std::find(following[client_name].begin(), following[client_name].end(), username) != following[client_name].end()) {
      reply->set_msg("FAILURE_ALREADY_EXISTS");
    } else {
      reply->set_msg("SUCCESS");
      following[client_name].push_back(username);
    }
    updateData();
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    string username = request->username();
    string client_name = *(request->arguments().begin());

    if(std::find(users.begin(), users.end(),username) == users.end() || username == client_name) {
      reply->set_msg("FAILURE_INVALID_USERNAME");
    } else if(std::find(following[client_name].begin(), following[client_name].end(),username) == following[client_name].end()) {
      reply->set_msg("FAILURE_NOT_EXISTS");
    } else {
      reply->set_msg("SUCCESS");
      auto i = std::find(following[client_name].begin(), following[client_name].end(), username);
      following[client_name].erase(i);
    }
    updateData(); 
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    string username = request->username();
    if(logged_in.find(username) == logged_in.end()) {
      reply->set_msg("SUCCESS");
      logged_in.insert(username);
      if(std::find(users.begin(), users.end(), username) == users.end()) {
        users.push_back(username);
      following[username].push_back(username);}
    } else {
      reply->set_msg("FAILURE_ALREADY_EXISTS");
    }
    updateData();
    return Status::OK;
  }

  void sendPost(ServerReaderWriter<Message, Message>* stream, string& user) {
    int size = allPosts.size();
    while(true) {
      if(allPosts.size() > size) {
        for(int i = size; i < allPosts.size(); i++) {
          Post p = allPosts[i];
          if(std::find(following[user].begin(), following[user].end(),p.username) == following[user].end()) continue;
          google::protobuf::Timestamp* stamp = new google::protobuf::Timestamp();
          stamp->set_seconds(p.post_time);
          Message message;
          message.set_username(p.username);
          message.set_msg(p.contents);
          message.set_allocated_timestamp(stamp);
          stream->Write(message);
        }
        size = allPosts.size();
      }
    }
  }


  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    Message init;
    stream->Read(&init);
    string user = init.username();

    int count = 0;
    for(int i = allPosts.size()-1; i >= 0 && count < 20; i--) {
      Post p = allPosts[i];
      if(std::find(following[user].begin(), following[user].end(),p.username) != following[user].end()) {
          google::protobuf::Timestamp* stamp = new google::protobuf::Timestamp();
          stamp->set_seconds(p.post_time);
          Message message;
          message.set_username(p.username);
          message.set_msg(p.contents);
          message.set_allocated_timestamp(stamp);
          stream->Write(message);
        count++;
      }
      
    }
 
    thread tt(&SNSServiceImpl::sendPost, this,stream, std::ref(user)) ;
    tt.detach();
    
    Message m;
    while(stream->Read(&m)) {
      Post p;
      p.username = m.username();
      p.contents = m.msg();
      p.post_time = m.timestamp().seconds();
      allPosts.push_back(p);
      updateData();
    }
    
    return Status::OK;
  }

  struct Post {
    string username, contents;
    unsigned post_time;
  };

  private:
  vector<string> users;
  unordered_set<string> logged_in;
  unordered_map<string, vector<string>> following;
  unordered_map<string,vector<Post>> posts;
  vector<Post> allPosts;


};

void RunServer(string ip, std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  SNSServiceImpl server;
  server.loadData();
  string address = ip + ":" + port_no;
  ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&server);
  std::unique_ptr<Server> srv(builder.BuildAndStart());
  srv->Wait();
}

void handleHeartBeat() {
  
}

int main(int argc, char** argv) {
  
  std::string coordinatorIP = "localhost";
  std::string coordinatorPort = "";
  std::string port = "3010";
  int id = 0;
  string type = "";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:cip:cp:id:t")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      case 'cip':
          coordinatorIP = optarg;
          break;
      case 'cp':
        coordinatorPort = optarg;
        break;
      case 'id':
        id = atoi(optarg);
        break;
      case 't':
        type = optarg;
        break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  //RunServer(port);
  return 0;
}
