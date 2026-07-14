#include <doctest/doctest.h>

#include <pxr/base/plug/registry.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

// Harness self-tests: prove the doctest + USD bootstrap works before any
// translate/ tests exist. If these fail, the problem is infrastructure
// (main.cpp's plugin registration, the DLL closure next to the exe, the
// stage-1 install), not translation code.

TEST_CASE("harness: core USD plugins are registered") {
	PXR_NS::PlugRegistry &registry = PXR_NS::PlugRegistry::GetInstance();
	CHECK(registry.GetPluginWithName("usd") != nullptr);
	CHECK(registry.GetPluginWithName("sdf") != nullptr);
}

TEST_CASE("harness: an in-memory stage can be created and traversed") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::CreateInMemory();
	REQUIRE(stage != nullptr);
	CHECK(stage->GetPseudoRoot().IsValid());
}
