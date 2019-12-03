#include "transport.h"
#include "utils.h"

namespace ray {
namespace streaming {

static constexpr int TASK_OPTION_RETURN_NUM_0 = 0;
static constexpr int TASK_OPTION_RETURN_NUM_1 = 1;

void Transport::Send(std::unique_ptr<LocalMemoryBuffer> buffer, RayFunction &function) {
  STREAMING_LOG(INFO) << "Transport::Send buffer size: " << buffer->Size();
  std::unordered_map<std::string, double> resources;
  TaskOptions options{TASK_OPTION_RETURN_NUM_0, true, resources};

  char meta_data[3] = {'R', 'A', 'W'};
  std::shared_ptr<LocalMemoryBuffer> meta =
      std::make_shared<LocalMemoryBuffer>((uint8_t *)meta_data, 3, true);

  std::vector<TaskArg> args;
  if (function.GetLanguage() == Language::PYTHON) {
    // see _raylet.pyx for assci
    char ascii_meta[5] = {'A', 'S', 'C', 'I', 'I'};
     std::shared_ptr<LocalMemoryBuffer> ascii_meta_buf =
      std::make_shared<LocalMemoryBuffer>((uint8_t *)ascii_meta, 5, true);
    auto dummy = "__RAY_DUMMY__";
    std::shared_ptr<LocalMemoryBuffer> dummyBuffer =
        std::make_shared<LocalMemoryBuffer>((uint8_t *)dummy, 13, true);
    args.emplace_back(TaskArg::PassByValue(
        std::make_shared<RayObject>(std::move(dummyBuffer), ascii_meta_buf, true)));
  }
  args.emplace_back(
      TaskArg::PassByValue(std::make_shared<RayObject>(std::move(buffer), meta, true)));
  
  STREAMING_CHECK(core_worker_ != nullptr);
  std::vector<ObjectID> return_ids;
  std::vector<std::shared_ptr<RayObject>> results;
  ray::Status st = core_worker_->SubmitActorTask(peer_actor_id_, function, args,
                                                         options, &return_ids);
  if (!st.ok()) {
    STREAMING_LOG(ERROR) << "SubmitActorTask failed. " << st;
  }

  Status get_st = core_worker_->Get(return_ids, -1, &results);
  if (!get_st.ok()) {
    STREAMING_LOG(ERROR) << "Get fail.";
  }
}

std::shared_ptr<LocalMemoryBuffer> Transport::SendForResult(
    std::shared_ptr<LocalMemoryBuffer> buffer, RayFunction &function, int64_t timeout_ms) {
  std::unordered_map<std::string, double> resources;
  TaskOptions options{TASK_OPTION_RETURN_NUM_1, true, resources};

  char meta_data[3] = {'R', 'A', 'W'};
  std::shared_ptr<LocalMemoryBuffer> meta =
      std::make_shared<LocalMemoryBuffer>((uint8_t *)meta_data, 3, true);

  std::vector<TaskArg> args;
  if (function.GetLanguage() == Language::PYTHON) {
    char ascii_meta[5] = {'A', 'S', 'C', 'I', 'I'};
     std::shared_ptr<LocalMemoryBuffer> ascii_meta_buf =
      std::make_shared<LocalMemoryBuffer>((uint8_t *)ascii_meta, 5, true);
    auto dummy = "__RAY_DUMMY__";
    std::shared_ptr<LocalMemoryBuffer> dummyBuffer =
        std::make_shared<LocalMemoryBuffer>((uint8_t *)dummy, 13, true);
    args.emplace_back(TaskArg::PassByValue(
        std::make_shared<RayObject>(std::move(dummyBuffer), ascii_meta_buf, true)));
  }
  args.emplace_back(
      TaskArg::PassByValue(std::make_shared<RayObject>(buffer, meta, true)));

  STREAMING_CHECK(core_worker_ != nullptr);
  std::vector<ObjectID> return_ids;
  ray::Status st = core_worker_->SubmitActorTask(peer_actor_id_, function, args,
                                                 options, &return_ids);
  if (!st.ok()) {
    STREAMING_LOG(ERROR) << "SubmitActorTask fail.";
  }

  std::vector<std::shared_ptr<RayObject>> results;
  Status get_st = core_worker_->Get(return_ids, timeout_ms, &results);
  if (!get_st.ok()) {
    STREAMING_LOG(ERROR) << "Get fail.";
    return nullptr;
  }
  STREAMING_CHECK(results.size() >= 1);
  if (results[0]->IsException()) {
    STREAMING_LOG(ERROR) << "peer actor may has exceptions, should retry.";
    return nullptr;
  }
  STREAMING_CHECK(results[0]->HasData());
  if (results[0]->GetData()->Size() == 4) {
    STREAMING_LOG(WARNING) << "peer actor may not ready yet, should retry.";
    return nullptr;
  }

  std::shared_ptr<Buffer> result_buffer = results[0]->GetData();
  std::shared_ptr<LocalMemoryBuffer> return_buffer = std::make_shared<LocalMemoryBuffer>(
      result_buffer->Data(), result_buffer->Size(), true);
  return return_buffer;
}

std::shared_ptr<LocalMemoryBuffer> Transport::SendForResultWithRetry(
    std::unique_ptr<LocalMemoryBuffer> buffer, RayFunction &function, int retry_cnt, int64_t timeout_ms) {
  STREAMING_LOG(INFO) << "SendForResultWithRetry retry_cnt: " << retry_cnt
                      << " timeout_ms: " << timeout_ms << " function: " << function.GetFunctionDescriptor()[0];
  std::shared_ptr<LocalMemoryBuffer> buffer_shared = std::move(buffer);
  for (int cnt = 0; cnt < retry_cnt; cnt++) {
    auto result = SendForResult(buffer_shared, function, timeout_ms);
    if (result != nullptr) {
      return result;
    }
  }

  STREAMING_LOG(WARNING) << "SendForResultWithRetry fail after retry.";
  return nullptr;
}

}  // namespace streaming
}  // namespace ray