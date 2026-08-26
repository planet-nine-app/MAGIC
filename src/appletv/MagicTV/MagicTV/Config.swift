import Foundation

/// This app is "B" (TV, Target) in README-DEV.md's demo:
/// A[User's Client<Caster>] -> B[TV<Target>] -> C{Streaming Server<Gateway>} ->
/// D[Streamer's Client<Gateway>] -> E[Game Client<Gateway>] -> F{Server<Resolver>}
///
/// No config.h.example-style copy step here (Xcode projects don't have an
/// equivalent convention) - these are real working defaults matching the
/// rest of the demo chain's local ports. Edit directly for your setup.
enum Config {
    /// This app's own MAGIC stopName - must match the entry this app adds
    /// to spell.gateways, and must be what the NEXT hop's local spellbook
    /// expects to see immediately before itself.
    static let stopName = "tv"

    /// The spell this app relays. Must match a spellbook entry (single-hop,
    /// fount-only) in fount's own spellbooks/spellbook.js - see
    /// ../../streaming/README.md for the full chain wiring and why each
    /// participant's spellbook copy must differ.
    static let spellName = "throwPotion"

    /// fount instance this app self-registers its own Sessionless identity
    /// with. Point at a locally-running fount for development.
    static let fountBase = "http://localhost:3006"

    /// Next hop in the chain - C, the streaming server.
    static let nextHopURL = URL(string: "http://localhost:3200/magic/spell/\(spellName)")!

    /// Local port this app listens on for incoming casts (from A, the
    /// caster - see ../../streaming/demo/caster.js). The tvOS Simulator
    /// shares the host Mac's network stack, so this is reachable at
    /// http://localhost:8787/magic/spell/throwPotion from the host while
    /// running there - a real device needs its LAN IP instead.
    static let listenPort: UInt16 = 8787

    /// Placeholder video - a well-known public Apple sample HLS stream, so
    /// this app has something real to play out of the box. Point this at
    /// wherever your actual OBS output ends up (OBS -> RTMP -> a media
    /// server producing HLS, e.g. node-media-server or nginx-rtmp - that
    /// relay isn't built by this project, see ../../streaming/README.md).
    static let hlsURL = URL(string: "https://devstreaming-cdn.apple.com/videos/streaming/examples/bipbop_4x3/bipbop_4x3_variant.m3u8")!
}
