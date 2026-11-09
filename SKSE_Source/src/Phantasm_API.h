// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Severause
#pragma once
// Phantasm_API.h - the public API of Phantasm, the shared actor renderer (MIT). Copy this header unchanged into your
// plugin; it needs only the standard library. docs/API.md shows how to use it.
//
//   // At SKSE's kDataLoaded or later:
//   auto* get = reinterpret_cast<PHANTASM_API::RequestPluginAPIFn>(
//       GetProcAddress(GetModuleHandleW(L"Phantasm.dll"), "RequestPluginAPI"));
//   auto* api = get ? static_cast<const PHANTASM_API::Interface1*>(get(PHANTASM_API::kInterface1)) : nullptr;
//   if (!api) return;   // Phantasm absent or too old: no picture, never fail hard
//   PHANTASM_API::ClientId me = 0;
//   api->RegisterClient("mymod", &OnPhantasmEvent, nullptr, &me);
//   PHANTASM_API::ViewId view = 0;
//   api->OpenView(me, "face", actorFormId, 256, 320, PHANTASM_API::Framing::Figure, &view);
//   // ...OnPhantasmEvent gets Shown with the image name, its URL and the SRV; show the image on your page.
//   // CloseView before the page goes away: a view nobody closes keeps drawing, during play too.
//
// A view is published as a Magelight texture image when Magelight UI with GPU texture images (its D3D11 renderer) is
// installed. With no Magelight at all (or one too old for texture images, which counts as none), since 1.0.0, a view
// whose client set its `hostless` hint draws with no image: Shown carries the SRV alone (imageName and imageUrl empty)
// for the client's own D3D11 draw (another UI host, an overlay); any other view ends "no-image" there, as before. A
// Magelight that cannot take images (its CPU renderer, or before its first in-game frame) ends views "no-image". A
// Cutout, Portrait or item still draws on its own and needs no UI host (since 0.2.0; pluginVersion). Phantasm runs on
// Skyrim SE, AE and VR. Live views and stills work on Skyrim VR too, but VR has been tested less than SE and AE.
//
// What is frozen: every function, struct field, enum value and reason token below keeps its meaning forever.
// Interface1 grows only by appending, so RequestPluginAPI(kInterface1) keeps answering with it: a later build adds
// functions to the end of a table and fields to the end of a struct, and each carries its size. A function or field
// marked as added later is there only when `size >= offsetof(<struct>, <name>) + sizeof(<name>)`, so check before
// calling or reading it (a non-null Interface1 from an older build ends before them). An appended field starts at or
// past the older struct's sizeof, never in its tail padding, or that test would pass against it. What is NOT frozen:
// the hints and queries (SetHint / QueryHint: their keys, values and answers) and the renderer's look (pose, light,
// camera), which may change in any release; a client must keep working when a hint is ignored.
//
// Threads: every function but RebuildActor3D may be called from any thread, inside Magelight's Present included, and
// never blocks. A call that returns Ok has only been accepted: its work runs later on the game thread, view and hint
// changes in the order they were made, and its outcome arrives as an event. A call that returns anything else changes
// nothing and no event follows it. Events are delivered on the game thread, outside any render phase, so a handler may
// read RE:: state and call this API again (RebuildActor3D excepted, see it).

#include <cstdint>

struct ID3D11ShaderResourceView;

namespace PHANTASM_API
{
    inline constexpr std::uint32_t kInterface1 = 1;

    /// The returned pointer is the interface table for that version (cast it to Interface1* for kInterface1), or null
    /// when this Phantasm does not serve that version. The table lives as long as the process.
    using RequestPluginAPIFn = void* (*)(std::uint32_t interfaceVersion);

    using ClientId = std::uint32_t;   // 0 = none
    using ViewId   = std::uint32_t;   // 0 = none; never reused within a process

    enum class Result : std::int32_t
    {
        Ok            = 0,   // accepted (the outcome comes as an event)
        BadArgument   = 1,   // a null out pointer, a name that breaks the naming rule, a zero size, a NaN or
                             // infinite Orbit / Zoom number...
        UnknownClient = 2,
        UnknownView   = 3,   // never opened, or closed: Ended, or closed with its Ended still on the way (CloseView,
                             // a game load or a refusal removes a view at once and tells the client later)
        NotOwner      = 4,   // reserved
        NameTaken     = 5,   // RegisterClient: another client holds that name
        Unsupported   = 6,   // not in this build (Framing::Portrait for a live view)
        BadThread     = 7,   // RebuildActor3D from the wrong thread, from an event handler or inside a render phase
        TooMany       = 8,   // a client may hold at most 16 views
    };

    /// What part of the actor a view shows. The renderer chooses the camera, light and pose; these name the outcome.
    enum class Framing : std::uint32_t
    {
        Figure   = 0,   // the whole figure, head to feet
        Portrait = 1,   // a still only: head and shoulders drawn on their own as a Cutout is (OpenView: Unsupported)
        Cutout   = 2,   // a still only: the whole figure drawn on its own for a cutout (see RequestStill)
    };

    enum class EventKind : std::uint32_t
    {
        /// The view is drawing. imageName / imageUrl / srv / width / height are valid (imageName and imageUrl empty
        /// for a hostless view, which has no image). Sent on its first frame (up
        /// to about 5 s after OpenView while the actor's 3D settles; nothing is sent meanwhile) and again whenever
        /// any of them changes (a resize rebuilds the offscreen); `lastSeen` says the picture is the actor's last
        /// look this session, kept while they cannot be drawn live (unloaded, mid-rebuild).
        Shown = 1,
        /// The view is open but not drawing; `reason` says why. Its image turns transparent (a hostless view's client
        /// stops drawing the SRV, released at this event). Orbit, Zoom and Refresh
        /// are dropped while it waits, and its camera starts fresh when it draws again. It comes back with Shown by
        /// itself once the views opened after it close and the renderer can draw again.
        Waiting = 2,
        /// The view is gone for good and its id is dead; `reason` says why. Sent once, after which nothing more is
        /// sent for it. CloseView, a game load and every refusal of an accepted view end here: open it again to retry.
        /// It can arrive after the client has opened the same slot again under a new id (a game load's Ended can come
        /// after kPostLoadGame), so match events by view id, never by slot or image name.
        Ended = 3,
        /// The answer to QueryHint: `key` is the key asked about (lower case), `json` the answer (docs/DESIGN.md).
        Reply = 4,
    };

    /// Reasons are short lower-case tokens; a client must treat an unknown one as "the picture is not available".
    /// Waiting: "busy" (the view cannot draw now: more views are open than draw at once, the newest drawing, or the
    /// renderer is waiting out a GPU refusal; either way it comes back by itself).
    /// Ended: "closed" (CloseView), "game-load" (a save was loaded or a new game started: open views again after
    /// SKSE's kPostLoadGame / kNewGame; it may arrive after that), "missing" / "dead" / "unrenderable" (the actor
    /// cannot be drawn; "unrenderable" also for a view opened before Phantasm's kDataLoaded), "not-loaded" (too far
    /// to draw and no look kept this session), "collect-failed", "vr" (reserved: not sent by this build),
    /// "no-image" (Magelight is installed but draws no GPU images: on its CPU renderer, or not yet past its first
    /// in-game frame, when opening the view again later succeeds; also with no Magelight or one too old, unless the
    /// view's `hostless` hint is on, since 1.0.0),
    /// "gpu-unavailable", "image-register-failed", "faulted" (a frame crashed and the view was stopped).
    struct Event
    {
        std::uint32_t             size;        // sizeof(Event) of the Phantasm that sent it; read fields within it
        EventKind                 kind;
        ViewId                    view;
        std::uint32_t             actorFormId; // the actor the view shows
        const char*               reason;      // Waiting / Ended, else ""
        const char*               imageName;   // Shown: "phantasm.<client>.<slot>", else "" (and for a hostless view)
        /// Shown: Magelight's URL for an <img src> or CSS url(), else "" (and for a hostless view, since 1.0.0: draw
        /// the srv yourself). Give the element a CSS width AND height
        /// from the view's own size (OpenView's / ResizeView's width and height over devicePixelRatio), with
        /// object-fit fill (the default) and never height:auto: the image's natural size is the slot's first
        /// offscreen, fixed for the life of the process (a resize or a reopen at another size keeps it), and it is
        /// supersampled, so it is neither the view's size nor its aspect. Never size it from width / height below.
        const char*               imageUrl;
        /// Shown: the offscreen, for an ImGui::Image or your own D3D11 draw on the engine's device. Alive for the
        /// call only: AddRef it to draw it later and Release it at the view's next Shown (a rebuilt offscreen, after
        /// a resize, comes with one), Waiting or Ended, or at ReleaseClient, after which no event comes (the
        /// offscreen is released while the view waits). A Reply never rebuilds or releases it: keep it across one.
        /// Never write to it.
        ID3D11ShaderResourceView* srv;
        std::uint32_t             width;       // Shown: the offscreen's size in pixels (supersampled, about 2x)
        std::uint32_t             height;
        bool                      lastSeen;    // Shown: see Shown
        const char*               key;         // Reply: the key asked about
        const char*               json;        // Reply: the answer
    };
    /// Every string in an Event lives only for the call: copy what you keep.
    using EventFn = void (*)(const Event* event, void* user);

    /// RequestStill's answer. reason tokens: not-drawing, view-ended, timed-out, read-failed; for a Cutout or a
    /// Portrait also busy, cancelled, missing, dead, not-loaded, not-drawn, gpu-unavailable, faulted and the view
    /// refusals (no-image...); for an item still busy, cancelled, missing, no-model, read-failed, gpu-unavailable,
    /// faulted and the view refusals. view-drawing (a view drew, so the shot could not) is not sent since 0.4.0.
    struct Still
    {
        std::uint32_t       size;
        std::uint32_t       requestId;
        bool                ok;
        const char*         reason;          // when !ok, a token as above
        std::uint32_t       width, height;
        const std::uint8_t* rgba;            // width * height * 4 bytes, sRGB: on the view's backdrop, or for a
                                             // Cutout, a Portrait or an item still premultiplied by alpha in linear
                                             // light on transparent black
        float               standingPx;      // a Cutout or a Portrait: the figure's standing height in the picture's
                                             // pixels; else 0
        std::uint32_t       reserved0;       // since 0.4.0: always 0 (keeps the field below past the 0.3.0 end)
        /// Since 0.4.0 (read it only when size covers it): a Cutout or a Portrait drawn in the classic pose because
        /// some of the actor's skins were not bound to their skeleton (docs/DESIGN.md, Stills). Usually the engine
        /// binds them later, and a shot taken then shows the actor as the game does, so a client that keeps the
        /// picture may ask again; a head part the game hides can stay unbound for good. False for every other still.
        bool                classicPose;
    };
    using StillFn = void (*)(const Still* still, void* user);

    struct Interface1
    {
        std::uint32_t size;               // sizeof(Interface1) of this Phantasm
        std::uint32_t interfaceVersion;   // kInterface1
        std::uint32_t pluginVersion;      // MAJOR*10000 + MINOR*100 + PATCH

        /// Registers a client by name: 1-24 characters of a-z, 0-9, '-' and '_' (it is part of every image name).
        /// One registration per name per process; onEvent gets every event of the client's views.
        Result (*RegisterClient)(const char* name, EventFn onEvent, void* user, ClientId* out);
        /// Ends every view of the client and frees the name; no event is sent for those views, so release any SRV
        /// kept from them now. Called on the game thread (a handler included), nothing reaches the client after it
        /// returns; from another thread, one event already being delivered may still arrive.
        Result (*ReleaseClient)(ClientId client);

        /// Opens a live view of an actor in one of the client's slots (same naming rule as the client name). Its
        /// image is "phantasm.<client>.<slot>" for the life of the process, so a page may name it ahead of Shown.
        /// Slots are a small fixed set the client reuses, one per place on its page, never a per-actor id: every slot
        /// ever opened keeps its image registered until the process ends. Opening a slot that already holds a view
        /// keeps that view (same id) and points it at this actor, size and framing. width / height: the size the
        /// picture is shown at, in device pixels (CSS px x devicePixelRatio); the renderer supersamples on its own. A
        /// side under 4 is accepted and drawn at 4 (the page scales it down); zero is BadArgument. actorFormId 0x14
        /// is the player.
        Result (*OpenView)(ClientId client, const char* slot, std::uint32_t actorFormId, std::uint32_t width,
                           std::uint32_t height, Framing framing, ViewId* out);
        /// Close the view before the page showing it goes away.
        Result (*CloseView)(ViewId view);
        /// The view's new size, by OpenView's rules.
        Result (*ResizeView)(ViewId view, std::uint32_t width, std::uint32_t height);
        /// Turns the camera around the actor: dx degrees of yaw, dy pixels of pointer travel for pitch (a drag).
        /// A NaN or infinite number is BadArgument.
        Result (*Orbit)(ViewId view, float dx, float dy);
        /// Zooms toward the point (u, v) of the picture, 0..1 from the top left (toward the face, whatever u and v,
        /// while the `aim` hint is face); dz is a wheel's deltaY. A NaN or infinite number is BadArgument.
        Result (*Zoom)(ViewId view, float dz, float u, float v);
        /// The actor's look changed (an equip, a preset): the view takes it on its next frame.
        Result (*Refresh)(ViewId view);

        /// Best-effort, unfrozen view settings (docs/DESIGN.md lists the keys this build knows; keys are
        /// case-insensitive). An unknown key is ignored; an empty value removes the hint (the default applies); a
        /// value is applied when the view draws.
        Result (*SetHint)(ViewId view, const char* key, const char* value);
        /// Asks for a hint's current value or choices; the answer comes as a Reply event. Only the query keys
        /// docs/DESIGN.md lists are answered: "idles" and "expression" on an actor view, and since 1.0.0 "freeze" on
        /// an actor view and "stats" on an actor or item view. Every other key, SetHint's keys included, answers
        /// {"known":false}, which is how a client tells a build without one.
        Result (*QueryHint)(ViewId view, const char* key);

        /// One picture of an actor, handed to done once on the game thread (rgba alive for the call only), scaled to
        /// fit width x height (docs/DESIGN.md, Stills). Figure: what the drawing view of that actor shows. Cutout: the
        /// actor drawn on their own (their standing idle, a neutral face, the Hearth light, no weapons unless the
        /// weapons hint asks, no floor, transparent ground); since 0.4.0 beside any views (before: refused
        /// "view-drawing" while a view drew, and cancelled by a view starting). While another shot (a Cutout, a
        /// Portrait or an item still) runs it waits its turn (before 0.2.0: "busy"; since then "busy" only once 32
        /// wait); a shot whose frames stop coming answers "timed-out" after about 10 s. Portrait: the same shot of their head and shoulders (a build whose `size`
        /// does not cover RequestStillHinted answers it Unsupported). A shot scaled below its own size is averaged
        /// in sRGB, which darkens soft edges: ask for a large size and scale it yourself in linear light.
        Result (*RequestStill)(ClientId client, std::uint32_t actorFormId, std::uint32_t width, std::uint32_t height,
                               Framing framing, StillFn done, void* user, std::uint32_t* requestId);

        /// Rebuilds an actor's 3D (Update3DModel) at once so that no view of them shows a blank or a torn frame: the
        /// views let go of the old 3D first and take the new one at once. Call it instead of Update3DModel, from
        /// SKSE's UI-task queue (AddUITask) or a game event on the main thread: never inside SKSE's AddTask drain,
        /// where RaceMenu's armor hook can deadlock it, and never inside a render phase (Magelight's Present, a
        /// menu's PostDisplay). Returns BadThread off the main thread (on SE/AE), inside Phantasm's own render phase,
        /// and from inside a Phantasm event handler (which runs in the AddTask drain: post the rebuild with
        /// AddUITask). A UI task drained during a loading screen can run off the main thread and then gets
        /// BadThread: post the rebuild again after the load (or at the actor's next look change). `who` names the caller in
        /// Phantasm's log.
        Result (*RebuildActor3D)(std::uint32_t actorFormId, const char* who);

        /// Opens a live view of an item (a base form with a model: armour, weapon, book, misc item...) in one of the
        /// client's slots, by OpenView's rules: the slot's image name, its size, a held slot re-targeted (a slot that
        /// showed an actor shows the item). Actor and item views count alike: the newest open views of either kind
        /// draw (up to 4, 2 on VR, since 0.4.0; one before), the others wait (docs/DESIGN.md section 6). Orbit, Zoom, ResizeView and CloseView act on it as on an actor view, and its
        /// events are the same (actorFormId carries the item's FormID); Refresh and stills do not apply to it, and of
        /// the hints only `worn` (since 0.3.0: male / female draws an armor as worn by a man or a woman, its addons'
        /// models without the body skin they carry; off, the default, its ground model; changing it loads the item
        /// again) and `hostless` (since 1.0.0, as an actor view's), and of the queries only "stats" (since 1.0.0). A
        /// form with no model draws an empty card. While an item view draws, an item-menu carrier is open and the game
        /// refuses to save. Check `size` before calling: older builds end the table above.
        Result (*OpenItemView)(ClientId client, const char* slot, std::uint32_t itemFormId, std::uint32_t width,
                               std::uint32_t height, ViewId* out);
        /// Ends the client's Cutout, Portrait or item still `requestId` if it is still running: its done is called
        /// with "cancelled". Ok, and nothing happens, for a request already answered or another client's. Added after
        /// Interface1's first build: check `size` first.
        Result (*CancelStill)(ClientId client, std::uint32_t requestId);

        /// RequestStill with still hints: "key=value" pairs separated by ';' (keys any case). Like view hints they are
        /// unfrozen and best-effort: an unknown key or value is ignored. This build knows, for a Cutout or a Portrait,
        /// `turn` (the camera's turn from the figure's front in degrees, toward the figure's left for a positive
        /// turn; the default is a slight turn), `lighting` (a lighting hint preset: studio, daylight, hearth,
        /// torchlight, moonlight; the default is hearth), and since 0.2.0 `pitch` (degrees above level, at most 80
        /// either way; the default is a little above) and `weapons` (on / off: draw what they carry; off by
        /// default), and since 1.1.0, for a Portrait only, `aim` (face: the camera faces the actor's head from where
        /// it looks, turn and pitch measured from there; figure, the default, from their heading) and `zoom` (0.5 to
        /// 3: 1, the default, frames the head and shoulders, about 2 the face alone, the crown kept in the frame); a
        /// Figure still takes the view's own camera and light and ignores them. hints may be null.
        /// Since 0.2.0 shots wait in a queue behind the one running ("busy" only past 32 waiting), and shots of one
        /// actor in a row share its loaded look, so ask for every angle at once. Added after Interface1's first
        /// build: check `size` first.
        Result (*RequestStillHinted)(ClientId client, std::uint32_t actorFormId, std::uint32_t width,
                                     std::uint32_t height, Framing framing, const char* hints, StillFn done, void* user,
                                     std::uint32_t* requestId);
        /// One picture of an item's model (a base form with a model, as OpenItemView draws it, with its effects and
        /// particles run for a moment), drawn on its own, on a transparent ground, scaled to fit width x height,
        /// handed to done once on the game thread. Since 0.4.0 it draws beside any views (before: refused
        /// "view-drawing" while a view drew); it waits behind a running shot as RequestStill's do (before 0.2.0:
        /// "busy"). Hints: `turn` (degrees from the
        /// item view's opening side), since 0.2.0 `pitch` (degrees above level; 15 by default), since 0.3.0 `worn`
        /// (male / female: an armor drawn as worn by a man or a woman, its addons' models without the body skin they
        /// carry, `turn` then measured from the wearer's front; off by default, the ground model). While it draws, an
        /// item-menu carrier is open for about half a second (saving is refused meanwhile). Since 0.3.0 shots of one
        /// item (and one `worn`) in a row share its load and take a fraction of that, and the queue takes the next
        /// shot of the subject just shot ahead of older ones, so ask for every angle at once. Added after Interface1's
        /// first build: check `size` first.
        Result (*RequestItemStill)(ClientId client, std::uint32_t itemFormId, std::uint32_t width, std::uint32_t height,
                                   const char* hints, StillFn done, void* user, std::uint32_t* requestId);
    };
}

#ifdef PHANTASM_EXPORTS
/// Exported by Phantasm.dll; a client reaches it with GetProcAddress (see RequestPluginAPIFn).
extern "C" __declspec(dllexport) void* RequestPluginAPI(std::uint32_t interfaceVersion);
#endif
