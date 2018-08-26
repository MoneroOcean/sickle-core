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
const unsigned min_blob_len = 76;
const unsigned max_blob_len = 96;

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

const std::map<std::string, cn_hash_fun> algo2fn[max_ways][2] = {
    { MAP_ALGO2FN(single, 0), MAP_ALGO2FN(single, 1) },
    { MAP_ALGO2FN(double, 0), MAP_ALGO2FN(double, 1) },
    { MAP_ALGO2FN(triple, 0), MAP_ALGO2FN(triple, 1) },
    { MAP_ALGO2FN(quad,   0), MAP_ALGO2FN(quad,   1) },
    { MAP_ALGO2FN(penta,  0), MAP_ALGO2FN(penta,  1) }
};

const std::map<std::string, unsigned> algo2mem = {
        { "cn",                     xmrig::CRYPTONIGHT_MEMORY }, 
        { "cryptonight",            xmrig::CRYPTONIGHT_MEMORY }, 
        { "cn/0",                   xmrig::CRYPTONIGHT_MEMORY }, 
        { "cryptonight/0",          xmrig::CRYPTONIGHT_MEMORY }, 
        { "cn/1",                   xmrig::CRYPTONIGHT_MEMORY }, 
        { "cryptonight/1",          xmrig::CRYPTONIGHT_MEMORY }, 
        { "cn/xtl",                 xmrig::CRYPTONIGHT_MEMORY }, 
        { "cryptonight/xtl",        xmrig::CRYPTONIGHT_MEMORY }, 
        { "cn/msr",                 xmrig::CRYPTONIGHT_MEMORY }, 
        { "cryptonight/msr",        xmrig::CRYPTONIGHT_MEMORY }, 
        { "cn/xao",                 xmrig::CRYPTONIGHT_MEMORY }, 
        { "cryptonight/xao",        xmrig::CRYPTONIGHT_MEMORY }, 
        { "cn/rto",                 xmrig::CRYPTONIGHT_MEMORY }, 
        { "cryptonight/rto",        xmrig::CRYPTONIGHT_MEMORY }, 
        { "cn-lite",                xmrig::CRYPTONIGHT_LITE_MEMORY }, 
        { "cryptonight-lite",       xmrig::CRYPTONIGHT_LITE_MEMORY }, 
        { "cn-lite/0",              xmrig::CRYPTONIGHT_LITE_MEMORY }, 
        { "cryptonight-lite/0",     xmrig::CRYPTONIGHT_LITE_MEMORY }, 
        { "cn-lite/1",              xmrig::CRYPTONIGHT_LITE_MEMORY }, 
        { "cryptonight-lite/1",     xmrig::CRYPTONIGHT_LITE_MEMORY }, 
        { "cn-heavy",               xmrig::CRYPTONIGHT_HEAVY_MEMORY }, 
        { "cryptonight-heavy",      xmrig::CRYPTONIGHT_HEAVY_MEMORY }, 
        { "cn-heavy/0",             xmrig::CRYPTONIGHT_HEAVY_MEMORY }, 
        { "cryptonight-heavy/0",    xmrig::CRYPTONIGHT_HEAVY_MEMORY }, 
        { "cn-heavy/xhv",           xmrig::CRYPTONIGHT_HEAVY_MEMORY }, 
        { "cryptonight-heavy/xhv",  xmrig::CRYPTONIGHT_HEAVY_MEMORY }, 
        { "cn-heavy/tube",          xmrig::CRYPTONIGHT_HEAVY_MEMORY }, 
        { "cryptonight-heavy/tube", xmrig::CRYPTONIGHT_HEAVY_MEMORY }
};

static inline uint32_t *nonce(uint8_t* const blob, const unsigned blob_len, const unsigned way) {
    return reinterpret_cast<uint32_t*>(blob + (way * blob_len) + 39);
}

inline static uint64_t *result(uint8_t* const hash, const unsigned way) {
    return reinterpret_cast<uint64_t*>(hash + (way * 32) + 24);
}

static inline unsigned char hf_hex2bin(const char c, bool& err) {
    if (c >= '0' && c <= '9')      return c - '0';
    else if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
    else if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
    err = true;
    return 0;
}

static bool fromHex(const char* in, unsigned int len, unsigned char* out) {
    bool error = false;
    for (unsigned int i = 0; i < len; ++i, ++out, in += 2) {
        *out = (hf_hex2bin(*in, error) << 4) | hf_hex2bin(*(in + 1), error);
        if (error) return false;
    }
    return true;
}

class Simple: public AsyncWorker {

    private:

        void send_error(const AsyncProgressQueueWorker<char>::ExecutionProgress& progress, const char* const sz) {
            MessageValues values;
            values["message"] = "Bad blob hex";
            sendToNode(progress, Message("error", values));
        }

    public:

        Simple(Nan::Callback* const data, Nan::Callback* const complete, Nan::Callback* const error_callback, const v8::Local<v8::Object>& options)
            : AsyncWorker(data, complete, error_callback) {
        }
         
        void Execute(const AsyncProgressQueueWorker<char>::ExecutionProgress& progress) {
            cn_hash_fun fn = nullptr;
            struct cryptonight_ctx ctx_mem[max_ways] = {};
            struct cryptonight_ctx* ctx[max_ways];
            unsigned ways = 0;
            unsigned mem = 0;
            uint8_t blob[max_ways * max_blob_len];
            unsigned blob_len = 0;
            uint8_t hash[max_ways * 32];
            uint64_t target = 0;

            for (unsigned i = 0; i != ways; ++i) ctx[i] = &ctx_mem[i];
            
            while (true) {
                std::deque<Message> messages;
                fromNode.readAll(messages);
                for (std::deque<Message>::const_iterator pi = messages.begin(); pi != messages.end(); ++ pi) {
                    if (pi->name == "job") {
                        const std::string algo           = pi->values.at("algo");
                        const unsigned is_soft_aes       = SOFT_AES; //pi->values.at("is_soft_aes") ? 0 : 1;
                        const unsigned new_ways          = atoi(pi->values.at("ways").c_str());
                        const char* const new_blob_hex   = pi->values.at("blob_hex").c_str();
                        const unsigned new_blob_len      = pi->values.at("blob_hex").size();
                        const std::string new_target_str = pi->values.at("target");

                        const std::map<std::string, cn_hash_fun>::const_iterator pi_fn = algo2fn[ways][is_soft_aes].find(algo);
                        if (pi_fn == algo2fn[ways][is_soft_aes].end()) {
                            send_error(progress, "Unsupported algo");
                            continue;
                        }
                        if ((new_blob_len & 1) || new_blob_len < min_blob_len || new_blob_len >= max_blob_len) {
                            send_error(progress, "Bad blob length");
                            continue;
                        }
                        uint8_t blob1[max_blob_len];
                        if (!fromHex(new_blob_hex, new_blob_len >> 1, blob1)) {
                            send_error(progress, "Bad blob hex");
                            continue;
                        }
                        if (new_target_str.size() <= 8) {
                            uint32_t tmp = 0;
                            if (!fromHex(new_target_str.c_str(), 8, reinterpret_cast<unsigned char*>(&tmp)) || tmp == 0) {
                                send_error(progress, "Bad target hex");
                                continue;
                            }
                            target = 0xFFFFFFFFFFFFFFFFULL / (0xFFFFFFFFULL / static_cast<uint64_t>(tmp));
                        } else if (new_target_str.size() <= 16) {
                            uint64_t tmp = 0;
                            if (!fromHex(new_target_str.c_str(), 16, reinterpret_cast<unsigned char*>(&tmp)) || tmp == 0) {
                                send_error(progress, "Bad target hex");
                                continue;
                            }
                            target = tmp;
                        } else {
                            send_error(progress, "Bad target hex");
                            continue;
                        }
                        blob_len = new_blob_len >> 1;
                        const unsigned new_mem = algo2mem.at(algo);
                        if (ways != new_ways || mem != new_mem) {
                            for (unsigned i = 0; i != ways; ++i) if (ctx[i]->memory) _mm_free(ctx[i]->memory); // free previous ways
                            ways = new_ways;
                            mem  = new_mem;
                            for (unsigned i = 0; i != ways; ++i) ctx[i]->memory = static_cast<uint8_t *>(_mm_malloc(mem, 4096));
                        }
                        for (unsigned i = 0; i != ways; ++i) {
                            memcpy(blob + blob_len*i, blob1, blob_len);
                            *nonce(blob, blob_len, i) = 0;
                        }
                        fn = pi_fn->second;
                 
                    } else if (pi->name == "pause") {
                        fn = nullptr;
                    } else if (pi->name == "close") {
                        for (unsigned i = 0; i != ways; ++i) if (ctx[i]->memory) _mm_free(ctx[i]->memory);
                        return;
                    }
                }
                if (fn) {
                    fn(blob, blob_len, hash, ctx);
                    for (unsigned i = 0; i != ways; ++i) {
                        if (*result(hash, i) < target) {
                            MessageValues values;
                            values["nonce"] = std::to_string(*nonce(blob, blob_len, i));
                            sendToNode(progress, Message("result", values));
                        }
                        ++ *nonce(blob, blob_len, i);
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
