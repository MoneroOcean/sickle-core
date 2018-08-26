#include "async-worker.h"

#if defined(__ARM_ARCH)
#define XMRIG_ARM 1
#include "xmrig/crypto/CryptoNight_arm.h"
#else
#include "xmrig/crypto/CryptoNight_x86.h"
#endif

#if (defined(__AES__) && (__AES__ == 1)) || (defined(__ARM_FEATURE_CRYPTO) && (__ARM_FEATURE_CRYPTO == 1))
#define SOFT_AES false
#else
#warning Using software AES
#define SOFT_AES true
#endif

const unsigned max_ways = 5;

typedef void (*cn_hash_fun)(const uint8_t *blob, size_t size, uint8_t *output, cryptonight_ctx **ctx);

#define MAP_ALGO2FN(hash, soft_aes) {\
        { "cn",                     cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_1> },\
        { "cryptonight",            cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_1> },\
        { "cn/0",                   cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_0> },\
        { "cryptonight/0",          cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_0> },\
        { "cn/1",                   cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_1> },\
        { "cryptonight/1",          cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_1> },\
        { "cn/xtl",                 cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_XTL> },\
        { "cryptonight/xtl",        cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_XTL> },\
        { "cn/msr",                 cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_MSR> },\
        { "cryptonight/msr",        cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_MSR> },\
        { "cn/xao",                 cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_XAO> },\
        { "cryptonight/xao",        cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_XAO> },\
        { "cn/rto",                 cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_RTO> },\
        { "cryptonight/rto",        cryptonight_##hash##_hash<xmrig::CRYPTONIGHT,       soft_aes, xmrig::VARIANT_RTO> },\
        { "cn-lite",                cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_LITE,  soft_aes, xmrig::VARIANT_1> },\
        { "cryptonight-lite",       cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_LITE,  soft_aes, xmrig::VARIANT_1> },\
        { "cn-lite/0",              cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_LITE,  soft_aes, xmrig::VARIANT_0> },\
        { "cryptonight-lite/0",     cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_LITE,  soft_aes, xmrig::VARIANT_0> },\
        { "cn-lite/1",              cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_LITE,  soft_aes, xmrig::VARIANT_1> },\
        { "cryptonight-lite/1",     cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_LITE,  soft_aes, xmrig::VARIANT_1> },\
        { "cn-heavy",               cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_HEAVY, soft_aes, xmrig::VARIANT_0> },\
        { "cryptonight-heavy",      cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_HEAVY, soft_aes, xmrig::VARIANT_0> },\
        { "cn-heavy/0",             cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_HEAVY, soft_aes, xmrig::VARIANT_0> },\
        { "cryptonight-heavy/0",    cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_HEAVY, soft_aes, xmrig::VARIANT_0> },\
        { "cn-heavy/xhv",           cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_HEAVY, soft_aes, xmrig::VARIANT_XHV> },\
        { "cryptonight-heavy/xhv",  cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_HEAVY, soft_aes, xmrig::VARIANT_XHV> },\
        { "cn-heavy/tube",          cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_HEAVY, soft_aes, xmrig::VARIANT_TUBE> },\
        { "cryptonight-heavy/tube", cryptonight_##hash##_hash<xmrig::CRYPTONIGHT_HEAVY, soft_aes, xmrig::VARIANT_TUBE> }\
    }

std::map<std::string, cn_hash_fun> algo2fn[max_ways][2] = {
    { MAP_ALGO2FN(single, 0), MAP_ALGO2FN(single, 1) },
    { MAP_ALGO2FN(double, 0), MAP_ALGO2FN(double, 1) },
    { MAP_ALGO2FN(triple, 0), MAP_ALGO2FN(triple, 1) },
    { MAP_ALGO2FN(quad,   0), MAP_ALGO2FN(quad,   1) },
    { MAP_ALGO2FN(penta,  0), MAP_ALGO2FN(penta,  1) }
};

class Simple: public AsyncWorker {

    private:

        inline uint32_t *nonce(uint8_t* const blob, const unsigned blob_len, const unsigned way) const {
            return reinterpret_cast<uint32_t*>(blob + (way * blob_len) + 39);
        }
    
        inline uint64_t *result(uint8_t* const hash, const unsigned way) const {
            return reinterpret_cast<uint64_t*>(hash + (way * 32) + 24);
        }
    
    public:

        Simple(Nan::Callback* const data, Nan::Callback* const complete, Nan::Callback* const error_callback, const v8::Local<v8::Object>& options)
            : AsyncWorker(data, complete, error_callback) {
        }
         
        void Execute(const AsyncProgressQueueWorker<char>::ExecutionProgress& progress) {
            cn_hash_fun fn = nullptr;
            struct cryptonight_ctx ctx[max_ways] = { 0 };
            unsigned ways = 0;
            unsigned mem = 0;
            uint8_t* blob = nullptr;
            unsigned blob_len = 0;
            uint8_t hash[max_way * 32] = { 0 };
            uint64_t target = 0;
            
            while (true) {
                std::deque<Message> messages;
                fromNode.readAll(messages);
                for (std::deque<Message>::const_iterator pi = messages.begin(); pi != messages.end(); ++ pi) {
                    if (pi->name == "job") {
                        const std::string algo      = pi->values["algo"];
                        const unsigned is_soft_aes  = SOFT_AES; //pi->values["is_soft_aes"] ? 0 : 1;
                        const unsigned new_ways     = atoi(pi->values["ways"].c_str());
                        const unsigned new_mem      = atoi(pi->values["mem"].c_str());
                        const unsigned new_blob_len = atoi(pi->values["blob_len"].c_str());
                        target = atoi(pi->values["target"].c_str());
                        uint8_t* const new_blob     = pi->values["blob"].c_str();
                        if (ways != new_ways || mem != new_mem) {
                            if (ways) for (int i = 0; != ways; ++i) if (ctx[i]->memory) _mm_free(ctx[i]->memory); // free previous ways
                            ways = new_ways;
                            mem  = new_mem;
                            for (int i = 0; != ways; ++i) ctx[i]->memory = static_cast<uint8_t *>(_mm_malloc(mem, 4096));
                        }
                        if (blob_len != new_blob_len) {
                            if (blob) free(blob); // free previous blob
                            blob_len = new_blob_len;
                            blob = static_cast<uint8_t*>(malloc(blob_len));
                        }
                        for (int i = 0; != ways; ++i) {
                            memcpy(blob + blob_len*i, new_blob, blob_len);
                            nonce(blob, blob_len, i) = 0;
                        }
                        fn = algo2fn[ways][is_soft_aes][algo];
                 
                    } else if (pi->name == "pause") {
                        fn = nullptr;
                    } else if (pi->name == "close") {
                        if (ways) for (int i = 0; != ways; ++i) if (ctx[i]->memory) _mm_free(ctx[i]->memory);
                        if (blob) free(blob);
                        return;
                    }
                }
                if (fn) {
                    fn(blob, blob_len, hash, ctx);
                    for (int i = 0; != ways; ++i) {
                        if (result(hash, i) < target) {
                            MessageValues values;
                            values["nonce"] = std::to_string(nonce(blob, blob_len, i));
                            sendToNode(progress, Message("result", values));
                        }
                        ++ nonce(blob, blob_len, i);
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }
        }
};

AsyncWorker* create_worker(Nan::Callback* const data, Nan::Callback* const complete, Nan::Callback* const error_callback, v8::Local<v8::Object>& options) {
    return new Simple(data, complete, error_callback, options);
}

NODE_MODULE(sickle_core, AsyncWorkerWrapper::Init)
