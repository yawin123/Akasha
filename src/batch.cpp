#include "store_internal.hpp"

#include <format>
#include <string>

namespace akasha {

// ── BatchStruct ───────────────────────────────────────────────────────────────

BatchStruct::BatchStruct(const Store& store, std::string_view key_prefix) : store_(const_cast<Store&>(store)) {
    auto [d_id, sp] = parse_batch_prefix(key_prefix);
    dataset_id_ = std::string(d_id);
    std::string subkey_prefix_ = std::string(sp);

    key_stack_.push(subkey_prefix_);

    // Acquire sources_mutex (shared) + find source + acquire file lock (exclusive)
    {
        std::shared_lock<std::shared_mutex> sources_guard(store_.sources_mutex_);
        source_ = const_cast<Store::Source*>(store_.find_source(dataset_id_));
        lock();
        // sources_guard released here — fine, we hold the file_lock which
        // prevents concurrent writes. source_ pointer remains valid as long
        // as the file_lock is held (unload() also acquires file_lock).
    }
}
void BatchStruct::push_key(std::string_view relative_key) const {
    key_stack_.push(std::format("{}/{}", key_stack_.top(), relative_key));
}

void BatchStruct::pop_key() const {
    if(!key_stack_.empty()) key_stack_.pop();
}

void BatchStruct::lock() {
    if (!file_lock_ && source_ && source_->file_lock) {
        file_lock_ = std::unique_lock<std::shared_mutex>(*source_->file_lock);
        OnLockAcquired();
    }
}

void BatchStruct::unlock() {
    if (file_lock_) {
        file_lock_.unlock();
        OnLockReleased();
    }
}

bool BatchStruct::has(std::string_view key_path) const {
    if (!file_lock_ || source_ == nullptr) return false;
    const std::string full = std::format("{}/{}", key_stack_.top(), key_path);
    return store_.has_key_no_lock(source_, full);
}

bool BatchStruct::has_value() const {
    if (!file_lock_ || source_ == nullptr) return false;
    const std::string& cur = key_stack_.top();
    const std::string_view key = cur.empty() ? "__root__" : std::string_view(cur);
    return store_.get_bytes_no_lock(source_, key).has_value();
}

bool BatchStruct::has_keys() const {
    if (!file_lock_ || source_ == nullptr) return false;
    return store_.has_subkeys_no_lock(source_, key_stack_.top());
}

std::vector<std::string> BatchStruct::keys() const {
    if (!file_lock_ || source_ == nullptr) return {};
    return store_.get_subkeys_no_lock(source_, key_stack_.top());
}

// ── BatchWriter ───────────────────────────────────────────────────────────────

BatchWriter::BatchWriter(const Store& store, std::string_view key_prefix) : BatchStruct(store, key_prefix) {}

BatchWriter::~BatchWriter() noexcept {
    if(file_lock_ && !committed_) {
        // Best-effort flush
        (void)store_.flush_source(source_);
    }
    // unique_lock destructor releases the file lock automatically
}

Status BatchWriter::set_raw(std::string_view relative_key, const void* bytes, std::size_t size, detail::TypeTag tag) {
    if (!file_lock_ || committed_ || source_ == nullptr || !source_->dataset_map || !source_->storage)
        return Status::file_write_error;

    const std::string subkey = std::format("{}/{}", key_stack_.top(), relative_key);
    return store_.set_bytes_no_lock(source_, dataset_id_, subkey, bytes, size, tag);
}

Status BatchWriter::set_null(std::string_view relative_key) {
    return set_raw(relative_key, nullptr, 0, detail::TypeTag::null_type);
}

void BatchWriter::clear_children() {
    if (!file_lock_ || committed_ || source_ == nullptr) return;
    store_.clear_no_lock(source_, key_stack_.top());
}

Status BatchWriter::commit() {
    if (!file_lock_ || committed_ || source_ == nullptr)  return Status::file_write_error;

    const Status st = store_.flush_source(source_) ? Status::ok : Status::file_write_error;

    unlock();  // release lock after flush attempt
    return st;
}

void BatchWriter::OnLockAcquired() {
    committed_ = false;
}

void BatchWriter::OnLockReleased() {
    committed_ = true;
}

// ── BatchReader ────────────────────────────────────────────────────────────

BatchReader::BatchReader(const Store& store, std::string_view key_prefix) : BatchStruct(store, key_prefix) {}

std::optional<std::string_view> BatchReader::get_raw(std::string_view relative_key) const {
    if (!file_lock_ || source_ == nullptr || !source_->dataset_map || !source_->storage) return std::nullopt;

    const std::string subkey = std::format("{}/{}", key_stack_.top(), relative_key);
    return store_.get_bytes_no_lock(source_, subkey);
}

std::vector<std::string> BatchReader::get_children() const {
    auto s = get<std::string>(std::string_view("__children__"));
    if (!s || s->empty()) return {};
    std::vector<std::string> result;
    std::string_view sv = *s;
    while (!sv.empty()) {
        const auto nl = sv.find('\n');
        if (nl == std::string_view::npos) {
            result.emplace_back(sv);
            break;
        }
        result.emplace_back(sv.substr(0, nl));
        sv = sv.substr(nl + 1);
    }
    return result;
}

}  // namespace akasha
