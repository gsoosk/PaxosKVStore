// Client side of keyvaluestore.

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "keyvaluestore.grpc.pb.h"
#include "time_log.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using keyvaluestore::DeleteRequest;
using keyvaluestore::EmptyMessage;
using keyvaluestore::GetRequest;
using keyvaluestore::GetResponse;
using keyvaluestore::KeyValueStore;
using keyvaluestore::PutRequest;

// #define TIME_LOG() std::cout << TimeNow();

class KeyValueStoreClient {
 public:
  KeyValueStoreClient(std::shared_ptr<Channel> channel)
      : stub_(KeyValueStore::NewStub(channel)) {}

  // Requests a key and displays the key and its corresponding value as a pair
  void GetValue(const std::string& key) {
    // Context for the client.
    ClientContext context;
    GetRequest request;
    request.set_key(key);
    GetResponse response;
    Status status = stub_->GetValue(&context, request, &response);
    if (!status.ok()) {
      TIME_LOG << "Error Code " << status.error_code() << ". "
               << status.error_message() << std::endl;
    } else {
      TIME_LOG << key << " : " << response.value() << std::endl;
    }
  }
  // Put a (key, value) pair to the store.
  void PutPair(const std::string& key, const std::string& value) {
    // Context for the client.
    ClientContext context;
    PutRequest request;
    request.set_key(key);
    request.set_value(value);
    EmptyMessage response;
    Status status = stub_->PutPair(&context, request, &response);
    if (!status.ok()) {
      TIME_LOG << "Error Code " << status.error_code() << ". "
               << status.error_message() << std::endl;
    } else {
      TIME_LOG << "Pair (" << request.key() << ", " << request.value()
               << ") is now added to the store." << std::endl;
    }
  }

  // Delete a (key, value) pair according to the given key.
  void DeletePair(const std::string& key) {
    // Context for the client.
    ClientContext context;
    DeleteRequest request;
    request.set_key(key);
    EmptyMessage response;
    Status status = stub_->DeletePair(&context, request, &response);
    if (!status.ok()) {
      TIME_LOG << "Error Code " << status.error_code() << ". "
               << status.error_message() << std::endl;
    } else {
      TIME_LOG << "Key (" << request.key() << ") is deleted." << std::endl;
    }
  }

 private:
  std::unique_ptr<KeyValueStore::Stub> stub_;
};

std::string ToLowerCase(const std::string& s) {
  std::string lower(s);
  for (int i = 0; i < s.length(); ++i) {
    if (s[i] <= 'Z' && s[i] >= 'A') lower[i] = s[i] - ('Z' - 'z');
  }
  return lower;
}

// Prepopulate key-value store
void Prepopulate(KeyValueStoreClient* client) {
  std::vector<std::pair<std::string, std::string>> key_values = {
      {"apple", "red"},      {"lemon", "yellow"},     {"orange", "orange"},
      {"strawberry", "red"}, {"watermelon", "green"}, {"grape", "purple"},
      {"coconut", "brown"},  {"avocado", "green"}};
  TIME_LOG << "************************************************" << std::endl;
  TIME_LOG << "Prepopulating key value store." << std::endl;
  TIME_LOG << "------------------------------------------------" << std::endl;
  // Send PUT Requests.
  for (auto item : key_values) {
    TIME_LOG << "Prepopulating: " << item.first << " " << item.second
             << std::endl;
    client->PutPair(item.first, item.second);
  }
}

// Run some tests.
void TestRuns(KeyValueStoreClient* client) {
  std::vector<std::pair<std::string, std::string>> key_values = {
      {"banana", "yellow"},
      {"coconut", "white"},
      {"blueberry", "blue"},
      {"kiwi", "brown"},
      {"apple", "green"}};
  TIME_LOG << "************************************************" << std::endl;
  TIME_LOG << "Start testing PUT, GET, DELETE operations." << std::endl;
  TIME_LOG << "------------------------------------------------" << std::endl;
  // Send DELETE Requests.
  for (auto item : key_values) {
    TIME_LOG << "Sending request: DELETE " << item.first << std::endl;
    client->DeletePair(item.first);
  }
  TIME_LOG << "------------------------------------------------" << std::endl;
  // Send GET Requests.
  for (auto item : key_values) {
    TIME_LOG << "Sending request: GET " << item.first << std::endl;
    client->GetValue(item.first);
  }
  TIME_LOG << "------------------------------------------------" << std::endl;
  // Send PUT Requests.
  for (auto item : key_values) {
    TIME_LOG << "Sending request: PUT " << item.first << " " << item.second
             << std::endl;
    client->PutPair(item.first, item.second);
  }
  TIME_LOG << "------------------------------------------------" << std::endl;
  // Send GET Requests.
  for (auto item : key_values) {
    TIME_LOG << "Sending request: GET " << item.first << std::endl;
    client->GetValue(item.first);
  }

  TIME_LOG << "------------------------------------------------" << std::endl;
  TIME_LOG << "End of test." << std::endl;
  TIME_LOG << "************************************************" << std::endl;
}

void RunClient(const std::string& server_address, bool auto_run) {
  TIME_LOG << "Listening to server_address: " << server_address << std::endl;
  // Instantiate the client.
  KeyValueStoreClient client(
      grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
  if (auto_run) {
    // Prepopulate the key value store.
    Prepopulate(&client);
    // Send a number of requests automatically.
    TestRuns(&client);
  }
  TIME_LOG << "Please enter your request: (Separate words with a white space.)"
           << std::endl;
  TIME_LOG
      << "Query examples: (Keywords GET/PUT/DELETE are NOT case-sensitive.)"
      << std::endl;
  TIME_LOG << "\"GET apple\" / \"PUT apple red\" / \"DELETE apple\""
           << std::endl;
  while (true) {
    std::string query;
    std::getline(std::cin, query);
    if (ToLowerCase(query) == "exit") break;
    std::stringstream querystream(query);
    std::string item;
    std::vector<std::string> args;
    while (std::getline(querystream, item, ' ')) {
      args.push_back(std::move(item));
    }
    if (args.size() == 2 && ToLowerCase(args[0]) == "get") {
      TIME_LOG << "Sending request: GET " << args[1] << std::endl;
      client.GetValue(args[1]);
    } else if (args.size() == 3 && ToLowerCase(args[0]) == "put") {
      TIME_LOG << "Sending request: PUT " << args[1] << " " << args[2]
               << std::endl;
      client.PutPair(args[1], args[2]);
    } else if (args.size() == 2 && ToLowerCase(args[0]) == "delete") {
      TIME_LOG << "Sending request: DELETE " << args[1] << std::endl;
      client.DeletePair(args[1]);
    } else {
      TIME_LOG << "Invalid command." << std::endl;
    }
  }
}

int main(int argc, char** argv) {
  // Set server address.
  std::string server_address = "localhost:8000";
  bool auto_run = false;
  if (argc <= 2) {
    std::cerr << "Usage: `./client <server_address> <auto_run>`" << std::endl
              << "Like this:" << std::endl
              << "`./client 0.0.0.0:8000 true`" << std::endl;
    return -1;
  } else {
    server_address = argv[1];
    std::string auto_run_str = ToLowerCase(std::string(argv[2]));
    if (auto_run_str == "true") {
      auto_run = true;
    } else if (auto_run_str == "false") {
      auto_run = false;
    } else {
      std::cerr << "Usage: `./client <server_address> <auto_run>`" << std::endl
                << "Like this:" << std::endl
                << "`./client 0.0.0.0:8000 true`" << std::endl;
      return -1;
    }
  }
  TIME_LOG << "Server address set to " << server_address << std::endl;

  RunClient(server_address, auto_run);

  return 0;
}
