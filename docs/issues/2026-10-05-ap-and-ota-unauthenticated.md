# The setup AP is open and every endpoint is unauthenticated

Status: Open.

Date: 2026-10-05

## Summary

The **Smart Weather Clock** AP has no password and is always on. `/update_ota`,
`/api/set`, `/connect`, `/restart` and `/format_storage` need no credentials.
Anyone within WiFi range can join the AP and reflash or reconfigure the clock.
Stock had the same exposure
([analysis](../specifications/stock-firmware-analysis.md)).

## Options

- A fixed WPA2 password on the AP, documented and shown on the panel. Cheap,
  but a lost panel means a lost password, so it has to be documented.
- An admin password for mutating endpoints, set on the setup page and stored
  in `/config.json`. Recovery would need its own rule, since recovery runs
  without storage.
- Turn the AP off once the STA link has been up for a while, and bring it back
  in recovery or when the link is lost.
