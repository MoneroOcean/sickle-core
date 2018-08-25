#include "streaming-worker.h"

class Simple : public StreamingWorker {
   public:
   Simple(Callback *data, Callback *complete, Callback *error_callback, v8::Local<v8::Object> & options) 
          : StreamingWorker(data, complete, error_callback){

        // nothing needs to be here - just make sure you call base constructor
        
        if (options->IsObject() ) {
          // extract any options you want to work with. The user of the addon must supply these from JavaScript
          // The options parameter is for your JavaScript code to pass in 
          // an options object.  You can use this for whatever you want.
        }
    }
     
    // You must match the call signature here, `Execute` is a virtual function
    // defined on Streaing Worker.
    void Execute (const AsyncProgressWorker::ExecutionProgress& progress) {

      // Just send 100 integers and stop
      for (int i = 0; i < 100; i++ ) {
        Message tosend("integer", std::to_string(i));
        writeToNode(progress, tosend);
      }
    }
};

StreamingWorker* create_worker(Callback *data, Callback *complete, Callback *error_callback, v8::Local<v8::Object> & options) {
  return new Simple(data, complete, error_callback, options);
}

NODE_MODULE(sickle_core, StreamWorkerWrapper::Init)
