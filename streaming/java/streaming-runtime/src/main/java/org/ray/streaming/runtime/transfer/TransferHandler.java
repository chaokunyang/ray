package org.ray.streaming.runtime.transfer;

import org.ray.runtime.RayNativeRuntime;
import org.ray.runtime.functionmanager.FunctionDescriptor;
import org.ray.runtime.functionmanager.JavaFunctionDescriptor;
import org.ray.streaming.runtime.util.JniUtils;

public class TransferHandler {

  static {
    try {
      Class.forName(RayNativeRuntime.class.getName());
    } catch (ClassNotFoundException e) {
      throw new RuntimeException(e);
    }
    JniUtils.loadLibrary("streaming_java");
  }

  private long writerClientNative;
  private long readerClientNative;

  public TransferHandler(long coreWorkerNative,
                         JavaFunctionDescriptor writerAsyncFunc,
                         JavaFunctionDescriptor writerSyncFunc,
                         JavaFunctionDescriptor readerAsyncFunc,
                         JavaFunctionDescriptor readerSyncFunc) {
    writerClientNative = createWriterClientNative(
        coreWorkerNative, writerAsyncFunc, writerSyncFunc);
    readerClientNative = createReaderClientNative(
        coreWorkerNative, readerAsyncFunc, readerSyncFunc);
  }

  public void onWriterMessage(byte[] buffer) {
    handleWriterMessageNative(writerClientNative, buffer);
  }

  public byte[] onWriterMessageSync(byte[] buffer) {
    return handleWriterMessageSyncNative(writerClientNative, buffer);
  }

  public void onReaderMessage(byte[] buffer) {
    handleReaderMessageNative(readerClientNative, buffer);
  }

  public byte[] onReaderMessageSync(byte[] buffer) {
    return handleReaderMessageSyncNative(readerClientNative, buffer);
  }

  private native long createWriterClientNative(
      long coreWorkerNative,
      FunctionDescriptor async_func,
      FunctionDescriptor sync_func);

  private native long createReaderClientNative(
      long coreWorkerNative,
      FunctionDescriptor async_func,
      FunctionDescriptor sync_func);

  private native void handleWriterMessageNative(long handler, byte[] buffer);

  private native byte[] handleWriterMessageSyncNative(long handler, byte[] buffer);

  private native void handleReaderMessageNative(long handler, byte[] buffer);

  private native byte[] handleReaderMessageSyncNative(long handler, byte[] buffer);
}
