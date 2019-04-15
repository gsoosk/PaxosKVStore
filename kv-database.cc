#include "kv-database.h"

namespace keyvaluestore {

using grpc::Status;

KeyValueDataBase::ValueMutator::~ValueMutator() {
  std::unique_lock<std::shared_mutex> writer_lock(kv_db_->lock_map_mtx_);
  kv_db_->rev_lock_key_map_.erase(key_);
  kv_db_->lock_key_map_.erase(lock_key_);
}

// Return whether the value is found.
bool KeyValueDataBase::ValueMutator::GetValue(std::string* value) {
  std::shared_lock<std::shared_mutex> reader_lock(kv_db_->data_mtx_);
  auto iter = kv_db_->data_map_.find(key_);
  if (iter == kv_db_->data_map_.end()) return false;
  *value = iter->second;
  return true;
}

// Returns true if the value is overwritten, false if the key-val
// pair is newly added.
bool KeyValueDataBase::ValueMutator::SetValue(const std::string& val) {
  std::unique_lock<std::shared_mutex> writer_lock(kv_db_->data_mtx_);
  bool found = kv_db_->data_map_.find(key_) != kv_db_->data_map_.end();
  kv_db_->data_map_[key_] = val;
  return found;
}
// Returns true if the deletion actually happens, false if the key
// didn't exist.
bool KeyValueDataBase::ValueMutator::DeleteEntry() {
  std::unique_lock<std::shared_mutex> writer_lock(kv_db_->data_mtx_);
  bool found = kv_db_->data_map_.find(key_) != kv_db_->data_map_.end();
  kv_db_->data_map_.erase(key_);
  return found;
}

Status KeyValueDataBase::TryLock(const std::string& lock_key,
                                 const std::string& key) {
  std::unique_lock<std::shared_mutex> writer_lock(lock_map_mtx_);
  auto iter = rev_lock_key_map_.find(key);
  if (iter != rev_lock_key_map_.end()) {
    return Status(grpc::StatusCode::ALREADY_EXISTS,
                  iter->first + " already locked by " + iter->second);
  }
  rev_lock_key_map_[key] = lock_key;
  lock_key_map_[lock_key] = key;
  return Status::OK;
}

KeyValueDataBase::ValueMutator KeyValueDataBase::Unlock(
    const std::string& lock_key, const std::string& key) {
  return ValueMutator(key, lock_key, this);
}

}  // namespace keyvaluestore
