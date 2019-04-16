#include "kv-database.h"

namespace keyvaluestore {

using grpc::Status;

// Return whether the value is found.
bool KeyValueDataBase::GetValue(const std::string& key, std::string* value) {
  std::shared_lock<std::shared_mutex> reader_lock(kv_db_->data_mtx_);
  auto iter = kv_db_->data_map_.find(key);
  if (iter == kv_db_->data_map_.end()) return false;
  *value = iter->second;
  return true;
}

// Returns true if the value is overwritten, false if the key-val
// pair is newly added.
bool KeyValueDataBase::SetValue(const std::string& key,
                                const std::string& val) {
  std::unique_lock<std::shared_mutex> writer_lock(kv_db_->data_mtx_);
  bool found = kv_db_->data_map_.find(key) != kv_db_->data_map_.end();
  kv_db_->data_map_[key] = val;
  return found;
}

// Returns true if the deletion actually happens, false if the key
// didn't exist.
bool KeyValueDataBase::DeleteEntry(const std::string& key) {
  std::unique_lock<std::shared_mutex> writer_lock(kv_db_->data_mtx_);
  bool found = kv_db_->data_map_.find(key) != kv_db_->data_map_.end();
  kv_db_->data_map_.erase(key);
  return found;
}

// Returns the mapped Paxos log for given key & round.
// A new element will be constructed using its default constructor and inserted
// if key or round is not found.
PaxosLog KeyValueDataBase::GetPaxosLog(const std::string& key, int round) {
  std::shared_lock<std::shared_mutex> reader_lock(kv_db_->paxos_logs_mtx_);
  return kv_db_->paxos_logs_map_[key][round];
}

// Returns the latest Paxos round number for the given key.
// A new element will be constructed using its default constructor and
// inserted if key is not found.
// Returns 0 if round is not found for given key.
int KeyValueDataBase::GetLatestRound(const std::string& key) {
  std::shared_lock<std::shared_mutex> reader_lock(kv_db_->paxos_logs_mtx_);
  if (kv_db_->paxos_logs_map_[key].empty()) return 0;
  return kv_db_->paxos_logs_map_[key].rbegin()->first;
}
// Update the promised_id in paxos_logs_map_ for the given key and round.
void KeyValueDataBase::AddPaxosLog(const std::string& key, int round,
                                   int promised_id) {
  std::unique_lock<std::shared_mutex> writer_lock(kv_db_->paxos_logs_mtx_);
  (kv_db_->paxos_logs_map_[key])[round].promised_id = promised_id;
}

// Update the acceptance info in paxos_logs_map_ for the given key and round.
void KeyValueDataBase::AddPaxosLog(const std::string& key, int round,
                                   int accepted_id, OperationType accepted_type,
                                   string accepted_value) {
  std::unique_lock<std::shared_mutex> writer_lock(kv_db_->paxos_logs_mtx_);
  (kv_db_->paxos_logs_map_[key])[round].accepted_id = accepted_id;
  (kv_db_->paxos_logs_map_[key])[round].accepted_type = accepted_type;
  (kv_db_->paxos_logs_map_[key])[round].accepted_value = accepted_value;
}

}  // namespace keyvaluestore
