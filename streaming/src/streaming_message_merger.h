#ifndef RAY_STREAMING_MESSAGE_MERGER_H
#define RAY_STREAMING_MESSAGE_MERGER_H

#include <algorithm>
#include <memory>
#include <vector>

namespace ray {
namespace streaming {

template <class T, class C>

class StreamingMessageMerger {
 private:
  std::vector<T> merge_vec_;
  C comparator_;

 public:
  StreamingMessageMerger(C &comparator) : comparator_(comparator){};

  inline void push(T &&item) {
    merge_vec_.push_back(std::forward<T>(item));
    std::push_heap(merge_vec_.begin(), merge_vec_.end(), comparator_);
  }

  inline void push(const T &item) {
    merge_vec_.push_back(item);
    std::push_heap(merge_vec_.begin(), merge_vec_.end(), comparator_);
  }

  inline void pop() {
    STREAMING_CHECK(!isEmpty());
    std::pop_heap(merge_vec_.begin(), merge_vec_.end(), comparator_);
    merge_vec_.pop_back();
  }

  inline void makeHeap() {
    std::make_heap(merge_vec_.begin(), merge_vec_.end(), comparator_);
  }

  inline T &top() { return merge_vec_.front(); }

  inline uint32_t size() { return merge_vec_.size(); }

  inline bool isEmpty() { return merge_vec_.empty(); }

  std::vector<T> &getRawVector() { return merge_vec_; }
};
}  // namespace streaming
}  // namespace ray

#endif  // RAY_STREAMING_MESSAGE_MERGER_H
