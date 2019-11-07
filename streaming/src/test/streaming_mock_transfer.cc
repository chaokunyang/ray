#include "gtest/gtest.h"
#include "streaming_reader.h"
#include "streaming_transfer.h"
#include "streaming_writer.h"

using namespace ray;
using namespace ray::streaming;

TEST(StreamingMockTransfer, mock_produce_consume) {
  std::shared_ptr<Config> transfer_config;
  MockProducer producer(transfer_config);
  MockConsumer consumer(transfer_config);
  ObjectID channel_id = ObjectID::FromRandom();
  ProducerChannelInfo producer_channel_info;
  producer_channel_info.channel_id = channel_id;
  producer_channel_info.current_seq_id = 0;
  ConsumerChannelInfo consumer_channel_info;
  consumer_channel_info.channel_id = channel_id;
  producer.CreateTransferChannel(producer_channel_info);
  uint8_t data[3] = {1, 2, 3};
  producer.ProduceItemToChannel(producer_channel_info, data, 3);
  uint8_t *data_consumed;
  uint32_t data_size_consumed;
  uint64_t data_seq_id;
  consumer.ConsumeItemFromChannel(consumer_channel_info, data_seq_id, data_consumed,
                                  data_size_consumed, -1);
  EXPECT_EQ(data_size_consumed, 3);
  EXPECT_EQ(data_seq_id, 1);
  EXPECT_EQ(std::memcmp(data_consumed, data, 3), 0);
  consumer.NotifyChannelConsumed(consumer_channel_info, 1);

  auto status = consumer.ConsumeItemFromChannel(consumer_channel_info, data_seq_id,
                                                data_consumed, data_size_consumed, -1);
  EXPECT_EQ(status, StreamingStatus::NoSuchItem);
}

class StreamingExchangeTest : public ::testing::Test {
 public:
  StreamingExchangeTest() {
    writer = std::make_shared<StreamingWriter>();
    reader = std::make_shared<StreamingReader>();
  }
  virtual ~StreamingExchangeTest() = default;
  void InitExchange(int channel_num = 1) {
    for (int i = 0; i < channel_num; ++i) {
      queue_vec.push_back(ObjectID::FromRandom());
    }
    std::vector<uint64_t> channel_id_vec(queue_vec.size(), 0);
    std::vector<uint64_t> queue_size_vec(queue_vec.size(), 10000);
    writer->Init(queue_vec, channel_id_vec, queue_size_vec);
    reader->Init(queue_vec, channel_id_vec, queue_size_vec, -1);
  }
  void DestroyExchange() {
    writer.reset();
    reader.reset();
  }

 protected:
  std::shared_ptr<StreamingWriter> writer;
  std::shared_ptr<StreamingReader> reader;
  std::vector<ObjectID> queue_vec;
};

TEST_F(StreamingExchangeTest, exchange_single_channel_test) {
  InitExchange();
  writer->Run();
  uint8_t data[4] = {1, 2, 3, 0xff};
  uint32_t data_size = 4;
  writer->WriteMessageToBufferRing(queue_vec[0], data, data_size);
  std::shared_ptr<StreamingReaderBundle> msg;
  reader->GetBundle(5000, msg);
  StreamingMessageBundlePtr bundle_ptr = StreamingMessageBundle::FromBytes(msg->data);
  auto &message_list = bundle_ptr->GetMessageList();
  auto &message = message_list.front();
  EXPECT_EQ(std::memcmp(message->RawData(), data, data_size), 0);
}

TEST_F(StreamingExchangeTest, exchange_multichannel_test) {
  int channel_num = 4;
  InitExchange(4);
  writer->Run();
  for (int i = 0; i < channel_num; ++i) {
    uint8_t data[4] = {1, 2, 3, (uint8_t)i};
    uint32_t data_size = 4;
    writer->WriteMessageToBufferRing(queue_vec[i], data, data_size);
    std::shared_ptr<StreamingReaderBundle> msg;
    reader->GetBundle(5000, msg);
    EXPECT_EQ(msg->from, queue_vec[i]);
    StreamingMessageBundlePtr bundle_ptr = StreamingMessageBundle::FromBytes(msg->data);
    auto &message_list = bundle_ptr->GetMessageList();
    auto &message = message_list.front();
    EXPECT_EQ(std::memcmp(message->RawData(), data, data_size), 0);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}