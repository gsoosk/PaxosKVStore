#include "kv-database.h"

namespace keyvaluestore {

using grpc::Status;
using keyvaluestore::PaxosLog;

// Return whether the value is found.
bool KeyValueDataBase::GetValue(const std::string& key, std::string* value) {
  std::shared_lock<std::shared_mutex> reader_lock(data_mtx_);
  auto iter = data_map_.find(key);
  if (iter == data_map_.end()) return false;
  *value = iter->second;
  return true;
}

// Returns true if the value is overwritten, false if the key-val
// pair is newly added.
bool KeyValueDataBase::SetValue(const std::string& key,
                                const std::string& val) {
  std::unique_lock<std::shared_mutex> writer_lock(data_mtx_);
  bool found = data_map_.find(key) != data_map_.end();
  data_map_[key] = val;
  return found;
}

// Returns true if the deletion actually happens, false if the key
// didn't exist.
bool KeyValueDataBase::DeleteEntry(const std::string& key) {
  std::unique_lock<std::shared_mutex> writer_lock(data_mtx_);
  bool found = data_map_.find(key) != data_map_.end();
  data_map_.erase(key);
  return found;
}

// Returns a copy of data_map_.
std::unordered_map<std::string, std::string> KeyValueDataBase::GetDataMap() {
  std::shared_lock<std::shared_mutex> reader_lock(data_mtx_);
  return data_map_;
}

// Returns a copy of PaxosLogsMap of a key.
std::map<int, keyvaluestore::PaxosLog> KeyValueDataBase::GetPaxosLogs(
    const std::string& key) {
  std::shared_lock<std::shared_mutex> reader_lock(paxos_logs_mtx_);
  return paxos_logs_map_[key];
}

// Returns the mapped Paxos log for given key & round.
// A new element will be constructed using its default constructor and inserted
// if key or round is not found.
PaxosLog KeyValueDataBase::GetPaxosLog(const std::string& key, int round) {
  std::shared_lock<std::shared_mutex> reader_lock(paxos_logs_mtx_);
  return paxos_logs_map_[key][round];
}

// Returns the latest Paxos round number for the given key.
// A new element will be constructed using its default constructor and
// inserted if key is not found.
// Returns 0 if round is not found for given key.
int KeyValueDataBase::GetLatestRound(const std::string& key) {
  std::shared_lock<std::shared_mutex> reader_lock(paxos_logs_mtx_);
  if (paxos_logs_map_[key].empty()) return 0;
  return paxos_logs_map_[key].rbegin()->first;
}
// Add PaxosLog when Acceptor receives a proposal.
void KeyValueDataBase::AddPaxosLog(const std::string& key, int round) {
  std::unique_lock<std::shared_mutex> writer_lock(paxos_logs_mtx_);
  (paxos_logs_map_[key])[round];
}
// Update the promised_id in paxos_logs_map_ for the given key and round.
void KeyValueDataBase::AddPaxosLog(const std::string& key, int round,
                                   int promised_id) {
  std::unique_lock<std::shared_mutex> writer_lock(paxos_logs_mtx_);
  (paxos_logs_map_[key])[round].set_promised_id(promised_id);
}

// Update the acceptance info in paxos_logs_map_ for the given key and round.
void KeyValueDataBase::AddPaxosLog(const std::string& key, int round,
                                   int accepted_id, OperationType accepted_type,
                                   std::string accepted_value) {
  std::unique_lock<std::shared_mutex> writer_lock(paxos_logs_mtx_);
  (paxos_logs_map_[key])[round].set_accepted_id(accepted_id);
  (paxos_logs_map_[key])[round].set_accepted_type(accepted_type);
  (paxos_logs_map_[key])[round].set_accepted_value(accepted_value);
}

// Add PaxosLog from recovery snapshot.
void KeyValueDataBase::AddPaxosLog(const std::string& key, int round,
                                   int promised_id, int accepted_id,
                                   OperationType accepted_type,
                                   std::string accepted_value) {
  std::unique_lock<std::shared_mutex> writer_lock(paxos_logs_mtx_);
  (paxos_logs_map_[key])[round].set_promised_id(promised_id);
  (paxos_logs_map_[key])[round].set_accepted_id(accepted_id);
  (paxos_logs_map_[key])[round].set_accepted_type(accepted_type);
  (paxos_logs_map_[key])[round].set_accepted_value(accepted_value);
}

}  // namespace keyvaluestore
