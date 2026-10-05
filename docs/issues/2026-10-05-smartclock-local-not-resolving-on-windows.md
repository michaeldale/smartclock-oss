# `smartclock.local` did not resolve from Windows `ping`

Status: Explained 2026-10-05: the workstation is on a different subnet; mDNS does not cross subnets.

Date: 2026-10-05

## Summary

With 0.1.0 online at 10.0.1.118, `ping -n 1 smartclock.local` on the Windows
workstation returned "could not find host". The IP address worked.

## Notes

- `ping` on Windows does not always use mDNS for `.local` names, so this is
  weak evidence on its own.
- `MDNS.begin("smartclock")` runs in `setup()` before the STA link is up, and
  relies on the core restarting the responder when the interface comes up.

## Next

Try http://smartclock.local/ in a browser and from a phone, and run a
`dns-sd -B _http._tcp` or similar browse. If it is not visible there either,
restart the responder (or call `MDNS.notifyAPChange()`) once the STA link
connects.

## 2026-10-05: explained

The workstation and the clock (10.0.1.118) are on different subnets. mDNS is
link-local multicast, so `.local` names never resolve across a router. This
is not a firmware fault. The firmware now registers a per-unit DHCP hostname
(`smartclock-<chip id>`), so a router that publishes DHCP names in DNS can
resolve the clock from other subnets. mDNS still works from devices on the
clock's own subnet ([record](../decisions/2026-10-05-open-meteo-reset-reason-and-discovery.md)).
