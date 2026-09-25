#include <catch2/catch_test_macros.hpp>
#include "rk/NrStage.hpp"
#include "rk/Settings.hpp"

TEST_CASE("NR evaluation controls can change before GPU initialization",
    "[nr][settings]") {
    auto settings=rk::defaultSettings();
    settings.values["NeuralRendering.Enabled"]=true;
    rk::NrStage stage;
    const auto configured=stage.configure(settings);
    REQUIRE(std::holds_alternative<bool>(configured));
    REQUIRE(std::get<bool>(configured));
    REQUIRE(stage.enabled());

    const auto unchanged=stage.updateRuntime(settings);
    REQUIRE(std::holds_alternative<bool>(unchanged));
    REQUIRE_FALSE(std::get<bool>(unchanged));

    settings.values["NeuralRendering.Style"]=std::int64_t{3};
    settings.values["NeuralRendering.Intensity"]=0.75;
    settings.values["NeuralRendering.SkinStructureStrength"]=rk::Text{"Auto"};
    const auto changed=stage.updateRuntime(settings);
    REQUIRE(std::holds_alternative<bool>(changed));
    REQUIRE(std::get<bool>(changed));

    settings.values["NeuralRendering.Enabled"]=false;
    const auto disabled=stage.updateRuntime(settings);
    REQUIRE(std::holds_alternative<bool>(disabled));
    REQUIRE(std::get<bool>(disabled));
    REQUIRE_FALSE(stage.enabled());
}

TEST_CASE("NR live controls reject invalid skin values","[nr][settings]") {
    auto settings=rk::defaultSettings();
    rk::NrStage stage;
    REQUIRE(std::holds_alternative<bool>(stage.configure(settings)));
    settings.values["NeuralRendering.SkinStructureStrength"]=rk::Text{"invalid"};
    REQUIRE(std::holds_alternative<rk::Error>(stage.updateRuntime(settings)));
}

TEST_CASE("NR live enable preserves creation-contract validation","[nr][settings]") {
    auto settings=rk::defaultSettings();
    settings.values["NeuralRendering.PassCount"]=std::int64_t{3};
    rk::NrStage stage;
    REQUIRE(std::holds_alternative<bool>(stage.configure(settings)));
    settings.values["NeuralRendering.Enabled"]=true;
    REQUIRE(std::holds_alternative<rk::Error>(stage.updateRuntime(settings)));
    REQUIRE_FALSE(stage.enabled());
}

TEST_CASE("NR updates remain unavailable after startup configuration rejection",
    "[nr][settings]") {
    auto settings=rk::defaultSettings();
    settings.values["NeuralRendering.Enabled"]=true;
    settings.values["NeuralRendering.Backend"]=rk::Choice{"Streamline"};
    rk::NrStage stage;
    REQUIRE(std::holds_alternative<rk::Error>(stage.configure(settings)));
    settings.values["NeuralRendering.Backend"]=rk::Choice{"Direct"};
    REQUIRE(std::holds_alternative<rk::Error>(stage.updateRuntime(settings)));
}
