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
            for (MessageValues::const_iterator pi2 = pi->values.begin(); pi2 != pi->values.end(); ++ pi2) {
                puts(pi2->first.c_str());
                puts(pi2->second.c_str());
            }
        }
        if (++i % 1000000 == 0) {
            MessageValues values;
            values["integer"] = std::to_string(i);
            sendToNode(progress, Message("result", values));
        }
      }
    }
};

AsyncWorker* create_worker(Nan::Callback* const data, Nan::Callback* const complete, Nan::Callback* const error_callback, v8::Local<v8::Object>& options) {
  return new Simple(data, complete, error_callback, options);
}

NODE_MODULE(sickle_core, AsyncWorkerWrapper::Init)
