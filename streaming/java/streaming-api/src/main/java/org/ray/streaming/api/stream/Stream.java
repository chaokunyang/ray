package org.ray.streaming.api.stream;

import java.io.Serializable;
import java.util.Optional;
import org.ray.streaming.api.Language;
import org.ray.streaming.api.context.StreamingContext;
import org.ray.streaming.api.partition.Partition;
import org.ray.streaming.api.partition.impl.RoundRobinPartition;
import org.ray.streaming.operator.StreamOperator;
import org.ray.streaming.python.PythonOperator;
import org.ray.streaming.python.PythonPartition;
import org.ray.streaming.python.stream.PythonStream;

/**
 * Abstract base class of all stream types.
 *
 * @param <T> Type of the data in the stream.
 */
public abstract class Stream<T> implements Serializable {
  private int id;
  private int parallelism = 1;
  private StreamOperator operator;
  private Stream<T> inputStream;
  private StreamingContext streamingContext;
  private Partition<T> partition;

  private Stream<T> referencedStream;

  @SuppressWarnings("unchecked")
  public Stream(StreamingContext streamingContext, StreamOperator streamOperator) {
    this.streamingContext = streamingContext;
    this.operator = streamOperator;
    this.id = streamingContext.generateId();
    this.partition = selectPartition();
  }

  public Stream(Stream<T> inputStream, StreamOperator streamOperator) {
    this.inputStream = inputStream;
    this.parallelism = inputStream.getParallelism();
    this.streamingContext = this.inputStream.getStreamingContext();
    this.operator = streamOperator;
    this.id = streamingContext.generateId();
    this.partition = selectPartition();
  }

  /**
   * Create a reference of referenced stream
   */
  protected Stream(Stream<T> referencedStream) {
    this.referencedStream = referencedStream;
  }

  @SuppressWarnings("unchecked")
  private Partition<T> selectPartition() {
    switch (operator.getLanguage()) {
      case PYTHON:
        return PythonPartition.RoundRobinPartition;
      case JAVA:
        return new RoundRobinPartition<>();
      default:
        throw new UnsupportedOperationException(
          "Unsupported language " + operator.getLanguage());
    }
  }

  public Stream<T> getInputStream() {
    return referencedStream != null ? referencedStream.getInputStream() : inputStream;
  }

  public StreamOperator getOperator() {
    return referencedStream != null ? referencedStream.getOperator() : operator;
  }

  public StreamingContext getStreamingContext() {
    return referencedStream != null ? referencedStream.getStreamingContext() : streamingContext;
  }

  public int getParallelism() {
    return referencedStream != null ? referencedStream.getParallelism() : parallelism;
  }

  public Stream<T> setParallelism(int parallelism) {
    if (referencedStream != null) {
      referencedStream.setParallelism(parallelism);
    } else {
      this.parallelism = parallelism;
    }
    return this;
  }

  public int getId() {
    return referencedStream != null ? referencedStream.getId() : id;
  }

  public Partition<T> getPartition() {
    return referencedStream != null ? referencedStream.getPartition() : partition;
  }

  protected void setPartition(Partition<T> partition) {
    if (referencedStream != null) {
      referencedStream.setPartition(partition);
    } else {
      this.partition = partition;
    }
  }

  public abstract Language getLanguage();
}
