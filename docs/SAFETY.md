# Safety: what this project does and does not claim

- **This is not a certified alarm system.** It is a home project: its rules are tested on a computer and have never run on a real installation. Do not rely on it where a certified system, or a monitoring service, is required, and keep the protections the installation already has (smoke detectors that sound by themselves, for example).
- **A siren is loud.** It stops by itself after a time; keep that time reasonable and respect the rules of where you live about sirens.
- **A wired loop and a siren relay carry low voltage.** Mains never goes on the node's board; whatever a relay switches is wired by someone qualified, with its own protection. See the base board's notes in ARMOR-ELECTRICAL.
- **A smoke detector or a panic button that is sounding is never left out** when arming is forced.
- **The PIN** is checked without leaking where it differs, and repeated wrong PINs lock the panel for a while. A PIN is only as private as the place it is typed and the way it is stored; the settings that hold it must be protected.
- **When the server is away** the panel decides alone. If the panel or the radio's node is also down, nobody guards: plan the power and, if it matters, a second radio.
