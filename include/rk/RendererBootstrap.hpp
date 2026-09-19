#pragma once
#include "rk/RendererHook.hpp"
#include "rk/Settings.hpp"
namespace rk {
using RendererObserved = void(*)(const RendererSnapshot&);
Result<bool> installRendererObserver(const Settings& settings, RendererObserved notification);
bool rendererObserverArmed() noexcept;
}
