# FlipOS Third-Party Notices

FlipOS includes software from third-party projects. Their copyrights and
licenses remain with their respective authors.

## Gambatte / libretro port

- Component: Gambatte libretro core
- Vendored source: `external/gambatte-libretro/`
- License: GNU General Public License version 2

The complete GPLv2 license text is preserved at
`external/gambatte-libretro/COPYING`.

## mGBA

- Component: mGBA
- Vendored source: `external/mgba/`
- License: Mozilla Public License 2.0

The complete MPL 2.0 license text is preserved at
`external/mgba/LICENSE`.

## uMTP-Responder

- Project: uMTP-Responder
- Upstream: https://github.com/viveris/uMTP-Responder
- Vendored revision: 2241a803c27f1fff7537cb9504befeb810b880d3
- Copyright: Viveris Technologies and contributors
- License: GNU General Public License version 3 or later

The corresponding vendored source used to build the FlipOS MTP responder is
kept in `external/uMTP-Responder/`. The upstream license text is preserved at
`external/uMTP-Responder/LICENSE`.

FlipOS carries a small build-time adaptation for Miyoo Flip kernels that are
compiled without POSIX message queues. It disables uMTP-Responder's optional
runtime command queue; normal MTP file-transfer operation is unchanged.

## Source availability

The source used to build the above components is included in the public FlipOS
repository alongside the FlipOS source changes that integrate them.
