# FlipOS Beta 1 Installation

## Before you begin

FlipOS Beta 1 is **beta software**. Back up anything important from your SD card before installing.

The release does not include ROMs or copyrighted BIOS files.

## Fresh install

1. Download **FlipOS-Beta1.zip** from the GitHub Beta 1 release.
2. Power the Miyoo Flip off and remove the SD card.
3. Back up the SD card on your computer.
4. Extract the ZIP **directly to the root of the SD card**.
5. Make sure your extraction tool preserves the hidden `.tmp_update` directory.
6. Safely eject the SD card and put it back in the Miyoo Flip.
7. Power the Flip on.
8. Follow the FlipOS first-boot setup wizard.

Do not rename or move the extracted `.tmp_update` or `flipos-dev` folders.

## Adding ROMs, BIOS and backing up saves

Once FlipOS is running, connect the Flip to a computer with a USB data cable.

The computer should automatically show a portable device named **FlipOS** containing:

- `ROMs`
- `BIOS`
- `Saves`

These locations are read/write.

Use `ROMs` to copy games, `BIOS` for your own BIOS files, and `Saves` to make manual backups of battery saves and RTC files.

### Supported ROM folders

Beta 1 supports Game Boy, Game Boy Color and Game Boy Advance. FlipOS will organise the normal ROM locations on the SD card.

## BIOS

BIOS files are optional for some systems/features, but if you choose to use them they must be supplied by you. FlipOS does not distribute BIOS files.

## Updating later

Beta 1 includes a GitHub Release update path. Updates are downloaded, verified and staged before installation.

Because this is still Beta, **do not remove power or the SD card while an update is being installed**.

## Removing FlipOS

Keep the SD-card backup you made before installation. Restoring that backup is the safest way to return the card to its previous state.

## Problems

Please report Beta problems at:

https://github.com/AnthonyWCurtis/FlipOS/issues

Include as much detail as you can about what happened and how to reproduce it.
