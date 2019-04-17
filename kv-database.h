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

// struct PaxosLog {
//   PaxosLog() : promised_id(0), accepted_id(0) {}
//   int promised_id;
//   int accepted_id;
//   OperationType accepted_type;
//   std::string accepted_value;
// };

// An in-memory implementation of a key-value database.
//
// Thread-safe.
class KeyValueDataBase {
 public:
  // Returns whether the value is found.
  bool GetValue(const std::string& key, std::string* value);
  // Returns true if the value is overwritten, false if the key-val
  // pair is newly added.
  bool SetValue(const std::string& key, const std::string& val);
  // Returns true if the deletion actually happens, false if the key
  // didn't exist.
  bool DeleteEntry(const std::string& key);

  // Returns a copy of data_map_.
  std::unordered_map<std::string, std::string> GetDataMap();

  // Returns a copy of PaxosLogsMap of a key.
  std::map<int, keyvaluestore::PaxosLog> GetPaxosLogs(const std::string& key);

  // Returns a copy of paxos_logs_map_.
  std::unordered_map<std::string, std::map<int, keyvaluestore::PaxosLog>>
  GetPaxosLogsMap();
  // Returns the mapped Paxos log for given key & round.
  // A new element will be constructed using its default constructor and
  // inserted if key or round is not found.
  keyvaluestore::PaxosLog GetPaxosLog(const std::string& key, int round);

  // Returns the latest Paxos round number for the given key.
  // A new element will be constructed using its default constructor and
  // inserted if key or round is not found.
  int GetLatestRound(const std::string& key);

  // Add PaxosLog when Acceptor receives a proposal.
  void AddPaxosLog(const std::string& key, int round);
  // Add PaxosLog when Acceptor promises a proposal.
  void AddPaxosLog(const std::string& key, int round, int promised_id);
  // Add PaxosLog when Acceptor accepts a proposal.
  void AddPaxosLog(const std::string& key, int round, int accepted_id,
                   OperationType accepted_type, std::string accepted_value);
  // Add PaxosLog from recovery snapshot.
  void AddPaxosLog(const std::string& key, int round, int promised_id,
                   int accepted_id, OperationType accepted_type,
                   std::string accepted_value);

 private:
  std::unordered_map<std::string, std::string> data_map_;
  std::shared_mutex data_mtx_;
  std::unordered_map<std::string, std::map<int, keyvaluestore::PaxosLog>>
      paxos_logs_map_;
  std::shared_mutex paxos_logs_mtx_;
};

}  // namespace keyvaluestore

#endif