#include "queue_service.h"
#include "utils.h"
#include "util/streaming_util.h"

namespace ray {
namespace streaming {

constexpr uint64_t COMMON_SYNC_CALL_TIMEOUTT_MS = 5*1000;

std::shared_ptr<UpstreamService> UpstreamService::upstream_service_ = nullptr;
std::shared_ptr<DownstreamService> DownstreamService::downstream_service_ = nullptr;

RayFunction UpstreamService::PEER_SYNC_FUNCTION;
RayFunction UpstreamService::PEER_ASYNC_FUNCTION;
RayFunction DownstreamService::PEER_SYNC_FUNCTION;
RayFunction DownstreamService::PEER_ASYNC_FUNCTION;

std::shared_ptr<Message> QueueService::ParseMessage(
    std::shared_ptr<LocalMemoryBuffer> buffer) {
  uint8_t *bytes = buffer->Data();
  uint8_t *p_cur = bytes;
  uint32_t *magic_num = (uint32_t *)p_cur;
  STREAMING_CHECK(*magic_num == Message::MagicNum) << *magic_num << " " << Message::MagicNum;

  p_cur += sizeof(Message::MagicNum);
  queue::protobuf::StreamingQueueMessageType *type = (queue::protobuf::StreamingQueueMessageType *)p_cur;

  std::shared_ptr<Message> message = nullptr;
  switch (*type) {
  case queue::protobuf::StreamingQueueMessageType::StreamingQueueNotificationMsgType:
    message = NotificationMessage::FromBytes(bytes);
    break;
  case queue::protobuf::StreamingQueueMessageType::StreamingQueueDataMsgType:
    message = DataMessage::FromBytes(bytes);
    break;
  case queue::protobuf::StreamingQueueMessageType::StreamingQueueCheckMsgType:
    message = CheckMessage::FromBytes(bytes);
    break;
  case queue::protobuf::StreamingQueueMessageType::StreamingQueueCheckRspMsgType:
    message = CheckRspMessage::FromBytes(bytes);
    break;
  default:
    STREAMING_CHECK(false) << "nonsupport message type: "
                           << queue::protobuf::StreamingQueueMessageType_Name(*type);
    break;
  }

  return message;
}

// Message will be handled in io_service queue thread.
void QueueService::DispatchMessage(std::shared_ptr<LocalMemoryBuffer> buffer) {
  queue_service_.post(
      boost::bind(&QueueService::DispatchMessageInternal, this, buffer, nullptr));
}

std::shared_ptr<LocalMemoryBuffer> QueueService::DispatchMessageSync(
    std::shared_ptr<LocalMemoryBuffer> buffer) {
  std::shared_ptr<LocalMemoryBuffer> result = nullptr;
  std::shared_ptr<PromiseWrapper> promise = std::make_shared<PromiseWrapper>();
  queue_service_.post(
      boost::bind(&QueueService::DispatchMessageInternal, this, buffer,
                  [&promise, &result](std::shared_ptr<LocalMemoryBuffer> rst) {
                    result = rst;
                    promise->Notify(ray::Status::OK());
                  }));
  Status st = promise->Wait();
  STREAMING_CHECK(st.ok());

  return result;
}

std::shared_ptr<Transport> QueueService::GetOutTransport(const ObjectID &queue_id) {
  auto it = out_transports_.find(queue_id);
  if (it == out_transports_.end()) return nullptr;

  return it->second;
}

void QueueService::AddPeerActor(const ObjectID &queue_id, const ActorID &actor_id) {
  actors_.emplace(queue_id, actor_id);
  out_transports_.emplace(queue_id, std::make_shared<ray::streaming::Transport>(core_worker_, actor_id));
}

ActorID QueueService::GetPeerActor(const ObjectID &queue_id) {
  auto it = actors_.find(queue_id);
  STREAMING_CHECK(it != actors_.end());
  return it->second;
}

void QueueService::Release() {
    actors_.clear();
    out_transports_.clear();
}

void QueueService::Init() {
  queue_thread_ = std::thread(&QueueService::QueueThreadCallback, this);
}

void QueueService::Stop() {
  STREAMING_LOG(INFO) << "QueueService Stop.";
  queue_service_.stop();
  if (queue_thread_.joinable()) {
    queue_thread_.join();
  }
}

std::shared_ptr<UpstreamService> UpstreamService::GetService(CoreWorker* core_worker, const ActorID &actor_id) {
  if (nullptr == upstream_service_) {
    upstream_service_ = std::make_shared<UpstreamService>(core_worker, actor_id);
  }
  return upstream_service_;
}

std::shared_ptr<WriterQueue> UpstreamService::CreateUpstreamQueue(const ObjectID &queue_id,
                                                         const ActorID &actor_id,
                                                         const ActorID &peer_actor_id,
                                                         uint64_t size) {
  STREAMING_LOG(INFO) << "CreateUpstreamQueue: " << queue_id
                      << " " << actor_id << "->" << peer_actor_id;
  std::shared_ptr<WriterQueue> queue = GetUpQueue(queue_id);
  if (queue != nullptr) {
    STREAMING_LOG(WARNING) << "Duplicate to create up queue." << queue_id;
    return queue;
  }

  queue = std::unique_ptr<streaming::WriterQueue>(new streaming::WriterQueue(
            queue_id, actor_id, peer_actor_id, size, GetOutTransport(queue_id)));
  upstream_queues_[queue_id] = queue;

  return queue;
}

bool UpstreamService::UpstreamQueueExists(const ObjectID &queue_id) {
  return nullptr != GetUpQueue(queue_id);
}

std::shared_ptr<streaming::WriterQueue> UpstreamService::GetUpQueue(
    const ObjectID &queue_id) {
  auto it = upstream_queues_.find(queue_id);
  if (it == upstream_queues_.end()) return nullptr;

  return it->second;
}

bool UpstreamService::CheckQueueSync(const ObjectID &queue_id, QueueType type) {
  ActorID peer_actor_id = GetPeerActor(queue_id);
  STREAMING_LOG(INFO) << "CheckQueueSync queue_id: " << queue_id
                      << " peer_actor_id: " << peer_actor_id;

  CheckMessage msg(actor_id_, peer_actor_id, queue_id);
  std::unique_ptr<LocalMemoryBuffer> buffer = msg.ToBytes();

  auto transport_it = GetOutTransport(queue_id);
  STREAMING_CHECK(transport_it != nullptr);
  std::shared_ptr<LocalMemoryBuffer> result_buffer =
      transport_it->SendForResultWithRetry(std::move(buffer), DownstreamService::PEER_SYNC_FUNCTION, 10, COMMON_SYNC_CALL_TIMEOUTT_MS);
  if (result_buffer == nullptr) {
    return false;
  }

  std::shared_ptr<Message> result_msg = ParseMessage(result_buffer);
  STREAMING_CHECK(result_msg->Type() ==
                  queue::protobuf::StreamingQueueMessageType::StreamingQueueCheckRspMsgType);
  std::shared_ptr<CheckRspMessage> check_rsp_msg =
      std::dynamic_pointer_cast<CheckRspMessage>(result_msg);
  STREAMING_LOG(INFO) << "CheckQueueSync return queue_id: " << check_rsp_msg->QueueId();
  STREAMING_CHECK(check_rsp_msg->PeerActorId() == actor_id_);

  return queue::protobuf::StreamingQueueError::OK == check_rsp_msg->Error();
}

void UpstreamService::WaitQueues(const std::vector<ObjectID> &queue_ids, int64_t timeout_ms,
                              std::vector<ObjectID> &failed_queues, QueueType type) {
  failed_queues.insert(failed_queues.begin(), queue_ids.begin(), queue_ids.end());
  uint64_t start_time_us = current_time_ms();
  uint64_t current_time_us = start_time_us;
  while (!failed_queues.empty() && current_time_us < start_time_us + timeout_ms * 1000) {
    for (auto it = failed_queues.begin(); it != failed_queues.end();) {
      if (CheckQueueSync(*it, type)) {
        STREAMING_LOG(INFO) << "Check queue: " << *it << " return, ready.";
        it = failed_queues.erase(it);
      } else {
        STREAMING_LOG(INFO) << "Check queue: " << *it << " return, not ready.";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        it++;
      }
    }
    current_time_us = current_time_ms();
  }
}

void UpstreamService::DispatchMessageInternal(
    std::shared_ptr<LocalMemoryBuffer> buffer,
    std::function<void(std::shared_ptr<LocalMemoryBuffer>)> callback) {
  std::shared_ptr<Message> msg = ParseMessage(buffer);
  STREAMING_LOG(DEBUG) << "QueueService::DispatchMessageInternal: "
                       << " qid: " << msg->QueueId() << " actorid " << msg->ActorId()
                       << " peer actorid: " << msg->PeerActorId()
                       << " type: " << queue::protobuf::StreamingQueueMessageType_Name(msg->Type());

  if (msg->Type() == queue::protobuf::StreamingQueueMessageType::StreamingQueueNotificationMsgType) {
    OnNotify(std::dynamic_pointer_cast<NotificationMessage>(msg));
  } else if (msg->Type() == queue::protobuf::StreamingQueueMessageType::StreamingQueueCheckRspMsgType) {
    STREAMING_CHECK(false) << "Should not receive StreamingQueueCheckRspMsg";
  } else {
    STREAMING_CHECK(false) << "message type should be added: "
                           << queue::protobuf::StreamingQueueMessageType_Name(msg->Type());
  }
}

void UpstreamService::OnNotify(std::shared_ptr<NotificationMessage> notify_msg) {
  auto queue = GetUpQueue(notify_msg->QueueId());
    if (queue == nullptr) {
        STREAMING_LOG(WARNING) << "Can not find queue for " << queue::protobuf::StreamingQueueMessageType_Name(notify_msg->Type())
                             << ", maybe queue has been destroyed, ignore it."
                             << " seq id: " << notify_msg->SeqId();
      return;
    }
    queue->OnNotify(notify_msg);
}

void UpstreamService::ReleaseAllUpQueues() {
  STREAMING_LOG(INFO) << "ReleaseAllUpQueues";
  upstream_queues_.clear();
  Release();
}

std::shared_ptr<DownstreamService> DownstreamService::GetService(CoreWorker* core_worker, const ActorID &actor_id) {
  if (nullptr == downstream_service_) {
    downstream_service_ = std::make_shared<DownstreamService>(core_worker, actor_id);
  }
  return downstream_service_;
}

bool DownstreamService::DownstreamQueueExists(const ObjectID &queue_id) {
  return nullptr != GetDownQueue(queue_id);
}

std::shared_ptr<ReaderQueue> DownstreamService::CreateDownstreamQueue(const ObjectID &queue_id,
                                                           const ActorID &actor_id,
                                                           const ActorID &peer_actor_id) {
  STREAMING_LOG(INFO) << "CreateDownstreamQueue: " << queue_id
                      << " " << peer_actor_id << "->" << actor_id;
  auto it = downstream_queues_.find(queue_id);
  if (it != downstream_queues_.end()) {
    STREAMING_LOG(WARNING) << "Duplicate to create down queue!!!! " << queue_id;
    return it->second;
  }

  std::shared_ptr<streaming::ReaderQueue> queue =
      std::unique_ptr<streaming::ReaderQueue>(new streaming::ReaderQueue(
          queue_id, actor_id, peer_actor_id, GetOutTransport(queue_id)));
  downstream_queues_[queue_id] = queue;
  return queue;
}

std::shared_ptr<streaming::ReaderQueue> DownstreamService::GetDownQueue(
    const ObjectID &queue_id) {
  auto it = downstream_queues_.find(queue_id);
  if (it == downstream_queues_.end()) return nullptr;

  return it->second;
}

std::shared_ptr<LocalMemoryBuffer> DownstreamService::OnCheckQueue(
    std::shared_ptr<CheckMessage> check_msg) {
  queue::protobuf::StreamingQueueError err_code = queue::protobuf::StreamingQueueError::OK;

  auto down_queue = downstream_queues_.find(check_msg->QueueId());
  if (down_queue == downstream_queues_.end()) {
    STREAMING_LOG(WARNING) << "OnCheckQueue " << check_msg->QueueId() << " not found.";
    err_code = queue::protobuf::StreamingQueueError::QUEUE_NOT_EXIST;
  }

  CheckRspMessage msg(check_msg->PeerActorId(), check_msg->ActorId(),
                      check_msg->QueueId(), err_code);
  std::shared_ptr<LocalMemoryBuffer> buffer = msg.ToBytes();

  return buffer;
}

void DownstreamService::ReleaseAllDownQueues() {
  STREAMING_LOG(INFO) << "ReleaseAllDownQueues size: " << downstream_queues_.size();
  downstream_queues_.clear();
  Release();
}

void DownstreamService::DispatchMessageInternal(
    std::shared_ptr<LocalMemoryBuffer> buffer,
    std::function<void(std::shared_ptr<LocalMemoryBuffer>)> callback) {
  std::shared_ptr<Message> msg = ParseMessage(buffer);
  STREAMING_LOG(DEBUG) << "QueueService::DispatchMessageInternal: "
                       << " qid: " << msg->QueueId() << " actorid " << msg->ActorId()
                       << " peer actorid: " << msg->PeerActorId()
                       << " type: " << queue::protobuf::StreamingQueueMessageType_Name(msg->Type());

  if (msg->Type() == queue::protobuf::StreamingQueueMessageType::StreamingQueueDataMsgType) {
    OnData(std::dynamic_pointer_cast<DataMessage>(msg));
  } else if (msg->Type() == queue::protobuf::StreamingQueueMessageType::StreamingQueueCheckMsgType) {
    std::shared_ptr<LocalMemoryBuffer> check_result =
        this->OnCheckQueue(std::dynamic_pointer_cast<CheckMessage>(msg));
    if (callback != nullptr) {
      callback(check_result);
    }
  } else {
    STREAMING_CHECK(false) << "message type should be added: "
                           << queue::protobuf::StreamingQueueMessageType_Name(msg->Type());
  }
}

void DownstreamService::OnData(std::shared_ptr<DataMessage> msg) {
    auto queue = GetDownQueue(msg->QueueId());
    if (queue == nullptr) {
      STREAMING_LOG(WARNING) << "Can not find queue for " << queue::protobuf::StreamingQueueMessageType_Name(msg->Type())
                             << ", maybe queue has been destroyed, ignore it."
                             << " seq id: " << msg->SeqId();
      return;
    }

    QueueItem item(msg);
    queue->OnData(item);
}

}  // namespace streaming
}  // namespace ray