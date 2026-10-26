# Changelog

## [0.5.0](https://github.com/lucyd-dev/LucydDeck-firmware/compare/v0.4.0...v0.5.0) (2026-10-26)


### Features

* **usb:** name the deck "LucydDeck" on USB ([#23](https://github.com/lucyd-dev/LucydDeck-firmware/issues/23)) ([bf0f837](https://github.com/lucyd-dev/LucydDeck-firmware/commit/bf0f837bc7e6e3b3da72842cb8a137e01c6147f4)), closes [#22](https://github.com/lucyd-dev/LucydDeck-firmware/issues/22)

## [0.4.0](https://github.com/lucyd-dev/LucydDeck-firmware/compare/v0.3.1...v0.4.0) (2026-10-26)


### Features

* **protocol:** validate entity names against a character whitelist ([bf62a0f](https://github.com/lucyd-dev/LucydDeck-firmware/commit/bf62a0fbb74bcc7e96b1a9760c9fc05b705931c7)), closes [#11](https://github.com/lucyd-dev/LucydDeck-firmware/issues/11)
* **usb:** add CMD_ENTER_BOOTLOADER ([#18](https://github.com/lucyd-dev/LucydDeck-firmware/issues/18)) ([3c583ca](https://github.com/lucyd-dev/LucydDeck-firmware/commit/3c583caf1fe66f4219a110a19fe6d87ff8710d55))
* **usb:** add page and icon delete commands ([f188f0d](https://github.com/lucyd-dev/LucydDeck-firmware/commit/f188f0d38649464ac0ffa2d2d22a15c765d580b0)), closes [#9](https://github.com/lucyd-dev/LucydDeck-firmware/issues/9)


### Bug Fixes

* **board:** install the GPIO ISR service before the panel starts ([b66f274](https://github.com/lucyd-dev/LucydDeck-firmware/commit/b66f2748028fbf26b06bd2ae2dc2935261a17715))
* **display:** repaint the deck when a transfer changes the page on screen ([ebae448](https://github.com/lucyd-dev/LucydDeck-firmware/commit/ebae448fba3890411eb4fc3dec9159f2c60cad14)), closes [#13](https://github.com/lucyd-dev/LucydDeck-firmware/issues/13)
* **storage:** replace an existing file when a transfer finalizes ([9613ea7](https://github.com/lucyd-dev/LucydDeck-firmware/commit/9613ea7ff32440147d562f0e39839f93b143a7a3))
* **storage:** sweep stale transfer files at mount ([b738019](https://github.com/lucyd-dev/LucydDeck-firmware/commit/b738019ffd3108c84232e4a1ad8e151d8eb5b1c4)), closes [#12](https://github.com/lucyd-dev/LucydDeck-firmware/issues/12)
* **usb:** add host sequence reset command ([#7](https://github.com/lucyd-dev/LucydDeck-firmware/issues/7)) ([5cf64f5](https://github.com/lucyd-dev/LucydDeck-firmware/commit/5cf64f52cdb07a9d498ef2ba0921164e3e299b28))

## [0.3.1](https://github.com/lucyd-dev/LucydDeck-firmware/compare/v0.3.0...v0.3.1) (2026-09-18)


### Reverts

* removed longPress sequence completely ([4c46f72](https://github.com/lucyd-dev/LucydDeck-firmware/commit/4c46f7251db71dec4d15e593ba82ec763b7fce9d))

## [0.3.0](https://github.com/lucyd-dev/LucydDeck-firmware/compare/v0.2.0...v0.3.0) (2026-09-13)


### Features

* **protocol:** consolidate device info and split navigation commands ([#4](https://github.com/lucyd-dev/LucydDeck-firmware/issues/4)) ([497d81c](https://github.com/lucyd-dev/LucydDeck-firmware/commit/497d81cca483ddb30bf05582945b70f77ae4a2ed))
* **ui:** add optional backgroundColor config for buttons ([#3](https://github.com/lucyd-dev/LucydDeck-firmware/issues/3)) ([d801234](https://github.com/lucyd-dev/LucydDeck-firmware/commit/d801234bd9a2ca30b24c4477042b56b125b70056))


### Bug Fixes

* **config:** added label assignment at page load ([7548e7b](https://github.com/lucyd-dev/LucydDeck-firmware/commit/7548e7bf4d5bd95f9487b2ac67af9e1177dcea14))

## v0.2.0

refactor: complete firmware rework and architecture overhaul (v0.2.0)

## v0.1.0

Uploaded current working prototype.
