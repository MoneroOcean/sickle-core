#include "async-worker.h"

class Simple: public AsyncWorker {
  public:
    Simple(Nan::Callback* const data, Nan::Callback* const complete, Nan::Callback* const error_callback, const v8::Local<v8::Object>& options)
      : AsyncWorker(data, complete, error_callback) {
    }
     
    void Execute(const AsyncProgressQueueWorker<char>::ExecutionProgress& progress) {
      int i = 0;
      while (true) {
        std::deque<Message> messages;
        fromNode.readAll(messages);
        for (std::deque<Message>::const_iterator pi = messages.begin(); pi != messages.end(); ++ pi) {
            if (pi->name == "close") return;
            puts(pi->name.c_str());
            puts(pi->data.c_str());
        }
        if (++i % 1000000 == 0) {
          Message tosend("integer", std::to_string(i));
          sendToNode(progress, tosend);
        }
      }
    }
};

AsyncWorker* create_worker(Nan::Callback* const data, Nan::Callback* const complete, Nan::Callback* const error_callback, v8::Local<v8::Object>& options) {
  return new Simple(data, complete, error_callback, options);
}

NODE_MODULE(sickle_core, AsyncWorkerWrapper::Init)
