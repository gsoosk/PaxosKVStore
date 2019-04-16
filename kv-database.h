#ifndef KV_DATABASE_H
#define KV_DATABASE_H

#include <grpcpp/grpcpp.h>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "keyvaluestore.grpc.pb.h"

namespace keyvaluestore {
using PaxosLogsMap = std::unordered_map<std::string, std::map<int, PaxosLog>>;

struct PaxosLog {
  PaxosLog() : promised_id(0), accepted_id(0) {}
  int promised_id;
  int accepted_id;
  OperationType accepted_type;
  string accepted_value;
};

// An in-memory implementation of a key-value database.
//
// Thread-safe.
class KeyValueDataBase {
 public:
  // Return whether the value is found.
  bool GetValue(std::string* value);
  // Returns true if the value is overwritten, false if the key-val
  // pair is newly added.
  bool SetValue(const std::string& val);
  // Returns true if the deletion actually happens, false if the key
  // didn't exist.
  bool DeleteEntry();
  // Returns reference to the PaxosLogs of a given key.
  // A new entry with empty value will be created if key doesn't exist.
  const std::map<int, PaxosLog>& GetPaxosLogs(const std::string& key);
  // Add PaxosLog when Acceptor promises a proposal.
  void AddPaxosLog(const std::string& key, int round, int promised_id);
  // Add PaxosLog when Acceptor accepts a proposal.
  void AddPaxosLog(const std::string& key, int round, int accepted_id,
                   OperationType accepted_type, string accepted_value);

 private:
  std::unordered_map<std::string, std::string> data_map_;
  std::shared_mutex data_mtx_;
  PaxosLogsMap paxos_logs_map_;
  std::shared_mutex paxos_logs_mtx_;
};

}  // namespace keyvaluestore

#endif