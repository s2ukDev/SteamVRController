#include <algorithm>
#include <cstdint>
#include <vector>
#include <cmath>
#include <jni.h>

constexpr float JOYSTICK_DEADZONE = 0.27f;

float floatRound(
        float x,
        int precision)
{
    float power_of_10 = std::pow(10, precision);
    return std::round(x * power_of_10)  / power_of_10;
}

static void readSVec2(JNIEnv* env, jobject vec2Obj, float &outx, float &outy) {
    outx = outy = 0.0f;
    if (vec2Obj == nullptr) return;

    jclass cls = env->GetObjectClass(vec2Obj);
    if (cls == nullptr) return;

    jfieldID fx = env->GetFieldID(cls, "x", "F");
    jfieldID fy = env->GetFieldID(cls, "y", "F");
    if (fx && fy) {
        outx = env->GetFloatField(vec2Obj, fx);
        outy = env->GetFloatField(vec2Obj, fy);
        env->DeleteLocalRef(cls);
        return;
    }

    jmethodID mx = env->GetMethodID(cls, "getX", "()F");
    jmethodID my = env->GetMethodID(cls, "getY", "()F");
    if (mx && my) {
        outx = env->CallFloatMethod(vec2Obj, mx);
        outy = env->CallFloatMethod(vec2Obj, my);
    }

    env->DeleteLocalRef(cls);
}

void readSVec3(JNIEnv* env, jobject vec3Obj, float &outx, float &outy, float &outz) {
    outx = outy = outz = 0.0f;
    if (vec3Obj == nullptr) return;

    jclass cls = env->GetObjectClass(vec3Obj); // safer than FindClass for instances
    if (cls == nullptr) return;

    jfieldID fx = env->GetFieldID(cls, "x", "F");
    jfieldID fy = env->GetFieldID(cls, "y", "F");
    jfieldID fz = env->GetFieldID(cls, "z", "F");

    if (fx && fy && fz) {
        outx = env->GetFloatField(vec3Obj, fx);
        outy = env->GetFloatField(vec3Obj, fy);
        outz = env->GetFloatField(vec3Obj, fz);
    }

    env->DeleteLocalRef(cls);
}

static std::string base64_encode(const std::string &in) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < in.size()) {
        unsigned a = static_cast<unsigned char>(in[i++]);
        unsigned b = static_cast<unsigned char>(in[i++]);
        unsigned c = static_cast<unsigned char>(in[i++]);
        out.push_back(table[(a >> 2) & 0x3F]);
        out.push_back(table[((a & 0x3) << 4) | (b >> 4)]);
        out.push_back(table[((b & 0xF) << 2) | (c >> 6)]);
        out.push_back(table[c & 0x3F]);
    }
    size_t rem = in.size() - i;
    if (rem == 1) {
        unsigned a = static_cast<unsigned char>(in[i]);
        out.push_back(table[(a >> 2) & 0x3F]);
        out.push_back(table[((a & 0x3) << 4)]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        unsigned a = static_cast<unsigned char>(in[i]);
        unsigned b = static_cast<unsigned char>(in[i+1]);
        out.push_back(table[(a >> 2) & 0x3F]);
        out.push_back(table[((a & 0x3) << 4) | (b >> 4)]);
        out.push_back(table[((b & 0xF) << 2)]);
        out.push_back('=');
    }
    return out;
}

static bool isBase64(const std::string& s) {
    static const std::string table =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t len = 0;
    for (unsigned char c : s) {
        if (std::isspace(c)) continue;
        if (table.find(c) != std::string::npos) {
            ++len;
            continue;
        }
        if (c == '=') {
            ++len;
            continue;
        }
        return false;
    }

    return (len % 4 == 0);
}

static std::vector<uint8_t> base64_decode(std::string& inputData) {
    if (!isBase64(inputData)) {
        return {};
    }

    static const std::array<int8_t, 256> rev = []() {
        std::array<int8_t, 256> arr;
        arr.fill(-1);
        const std::string table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        for (size_t i = 0; i < table.size(); ++i) arr[static_cast<unsigned char>(table[i])] = static_cast<int8_t>(i);
        arr[static_cast<unsigned char>('=')] = -2;
        return arr;
    }();

    std::vector<uint8_t> out;
    out.reserve((inputData.size() * 3) / 4);

    int q[4];
    int qPos = 0;

    auto flush_quad = [&]() {
        int v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3];

        if (v0 < 0 || v1 < 0) throw std::invalid_argument("invalid b64 input");

        if (v2 == -2 && v3 == -2) {
            // 1 byte output "xx=="
            uint8_t b0 = static_cast<uint8_t>((v0 << 2) | (v1 >> 4));
            out.push_back(b0);
        }
        else if (v3 == -2) {
            // 2 byte output "xxx="
            if (v2 < 0) throw std::invalid_argument("invalid b64 padding");
            uint8_t b0 = static_cast<uint8_t>((v0 << 2) | (v1 >> 4));
            uint8_t b1 = static_cast<uint8_t>((v1 & 0xF) << 4 | (v2 >> 2));
            out.push_back(b0);
            out.push_back(b1);
        }
        else {
            // no padding, 3 bytes v2 & v3 >= 0
            if (v2 < 0 || v3 < 0) throw std::invalid_argument("invalid b64 input");

            uint8_t b0 = static_cast<uint8_t>((v0 << 2) | (v1 >> 4));
            uint8_t b1 = static_cast<uint8_t>((v1 & 0xF) << 4 | (v2 >> 2));
            uint8_t b2 = static_cast<uint8_t>((v2 & 0x3) << 6 | v3);
            out.push_back(b0);
            out.push_back(b1);
            out.push_back(b2);
        }
    };

    for (size_t i = 0; i < inputData.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(inputData[i]);
        if (std::isspace(c)) continue;

        int8_t val = rev[c];
        if (val == -1) throw std::invalid_argument("invalid b64 char");

        q[qPos++] = val;
        if (qPos == 4) {
            if (q[2] == -2 && q[3] != -2) throw std::invalid_argument("invalid b64 padding");
            flush_quad();
            qPos = 0;
        }
    }

    if (qPos != 0) {
        throw std::invalid_argument("truncated b64 wrong padding");
    }

    return out;
}

static uint64_t zigzag64(int64_t v) {
    // ZigZag: map signed -> unsigned
    return (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63);
}

static int64_t zigzag64Decode(uint64_t z) {
    // s = (z >> 1) ^ - (int64_t)(z & 1)
    return static_cast<int64_t>((z >> 1) ^ (~(z & 1) + 1)); // (z >> 1) ^ -static_cast<int64_t>(z & 1)
}

static void appendVarUint64(std::string &out, uint64_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<char>(value));
}

static std::optional<std::pair<uint64_t, size_t>> readVarUint64(const uint8_t* data, size_t len) {
    uint64_t result = 0;
    int shift = 0;
    size_t i = 0;
    while (i < len && shift <= 63) {
        uint8_t byte = data[i++];
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return std::make_pair(result, i);
        }
        shift += 7;
    }

    return std::nullopt;
}

static std::string jstringToStdString(JNIEnv* env, jstring jstr) {
    if (jstr == nullptr) return {};
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string out = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(jstr, chars);
    return out;
}

struct DecompressResult {
    bool ok = false;
    bool leftController = false;
    float amplitude = 0.0f;
    float frequency = 0.0f;
    float durationSeconds = 0.0f;
};

DecompressResult decompressResponseData(const std::string& base64Input) {
    DecompressResult res;
    auto base64Input_ = base64Input;

    std::vector<uint8_t> buf = base64_decode(base64Input_);
    if (buf.empty()) {
        res.ok = false;
        return res;
    }

    if (buf.size() < 1) {
        res.ok = false;
        return res;
    }

    size_t offset = 0;
    uint8_t flags = buf[offset++];
    res.leftController = (flags & (1 << 0)) != 0;

    // (amp, freq, duration)
    const size_t remaining = buf.size() - offset;
    const uint8_t* pdata = buf.data() + offset;
    size_t used = 0;

    // amplitude
    auto ampPair = readVarUint64(pdata, remaining);
    if (!ampPair.has_value()) { res.ok = false; return res; }
    uint64_t ampZig = ampPair->first;
    used = ampPair->second;
    offset += used;
    pdata += used;

    // frequency
    size_t remain2 = buf.size() - offset;
    auto freqPair = readVarUint64(pdata, remain2);
    if (!freqPair.has_value()) { res.ok = false; return res; }
    uint64_t freqZig = freqPair->first;
    used = freqPair->second;
    offset += used;
    pdata += used;

    // duration
    size_t remain3 = buf.size() - offset;
    auto durPair = readVarUint64(pdata, remain3);
    if (!durPair.has_value()) { res.ok = false; return res; }
    uint64_t durZig = durPair->first;
    used = durPair->second;
    offset += used;

    // ZigZag decode
    int64_t ampInt = zigzag64Decode(ampZig);
    int64_t freqInt = zigzag64Decode(freqZig);
    int64_t durInt = zigzag64Decode(durZig);

    // Scale down values from original
    // ampInt = round(amplitude * 1000.0f)
    // freqInt = round(frequency * 100.0f)
    // durInt = round(duration * 1000.0f)
    res.amplitude = static_cast<float>(static_cast<double>(ampInt) / 1000.0);
    res.frequency = static_cast<float>(static_cast<double>(freqInt) / 100.0);
    res.durationSeconds  = static_cast<float>(static_cast<double>(durInt) / 1000.0);

    // clamp the min value or else you will read zero (required in some scenarios, because of openvr api)
    if (res.durationSeconds <= 0.0f) res.durationSeconds = 0.005f;

    res.ok = true;
    return res;
}

static jobject createInboundDecompressResults(JNIEnv* env, const DecompressResult& result) {
    const char* clsName = "org/s2uk/vrcontroller/InboundDecompressResults";
    jclass cls = env->FindClass(clsName);
    if (cls == nullptr) { env->ExceptionClear(); return nullptr; }

    // Constructor
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(ZZFFF)V");
    if (ctor == nullptr) { env->ExceptionClear(); return nullptr; }

    jvalue args[5];
    args[0].z = result.ok ? JNI_TRUE : JNI_FALSE;
    args[1].z = result.leftController ? JNI_TRUE : JNI_FALSE;
    args[2].f = static_cast<jfloat>(result.amplitude);
    args[3].f = static_cast<jfloat>(result.frequency);
    args[4].f = static_cast<jfloat>(result.durationSeconds);

    jobject obj = env->NewObjectA(cls, ctor, args);
    return obj;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_s2uk_vrcontroller_MainActivity_decompressInboundPacket(JNIEnv *env, jobject /*thiz*/,
                                                                jstring in_data_b64) {
    const std::string base64 = jstringToStdString(env, in_data_b64);

    DecompressResult res = decompressResponseData(base64);

    if (!res.ok) {
        DecompressResult fallback;
        fallback.ok = false;
        fallback.leftController = false;
        fallback.amplitude = 0.0f;
        fallback.frequency = 0.0f;
        fallback.durationSeconds = 0.0f;
        return createInboundDecompressResults(env, fallback);
    }

    return createInboundDecompressResults(env, res);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_s2uk_vrcontroller_MainActivity_joyConvertToVec2(
        JNIEnv *env, jobject /*thiz*/, jint angle, jint strength) {

    float norm = std::clamp(static_cast<float>(strength) / 100.0f, 0.0f, 1.0f);

    constexpr float PI = 3.14159265358979323846f;
    float rad = (90.0f - static_cast<float>(angle)) * PI / 180.0f;

    float x = norm * sinf(rad);
    float y = norm * cosf(rad);

    if (fabsf(x) < 1e-6f) x = 0.0f;
    if (fabsf(y) < 1e-6f) y = 0.0f;

    jclass vecClass = env->FindClass("org/s2uk/vrcontroller/SVec2");
    if (!vecClass) return nullptr;

    jmethodID ctor = env->GetMethodID(vecClass, "<init>", "(FF)V");
    if (!ctor) return nullptr;

    jobject vecObj = env->NewObject(vecClass, ctor, floatRound(x, 5), floatRound(y, 5));
    return vecObj;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_org_s2uk_vrcontroller_MainActivity_isJoyInDZ(JNIEnv *env, jobject /*thiz*/, jobject joy_data) {
    if (joy_data == nullptr) return false;

    jclass vecClass = env->GetObjectClass(joy_data);
    if (vecClass == nullptr) return false;
    jfieldID fx = env->GetFieldID(vecClass, "x", "F");
    jfieldID fy = env->GetFieldID(vecClass, "y", "F");

    if (fx == nullptr || fy == nullptr) {
        env->DeleteLocalRef(vecClass);
        return false;
    }
    jfloat x = env->GetFloatField(joy_data,fx);
    jfloat y = env->GetFloatField(joy_data,fy);

    env->DeleteLocalRef(vecClass);

    float magnitude = std::hypot(x,y);
    return magnitude <= JOYSTICK_DEADZONE;
}
extern "C"
JNIEXPORT jstring JNICALL
Java_org_s2uk_vrcontroller_MainActivity_compressDataBeforeSending(JNIEnv *env, jobject /*thiz*/,
                                                                 jboolean left_controller,
                                                                 jint trigger_state,
                                                                 jint grip_state,
                                                                 jboolean btn_system_or_menu_state,
                                                                 jboolean btn_a_or_x_state,
                                                                 jboolean btn_b_or_y_state,
                                                                 jobject gyro_angle,
                                                                 jobject joy_data,
                                                                 jint joy_state,
                                                                 jboolean joy_in_dz,
                                                                 jfloat controller_battery_percentage,
                                                                 jboolean controller_battery_plugged) {
    // Convert JNI classes to cpp
    float gx=0.0f, gy=0.0f, gz=0.0f;
    readSVec3(env, gyro_angle, gx, gy, gz);

    float jx=0.0f, jy=0.0f;
    readSVec2(env, joy_data, jx, jy);

    const double gyroScale = 1000.0;
    const double joyScale  = 100000.0;

    std::string buf;
    buf.reserve(64);

    uint8_t flags = 0;
    if (left_controller)          flags |= (1 << 0); // bit0
    if (btn_system_or_menu_state) flags |= (1 << 1); // bit1
    if (btn_a_or_x_state)         flags |= (1 << 2); // bit2
    if (btn_b_or_y_state)         flags |= (1 << 3); // bit3
    if (controller_battery_plugged) flags |= (1 << 4); // bit4
    if (joy_in_dz)                flags |= (1 << 5); // bit5
    buf.push_back(static_cast<char>(flags));

    // bits0-1 trigger, bits2-3 grip, bits4-5 joy_state (each 0..2)
    uint8_t modes = static_cast<uint8_t>((trigger_state & 0x3) | ((grip_state & 0x3) << 2) | ((joy_state & 0x3) << 4));
    buf.push_back(static_cast<char>(modes));

    // battery percentage (clamp 0..100) - 1 byte
    int batteryPercentage = static_cast<int>(std::lround(controller_battery_percentage * 100.0f)); // if caller passed 0.0..1.0
    if (batteryPercentage < 0) batteryPercentage = 0;
    if (batteryPercentage > 100) batteryPercentage = 100;
    buf.push_back(static_cast<char>(static_cast<uint8_t>(batteryPercentage)));

    // pack gyro: gx, gy, gz => int64 -> ZigZag -> varUint64
    {
        int64_t gxi = static_cast<int64_t>(std::llround(gx * gyroScale));
        int64_t gyi = static_cast<int64_t>(std::llround(gy * gyroScale));
        int64_t gzi = static_cast<int64_t>(std::llround(gz * gyroScale));
        appendVarUint64(buf, zigzag64(gxi));
        appendVarUint64(buf, zigzag64(gyi));
        appendVarUint64(buf, zigzag64(gzi));
    }

    // pack joystick: jx, jy
    {
        int64_t jxi = static_cast<int64_t>(std::llround(jx * joyScale));
        int64_t jyi = static_cast<int64_t>(std::llround(jy * joyScale));
        appendVarUint64(buf, zigzag64(jxi));
        appendVarUint64(buf, zigzag64(jyi));
    }

    std::string b64 = base64_encode(buf);

    return env->NewStringUTF(b64.c_str());
}