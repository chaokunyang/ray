package org.ray.streaming.api.stream;


import org.ray.streaming.api.Language;
import org.ray.streaming.api.context.StreamingContext;
import org.ray.streaming.api.function.impl.FilterFunction;
import org.ray.streaming.api.function.impl.FlatMapFunction;
import org.ray.streaming.api.function.impl.KeyFunction;
import org.ray.streaming.api.function.impl.MapFunction;
import org.ray.streaming.api.function.impl.SinkFunction;
import org.ray.streaming.api.partition.Partition;
import org.ray.streaming.api.partition.impl.BroadcastPartition;
import org.ray.streaming.operator.StreamOperator;
import org.ray.streaming.operator.impl.FilterOperator;
import org.ray.streaming.operator.impl.FlatMapOperator;
import org.ray.streaming.operator.impl.KeyByOperator;
import org.ray.streaming.operator.impl.MapOperator;
import org.ray.streaming.operator.impl.SinkOperator;
import org.ray.streaming.python.stream.PythonDataStream;

/**
 * Represents a stream of data.
 * <p>
 * This class defines all the streaming operations.
 *
 * @param <T> Type of data in the stream.
 */
public class DataStream<T> extends Stream<DataStream<T>, T> {

  public DataStream(StreamingContext streamingContext, StreamOperator streamOperator) {
    super(streamingContext, streamOperator);
  }

  public <R> DataStream(DataStream<R> input, StreamOperator streamOperator) {
    super(input, streamOperator);
  }

  /**
   * Create a java stream that reference passed python stream.
   * Changes in new stream will be reflected in referenced stream and vice versa
   */
  public DataStream(PythonDataStream referencedStream) {
    super(referencedStream);
  }

  /**
   * Apply a map function to this stream.
   *
   * @param mapFunction The map function.
   * @param <R>         Type of data returned by the map function.
   * @return A new DataStream.
   */
  public <R> DataStream<R> map(MapFunction<T, R> mapFunction) {
    return new DataStream<>(this, new MapOperator<>(mapFunction));
  }

  /**
   * Apply a flat-map function to this stream.
   *
   * @param flatMapFunction The FlatMapFunction
   * @param <R>             Type of data returned by the flatmap function.
   * @return A new DataStream
   */
  public <R> DataStream<R> flatMap(FlatMapFunction<T, R> flatMapFunction) {
    return new DataStream<>(this, new FlatMapOperator<>(flatMapFunction));
  }

  public DataStream<T> filter(FilterFunction<T> filterFunction) {
    return new DataStream<>(this, new FilterOperator<>(filterFunction));
  }

  /**
   * Apply a union transformation to this stream, with another stream.
   *
   * @param other Another stream.
   * @return A new UnionStream.
   */
  public UnionStream<T> union(DataStream<T> other) {
    return new UnionStream<>(this, null, other);
  }

  /**
   * Apply a join transformation to this stream, with another stream.
   *
   * @param other Another stream.
   * @param <O>   The type of the other stream data.
   * @param <R>   The type of the data in the joined stream.
   * @return A new JoinStream.
   */
  public <O, R> JoinStream<T, O, R> join(DataStream<O> other) {
    return new JoinStream<>(this, other);
  }

  public <R> DataStream<R> process() {
    // TODO(zhenxuanpan): Need to add processFunction.
    return new DataStream(this, null);
  }

  /**
   * Apply a sink function and get a StreamSink.
   *
   * @param sinkFunction The sink function.
   * @return A new StreamSink.
   */
  public DataStreamSink<T> sink(SinkFunction<T> sinkFunction) {
    return new DataStreamSink<>(this, new SinkOperator<>(sinkFunction));
  }

  /**
   * Apply a key-by function to this stream.
   *
   * @param keyFunction the key function.
   * @param <K>         The type of the key.
   * @return A new KeyDataStream.
   */
  public <K> KeyDataStream<K, T> keyBy(KeyFunction<T, K> keyFunction) {
    checkPartitionCall();
    return new KeyDataStream<>(this, new KeyByOperator<>(keyFunction));
  }

  /**
   * Apply broadcast to this stream.
   *
   * @return This stream.
   */
  public DataStream<T> broadcast() {
    checkPartitionCall();
    super.setPartition(new BroadcastPartition<>());
    return this;
  }

  /**
   * Apply a partition to this stream.
   *
   * @param partition The partitioning strategy.
   * @return This stream.
   */
  public DataStream<T> partitionBy(Partition<T> partition) {
    checkPartitionCall();
    setPartition(partition);
    return this;
  }

  /**
   * If parent stream is a python stream, we can't call partition related methods
   * in the java stream.
   */
  private void checkPartitionCall() {
    if (getInputStream() != null && getInputStream().getLanguage() == Language.PYTHON) {
      throw new RuntimeException("Partition related methods can't be called on a " +
          "java stream if parent stream is a python stream.");
    }
  }

  /**
   * Convert this stream as a python stream.
   * The converted stream and this stream are the same logical stream, which has same stream id.
   * Changes in converted stream will be reflected in this stream and vice versa.
   */
  public PythonDataStream asPython() {
    return new PythonDataStream(this);
  }

  @Override
  public Language getLanguage() {
    return Language.JAVA;
  }
}
