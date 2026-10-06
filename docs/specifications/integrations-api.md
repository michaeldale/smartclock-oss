# Integrations API

Three optional integrations, each **off by default**. Turn them on in the settings page
under **Integrations**:

| Integration | What it is for | Who may call it |
|---|---|---|
| [Custom faces](#custom-faces) | Your own faces as JSON, with live values | Signed-in page, or the API key |
| [Home Assistant dashboards](#home-assistant-dashboards-geekmagic-hacs) | The `geekmagic-hacs` integration pushes rendered dashboards | **Anyone on the network** (it cannot sign in) |
| [MQTT](#mqtt) | Home Assistant discovery; faces and variables over MQTT | Your MQTT broker |

The clock is at `http://<clock>/`, where `<clock>` is its IP address or
`smartclock-XXXXXX.local`.

## Custom faces

A face is a JSON object with a background colour and up to 32 **items**, drawn in
order on the 240×240 screen. Faces are stored on the clock (at most 12, 4 KB each)
and join the face rotation after the built-in faces.

### Authentication

Use a signed-in browser session, or the **API key** from Integrations → Custom faces.
Send the key as the header `X-Api-Key: <key>`, or as `?key=<key>`.

### Endpoints

| Method and path | Body | Does |
|---|---|---|
| `POST /api/faces?name=<name>` | face JSON | Saves the face; `&show=1`, or `"show": true` in the JSON, shows it now. An empty body deletes it |
| `GET /api/faces` | | `{"faces":[...], "showing":"<name>", "vars":{...}}` |
| `GET /api/faces/get?name=<name>` | | The face's JSON |
| `POST /api/faces/show?name=<name>` | | Shows the face now |
| `POST /api/faces/delete?name=<name>` | | Deletes the face |
| `POST /api/faces/vars` | `{"key":"value",...}` | Sets variables (merged with existing ones); values may be numbers |

Names use 1–20 characters from `a-z 0-9 _ -`. Variables are kept in memory (up to
24, keys up to 15 characters, values up to 40) and are lost on restart. Push them
again on a timer or whenever they change.

```bash
curl -X POST "http://<clock>/api/faces?name=power&show=1" -H "X-Api-Key: <key>" -H "Content-Type: application/json" -d @power.json
```

```bash
curl -X POST "http://<clock>/api/faces/vars" -H "X-Api-Key: <key>" -d '{"solar":"4.1","pct":62}'
```

### Face JSON

```json
{
  "bg": "#101820",
  "items": [
    {"type": "rect",   "x": 10, "y": 10, "w": 220, "h": 220, "r": 18, "color": "#1c2733"},
    {"type": "text",   "x": 120, "y": 40, "text": "Solar now", "font": 4, "color": "#9aa0a6", "align": "mc"},
    {"type": "text",   "x": 120, "y": 100, "text": "{solar}", "font": 6, "color": "#ffd60a", "align": "mc", "bg": "#1c2733", "pad": 200},
    {"type": "bar",    "x": 30, "y": 170, "w": 180, "h": 14, "value": "{pct}", "max": 100, "color": "#30d158", "track": "#2c3a48"},
    {"type": "text",   "x": 120, "y": 215, "text": "{time}", "font": 2, "color": "#ffffff", "align": "mc", "bg": "#1c2733", "pad": 200}
  ]
}
```

Coordinates are pixels from the top left. Colours are `"#rrggbb"`, `"#rgb"` or an
RGB565 number.

| `type` | Fields (defaults) |
|---|---|
| `text` | `x`, `y`, `text`, `font` (4), `color`, `align` (`tl`), `bg` (none = transparent), `pad` (0) |
| `rect` | `x`, `y`, `w`, `h`, `r` corner radius (0), `color`, `fill` (true) |
| `circle` | `x`, `y` (centre), `r` (10), `color`, `fill` (true) |
| `line` | `x`, `y`, `x2`, `y2`, `w` width (2), `color` |
| `ring` | `x`, `y` (centre), `r` (50), `w` thickness (8), `value`, `max` (100), `color`, `track`: a progress ring from 12 o'clock |
| `bar` | `x`, `y`, `w` (100), `h` (8), `value`, `max` (100), `color`, `track` |
| `icon` | `x`, `y` (centre), `size` (48), `icon` (`"{icon}"`): weather icon code such as `"10d"`, `bg` |
| `image` | `x`, `y`, `src`: a JPEG from the Photos card, by file name |

- **Fonts.** `2` is small text (16 px). `4` is medium text (26 px). `6` (48 px) and
  `8` (75 px) are large **digits only** (`0-9 : . -`). Fonts have no degree sign;
  write `"{temp} {unit}"`, or draw a small circle.
- **`align`** sets which point of the text `x`,`y` refers to: `tl`, `tc`, `tr`, `ml`,
  `mc`, `mr`, `bl`, `bc`, `br` (top/middle/bottom, left/centre/right).
- **Smooth updates.** The face is checked four times a second. Text with a `bg`
  colour, rings, bars, and icons with a `bg` are redrawn on their own when their
  value changes. `pad` (pixels) clears the area around text whose width varies. Any
  other item that changes, such as text without a `bg`, redraws the whole face,
  which you will see as a flicker.
- `value`, `max`, `x` and the other numbers may also be placeholders, e.g. `"{pct}"`.

### Placeholders

`{name}` in `text`, `value`, `max` and `icon` is replaced on the clock. That means
`{time}` and the weather stay current without the face being sent again.

| Placeholder | Example | | Placeholder | Example |
|---|---|---|---|---|
| `{time}` | `9:54` (12/24 h as set) | | `{temp}` `{feels}` | `21` (°C/°F as set) |
| `{hh}` `{mm}` `{ss}` | `9` `54` `26` | | `{hi}` `{lo}` | `24` `15` |
| `{ampm}` | `PM` (12 h only) | | `{unit}` | `C` or `F` |
| `{dow}` `{dow3}` | `Monday` `MON` | | `{hum}` | `64` |
| `{day}` `{month}` `{mon3}` `{year}` | `5` `October` `Oct` `2026` | | `{wind}` `{wunit}` | `15` `km/h` |
| `{date}` | `5 October` | | `{desc}` `{main}` | `light rain` `Rain` |
| `{sunrise}` `{sunset}` | `05:41` `18:12` | | `{city}` `{icon}` | `Sydney` `10d` |

Any other name is a **variable** set with `/api/faces/vars`, or over MQTT. An unknown
variable is empty. Weather placeholders show `--` until the first weather update.

## Home Assistant dashboards (geekmagic-hacs)

[geekmagic-hacs](https://github.com/adrienbrault/geekmagic-hacs) renders dashboards
(gauges, charts, weather, cameras, media and more) in Home Assistant and uploads
them as 240×240 JPEGs. Turning this integration on adds the stock SD Pro firmware's
endpoints, which that integration detects as "SD_PRO firmware":

| Path | Does |
|---|---|
| `GET /theme/list` | `{"themes":[{"id":0,"name":"Classic","enabled":true},...],"interval":10}` |
| `GET /theme/toggle?id=<0-6>&state=<0/1>` | Face in or out of rotation |
| `GET /theme/interval?val=<s>` | Rotation interval |
| `GET /photo/list` | `{"files":[{"name","size","enabled"}],"total","used","interval"}` |
| `POST /photo/upload` | Multipart JPEG upload (any field name), up to 200 KB |
| `GET /photo/toggle?name=<file>&state=<0/1>` | Photo in or out of the slideshow |
| `GET /photo/delete?name=<file>` | Delete a photo |
| `GET /photo/interval?val=<s>` | Seconds per photo |
| `GET /api/set?key=theme&value=2` | Show the Photo face (also `lcd_brightness`) |

**These work without signing in** while the integration is on, because
geekmagic-hacs cannot send credentials. They only reach photos, the face choice and
brightness. WiFi, the password, settings and firmware still need a sign-in. Prefer a
refresh interval of 30 s or longer: each update rewrites a photo in flash.

## MQTT

Set the broker, port, optional username and password, and a topic base (default:
the clock's hostname, `smartclock-XXXXXX`). The clock connects when it is on WiFi,
retries every 30 s, and announces itself to Home Assistant through **MQTT
discovery** (prefix `homeassistant/`):

| Entity | Topics |
|---|---|
| Light "Backlight" (JSON schema, brightness 0–100) | `<base>/backlight/set`, state `<base>/backlight` |
| Select "Face" (built-in and custom faces) | `<base>/face/set`, state `<base>/face` |
| Sensors: Temperature, Humidity, Weather, WiFi signal | `<base>/state` (JSON) |
| Button "Update weather" | `<base>/sync` |

Availability is on `<base>/status` (`online`/`offline`, retained). State is
published on every change and every 60 s.

With **Custom faces** also on, the clock subscribes to:

- `<base>/custom/<name>`: a face JSON (an empty payload deletes it), as for `POST /api/faces`.
  MQTT payloads are limited to about 1.5 KB.
- `<base>/vars`: `{"key":"value",...}`, as for `POST /api/faces/vars`.

A Home Assistant automation can then push values, for example every time a sensor
changes:

```yaml
action: mqtt.publish
data:
  topic: smartclock-XXXXXX/vars
  payload: '{"solar": "{{ states(''sensor.solar_power'') }}", "pct": {{ states(''sensor.battery'') | int }}}'
```
