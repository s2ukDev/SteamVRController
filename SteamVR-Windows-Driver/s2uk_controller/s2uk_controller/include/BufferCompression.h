#pragma once
#ifndef S2UK_BufferDecryptor
#define S2UK_BufferDecryptor

#include <vector>
#include "Crypto.h"
#include "VectorMath.h"

class ByteReader {
public:
    explicit ByteReader(std::vector<uint8_t> data) : bytes(std::move(data)), pos(0) {}
    size_t remaining() const { return bytes.size() - pos; }

    uint8_t readByte() {
        if (pos >= bytes.size()) throw std::out_of_range("unexpected buffer end");
        return bytes[pos++];
    }

    uint64_t readVarUint64() {
        uint64_t result = 0;
        unsigned shift = 0;
        const unsigned maxBytes = 10;
        unsigned consumed = 0;
        while (true) {
            if (pos >= bytes.size()) throw std::out_of_range("out of range");
            uint8_t b = bytes[pos++];
            ++consumed;
            result |= (uint64_t)(b & 0x7F) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
            if (shift >= 64 || consumed > maxBytes) throw std::invalid_argument("variant exeeds max bytes");
        }
        return result;
    }

private:
    std::vector<uint8_t> bytes;
    size_t pos;
};

class BufferCompression {
public:
    struct ControllerState {
        bool left_controller = false;
        bool btn_system_or_menu_state = false;
        bool btn_a_or_x_state = false;
        bool btn_b_or_y_state = false;
        bool controller_battery_plugged = false;
        bool joy_in_dz = false;

        uint8_t trigger_state = 0;
        uint8_t grip_state = 0;
        uint8_t joy_state = 0;

        double batteryPercentage = 0.0;

        Vec3 gyro{}; // gx, gy, gz
        Vec2 joy{};  // jx, jy
    };

    static ControllerState decryptControllerState(const std::string& b64) {
        auto b64_ = b64;
        const double gyroScale = 1000.0;
        const double joyScale = 100000.0;

        std::vector<uint8_t> bytes = s2uk_crypto::base64_decode(b64_);
        if (bytes.size() < 3) return {}; // throw std::invalid_argument("buffer too small");

        ByteReader r(std::move(bytes));

        uint8_t flags = r.readByte();
        ControllerState st;
        st.left_controller = (flags & (1 << 0)) != 0;
        st.btn_system_or_menu_state = (flags & (1 << 1)) != 0;
        st.btn_a_or_x_state = (flags & (1 << 2)) != 0;
        st.btn_b_or_y_state = (flags & (1 << 3)) != 0;
        st.controller_battery_plugged = (flags & (1 << 4)) != 0;
        st.joy_in_dz = (flags & (1 << 5)) != 0;

        uint8_t modes = r.readByte();
        st.trigger_state = (modes >> 0) & 0x3;
        st.grip_state = (modes >> 2) & 0x3;
        st.joy_state = (modes >> 4) & 0x3;

        uint8_t battery = r.readByte();
        if (battery > 100) battery = 100; // clamp
        st.batteryPercentage = battery / 100.0f;

        // read 5 zigzag varints: gxi, gyi, gzi, jxi, jyi
        int64_t gxi = s2uk_crypto::zigzag64Decode(r.readVarUint64());
        int64_t gyi = s2uk_crypto::zigzag64Decode(r.readVarUint64());
        int64_t gzi = s2uk_crypto::zigzag64Decode(r.readVarUint64());
        int64_t jxi = s2uk_crypto::zigzag64Decode(r.readVarUint64());
        int64_t jyi = s2uk_crypto::zigzag64Decode(r.readVarUint64());

        st.gyro.x = static_cast<double>(gxi) / gyroScale;
        st.gyro.y = static_cast<double>(gyi) / gyroScale;
        st.gyro.z = static_cast<double>(gzi) / gyroScale;

        st.joy.x = static_cast<double>(jxi) / joyScale;
        st.joy.y = static_cast<double>(jyi) / joyScale;

        return st;
    }

    static std::string compressResponseData(bool isLeftController, float amplitude, float frequency, float duration) {
        if (duration <= 0.0f) duration = 0.005f;

        std::string buf;
        buf.reserve(32);

        // 1 byte flags
        uint8_t flags = 0;
        if (isLeftController) flags |= 1 << 0; // bit0
        buf.push_back(static_cast<char>(flags));

        int64_t ampInt = static_cast<int64_t>(std::llround(amplitude * 1000.0f));
        int64_t freqInt = static_cast<int64_t>(std::llround(frequency * 100.0f));
        int64_t durInt = static_cast<int64_t>(std::llround(duration * 1000.0f));

        s2uk_crypto::appendVarUint64(buf, s2uk_crypto::zigzag64(ampInt));
        s2uk_crypto::appendVarUint64(buf, s2uk_crypto::zigzag64(freqInt));
        s2uk_crypto::appendVarUint64(buf, s2uk_crypto::zigzag64(durInt));

        // Base64 encode
        return s2uk_crypto::base64_encode(buf);
    }
};
#endif
