#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <filesystem>

// Isolated probe only. A partial/uncertain GPU lifecycle terminates the child.
// The core module stays resident until process exit; device outlives NR shutdown.
void runNrFeatureExperiment(HMODULE nr, ID3D12Device* device, const std::filesystem::path& corePath);
