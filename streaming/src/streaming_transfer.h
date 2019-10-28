#ifndef RAY_STREAMING_TRANSFER_H
#define RAY_STREAMING_TRANSFER_H
#include "config.h"
#include "queue_interface.h"
#include "streaming.h"
#include "streaming_channel.h"

namespace ray {
namespace streaming {

class ProducerTransfer {
 public:
  ProducerTransfer(std::shared_ptr<Config> &transfer_config);
  virtual ~ProducerTransfer() = default;
  virtual StreamingStatus CreateTransferChannel(ProducerChannelInfo &channel_info) = 0;
  virtual StreamingStatus DestroyTransferChannel(ProducerChannelInfo &channel_info) = 0;
  virtual StreamingStatus ClearTransferCheckpoint(ProducerChannelInfo &channel_info,
                                                  uint64_t checkpoint_id,
                                                  uint64_t checkpoint_offset) = 0;
  virtual StreamingStatus RefreshChannelInfo(ProducerChannelInfo &channel_info) = 0;
  virtual StreamingStatus ProduceItemToChannel(ProducerChannelInfo &channel_info,
                                               uint8_t *data, uint32_t data_size) = 0;
  virtual StreamingStatus WaitChannelsReady(std::vector<ObjectID> &channels,
                                            uint32_t timeout,
                                            std::vector<ObjectID> &abnormal_channels) = 0;
  virtual StreamingStatus NotfiyChannelConsumed(ProducerChannelInfo &channel_info,
                                                uint64_t channel_offset) = 0;

 protected:
  std::shared_ptr<Config> transfer_config_;
};

class ConsumerTransfer {
 public:
  ConsumerTransfer(std::shared_ptr<Config> &transfer_config);
  virtual ~ConsumerTransfer() = default;
  virtual StreamingStatus CreateTransferChannel(ConsumerChannelInfo &channel_info) = 0;
  virtual StreamingStatus DestroyTransferChannel(ConsumerChannelInfo &channel_info) = 0;
  virtual StreamingStatus ClearTransferCheckpoint(ConsumerChannelInfo &channel_info,
                                                  uint64_t checkpoint_id,
                                                  uint64_t checkpoint_offset) = 0;
  virtual StreamingStatus RefreshChannelInfo(ConsumerChannelInfo &channel_info) = 0;
  virtual StreamingStatus ConsumeItemFromChannel(ConsumerChannelInfo &channel_info,
                                                 uint64_t &offset_id, uint8_t *&data,
                                                 uint32_t &data_size,
                                                 uint32_t timeout) = 0;
  virtual StreamingStatus NotfiyChannelConsumed(ConsumerChannelInfo &channel_info,
                                                uint64_t offset_id) = 0;
  virtual StreamingStatus WaitChannelsReady(std::vector<ObjectID> &channels,
                                            uint32_t timeout,
                                            std::vector<ObjectID> &abnormal_channels) = 0;

 protected:
  std::shared_ptr<Config> transfer_config_;
};

class PlasmaProducer : public ProducerTransfer {
 public:
  PlasmaProducer(std::shared_ptr<Config> &transfer_config);
  ~PlasmaProducer();
  StreamingStatus CreateTransferChannel(ProducerChannelInfo &channel_info);
  StreamingStatus DestroyTransferChannel(ProducerChannelInfo &channel_info);
  StreamingStatus ClearTransferCheckpoint(ProducerChannelInfo &channel_info,
                                          uint64_t checkpoint_id,
                                          uint64_t checkpoint_offset);
  StreamingStatus RefreshChannelInfo(ProducerChannelInfo &channel_info);
  StreamingStatus RefreshUnconsumedBytes(ProducerChannelInfo &channel_info);
  StreamingStatus ProduceItemToChannel(ProducerChannelInfo &channel_info, uint8_t *data,
                                       uint32_t data_size);
  StreamingStatus WaitChannelsReady(std::vector<ObjectID> &channels, uint32_t timeout,
                                    std::vector<ObjectID> &abnormal_channels);
  StreamingStatus NotfiyChannelConsumed(ProducerChannelInfo &channel_info,
                                        uint64_t offset_id);

 private:
  StreamingStatus CreateQueue(ProducerChannelInfo &channel_info);
  /*!
   * @brief While resuming from FO, it's better way to reuse its original items since old
   * queue object
   * may have been created in ray actor in some worker. Then getting last message id from
   * existing queue
   * item and set this last message id as new offset id value before this queue is
   * subscribed by upstream.
   * @param q_id : queue obejct id
   * @param plasma_store_path : plasma store path in local
   * @return last message id in queue
   */
  uint64_t FetchLastMessageIdFromQueue(const ObjectID &queue_id,
                                       uint64_t &last_queue_seq_id);

 private:
  std::shared_ptr<QueueWriterInterface> queue_writer_;
  std::string plasma_store_socket_path_;
  RayletClient *raylet_client_;
};

class PlasmaConsumer : public ConsumerTransfer {
 public:
  PlasmaConsumer(std::shared_ptr<Config> &transfer_config);
  ~PlasmaConsumer();
  StreamingStatus CreateTransferChannel(ConsumerChannelInfo &channel_info);
  StreamingStatus DestroyTransferChannel(ConsumerChannelInfo &channel_info);
  StreamingStatus ClearTransferCheckpoint(ConsumerChannelInfo &channel_info,
                                          uint64_t checkpoint_id,
                                          uint64_t checkpoint_offset);
  StreamingStatus RefreshChannelInfo(ConsumerChannelInfo &channel_info);
  StreamingStatus ConsumeItemFromChannel(ConsumerChannelInfo &channel_info,
                                         uint64_t &offset_id, uint8_t *&data,
                                         uint32_t &data_size, uint32_t timeout);
  StreamingStatus NotfiyChannelConsumed(ConsumerChannelInfo &channel_info,
                                        uint64_t offset_id);
  StreamingStatus WaitChannelsReady(std::vector<ObjectID> &channels, uint32_t timeout,
                                    std::vector<ObjectID> &abnormal_channels);

 private:
  std::shared_ptr<QueueReaderInterface> queue_reader_;
  std::string plasma_store_socket_path_;
  RayletClient *raylet_client_;
};
}  // namespace streaming
}  // namespace ray
#endif  // RAY_STREAMING_TRANSFER_H
