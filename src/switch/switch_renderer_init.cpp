// Minimal R_BeginRegistration replacement for the Switch port.
//
// The upstream stub in posix_backbone_stubs.cpp does nothing, leaving
// `frontEndDataOut`, `s_cmdList`, and the inner command-buffer pointers
// inside s_backEndData[] uninitialized. Once we drop the Switch-only
// no-op from SCR_UpdateScreen, the very first R_BeginSharedCmdList
// dereferences those null pointers and the guest faults.
//
// This file fills the bare-minimum slots so the engine's command-pumping
// path can run end to end. No GL is actually called yet — commands are
// pushed into static buffers and discarded by the matching backend
// stubs. Once the queue is reliably populated, the next bite will start
// translating those commands into src/gfx_gl/ draw calls.

#include <cstdint>
#include <cstring>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "client_mp/client_mp.h"
#include "gfx_d3d/r_rendercmds.h"
#include "gfx_d3d/r_init.h"

// Engine-side globals — defined in src/gfx_d3d/r_rendercmds.cpp, just
// missing their inner allocations because R_InitRenderCommands never ran.
extern GfxBackEndData s_backEndData[2];
extern GfxCmdArray g_frontEndCmds[2];
extern GfxBackEndData *frontEndDataOut;
extern GfxCmdArray *s_cmdList;
extern unsigned int s_smpFrame;
extern unsigned int s_renderCmdBufferSize;
extern int s_renderCmdWarnSize;

namespace {

// Static command-buffer storage so we don't depend on Hunk / R_AllocGlobalVariable
// during early bring-up. ~1 MiB per frame matches the order of magnitude the
// upstream `s_renderCmdBufferSize = 98304 * maxClientViews` lands on.
constexpr size_t kCmdBufferSize = 1 << 20;
alignas(16) uint8_t g_cmdBuffer[2][kCmdBufferSize];

} // namespace

void R_BeginRegistration(vidConfig_t *vidConfigOut)
{
    for (int i = 0; i < 2; ++i) {
        g_frontEndCmds[i].cmds = g_cmdBuffer[i];
        g_frontEndCmds[i].usedTotal = 0;
        g_frontEndCmds[i].usedCritical = 0;
        g_frontEndCmds[i].lastCmd = nullptr;
        s_backEndData[i].commands = &g_frontEndCmds[i];
    }
    s_smpFrame = 0;
    frontEndDataOut = &s_backEndData[0];
    s_cmdList = &g_frontEndCmds[0];
    s_renderCmdBufferSize = static_cast<int>(kCmdBufferSize);
    s_renderCmdWarnSize = static_cast<int>(kCmdBufferSize) * 3 / 4;

    // Leaving rg.registered = 0 keeps R_BeginFrame and Material_*Override
    // out of the path (both touch unregistered renderer dvars). We reset
    // the command queue ourselves from switch_main between Com_Frame
    // iterations so RC_* opcodes don't accumulate.

    if (vidConfigOut) {
        *vidConfigOut = cls.vidConfig;
    }

#ifdef __SWITCH__
    static const char msg[] = "[switch_renderer_init] R_BeginRegistration done";
    svcOutputDebugString(msg, sizeof(msg) - 1);
#endif
}

void switch_reset_render_queue()
{
    // Counterpart to what R_BeginFrame + R_ClearCmdList would do upstream.
    for (int i = 0; i < 2; ++i) {
        g_frontEndCmds[i].usedTotal = 0;
        g_frontEndCmds[i].usedCritical = 0;
        g_frontEndCmds[i].lastCmd = nullptr;
    }
}

namespace gfx_gl { void apply_clear_color(float r, float g, float b, float a); }

void switch_dispatch_render_queue()
{
    // Walks the front-end command queue produced by SCR_UpdateScreen and
    // hands off the few opcodes we can already translate into GLES2 calls.
    // Unknown commands fall through so we don't stall the pipeline — they
    // get picked up bite by bite as we wire each RC_* into src/gfx_gl/.
    const GfxCmdArray *list = &g_frontEndCmds[0];
    if (!list->cmds || list->usedTotal <= 0) return;

    size_t pos = 0;
    while (pos < (size_t)list->usedTotal) {
        const auto *hdr = reinterpret_cast<const GfxCmdHeader *>(list->cmds + pos);
        if (hdr->id == 0 || hdr->byteCount == 0) break;
        switch (hdr->id) {
        case 4: { // RC_CLEAR_SCREEN
            const auto *cmd = reinterpret_cast<const GfxCmdClearScreen *>(hdr);
            gfx_gl::apply_clear_color(cmd->color[0], cmd->color[1], cmd->color[2], cmd->color[3]);
            break;
        }
        default:
            break;
        }
        pos += hdr->byteCount;
    }
}
