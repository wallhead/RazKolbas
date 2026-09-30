#include <catch2/catch_test_macros.hpp>
#include "rk/FgStreamlineFrameSession.hpp"
#include "rk/FgStreamlineRuntime.hpp"
#include <dxgi1_6.h>
#include <string>
#include <stdexcept>
#include <vector>

namespace {
struct Token final : sl::FrameToken {
    std::uint32_t index=101;
    operator std::uint32_t() const override { return index; }
};
rk::FgSourceFrame source(std::uint64_t id=1) {
    rk::FgSourceFrame f{};f.source=id;f.generation=2;f.presentToken=id+100;
    f.resetEpoch=3;f.render={32,20};f.display={64,40};
    f.ownerReady=true;f.worldActive=true;f.cameraValid=true;return f;
}
struct Fixture {
    Token token;
    std::uint64_t thread=7;
    std::string fail;
    std::string throwEvent;
    bool mutateConstants{};
    std::uint32_t nextIndex=101;
    std::vector<std::string> events;
    rk::FgStreamlineFrameCalls calls;
    Fixture() {
        calls.currentThread=[this]{return thread;};
        calls.newToken=[this](sl::FrameToken*& out) {
            events.push_back("token");out=fail=="null"?nullptr:&token;
            token.index=nextIndex++;
            return result("token");
        };
        calls.sleep=[this](const sl::FrameToken& t) {
            REQUIRE(&t==&token);events.push_back("sleep");return result("sleep");
        };
        calls.marker=[this](sl::PCLMarker marker,const sl::FrameToken& t) {
            REQUIRE(&t==&token);
            const auto name="marker"+std::to_string(static_cast<int>(marker));
            events.push_back(name);return result(name);
        };
        calls.inputs.setConstants=[this](const sl::Constants&,
            const sl::FrameToken& t,const sl::ViewportHandle& v) {
            REQUIRE(&t==&token);REQUIRE(static_cast<std::uint32_t>(v)==0);
            events.push_back("constants");
            if(mutateConstants)++token.index;
            return result("constants");
        };
        calls.inputs.setTags=[this](const sl::FrameToken& t,
            const sl::ViewportHandle&,const sl::ResourceTag*,std::uint32_t count,
            sl::CommandBuffer* commands) {
            REQUIRE(&t==&token);REQUIRE(count==5);REQUIRE(commands==nullptr);
            events.push_back("tags");return result("tags");
        };
    }
    sl::Result result(const std::string& event) const {
        if(throwEvent==event)throw std::runtime_error("callback failure");
        return fail==event?sl::Result::eErrorInvalidState:sl::Result::eOk;
    }
    rk::FgStreamlineFrameInputs packet() {
        using Microsoft::WRL::ComPtr;
        ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> adapter;
        ComPtr<ID3D12Device> device;
        REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
        REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
        REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device))));
        const auto f=source();rk::FgPreparedSubmission p{};
        p.source=f.source;p.generation=f.generation;p.presentToken=f.presentToken;
        p.resetEpoch=f.resetEpoch;p.render=f.render;p.display=f.display;
        p.swapBufferCount=2;p.copyTicket={1,1};
        const auto read=static_cast<D3D12_RESOURCE_STATES>(
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        for(unsigned i=0;i<5;++i) {
            const auto size=i==1||i==2?f.render:f.display;
            D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            d.Width=size.width;d.Height=size.height;d.DepthOrArraySize=1;
            d.MipLevels=1;d.SampleDesc.Count=1;
            d.Format=i==1?DXGI_FORMAT_R32_FLOAT:i==2?DXGI_FORMAT_R16G16_FLOAT:
                DXGI_FORMAT_R8G8B8A8_UNORM;
            D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
            REQUIRE(SUCCEEDED(device->CreateCommittedResource(&heap,
                D3D12_HEAP_FLAG_NONE,&d,read,nullptr,IID_PPV_ARGS(&p.resources[i]))));
        }
        rk::FgStreamlineReadStates states{};states.completedCopy=p.copyTicket;
        states.actual.fill(read);
        auto tags=rk::makeFgStreamlineTags(p,states);
        REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgStreamlineTagBundle>>(tags));
        rk::FgStreamlineFrameInputs out{};
        out.source=f.source;out.generation=f.generation;
        out.presentToken=f.presentToken;out.resetEpoch=f.resetEpoch;
        out.tags=std::move(std::get<std::unique_ptr<rk::FgStreamlineTagBundle>>(tags));
        return out;
    }
};
void render(Fixture&,rk::FgStreamlineFrameSession& session) {
    REQUIRE(std::holds_alternative<bool>(session.begin(source())));
    REQUIRE(std::holds_alternative<bool>(session.simulationEnd()));
    REQUIRE(std::holds_alternative<bool>(session.renderSubmitStart()));
    REQUIRE(std::holds_alternative<bool>(session.renderSubmitEnd()));
}
}

TEST_CASE("FG session owns one token and ordered markers, inputs and Present",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    render(f,session);auto packet=f.packet();
    REQUIRE(std::holds_alternative<bool>(session.submit(packet)));
    auto result=session.present([&]{f.events.push_back("present");return S_OK;});
    REQUIRE(std::holds_alternative<HRESULT>(result));
    REQUIRE(std::get<HRESULT>(result)==S_OK);
    REQUIRE(f.events==std::vector<std::string>{"token","sleep","marker0",
        "marker1","marker2","marker3","constants","tags","marker4",
        "present","marker5"});
    const auto count=f.events.size();
    REQUIRE(std::holds_alternative<rk::Error>(session.present([]{return S_OK;})));
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(source())));
    REQUIRE(f.events.size()==count);
    REQUIRE(std::holds_alternative<bool>(session.begin(source(2))));
    REQUIRE_FALSE(session.failed());
}

TEST_CASE("FG session permits thread handoff only after a completed real Present",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    render(f,session);auto packet=f.packet();
    REQUIRE(std::holds_alternative<bool>(session.submit(packet)));
    REQUIRE(std::holds_alternative<HRESULT>(session.present([]{return S_OK;})));
    f.thread=8;
    REQUIRE(std::holds_alternative<bool>(session.begin(source(2))));
    const auto count=f.events.size();
    f.thread=7;
    REQUIRE(std::holds_alternative<rk::Error>(session.simulationEnd()));
    REQUIRE(f.events.size()==count);
    f.thread=8;
    REQUIRE(std::holds_alternative<bool>(session.simulationEnd()));
}

TEST_CASE("FG session rejects missing or invalid identities before any SDK call",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    auto invalid=source();
    SECTION("source") {invalid.source=0;}
    SECTION("generation") {invalid.generation=0;}
    SECTION("token") {invalid.presentToken=0;}
    SECTION("reset epoch") {invalid.resetEpoch=0;}
    SECTION("owner") {invalid.ownerReady=false;}
    SECTION("extent") {invalid.render={};}
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(invalid)));
    REQUIRE(f.events.empty());
}

TEST_CASE("FG session rejects unavailable calls and wrong-thread or wrong-phase entry",
    "[fg_streamline_session]") {
    Fixture f;auto calls=f.calls;
    SECTION("missing marker") {calls.marker={};}
    SECTION("missing token") {calls.newToken={};}
    SECTION("missing sleep") {calls.sleep={};}
    SECTION("missing constants") {calls.inputs.setConstants={};}
    SECTION("missing tags") {calls.inputs.setTags={};}
    SECTION("missing thread") {calls.currentThread={};}
    SECTION("wrong phase") {
        rk::FgStreamlineFrameSession session(calls,sl::ViewportHandle{0u});
        REQUIRE(std::holds_alternative<rk::Error>(session.simulationEnd()));
        REQUIRE(std::holds_alternative<rk::Error>(session.present([]{return S_OK;})));
        REQUIRE(f.events.empty());
        REQUIRE(std::holds_alternative<bool>(session.begin(source())));
        const auto count=f.events.size();f.thread=8;
        REQUIRE(std::holds_alternative<rk::Error>(session.simulationEnd()));
        REQUIRE(std::holds_alternative<rk::Error>(session.begin(source(2))));
        REQUIRE(f.events.size()==count);return;
    }
    rk::FgStreamlineFrameSession session(calls,sl::ViewportHandle{0u});
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(source())));
    REQUIRE(f.events.empty());
}

TEST_CASE("FG session invalidates on SDK failure without repeating the frame",
    "[fg_streamline_session]") {
    Fixture f;
    SECTION("token error") {f.fail="token";}
    SECTION("null token") {f.fail="null";}
    SECTION("sleep error") {f.fail="sleep";}
    SECTION("marker error") {f.fail="marker0";}
    rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(source())));
    REQUIRE(session.failed());const auto count=f.events.size();
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(source(2))));
    REQUIRE(f.events.size()==count);
}

TEST_CASE("FG session rejects mismatched input identity before submission",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    render(f,session);auto packet=f.packet();
    SECTION("source") {++packet.source;}
    SECTION("generation") {++packet.generation;}
    SECTION("token") {++packet.presentToken;}
    SECTION("reset") {++packet.resetEpoch;}
    SECTION("tags") {packet.tags.reset();}
    const auto count=f.events.size();
    REQUIRE(std::holds_alternative<rk::Error>(session.submit(packet)));
    REQUIRE(f.events.size()==count);
}

TEST_CASE("FG session preserves a lower Present failure and consumes its attempt",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    render(f,session);auto packet=f.packet();
    REQUIRE(std::holds_alternative<bool>(session.submit(packet)));
    unsigned presents=0;
    const auto result=session.present([&]{++presents;return DXGI_ERROR_DEVICE_REMOVED;});
    REQUIRE(std::holds_alternative<HRESULT>(result));
    REQUIRE(std::get<HRESULT>(result)==DXGI_ERROR_DEVICE_REMOVED);
    REQUIRE(presents==1);
    REQUIRE(std::holds_alternative<rk::Error>(session.present([&]{++presents;return S_OK;})));
    REQUIRE(presents==1);REQUIRE(f.events.back()=="marker5");
}

TEST_CASE("FG session does not Present after rejected or partial input submission",
    "[fg_streamline_session]") {
    Fixture f;
    SECTION("constants") {f.fail="constants";}
    SECTION("tags") {f.fail="tags";}
    rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    render(f,session);auto packet=f.packet();
    REQUIRE(std::holds_alternative<rk::Error>(session.submit(packet)));
    REQUIRE(session.failed());unsigned presents=0;
    REQUIRE(std::holds_alternative<rk::Error>(session.present([&]{++presents;return S_OK;})));
    REQUIRE(presents==0);
}

TEST_CASE("FG session detects an SDK token changing between phases or within submission",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    SECTION("between simulation phases") {
        REQUIRE(std::holds_alternative<bool>(session.begin(source())));
        ++f.token.index;const auto count=f.events.size();
        REQUIRE(std::holds_alternative<rk::Error>(session.simulationEnd()));
        REQUIRE(f.events.size()==count);
    }
    SECTION("constants callback changes token") {
        render(f,session);auto packet=f.packet();f.mutateConstants=true;
        REQUIRE(std::holds_alternative<rk::Error>(session.submit(packet)));
        REQUIRE(f.events.back()=="constants");
    }
    REQUIRE(session.failed());
}

TEST_CASE("FG session invalidates after each later SDK marker failure",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    REQUIRE(std::holds_alternative<bool>(session.begin(source())));
    SECTION("simulation end") {
        f.fail="marker1";
        REQUIRE(std::holds_alternative<rk::Error>(session.simulationEnd()));
    }
    SECTION("render start") {
        REQUIRE(std::holds_alternative<bool>(session.simulationEnd()));
        f.fail="marker2";
        REQUIRE(std::holds_alternative<rk::Error>(session.renderSubmitStart()));
    }
    SECTION("render end") {
        REQUIRE(std::holds_alternative<bool>(session.simulationEnd()));
        REQUIRE(std::holds_alternative<bool>(session.renderSubmitStart()));
        f.fail="marker3";
        REQUIRE(std::holds_alternative<rk::Error>(session.renderSubmitEnd()));
    }
    REQUIRE(session.failed());const auto count=f.events.size();
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(source(2))));
    REQUIRE(f.events.size()==count);
}

TEST_CASE("FG session consumes Present even when its SDK marker or callback fails",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    render(f,session);auto packet=f.packet();
    REQUIRE(std::holds_alternative<bool>(session.submit(packet)));
    unsigned presents=0;
    SECTION("start marker") {
        f.fail="marker4";
        REQUIRE(std::holds_alternative<rk::Error>(session.present([&]{++presents;return S_OK;})));
        REQUIRE(presents==0);
    }
    SECTION("end marker") {
        f.fail="marker5";
        REQUIRE(std::holds_alternative<rk::Error>(session.present([&]{++presents;return S_OK;})));
        REQUIRE(presents==1);
    }
    SECTION("throwing Present") {
        REQUIRE(std::holds_alternative<rk::Error>(session.present([&]() -> HRESULT {
            ++presents;throw std::runtime_error("Present failure");
        })));
        REQUIRE(presents==1);REQUIRE(f.events.back()=="marker5");
    }
    SECTION("throwing Present changes token") {
        REQUIRE(std::holds_alternative<rk::Error>(session.present([&]() -> HRESULT {
            ++presents;++f.token.index;
            throw std::runtime_error("Present failure after token change");
        })));
        REQUIRE(presents==1);REQUIRE(f.events.back()=="marker4");
    }
    REQUIRE(session.failed());const auto count=presents;
    REQUIRE(std::holds_alternative<rk::Error>(session.present([&]{++presents;return S_OK;})));
    REQUIRE(presents==count);
}

TEST_CASE("FG session callback exceptions cannot escape or restart the session",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    SECTION("new token") {f.throwEvent="token";}
    SECTION("sleep") {f.throwEvent="sleep";}
    SECTION("marker") {f.throwEvent="marker0";}
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(source())));
    REQUIRE(session.failed());const auto count=f.events.size();
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(source(2))));
    REQUIRE(f.events.size()==count);
}

TEST_CASE("FG session rejects a recycled SDK frame index after a completed Present",
    "[fg_streamline_session]") {
    Fixture f;rk::FgStreamlineFrameSession session(f.calls,sl::ViewportHandle{0u});
    render(f,session);auto packet=f.packet();
    REQUIRE(std::holds_alternative<bool>(session.submit(packet)));
    REQUIRE(std::holds_alternative<HRESULT>(session.present([]{return S_OK;})));
    f.nextIndex=101;const auto count=f.events.size();
    REQUIRE(std::holds_alternative<rk::Error>(session.begin(source(2))));
    REQUIRE(session.failed());REQUIRE(f.events.size()==count+1);
    REQUIRE(f.events.back()=="token");
}

TEST_CASE("FG native frame calls reject an absent runtime owner",
    "[fg_streamline_session]") {
    REQUIRE(std::holds_alternative<rk::Error>(rk::makeFgStreamlineFrameCalls({})));
}
