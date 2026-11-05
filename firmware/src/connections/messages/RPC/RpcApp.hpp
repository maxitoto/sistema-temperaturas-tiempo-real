#pragma once
#include "DTOs/RpcDto.hpp"
#include "Skeletons/RpcSkeleton.hpp"
#include "../../proxy/TransportProxy.hpp"
#include <Arduino.h>
#include <functional>

namespace Messages::Rpc {

namespace Detail {
    template <typename T>
    struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct FunctionTraits<ReturnType(ClassType::*)(Args...) const> {
        using result_type = ReturnType;
        using args_tuple = std::tuple<std::decay_t<Args>...>;
    };
} // namespace Detail

class RpcResult {
public:
    RpcResult() = default;
    RpcResult(uint32_t ticketId, bool ok, std::vector<uint8_t> raw, std::string error = "")
        : ticketId_(ticketId), ok_(ok), raw_(std::move(raw)), error_(std::move(error)) {}

    uint32_t ticketId() const { return ticketId_; }
    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }

    template <typename T>
    T as() const {
        T val{};
        if (ok_ && !raw_.empty()) {
            MsgPack::deserialize(raw_, val);
        }
        return val;
    }

private:
    uint32_t ticketId_ = 0;
    bool ok_ = false;
    std::vector<uint8_t> raw_;
    std::string error_;
};

class server {
public:
    explicit server(Connections::TransportProxy& proxy)
        : proxy_(proxy), skeleton_(proxy) {
        skeleton_.loadConfig();
        proxy_.setRpcHandler([this](uint32_t clientId, uint32_t sessionId, const std::vector<uint8_t>& raw, bool) {
            RpcEnvelope env;
            if (!MsgPack::unpackEnvelope(raw, env)) return;

            if (env.type == PacketType::Request ||
               (env.type == PacketType::StreamChunk && (env.flow == RpcFlow::ClientStream || env.flow == RpcFlow::BidiStream))) {
                skeleton_.onIncomingPacket(clientId, sessionId, env);
            }
        });
    }

    template <typename TFunc>
    void bind(std::string_view name, TFunc&& fn, RpcSemantics semantics = RpcSemantics::AtMostOnce) {
        using Traits = Detail::FunctionTraits<std::decay_t<TFunc>>;
        using ReturnT = typename Traits::result_type;
        using ArgsTuple = typename Traits::args_tuple;

        skeleton_.bindRaw(hashEndpoint(name), RpcFlow::Unary, semantics,
            [func = std::forward<TFunc>(fn)](uint32_t, const RpcEnvelope& env) -> std::vector<uint8_t> {
                ArgsTuple args{};
                if constexpr (std::tuple_size_v<ArgsTuple> == 1) {
                    if (!MsgPack::deserialize(env.payload, args)) {
                        MsgPack::deserialize(env.payload, std::get<0>(args));
                    }
                } else if constexpr (std::tuple_size_v<ArgsTuple> > 1) {
                    MsgPack::deserialize(env.payload, args);
                }

                if constexpr (std::is_void_v<ReturnT>) {
                    std::apply(func, std::move(args));
                    std::vector<uint8_t> nilBuf;
                    MsgPack::packNil(nilBuf);
                    return nilBuf;
                } else {
                    ReturnT res = std::apply(func, std::move(args));
                    return MsgPack::serialize(res);
                }
            });
    }

    template <typename TIn, typename TOut, typename TStreamFunc>
    void bind_stream(std::string_view name, TStreamFunc&& fn,
                     RpcFlow flow = RpcFlow::ServerStream,
                     RpcSemantics semantics = RpcSemantics::Maybe) {
        Connections::TransportProxy* proxyPtr = &proxy_;
        skeleton_.bindRaw(hashEndpoint(name), flow, semantics,
            [proxyPtr, func = std::forward<TStreamFunc>(fn)](uint32_t clientId, const RpcEnvelope& env) -> std::vector<uint8_t> {
                StreamContext<TIn, TOut> ctx(clientId, env, *proxyPtr);
                func(ctx);
                return {};
            });
    }

    void run() {
        proxy_.schedule();
    }

    RpcSkeleton& skeleton() { return skeleton_; }

private:
    Connections::TransportProxy& proxy_;
    RpcSkeleton skeleton_;
};

} // namespace Messages::Rpc

namespace rpc = Messages::Rpc;