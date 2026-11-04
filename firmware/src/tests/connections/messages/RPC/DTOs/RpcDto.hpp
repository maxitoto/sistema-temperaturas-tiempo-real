#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <type_traits>
#include <cstring>

namespace Messages::Rpc {

constexpr uint8_t PROTOCOL_VERSION = 1;

enum class RpcFlow : uint8_t {
    Unary = 1,
    ServerStream = 2,
    ClientStream = 3,
    BidiStream = 4
};

enum class RpcSemantics : uint8_t {
    Maybe = 0,
    AtLeastOnce = 1,
    AtMostOnce = 2
};

enum class PacketType : uint8_t {
    Request = 1,
    Response = 2,
    StreamChunk = 3,
    StreamEnd = 4,
    Error = 5
};

namespace Flags {
    constexpr uint8_t NONE      = 0x00;
    constexpr uint8_t EOS       = 0x01;
    constexpr uint8_t REAL_TIME = 0x02;
}

struct RpcEnvelope {
    PacketType type = PacketType::Request;
    RpcFlow flow = RpcFlow::Unary;
    RpcSemantics semantics = RpcSemantics::AtMostOnce;
    uint8_t flags = Flags::NONE;
    uint32_t callId = 0;
    uint32_t streamId = 0;
    uint32_t methodId = 0;
    std::vector<uint8_t> payload;
};

constexpr uint32_t hashEndpoint(std::string_view str) {
    uint32_t hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

namespace Detail {
    struct DummyType {
        template <typename T>
        constexpr operator T() const noexcept;
    };

    template <typename T, typename... Args>
    constexpr auto isBracesConstructible(int)
        -> decltype(void(T{std::declval<Args>()...}), std::true_type{});

    template <typename, typename...>
    constexpr std::false_type isBracesConstructible(...);

    template <typename T, typename... Args>
    constexpr bool isBracesConstructibleV = decltype(isBracesConstructible<T, Args...>(0))::value;

    // Reflexión automática de structs DTO en C++17 hasta 15 campos (incluye NetworkStatusDto con bootId/resetReason)
    template <typename T>
    auto toTuple(T&& obj) {
        using Type = std::decay_t<T>;
        if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7, f8, f9] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7, f8] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6, f7] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6, f7);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5, f6] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5, f6);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4, f5] = obj;
            return std::make_tuple(f1, f2, f3, f4, f5);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3, f4] = obj;
            return std::make_tuple(f1, f2, f3, f4);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType, DummyType>) {
            auto&& [f1, f2, f3] = obj;
            return std::make_tuple(f1, f2, f3);
        } else if constexpr (isBracesConstructibleV<Type, DummyType, DummyType>) {
            auto&& [f1, f2] = obj;
            return std::make_tuple(f1, f2);
        } else if constexpr (isBracesConstructibleV<Type, DummyType>) {
            auto&& [f1] = obj;
            return std::make_tuple(f1);
        } else {
            return std::make_tuple();
        }
    }

    template <typename>
    struct IsTuple : std::false_type {};
    template <typename... T>
    struct IsTuple<std::tuple<T...>> : std::true_type {};
} // namespace Detail

class MsgPack {
public:
    static void packNil(std::vector<uint8_t>& buf) { buf.push_back(0xC0); }

    static void packValue(std::vector<uint8_t>& buf, bool v) {
        buf.push_back(v ? 0xC3 : 0xC2);
    }

    template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    static void packValue(std::vector<uint8_t>& buf, T v) {
        uint64_t u = static_cast<uint64_t>(v);
        buf.push_back(0xCF);
        for (int i = 7; i >= 0; --i) buf.push_back(static_cast<uint8_t>((u >> (i * 8)) & 0xFF));
    }

    static void packValue(std::vector<uint8_t>& buf, float v) {
        buf.push_back(0xCA);
        uint32_t bits = 0;
        std::memcpy(&bits, &v, 4);
        for (int i = 3; i >= 0; --i) buf.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
    }

    static void packValue(std::vector<uint8_t>& buf, double v) {
        buf.push_back(0xCB);
        uint64_t bits = 0;
        std::memcpy(&bits, &v, 8);
        for (int i = 7; i >= 0; --i) buf.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
    }

    template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
    static void packValue(std::vector<uint8_t>& buf, T v) {
        packValue(buf, static_cast<std::underlying_type_t<T>>(v));
    }

    static void packValue(std::vector<uint8_t>& buf, const std::string& s) {
        uint16_t len = static_cast<uint16_t>(s.size());
        buf.push_back(0xDA);
        buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(len & 0xFF));
        buf.insert(buf.end(), s.begin(), s.end());
    }

    static void packValue(std::vector<uint8_t>& buf, const char* s) {
        packValue(buf, std::string(s ? s : ""));
    }

    static void packValue(std::vector<uint8_t>& buf, const std::vector<uint8_t>& bin) {
        uint16_t len = static_cast<uint16_t>(bin.size());
        buf.push_back(0xC5);
        buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(len & 0xFF));
        buf.insert(buf.end(), bin.begin(), bin.end());
    }

    template <typename... Args>
    static void packValue(std::vector<uint8_t>& buf, const std::tuple<Args...>& tup) {
        constexpr size_t count = sizeof...(Args);
        if constexpr (count < 16) {
            buf.push_back(static_cast<uint8_t>(0x90 | (count & 0x0F)));
        } else {
            buf.push_back(0xDC);
            buf.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));
            buf.push_back(static_cast<uint8_t>(count & 0xFF));
        }
        if constexpr (count > 0) {
            std::apply([&buf](const auto&... item) { (packValue(buf, item), ...); }, tup);
        }
    }

    template <typename T, std::enable_if_t<std::is_class_v<T> &&
              !std::is_same_v<T, std::string> &&
              !std::is_same_v<T, std::vector<uint8_t>> &&
              !Detail::IsTuple<T>::value, int> = 0>
    static void packValue(std::vector<uint8_t>& buf, const T& dto) {
        packValue(buf, Detail::toTuple(dto));
    }

    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, bool& out) {
        if (offset >= len) return false;
        uint8_t tag = data[offset++];
        out = (tag == 0xC3);
        return (tag == 0xC3 || tag == 0xC2);
    }

    template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, T& out) {
        if (offset + 9 > len || data[offset++] != 0xCF) return false;
        uint64_t u = 0;
        for (int i = 0; i < 8; ++i) u = (u << 8) | data[offset++];
        out = static_cast<T>(u);
        return true;
    }

    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, float& out) {
        if (offset + 5 > len || data[offset++] != 0xCA) return false;
        uint32_t bits = 0;
        for (int i = 0; i < 4; ++i) bits = (bits << 8) | data[offset++];
        std::memcpy(&out, &bits, 4);
        return true;
    }

    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, double& out) {
        if (offset + 9 > len || data[offset++] != 0xCB) return false;
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) bits = (bits << 8) | data[offset++];
        std::memcpy(&out, &bits, 8);
        return true;
    }

    template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, T& out) {
        std::underlying_type_t<T> raw{};
        if (!unpackValue(data, len, offset, raw)) return false;
        out = static_cast<T>(raw);
        return true;
    }

    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, std::string& out) {
        if (offset + 3 > len || data[offset++] != 0xDA) return false;
        uint16_t sLen = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        if (offset + sLen > len) return false;
        out.assign(reinterpret_cast<const char*>(data + offset), sLen);
        offset += sLen;
        return true;
    }

    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, std::vector<uint8_t>& out) {
        if (offset + 3 > len || data[offset++] != 0xC5) return false;
        uint16_t bLen = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        if (offset + bLen > len) return false;
        out.assign(data + offset, data + offset + bLen);
        offset += bLen;
        return true;
    }

    template <typename... Args>
    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, std::tuple<Args...>& tup) {
        if constexpr (sizeof...(Args) == 0) {
            if (offset < len && (data[offset] & 0xF0) == 0x90) offset++;
            return true;
        } else {
            if (offset >= len) return false;
            uint8_t tag = data[offset++];
            if (tag == 0xDC) {
                if (offset + 2 > len) return false;
                offset += 2;
            } else if ((tag & 0xF0) != 0x90) {
                return false;
            }
            bool ok = true;
            std::apply([&](auto&... item) { ((ok = ok && unpackValue(data, len, offset, item)), ...); }, tup);
            return ok;
        }
    }

    template <typename T, std::enable_if_t<std::is_class_v<T> &&
              !std::is_same_v<T, std::string> &&
              !std::is_same_v<T, std::vector<uint8_t>> &&
              !Detail::IsTuple<T>::value, int> = 0>
    static bool unpackValue(const uint8_t* data, size_t len, size_t& offset, T& dto) {
        using TupleType = decltype(Detail::toTuple(dto));
        TupleType tup{};
        if (!unpackValue(data, len, offset, tup)) return false;
        dto = makeFromTuple<T>(std::move(tup), std::make_index_sequence<std::tuple_size_v<TupleType>>{});
        return true;
    }

    template <typename T>
    static std::vector<uint8_t> serialize(const T& obj) {
        std::vector<uint8_t> buf;
        packValue(buf, obj);
        return buf;
    }

    template <typename T>
    static bool deserialize(const std::vector<uint8_t>& buf, T& out) {
        size_t offset = 0;
        return unpackValue(buf.data(), buf.size(), offset, out);
    }

    static std::vector<uint8_t> packEnvelope(const RpcEnvelope& env) {
        auto tup = std::make_tuple(env.type, env.flow, env.semantics, env.flags,
                                   env.callId, env.streamId, env.methodId, env.payload);
        return serialize(tup);
    }

    static bool unpackEnvelope(const std::vector<uint8_t>& raw, RpcEnvelope& out) {
        std::tuple<PacketType, RpcFlow, RpcSemantics, uint8_t, uint32_t, uint32_t, uint32_t, std::vector<uint8_t>> tup;
        if (!deserialize(raw, tup)) return false;
        std::tie(out.type, out.flow, out.semantics, out.flags,
                 out.callId, out.streamId, out.methodId, out.payload) = std::move(tup);
        return true;
    }

private:
    template <typename T, typename Tuple, size_t... I>
    static T makeFromTuple(Tuple&& tup, std::index_sequence<I...>) {
        return T{std::move(std::get<I>(tup))...};
    }
};

} // namespace Messages::Rpc