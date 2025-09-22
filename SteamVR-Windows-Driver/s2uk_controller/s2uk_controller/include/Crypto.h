#pragma once
#ifndef S2UK_Crypto
#define S2UK_Crypto

#include <string>
#include <vector>
#include <cctype>
#include <array>

#include <stdexcept>

class s2uk_crypto {
public:
	static std::string base64_encode(const std::string& inputData) {
		static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		out.reserve(((inputData.size() + 2) / 3) * 4);
		size_t i = 0;
		while (i + 2 < inputData.size()) {
			unsigned a = static_cast<unsigned char>(inputData[i++]);
			unsigned b = static_cast<unsigned char>(inputData[i++]);
			unsigned c = static_cast<unsigned char>(inputData[i++]);
			out.push_back(table[(a >> 2) & 0x3F]);
			out.push_back(table[((a & 0x3) << 4) | (b >> 4)]);
			out.push_back(table[((b & 0xF) << 2) | (c >> 6)]);
			out.push_back(table[c & 0x3F]);
		}
		size_t rem = inputData.size() - i;
		if (rem == 1) {
			unsigned a = static_cast<unsigned char>(inputData[i]);
			out.push_back(table[(a >> 2) & 0x3F]);
			out.push_back(table[((a & 0x3) << 4)]);
			out.push_back('=');
			out.push_back('=');
		}
		else if (rem == 2) {
			unsigned a = static_cast<unsigned char>(inputData[i]);
			unsigned b = static_cast<unsigned char>(inputData[i + 1]);
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

	static void appendVarUint64(std::string& out, uint64_t value) {
		while (value >= 0x80) {
			out.push_back(static_cast<char>((value & 0x7F) | 0x80));
			value >>= 7;
		}
		out.push_back(static_cast<char>(value));
	}

	static uint64_t zigzag64(int64_t v) {
		return (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63);
	}

	static int64_t zigzag64Decode(uint64_t z) {
		// s = (z >> 1) ^ - (int64_t)(z & 1)
		return static_cast<int64_t>((z >> 1) ^ (~(z & 1) + 1)); // (z >> 1) ^ -static_cast<int64_t>(z & 1)
	}
};
#endif
