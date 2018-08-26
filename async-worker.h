#pragma once

#include <iostream>
#include <string>
#include <algorithm>
#include <iterator>
#include <thread>
#include <deque>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <nan.h>

template<typename Data> class PCQueue {

    private:

        std::mutex mu;
        std::condition_variable cond;
        std::deque<Data> buffer_;

    public:

        void write(Data data) {
            while (true) {
                std::unique_lock<std::mutex> locker(mu);
                buffer_.push_back(data);
                locker.unlock();
                cond.notify_all();
                return;
            }
        }

        Data read() {
            while (true)
            {
                std::unique_lock<std::mutex> locker(mu);
                cond.wait(locker, [this]() {
                    return buffer_.size() > 0;
                });
                Data back = buffer_.front();
                buffer_.pop_front();
                locker.unlock();
                cond.notify_all();
                return back;
            }
        }

        void readAll(std::deque<Data> & target) {
            std::unique_lock<std::mutex> locker(mu);
            std::copy(buffer_.begin(), buffer_.end(), std::back_inserter(target));
            buffer_.clear();
            locker.unlock();
        }
};

struct Message {
    std::string name;
    std::string data;
    Message(std::string name, std::string data) : name(name), data(data){}
};

class AsyncWorker: public Nan::AsyncProgressQueueWorker<char> {

    private:

        void drainQueue() {
            Nan::HandleScope scope;
            // drain the queue - since we might only get called once for many writes
            std::deque<Message> contents;
            toNode.readAll(contents);

            for (Message& msg : contents) {
                v8::Local<v8::Value> argv[] = {
                    Nan::New<v8::String>(msg.name.c_str()).ToLocalChecked(), 
                    Nan::New<v8::String>(msg.data.c_str()).ToLocalChecked()
                };
                progress->Call(2, argv);
            }
        }

    protected:
  
        void sendToNode(const AsyncProgressQueueWorker<char>::ExecutionProgress& progress, const Message& msg) {
            toNode.write(msg);
            progress.Send(reinterpret_cast<const char*>(&toNode), sizeof(toNode));
        }
  
        Nan::Callback* progress;
        Nan::Callback* error_callback;
        PCQueue<Message> toNode;

    public:

        AsyncWorker(Nan::Callback* progress, Nan::Callback* callback, Nan::Callback* error_callback)
            : Nan::AsyncProgressQueueWorker<char>(callback, "sickle-core::AsyncWorker"), progress(progress), error_callback(error_callback)
            {
            }
      
        ~AsyncWorker() {
            delete progress;
            delete error_callback;
        }
      
        void HandleErrorCallback() {
            Nan::HandleScope scope;
            v8::Local<v8::Value> argv[] = {
                v8::Exception::Error(Nan::New<v8::String>(ErrorMessage()).ToLocalChecked())
            };
            error_callback->Call(1, argv);
        }
      
        void HandleOKCallback() {
            drainQueue();
            callback->Call(0, NULL);
        }
      
        void HandleProgressCallback(const char* data, size_t size) {
            drainQueue();
        }
      
        PCQueue<Message> fromNode;
};

AsyncWorker* create_worker(Nan::Callback*, Nan::Callback*, Nan::Callback*, v8::Local<v8::Object>&);

class StreamWorkerWrapper: public Nan::ObjectWrap {

    private:

        explicit StreamWorkerWrapper(AsyncWorker * worker) : _worker(worker) {}
        ~StreamWorkerWrapper() {}

        static NAN_METHOD(New) {
            if (info.IsConstructCall()) {
                Nan::Callback *data_callback       = new Nan::Callback(info[0].As<v8::Function>());
                Nan::Callback *complete_callback   = new Nan::Callback(info[1].As<v8::Function>());
                Nan::Callback *error_callback      = new Nan::Callback(info[2].As<v8::Function>());
                v8::Local<v8::Object> options = info[3].As<v8::Object>();

                StreamWorkerWrapper *obj = new StreamWorkerWrapper(create_worker(data_callback, complete_callback, error_callback, options));
      
                obj->Wrap(info.This());
                info.GetReturnValue().Set(info.This());

                // start the worker
                AsyncQueueWorker(obj->_worker);

            } else {
                const int argc = 3;
                v8::Local<v8::Value> argv[argc] = {info[0], info[1], info[2]};
                v8::Local<v8::Function> cons = Nan::New(constructor());
                v8::Local<v8::Object> instance = Nan::NewInstance(cons, argc, argv).ToLocalChecked();
                info.GetReturnValue().Set(instance);
            }
        }

        static NAN_METHOD(sendToCpp) {
            v8::String::Utf8Value name(info[0]->ToString());
            v8::String::Utf8Value data(info[1]->ToString());
            StreamWorkerWrapper* obj = Nan::ObjectWrap::Unwrap<StreamWorkerWrapper>(info.Holder());
            obj->_worker->fromNode.write(Message(*name, *data));
        }

        static inline Nan::Persistent<v8::Function> & constructor() {
            static Nan::Persistent<v8::Function> my_constructor;
            return my_constructor;
        }

        AsyncWorker * _worker;

    public:

        static NAN_MODULE_INIT(Init) {
            v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
            tpl->SetClassName(Nan::New("AsyncWorker").ToLocalChecked());
            tpl->InstanceTemplate()->SetInternalFieldCount(2);

            SetPrototypeMethod(tpl, "sendToCpp", sendToCpp);
    
            constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
            Nan::Set(target, Nan::New("AsyncWorker").ToLocalChecked(),
            Nan::GetFunction(tpl).ToLocalChecked());
        }
};
