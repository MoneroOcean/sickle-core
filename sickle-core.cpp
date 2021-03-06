#include "async-worker.h"
#include <chrono>

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
const unsigned hash_len = 32;

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

static inline uint32_t *p_nonce(uint8_t* const blob, const unsigned blob_len, const unsigned way) {
    return reinterpret_cast<uint32_t*>(blob + (way * blob_len) + 39);
}

inline static uint64_t *p_result(uint8_t* const hash, const unsigned way) {
    return reinterpret_cast<uint64_t*>(hash + (way * hash_len) + 24);
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
            values["message"] = sz;
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
            uint8_t hash[max_ways * hash_len];
            uint32_t nonce = 0;
            uint64_t target = 0;
            uint64_t timestamp = 0;
            uint64_t hash_count = 0;

            for (unsigned i = 0; i != max_ways; ++i) ctx[i] = &ctx_mem[i];
            
            while (true) {
                std::deque<Message> messages;
                fromNode.readAll(messages);
                for (std::deque<Message>::const_iterator pi = messages.begin(); pi != messages.end(); ++ pi) {
                    if (pi->name == "job") {
                        const std::string algo           = pi->values.at("algo");
                        const unsigned is_soft_aes       = atoi(pi->values.at("soft_aes").c_str()) ? 1 : 0;
                        const unsigned new_ways          = atoi(pi->values.at("ways").c_str());
                        const std::string new_blob_str   = pi->values.at("blob_hex");
                        const char* const new_blob_hex   = new_blob_str.c_str();
                        const unsigned new_blob_len2     = new_blob_str.size();
                        const unsigned new_blob_len      = new_blob_len2 >> 1;
                        const std::string new_target_str = pi->values.at("target");

                        const std::map<std::string, cn_hash_fun>::const_iterator pi_fn = algo2fn[new_ways-1][is_soft_aes].find(algo);
                        if (pi_fn == algo2fn[new_ways-1][is_soft_aes].end()) {
                            send_error(progress, "Unsupported algo");
                            continue;
                        }
                        if ((new_blob_len2 & 1) || new_blob_len < min_blob_len || new_blob_len >= max_blob_len) {
                            send_error(progress, "Bad blob length");
                            continue;
                        }
                        uint8_t blob1[max_blob_len];
                        if (!fromHex(new_blob_hex, new_blob_len, blob1)) {
                            send_error(progress, "Bad blob hex");
                            continue;
                        }
                        if (new_target_str.size() <= sizeof(uint32_t)*2) {
                            uint32_t tmp = 0;
                            char str[sizeof(uint32_t)*2 + 1] = "00000000";
                            memcpy(str, new_target_str.c_str(), new_target_str.size());
                            if (!fromHex(str, sizeof(uint32_t), reinterpret_cast<unsigned char*>(&tmp)) || tmp == 0) {
                                send_error(progress, "Bad target hex");
                                continue;
                            }
                            target = 0xFFFFFFFFFFFFFFFFULL / (0xFFFFFFFFULL / static_cast<uint64_t>(tmp));
                        } else if (new_target_str.size() <= sizeof(uint64_t)*2) {
                            uint64_t tmp = 0;
                            char str[sizeof(uint64_t)*2 + 1] = "0000000000000000";
                            memcpy(str, new_target_str.c_str(), new_target_str.size());
                            if (!fromHex(str, sizeof(uint64_t), reinterpret_cast<unsigned char*>(&tmp)) || tmp == 0) {
                                send_error(progress, "Bad target hex");
                                continue;
                            }
                            target = tmp;
                        } else {
                            send_error(progress, "Bad target hex");
                            continue;
                        }
                        blob_len = new_blob_len;
                        const unsigned new_mem = algo2mem.at(algo);
                        if (ways != new_ways || mem != new_mem) {
                            // free previous ways
                            for (unsigned i = 0; i != ways; ++i) if (ctx[i]->memory) {
                                _mm_free(ctx[i]->memory);
                                ctx[i]->memory = nullptr;
                            }
                            ways = new_ways;
                            mem  = new_mem;
                            for (unsigned i = 0; i != ways; ++i) ctx[i]->memory = static_cast<uint8_t *>(_mm_malloc(mem, 4096));
                        }
                        nonce = 0;
                        for (unsigned i = 0; i != ways; ++i) {
                            memcpy(blob + blob_len*i, blob1, blob_len);
                            *p_nonce(blob, blob_len, i) = nonce++;
                        }
                        if (fn != pi_fn->second) {
                            fn = pi_fn->second;
                            timestamp  = 0;
                            hash_count = 0;
                        }
                 
                    } else if (pi->name == "pause") {
                        fn = nullptr;
                    } else if (pi->name == "close") {
                        for (unsigned i = 0; i != ways; ++i) if (ctx[i]->memory) _mm_free(ctx[i]->memory);
                        return;
                    }
                }
                if (fn) {
                    if ((hash_count & 0x7) == 0) {
                        const uint64_t new_timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
                        if (!timestamp || new_timestamp - timestamp > 60*1000) {
                            if (timestamp) {
                                MessageValues values;
                                values["hashrate"] = std::to_string(static_cast<float>(ways) * static_cast<float>(hash_count) / (new_timestamp - timestamp) * 1000.0f);
                                sendToNode(progress, Message("hashrate", values));
                            }
                            timestamp = new_timestamp;
                            hash_count = 0;
                        }
                    }
                    fn(blob, blob_len, hash, ctx);
                    for (unsigned i = 0; i != ways; ++i) {
                        uint32_t* const pnonce = p_nonce(blob, blob_len, i);
                        if (*p_result(hash, i) < target) {
                            MessageValues values;
                            values["nonce"] = std::to_string(*pnonce);
                            sendToNode(progress, Message("result", values));
                        }
                        *pnonce = nonce++;
                    }
                    ++ hash_count;
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
