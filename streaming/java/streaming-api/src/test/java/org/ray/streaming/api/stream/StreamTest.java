package org.ray.streaming.api.stream;

import static org.testng.Assert.assertEquals;

import org.ray.streaming.api.context.StreamingContext;
import org.ray.streaming.operator.impl.MapOperator;
import org.ray.streaming.python.stream.PythonDataStream;
import org.ray.streaming.python.stream.PythonKeyDataStream;
import org.testng.annotations.Test;

@SuppressWarnings("unchecked")
public class StreamTest {

  @Test
  public void testReferencedDataStream() {
    DataStream dataStream = new DataStream(StreamingContext.buildContext(),
        new MapOperator(value -> null));
    PythonDataStream pythonDataStream = dataStream.asPython();
    DataStream javaStream = pythonDataStream.asJava();
    assertEquals(dataStream.getId(), pythonDataStream.getId());
    assertEquals(dataStream.getId(), javaStream.getId());
    javaStream.setParallelism(10);
    assertEquals(dataStream.getParallelism(), pythonDataStream.getParallelism());
    assertEquals(dataStream.getParallelism(), javaStream.getParallelism());
  }

  @Test
  public void testReferencedKeyDataStream() {
    DataStream dataStream = new DataStream(StreamingContext.buildContext(),
        new MapOperator(value -> null));
    KeyDataStream keyDataStream = dataStream.keyBy(value -> null);
    PythonKeyDataStream pythonKeyDataStream = keyDataStream.asPython();
    KeyDataStream javaKeyDataStream = pythonKeyDataStream.asJava();
    assertEquals(keyDataStream.getId(), pythonKeyDataStream.getId());
    assertEquals(keyDataStream.getId(), javaKeyDataStream.getId());
    javaKeyDataStream.setParallelism(10);
    assertEquals(keyDataStream.getParallelism(), pythonKeyDataStream.getParallelism());
    assertEquals(keyDataStream.getParallelism(), javaKeyDataStream.getParallelism());
  }
}