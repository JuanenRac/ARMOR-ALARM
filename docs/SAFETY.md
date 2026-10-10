# Safety: what this project does and does not claim

- **This is not a certified alarm system.** It is a home project: its rules and its firmware are tested on a computer and in a browser against a stand-in node, and have never run on a board or on a real installation. Do not rely on it where a certified system, or a monitoring service, is required, and keep the protections the installation already has (smoke detectors that sound by themselves, for example).
- **A siren is loud.** It stops by itself after a time; keep that time reasonable and respect the rules of where you live about sirens.
- **A wired loop and a siren relay carry low voltage.** Mains never goes on the node's board; whatever a relay switches is wired by someone qualified, with its own protection. See the base board's notes in ARMOR-ELECTRICAL.
- **A smoke detector or a panic button that is sounding is never left out** when arming is forced.
- **The PIN** is checked without leaking where it differs, and repeated wrong PINs lock the panel for a while. A PIN is only as private as the place it is typed and the way it is stored; the settings that hold it must be protected.
- **The server's commands are off by default.** The server may arm and disarm only when the node's setting, the broker's access list and the server's own switch all allow it; a disarm from the server carries no PIN, so whoever can publish on the broker's command topic can disarm. Keep that permission to the server's account and the broker on a protected network.
- **A fault of the node is not an alarm.** A zone whose pin cannot be read reports nothing, and the siren relay starts open on every restart, so a restart never sounds the siren - and a node that has stopped is not guarding either.
- **When the server is away** the panel decides alone. If the panel or the radio's node is also down, nobody guards: plan the power and, if it matters, a second radio.
