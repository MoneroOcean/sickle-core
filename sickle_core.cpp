#include "streaming-worker.h"

class Simple : public StreamingWorker {
  public:
    Simple(Callback* const data, Callback* const complete, Callback* const error_callback, const v8::Local<v8::Object>& options)
      : StreamingWorker(data, complete, error_callback) {
    }
     
    void Execute (const AsyncProgressWorker::ExecutionProgress& progress) {
      for (int i = 0; i < 100; i++) {
        Message tosend("integer", std::to_string(i));
        writeToNode(progress, tosend);
      }
    }
};

StreamingWorker* create_worker(Callback* const data, Callback* const complete, Callback* const error_callback, v8::Local<v8::Object>& options) {
  return new Simple(data, complete, error_callback, options);
}

NODE_MODULE(sickle_core, StreamWorkerWrapper::Init)
