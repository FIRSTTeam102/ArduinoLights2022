# (this branch aims to add support for pattern control through digital pin values)
## Team 102's main Arduino light strip patterns
[Simple online light simulator (written in Python; hosted on https://trinket.io)](https://trinket.io/python/502dfc496f?runOption=run&showInstructions=true)

## Patterns and (possible) uses
Patterns that use the `alliance` variable (and can change color based on which alliance we are on) are marked with '*'

| Enum | Name | Description |
| ---- | ---- | ----------- |
| 0 | `P_OFF` | 🚫 Off (empty pattern) |
| 1 | `P_DEFAULT`* | 🔥 "It's fire" |
| 2 | `P_DISABLED` | ⛔ Alternating orange & black/off |
| 3 | `P_AUTO` | 🔁 Opposing blue sliders |
| 4 | `P_INTAKE` | 🟣 Purple sine slider on black/off background |
| 5 | `P_LIMELIGHT` | 🟢 Green (lime) slider on black/off background |
| 6 | `P_SHOOTING` | 🟠 Orange sine slider on black/off background |
| 7 | `P_CLIMBING` | ⏩ Double rainbow slider on black/off background |
| 8 | `P_ALLIANCE`* | 🚩 Similar to P_AUTO, but represents us on either alliance |
| 9 | `P_A_FIRE`* | 🔥 Alliance-colored less varied fire for better alliance representation |