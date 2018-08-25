#include "streaming-worker.h"

class Simple: public StreamingWorker {
  public:
    Simple(Nan::Callback* const data, Nan::Callback* const complete, Nan::Callback* const error_callback, const v8::Local<v8::Object>& options)
      : StreamingWorker(data, complete, error_callback) {
    }
     
    void Execute(const AsyncProgressQueueWorker<char>::ExecutionProgress& progress) {
      int i = 0;
      while (true) {
        std::deque<Message> messages;
        fromNode.readAll(messages);
        for (std::deque<Message>::const_iterator pi = messages.begin(); pi != messages.end(); ++ pi) {
            puts(pi->name.c_str());
            puts(pi->data.c_str());
        }
        if (++i % 100000 == 0) {
          Message tosend("integer", std::to_string(i));
          writeToNode(progress, tosend);
        }
      }
    }
};

StreamingWorker* create_worker(Nan::Callback* const data, Nan::Callback* const complete, Nan::Callback* const error_callback, v8::Local<v8::Object>& options) {
  return new Simple(data, complete, error_callback, options);
}

NODE_MODULE(sickle_core, StreamWorkerWrapper::Init)
