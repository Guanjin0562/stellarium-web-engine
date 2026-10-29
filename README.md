Stellarium Web Engine
=====================

About
-----

Stellarium Web Engine is a JavaScript planetarium renderer using
WebGL that can be embedded into a website.

Features
--------

- Atmosphere simulation.
- Gaia stars database access (more than 1 billion stars).
- HiPS surveys rendering.
- High resolution planet textures.
- Constellations.
- Support for adding layers and shapes in the sky view.
- Landscapes.


Extended fork: embeddable sky charts for the All Skies Encyclopaedia
--------------------------------------------------------------------

This fork (`cse` branch) turns the engine into a **parameterised, iframe
embeddable web component** for the [All Skies Encyclopaedia (ASE)](https://ase.exopla.net)
wiki: a page passes a sky culture, a constellation subset and a set of
layer toggles in the URL and gets back exactly that sky chart, with no UI.

Upstream: <https://github.com/Stellarium/stellarium-web-engine>
Demo: <https://guanjin0562.github.io/stellarium-web-engine/demo>

### URL parameters

All parameters are optional. Unknown or malformed values are ignored and
fall back to the defaults; numeric values are parsed and clamped before
they reach the engine.

| Parameter | Values | Description |
|---|---|---|
| `sc` | sky culture path | e.g. `skydata/skycultures/manchu`. The key is the last path segment and must match the culture directory name. |
| `names` | `english`, `native`, `pronounce`, `ipa`, `abbr` | Comma separated name fields, rendered as `A (B) (C)`. Default `english`. |
| `show` | layer list, or `none` | Layers to turn **on** (see below). `none` turns every optional layer off but keeps the stars. |
| `frame` | `icrf`, `altaz` | Mount frame. Default `icrf`. |
| `cst` | constellation id | Repeatable. Restricts the chart to a subset of constellations. |
| `cst_json` | URL-encoded JSON object | Repeatable. Adds an ad-hoc constellation that is not in the culture data. |
| `hide_others` | `1`, `0` | Hide everything outside the subset. Default on when a subset is given. |
| `interact` | `all`, `view`, `none` | `view` hides the chrome, `none` also disables pointer events on the canvas. |
| `date` | ISO 8601 string | Observer time. Invalid dates are ignored. |
| `lat`, `lng` | degrees | Observer position, `-90..90` / `-180..180`. |
| `elev` | metres | Observer altitude, `-1000..100000`. |
| `az` | degrees | View azimuth, wrapped into `0..360`. |
| `alt` | degrees | View altitude, `-90..90`. |
| `fov` | degrees | Field of view, `0.1..180`. |

### `show` layers

`lines`, `asterism_lines`, `labels`, `asterism_names`, `bounds`, `images`,
`stars`, `star_labels`, `sso`, `satellites`, `landscape`, `cardinals`,
`horizon`, `equator`, `ecliptic`, `galactic`, `azimuthal`, `equatorial`,
`equatorial_jnow`, `galactic_grid`, `meridian`

`sso` covers **every natural solar system body**. The engine keeps one
visibility flag per category and each defaults to `true` in C, so the flag
has to be applied to all of: `planets`, `minor_planets` (asteroids) and
`comets`. `satellites` (artificial) is a separate, opt-in layer.

### Defaults

The default is a clean sky chart: atmosphere, landscape, fog, cardinal
points and all solar system bodies are off; constellation lines, names and
boundaries are on; the mount frame is ICRF.

### Security notes

- Every query value is parsed and range checked before being handed to the
  engine (`NaN`/`Infinity` would corrupt the camera).
- Wikipedia extracts are fetched as HTML and pass through a whitelist
  sanitiser before rendering; links are forced to `https` and
  `rel="noopener noreferrer"`.
- `?sc=` loads a sky culture from an arbitrary URL **by design**. A host
  page that lets untrusted users build the embed URL should restrict `sc`
  to same-origin paths.

### License

AGPL-3.0 (see `LICENSE-AGPL-3.0.txt`). Note that AGPL is a **network**
license: a site that serves this bundle to users must also offer the
corresponding source, including modifications. This applies to the ASE
embedding as well.


Build the javascript version
----------------------------

You need to make sure you have both emscripten and sconstruct installed.

    # Setup emscripten path.
    source $PATH_TO_EMSDK/emsdk_env.sh

    # Build stellarium-web-engine.js and stellarium-web-engine.wasm
    # This will also copy the files into html/static/js
    make js

    # Now see apps/simple-html/ to try the library in a browser.

The Vue frontend (`apps/web-frontend`) is built with webpack 4 and needs the
legacy OpenSSL provider on Node 17+:

    cd apps/web-frontend
    NODE_OPTIONS=--openssl-legacy-provider CDN_ENV=/stellarium-web-engine/ \
      node node_modules/@vue/cli-service/bin/vue-cli-service.js build

Do not use `npm run build` on Windows in a restricted shell: npm may spawn
`wsl.exe`, which is blocked by some sandboxes.


Contributing
------------

In order for your contribution to Stellarium Web Engine to be accepted, you have to sign the
[Stellarium Web Contributor License Agreement (CLA)](doc/cla/sign-cla.md).
