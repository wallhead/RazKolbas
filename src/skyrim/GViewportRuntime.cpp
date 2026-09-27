#include <RE/Skyrim.h>

// RazKolbas uses CommonLib's GViewport ABI without linking the whole
// CommonLib implementation library. Keep this constructor identical to the
// pinned CommonLibSSE-NG source so GetViewport has a live output object.
namespace RE {
GViewport::GViewport() :
    bufferWidth(0),bufferHeight(0),left(0),top(0),width(1),height(1),
    scissorLeft(0),scissorTop(0),scissorWidth(0),scissorHeight(0),
    scale(1.0f),aspectRatio(1.0f),flags(Flag::kNone),pad34(0) {}
}
