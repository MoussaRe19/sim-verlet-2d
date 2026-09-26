# Basic 2D Verlet Physics Simulation

Networked 2D particle/cloth physics. Server runs an authoritative Verlet
simulation, Raylib clients connect, render the world, and can grab/drag/pin
particles.

Started as a single-process physics simulation, then turned it into 
client/server to learn server authority and state sync.
## Notes

- Server owns the only `World` that simulates. Clients just render.
- Physics runs on a fixed 60Hz tick, decoupled from networking.
- Clients send intents (grab/drag/release/pin), server validates and
  applies them — no direct client writes to particle state.
- Snapshots stream as unreliable/unsequenced chunks at 20Hz, tick-stamped
  so the client can drop stale ones. Chosen over ENet fragmentation, where
   losing a fragment prevents the fragmented packet from being reassembled
- Ownership is broadcast so clients see each other's grabs.

## Dependencies

* C99 (gcc/clang), GNU Make
* [Raylib](https://www.raylib.com/) — client only
* [ENet](http://enet.bespin.org/), [yyjson](https://github.com/ibireme/yyjson) — vendored

## Build

```bash
make        # sim_server, sim_client
make clean
```

## Run

```bash
./sim_server [-p port] [-m model.json] [--max-particles n] [--max-constraints n]
./sim_client [-a addr] [-p port] [-m model.json]
```

Client and server must load the **same model file** — topology never goes
over the wire, only positions.

| Input | Action |
|---|---|
| Left click | Grab |
| Left click + drag | Move |
| Release | Let go |
| Right click | Toggle pin |

## Layout

```
src/
├── core/     Verlet physics + World
├── io/       JSON model loading
├── net/      protocol, dispatch, ENet transport
├── client/   raylib rendering + input
├── utils/    vec math, rng
```

`World` is opaque outside `core/` — real fields in `world_internal.h`.
## License

Unlicense / Public domain.

> **Note:** *Toy project / prototype — code refactoring in progress.*
