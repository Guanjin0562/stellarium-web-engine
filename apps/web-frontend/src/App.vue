// Stellarium Web - Copyright (c) 2022 - Stellarium Labs SRL
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<v-app>
  <v-navigation-drawer v-if="!isDemoMode" v-model="nav" app stateless width="300">
    <v-layout column fill-height>
      <v-list dense>
        <template v-for="(item,i) in menuItems">
          <template v-if="$store.state[item.store_show_menu_item] === false"></template>
          <v-subheader v-else-if="item.header" v-text="item.header" class="grey--text text--darken-1" :key="i"/>
          <v-divider class="divider_menu" v-else-if="item.divider" :key="i"/>
          <v-list-item v-else-if="item.switch" @click.stop="toggleStoreValue(item.store_var_name)" :key="i">
            <v-list-item-action>
              <v-switch value :input-value="getStoreValue(item.store_var_name)" label=""></v-switch>
            </v-list-item-action>
            <v-list-item-content>
              <v-list-item-title>{{ item.title }}</v-list-item-title>
            </v-list-item-content>
          </v-list-item>
          <template v-else>
            <v-list-item v-if='item.link' target="_blank" rel="noopener" :href='item.link' :key="i">
              <v-list-item-icon><v-icon>{{ item.icon }}</v-icon></v-list-item-icon>
              <v-list-item-title v-text="item.title"/>
              <v-icon disabled>mdi-open-in-new</v-icon>
            </v-list-item>
            <v-list-item v-else-if='item.footer===undefined' @click.stop="toggleStoreValue(item.store_var_name)" :key="i">
              <v-list-item-icon><v-icon>{{ item.icon }}</v-icon></v-list-item-icon>
              <v-list-item-title v-text="item.title"/>
            </v-list-item>
          </template>
        </template>
      </v-list>
      <template v-for="(item,i) in menuComponents">
        <component :is="item" :key="i"></component>
      </template>
      <v-spacer></v-spacer>
      <v-list dense>
        <v-divider class="divider_menu"/>
        <template v-for="(item,i) in menuItems">
          <v-list-item v-if='item.footer' @click.stop="toggleStoreValue(item.store_var_name)" :key="i">
            <v-list-item-icon><v-icon>{{ item.icon }}</v-icon></v-list-item-icon>
            <v-list-item-title v-text="item.title"/>
          </v-list-item>
        </template>
      </v-list>
    </v-layout>
  </v-navigation-drawer>

  <v-main>
    <v-container class="fill-height" fluid style="padding: 0">
      <div id="stel" v-bind:class="{ right_panel: !isDemoMode && $store.state.showSidePanel }">
        <div style="position: relative; width: 100%; height: 100%">
          <component v-bind:is="guiComponent"></component>
          <canvas id="stel-canvas" ref='stelCanvas'></canvas>
        </div>
      </div>
    </v-container>
  </v-main>

  <!-- Demo mode: right-side parameter control panel (route /demo).
       Visibility is driven by $store.state.showSidePanel so the toolbar
       gear button can always toggle it.  It is gated on the engine being
       ready (not on initComplete) so that a slow, hung or skipped
       date/location initialisation can never swallow the panel. -->
  <demo-param-panel v-if="isDemoMode && $store.state.showSidePanel && engineReady"/>

</v-app>

</template>

<script>

import _ from 'lodash'
import Gui from '@/components/gui.vue'
import GuiLoader from '@/components/gui-loader.vue'
import DemoParamPanel from '@/components/demo-param-panel.vue'
import swh from '@/assets/sw_helpers.js'
import Moment from 'moment'

export default {
  data (context) {
    return {
      menuItems: [
        { title: this.$t('View Settings'), icon: 'mdi-settings', store_var_name: 'showViewSettingsDialog', store_show_menu_item: 'showViewSettingsMenuItem' },
        { title: this.$t('Planets Tonight'), icon: 'mdi-panorama-fisheye', store_var_name: 'showPlanetsVisibilityDialog', store_show_menu_item: 'showPlanetsVisibilityMenuItem' },
        { divider: true }
      ].concat(this.getPluginsMenuItems()).concat([
        { title: this.$t('Data Credits'), footer: true, icon: 'mdi-copyright', store_var_name: 'showDataCreditsDialog' }
      ]),
      menuComponents: [].concat(this.getPluginsMenuComponents()),
      guiComponent: 'GuiLoader',
      startTimeIsSet: false,
      initDone: false,
      dataSourceInitDone: false,
      // Set once when the WASM engine calls back as ready.  Kept as a plain
      // boolean because $store.state.stel is replaced on every engine value
      // change (every frame), which would make a v-if on it re-evaluate
      // continuously.
      engineReady: false
    }
  },
  components: { Gui, GuiLoader, DemoParamPanel },
  methods: {
    getPluginsMenuItems: function () {
      let res = []
      for (const i in this.$stellariumWebPlugins()) {
        const plugin = this.$stellariumWebPlugins()[i]
        if (plugin.menuItems) {
          res = res.concat(plugin.menuItems)
        }
      }
      return res
    },
    getPluginsMenuComponents: function () {
      let res = []
      for (const i in this.$stellariumWebPlugins()) {
        const plugin = this.$stellariumWebPlugins()[i]
        if (plugin.menuComponents) {
          res = res.concat(plugin.menuComponents)
        }
      }
      return res
    },
    toggleStoreValue: function (storeVarName) {
      this.$store.commit('toggleBool', storeVarName)
    },
    getStoreValue: function (storeVarName) {
      return _.get(this.$store.state, storeVarName)
    },
    setStateFromQueryArgs: function () {
      // Check whether the observing panel must be displayed. On the /demo
      // route the right side is always the parameter panel, which is opened
      // by default (and can be toggled with the toolbar gear button).
      this.$store.commit('setValue', {
        varName: 'showSidePanel',
        newValue: this.$route.path.startsWith('/p/') || this.isDemoMode
      })

      // Set the core's state from URL query arguments such
      // as date, location, view direction & fov
      var that = this

      // Query values come from untrusted markup (a wiki template, a shared
      // link), so nothing is passed to the engine without parsing and range
      // checking: NaN/Infinity would corrupt the camera or the observer.
      const num = (v, def, min, max) => {
        if (v === undefined || v === null || v === '') return def
        const n = Number(v)
        if (!isFinite(n)) return def
        if (min !== undefined && n < min) return min
        if (max !== undefined && n > max) return max
        return n
      }

      if (!this.initDone) {
        this.$stel.core.time_speed = 1
        let d = new Date()
        if (this.$route.query.date) {
          const parsed = new Moment(this.$route.query.date)
          if (parsed.isValid()) {
            d = parsed.toDate()
            this.$stel.core.observer.utc = d.getMJD()
            this.startTimeIsSet = true
          }
        }

        const lat = num(this.$route.query.lat, NaN, -90, 90)
        const lng = num(this.$route.query.lng, NaN, -180, 180)
        if (isFinite(lat) && isFinite(lng)) {
          const pos = { lat: lat, lng: lng, alt: num(this.$route.query.elev, 0, -1000, 100000), accuracy: 1 }
          swh.geoCodePosition(pos, that).then((loc) => {
            that.$store.commit('setCurrentLocation', loc)
          }, (error) => {
            console.log(error)
            // Geolocation / reverse geocoding failed: fall back to the
            // store location so that the startup time is still chosen and
            // initComplete is reached.
            that.applyLocation(that.$store.state.currentLocation)
          })
        }

        const az = num(this.$route.query.az, 0)
        this.$stel.core.observer.yaw = (((az % 360) + 360) % 360) * Math.PI / 180
        this.$stel.core.observer.pitch = num(this.$route.query.alt, 30, -90, 90) * Math.PI / 180
        this.$stel.core.fov = num(this.$route.query.fov, 120, 0.1, 180) * Math.PI / 180

        this.initDone = true
      }

      if (this.$route.path.startsWith('/skysource/')) {
        const name = decodeURIComponent(this.$route.path.substring(11))
        console.log('Will select object: ' + name)
        return swh.lookupSkySourceByName(name).then(ss => {
          if (!ss) {
            return
          }
          let obj = swh.skySource2SweObj(ss)
          if (!obj) {
            obj = this.$stel.createObj(ss.model, ss)
            this.$selectionLayer.add(obj)
          }
          if (!obj) {
            console.warning("Can't find object in SWE: " + ss.names[0])
          }
          swh.setSweObjAsSelection(obj)
        }, err => {
          console.log(err)
          console.log("Couldn't find skysource for name: " + name)
        })
      }
    },
    // Return the list of full id candidates for a cst parameter, so that
    // they can be searched with stel.getObj().  The candidates follow the
    // full id format "CON culture id" / "AST culture id".
    fullIdCandidates: function (id, culture) {
      const parts = id.trim().split(/\s+/)
      if (parts.length >= 3 && (parts[0] === 'CON' || parts[0] === 'AST')) {
        return [id.trim()]
      }
      if (parts.length === 2 && (parts[0] === 'CON' || parts[0] === 'AST')) {
        return [parts[0] + ' ' + culture + ' ' + parts[1]]
      }
      // culture+id or bare id: try both categories.
      const core = parts.length === 2 ? id.trim() : culture + ' ' + id.trim()
      return ['CON ' + core, 'AST ' + core]
    },
    // Apply the embed URL parameters: cst / cst_json / frame / show /
    // interact / hide_others.  This must be called once all the data
    // sources have been configured.
    applyEmbedQueryArgs: function () {
      const stel = this.$stel
      const q = this.$route.query

      // Gather the repeatable cst / cst_json parameters.
      const cstParams = []
      if (Array.isArray(q.cst)) cstParams.push(...q.cst)
      else if (q.cst) cstParams.push(q.cst)
      const cstJsonParams = []
      if (Array.isArray(q.cst_json)) cstJsonParams.push(...q.cst_json)
      else if (q.cst_json) cstJsonParams.push(q.cst_json)

      const hasSubset = cstParams.length > 0 || cstJsonParams.length > 0
      if (!hasSubset && !q.frame && !q.show && !q.interact && !q.names) return

      // 1. Create the custom skyculture from the cst_json objects.
      const customObjs = []
      for (const s of cstJsonParams) {
        try {
          customObjs.push(JSON.parse(decodeURIComponent(s)))
        } catch (e) {
          console.warn('Cannot parse cst_json parameter: ' + s)
        }
      }
      if (customObjs.length) {
        stel.core.skycultures.add_custom_csts(customObjs)
      }

      // 2. View frame (equatorial / altaz).  When a subset is set we
      // default to the equatorial frame so that the display does not
      // depend on time or location.
      let frame = q.frame
      if (!frame && hasSubset) frame = 'icrf'
      if (frame === 'icrf') {
        stel.core.mount_frame = stel.FRAME_ICRF
      } else if (frame === 'altaz') {
        stel.core.mount_frame = stel.FRAME_OBSERVED
      }

      // 2b. Name fields, freely combinable via a comma separated list:
      // english / native / pronounce / ipa / abbr.  Default is english.
      // Legacy single values (both, native_pronounce) are still accepted.
      const nameFormat = {
        english: 'english',
        native: 'native',
        both: 'english,native',
        native_pronounce: 'native,pronounce'
      }
      if (q.names) {
        stel.core.skycultures.name_format =
          nameFormat[q.names] || q.names
      }

      // 3. Display toggles.
      if (q.show) {
        const list = q.show.split(',').map(s => s.trim())
        const allOff = list.indexOf('none') >= 0
        const apply = (mod, attr, name) => {
          if (!mod) return
          if (allOff) {
            // 'none' turns off every optional layer, but keeps the base
            // star field: stars must never be hidden by 'none'.
            if (name !== 'stars') mod[attr] = false
          } else if (list.indexOf(name) >= 0) {
            mod[attr] = true
          }
        }
        apply(stel.core.constellations, 'lines_visible', 'lines')
        apply(stel.core.constellations, 'labels_visible', 'labels')
        apply(stel.core.constellations, 'asterism_lines_visible', 'asterism_lines')
        apply(stel.core.constellations, 'asterism_labels_visible', 'asterism_names')
        apply(stel.core.constellations, 'bounds_visible', 'bounds')
        apply(stel.core.constellations, 'images_visible', 'images')
        apply(stel.core.stars, 'visible', 'stars')
        apply(stel.core.stars, 'labels_visible', 'star_labels')
        // "sso" covers every natural solar system body: one engine module per
        // category (planets / minor planets / comets).  Artificial satellites
        // are a separate, opt-in layer.
        apply(stel.core.planets, 'visible', 'sso')
        apply(stel.core.minor_planets, 'visible', 'sso')
        apply(stel.core.comets, 'visible', 'sso')
        apply(stel.core.satellites, 'visible', 'satellites')
        // Landscape, cardinal points and coordinate grids/lines.
        apply(stel.core.landscapes, 'visible', 'landscape')
        apply(stel.core.cardinals, 'visible', 'cardinals')
        apply(stel.core.lines.horizon, 'visible', 'horizon')
        apply(stel.core.lines.equator_line, 'visible', 'equator')
        apply(stel.core.lines.ecliptic, 'visible', 'ecliptic')
        apply(stel.core.lines.galactic, 'visible', 'galactic')
        apply(stel.core.lines.azimuthal, 'visible', 'azimuthal')
        apply(stel.core.lines.equatorial, 'visible', 'equatorial')
        apply(stel.core.lines.equatorial_jnow, 'visible', 'equatorial_jnow')
        apply(stel.core.lines.galactic_grid, 'visible', 'galactic_grid')
        apply(stel.core.lines.meridian, 'visible', 'meridian')
      }

      // 4. Interaction mode.
      if (q.interact === 'view' || q.interact === 'none') {
        this.$store.commit('setValue', { varName: 'showMainToolBar', newValue: false })
        this.$store.commit('setValue', { varName: 'showNavigationDrawer', newValue: false })
        // On /demo the panel is the only UI left, so keep it: otherwise a
        // shared link carrying interact=view would open a page with no
        // controls at all.
        if (!this.isDemoMode) {
          this.$store.commit('setValue', { varName: 'showSidePanel', newValue: false })
        }
      }
      if (q.interact === 'none') {
        // Swallow clicks and disable the canvas input.
        stel.on('click', function () { return true })
        const canvas = document.getElementById('stel-canvas')
        if (canvas) canvas.style.pointerEvents = 'none'
      }

      // 5. Focus subset: hide_others / set_focus / framing.  The
      // constellation objects only exist once the skyculture data has
      // been loaded asynchronously, so we poll until they are
      // available before applying the subset.
      if (!hasSubset) return

      const culture = q.sc ? q.sc.substring(q.sc.lastIndexOf('/') + 1) : 'western'

      if (q.hide_others === undefined) {
        // Default: hide everything outside the subset.
        stel.core.constellations.hide_others = hasSubset
      } else if (q.hide_others === '1' || q.hide_others === 'true') {
        stel.core.constellations.hide_others = true
      } else if (q.hide_others === '0' || q.hide_others === 'false') {
        stel.core.constellations.hide_others = false
      }

      const showLines = !(q.show && q.show.split(',').indexOf('none') >= 0)
      this.applyFocusAndFrame(cstParams, culture, frame, customObjs, showLines)
    },
    // Wait for the constellation objects to be available (their data is
    // loaded asynchronously), then set the focus subset, make sure the
    // lines are visible and frame the view on all the subset members.
    applyFocusAndFrame: function (cstParams, culture, frame, customObjs, showLines) {
      const stel = this.$stel
      const self = this
      const tryResolve = function (attempt) {
        const focusObjs = []
        const matchedIds = []
        for (const c of cstParams) {
          const candidates = self.fullIdCandidates(c, culture)
          let found = false
          for (const cand of candidates) {
            const obj = stel.getObj(cand)
            if (obj) {
              focusObjs.push(obj)
              matchedIds.push(cand)
              found = true
              break
            }
          }
          if (!found) {
            if (attempt < 40) {
              setTimeout(function () { tryResolve(attempt + 1) }, 500)
              return
            }
            console.warn('Cannot find constellation: ' + c)
            return
          }
        }
        // Custom constellations from cst_json: their id is already a full
        // id, so resolve them directly through getObj().
        for (const o of (customObjs || [])) {
          if (!o || !o.id) continue
          const obj = stel.getObj(o.id)
          if (obj) {
            focusObjs.push(obj)
            matchedIds.push(o.id)
          } else {
            if (attempt < 40) {
              setTimeout(function () { tryResolve(attempt + 1) }, 500)
              return
            }
            console.warn('Cannot find custom constellation: ' + o.id)
            return
          }
        }
        // The focus list uses the exact matched ids so that
        // constellation_in_focus() can match them (it needs the full id
        // or a suffix starting right after a space).
        stel.core.constellations.set_focus(matchedIds)
        // Only force the lines on when they were not explicitly disabled
        // through the show parameter.
        stel.core.constellations.lines_visible = showLines !== false
        if (focusObjs.length) {
          self.frameOnConstellations(focusObjs, frame || 'icrf')
        }
      }
      tryResolve(0)
    },
    // Compute the bounding cap of a list of constellation objects and
    // frame the view so that all of them are visible.  The constellation
    // data is loaded lazily, so we poll until everything is ready.
    frameOnConstellations: function (objs, frame) {
      const stel = this.$stel
      const obs = stel.observer.clone()
      const tryFrame = function (attempt) {
        const caps = []
        for (const obj of objs) {
          const pvo = obj.getInfo('pvo', obs)
          if (pvo === undefined) {
            if (attempt < 40) {
              setTimeout(function () { tryFrame(attempt + 1) }, 500)
              return
            }
            continue
          }
          const radius = obj.getInfo('radius', obs)
          caps.push({ c: [pvo[0][0], pvo[0][1], pvo[0][2]], r: radius })
        }
        if (!caps.length) return
        // Center: mean direction of all the caps.
        let cx = 0
        let cy = 0
        let cz = 0
        for (const cap of caps) {
          cx += cap.c[0]
          cy += cap.c[1]
          cz += cap.c[2]
        }
        const n = Math.sqrt(cx * cx + cy * cy + cz * cz)
        if (n < 1e-9) return
        cx /= n
        cy /= n
        cz /= n
        // Maximum angular distance to any cap rim.
        let maxAng = 0
        for (const cap of caps) {
          const dot = Math.max(-1, Math.min(1,
            cap.c[0] * cx + cap.c[1] * cy + cap.c[2] * cz))
          const d = Math.acos(dot) + cap.r
          if (d > maxAng) maxAng = d
        }
        const fov = Math.min(maxAng * 2 * 1.2, 170 * Math.PI / 180)
        stel.core.fov = fov
        // In altaz mode the lookAt position must be converted to the
        // observed frame, otherwise the yaw/pitch directly are the ICRF
        // (equatorial) coordinates.
        let dir
        if (frame === 'altaz') {
          dir = stel.convertFrame(obs, 'ICRF', 'OBSERVED', [cx, cy, cz, 0])
        } else {
          dir = [cx, cy, cz, 0]
        }
        stel.lookAt([dir[0], dir[1], dir[2]], 0)
      }
      tryFrame(0)
    },
    // Apply a location to the observer and finish the date/time startup.
    //
    // Extracted from the storeCurrentLocation watcher: that watcher only
    // fires when a location is *committed*, and a bare URL (no lng/lat
    // query args) never commits one.  The startup time was therefore never
    // chosen and `initComplete` stayed false forever - and since the demo
    // panel is gated on it, the panel silently failed to render.
    applyLocation: function (loc) {
      if (!this.$stel || !loc) return
      const DD2R = Math.PI / 180
      const core = this.$stel.core
      core.observer.latitude = loc.lat * DD2R
      core.observer.longitude = loc.lng * DD2R
      core.observer.elevation = loc.alt

      // At startup, we need to wait for the location to be set before
      // deciding which startup time to set so that it's night time.
      if (!this.startTimeIsSet) this.setStartupTime(0)
      // Init of time and date is complete
      this.$store.commit('setValue', { varName: 'initComplete', newValue: true })
    },
    // Pick the startup time ("next time the night sky is visible").
    //
    // getTimeAfterSunset() needs the Sun object, which only exists once the
    // solar system data has been loaded - at engine-ready time it usually
    // does not.  Throwing from here used to abort the whole engine-ready
    // callback (leaving no data sources at all), so poll for a while
    // instead and give up quietly; the engine default time is a valid
    // fallback.
    setStartupTime: function (attempt) {
      if (this.startTimeIsSet || !this.$stel) return
      const hasSun = !!this.$stel.getObj('NAME Sun')
      if (!hasSun) {
        if (attempt < 40) {
          const self = this
          setTimeout(function () { self.setStartupTime(attempt + 1) }, 250)
        }
        return
      }
      try {
        this.$stel.core.observer.utc = swh.getTimeAfterSunset(this.$stel)
        this.startTimeIsSet = true
      } catch (e) {
        console.error('Could not compute the startup time: ' + e)
      }
    }
  },
  computed: {
    // The /demo route renders the same sky but with the right-side
    // parameter panel (and without the navigation drawer / main toolbar).
    // A trailing slash is accepted: GitHub Pages / a hand-typed URL can
    // deliver "/demo/", and an exact match would silently drop the panel.
    isDemoMode: function () {
      return /^\/demo\/?$/.test(this.$route.path)
    },
    nav: {
      get: function () {
        return this.$store.state.showNavigationDrawer
      },
      set: function (v) {
        if (this.$store.state.showNavigationDrawer !== v) {
          this.$store.commit('toggleBool', 'showNavigationDrawer')
        }
      }
    },
    storeCurrentLocation: function () {
      return this.$store.state.currentLocation
    }
  },
  watch: {
    storeCurrentLocation: function (loc) {
      this.applyLocation(loc)
    },
    $route: function () {
      // react to route changes...
      this.setStateFromQueryArgs()
      // The right-side demo panel only exists on the /demo route; show it
      // there by default (the toolbar gear button can toggle it off/on).
      this.$store.commit('setValue', {
        varName: 'showSidePanel',
        newValue: this.isDemoMode
      })
    }
  },
  mounted: function () {
    var that = this

    // Demo mode: hide the main toolbar and the navigation drawer; the
    // right-side demo panel replaces them (the interact setting on the
    // panel can bring the toolbar back).
    if (this.isDemoMode) {
      this.$store.commit('setValue', { varName: 'showMainToolBar', newValue: false })
      this.$store.commit('setValue', { varName: 'showNavigationDrawer', newValue: false })
      // Always open the parameter panel when entering the demo route.
      this.$store.commit('setValue', { varName: 'showSidePanel', newValue: true })
    }

    for (const i in this.$stellariumWebPlugins()) {
      const plugin = this.$stellariumWebPlugins()[i]
      if (plugin.onAppMounted) {
        plugin.onAppMounted(that)
      }
    }

    import('@/assets/js/stellarium-web-engine.wasm').then(f => {
      // Initialize the StelWebEngine viewer singleton
      // After this call, the StelWebEngine state will always be available in vuex store
      // in the $store.stel object in a reactive way (useful for vue components).
      // To modify the state of the StelWebEngine, it's enough to call/set values directly on the $stel object
      try {
        swh.initStelWebEngine(that.$store, f.default, that.$refs.stelCanvas, function () {
          // Start auto location detection (even if we don't use it)
          swh.getGeolocation().then(p => swh.geoCodePosition(p, that)).then((loc) => {
            that.$store.commit('setAutoDetectedLocation', loc)
          }, (error) => { console.log(error) })

          that.$stel.setFont('regular', process.env.BASE_URL + 'fonts/Roboto-Regular.ttf', 1.38)
          that.$stel.setFont('bold', process.env.BASE_URL + 'fonts/Roboto-Bold.ttf', 1.38)
          // Fallback fonts for scripts not covered by Roboto (CJK,
          // Devanagari, ...).  Loading several fonts for the same face adds
          // them as fallbacks, so labels in these scripts render correctly.
          // Both the regular and the bold faces get a CJK fallback: without
          // it, bold labels (selected stars, constellation names) would
          // render CJK characters as boxes.
          that.$stel.setFont('regular', process.env.BASE_URL + 'fonts/NotoSansSC.ttf')
          that.$stel.setFont('bold', process.env.BASE_URL + 'fonts/NotoSansSC-Bold.ttf')
          that.$stel.setFont('regular', process.env.BASE_URL + 'fonts/NotoSansDevanagari.ttf')
          // Arabic font (DejaVu Sans), used for the native Arabic star names
          // of the arabic sky culture.  Registered on both faces since
          // constellation names always render bold.
          that.$stel.setFont('regular', process.env.BASE_URL + 'fonts/DejaVuSans.ttf')
          that.$stel.setFont('bold', process.env.BASE_URL + 'fonts/DejaVuSans-Bold.ttf')
          // Manchu (Mongolian script) font, used for vertical native labels
          // of the chinese_manchu sky culture.  Registered on both faces
          // since constellation names always render bold.
          that.$stel.setFont('regular', process.env.BASE_URL + 'fonts/AlariŠanyanManchu-Regular.ttf')
          that.$stel.setFont('bold', process.env.BASE_URL + 'fonts/AlariŠanyanManchu-Regular.ttf')
          that.$stel.core.constellations.show_only_pointed = false

          // Default "sky chart" mode: clean star chart without atmosphere,
          // landscape, grids and solar system bodies.  URL args can
          // override these defaults (e.g. show=sso to display the planets).
          that.$stel.core.constellations.lines_visible = true
          that.$stel.core.constellations.labels_visible = true
          that.$stel.core.constellations.bounds_visible = true
          that.$stel.core.atmosphere.visible = false
          that.$stel.core.landscapes.visible = false
          that.$stel.core.landscapes.fog_visible = false
          that.$stel.core.lines.azimuthal.visible = false
          that.$stel.core.cardinals.visible = false
          that.$stel.core.mount_frame = that.$stel.FRAME_ICRF
          // Solar system bodies are four independent engine modules, all of
          // which default to visible=true in C.  Turning off only `planets`
          // leaves asteroids, comets and artificial satellites on screen.
          that.$stel.core.planets.visible = false
          that.$stel.core.minor_planets.visible = false
          that.$stel.core.comets.visible = false
          that.$stel.core.satellites.visible = false
          that.$stel.core.stars.labels_visible = false
          // Names default to english (no translation) so that a sky chart
          // always shows the english names whatever the page locale is.
          that.$stel.core.skycultures.name_format_style = that.$stel.NAME_ENGLISH

          that.setStateFromQueryArgs()

          // Only now is the initial state fully applied, so it is safe for
          // the demo panel (and anything else gated on it) to read the
          // engine.  Vue renders on the next tick, i.e. after this whole
          // callback, so the panel never sees a half-applied state.  Set
          // before the location step below so that a failure there cannot
          // keep the panel from rendering.
          that.engineReady = true

          // Date/location startup.  The storeCurrentLocation watcher is the
          // normal path, but it only runs when a location is committed, i.e.
          // when the URL carries lng/lat.  On a bare URL nothing is ever
          // committed: without this call the observer keeps the engine
          // default, the startup time is never chosen and `initComplete`
          // stays false.  When lng/lat *are* given, leave it to the promise
          // above so the time is computed for the requested location.
          const qLoc = that.$route.query
          const hasLocArg = qLoc.lng !== undefined && qLoc.lat !== undefined &&
            qLoc.lng !== '' && qLoc.lat !== '' &&
            isFinite(Number(qLoc.lng)) && isFinite(Number(qLoc.lat))
          if (!hasLocArg) that.applyLocation(that.$store.state.currentLocation)
          that.guiComponent = 'Gui'
          for (const i in that.$stellariumWebPlugins()) {
            const plugin = that.$stellariumWebPlugins()[i]
            if (plugin.onEngineReady) {
              plugin.onEngineReady(that)
            }
          }

          if (!that.dataSourceInitDone) {
            // Set all default data sources
            const core = that.$stel.core
            core.stars.addDataSource({ url: process.env.BASE_URL + 'skydata/stars' })

            // Allow to specify a custom path for sky culture data
            if (that.$route.query.sc) {
              const key = that.$route.query.sc.substring(that.$route.query.sc.lastIndexOf('/') + 1)
              core.skycultures.addDataSource({ url: that.$route.query.sc, key: key })
              core.skycultures.current_id = key
            } else {
              core.skycultures.addDataSource({ url: process.env.BASE_URL + 'skydata/skycultures/western', key: 'western' })
            }

            core.dsos.addDataSource({ url: process.env.BASE_URL + 'skydata/dso' })
            core.landscapes.addDataSource({ url: process.env.BASE_URL + 'skydata/landscapes/guereins', key: 'guereins' })
            core.milkyway.addDataSource({ url: process.env.BASE_URL + 'skydata/surveys/milkyway' })
            core.minor_planets.addDataSource({ url: process.env.BASE_URL + 'skydata/mpcorb.dat', key: 'mpc_asteroids' })
            core.planets.addDataSource({ url: process.env.BASE_URL + 'skydata/surveys/sso/moon', key: 'moon' })
            core.planets.addDataSource({ url: process.env.BASE_URL + 'skydata/surveys/sso/sun', key: 'sun' })
            core.planets.addDataSource({ url: process.env.BASE_URL + 'skydata/surveys/sso/moon', key: 'default' })
            core.comets.addDataSource({ url: process.env.BASE_URL + 'skydata/CometEls.txt', key: 'mpc_comets' })
            core.satellites.addDataSource({ url: process.env.BASE_URL + 'skydata/tle_satellite.jsonl.gz', key: 'jsonl/sat' })
          }
          that.applyEmbedQueryArgs()
        })
      } catch (e) {
        this.$store.commit('setValue', { varName: 'wasmSupport', newValue: false })
      }
    })
  }
}
</script>

<style>

a {
  color: #82b1ff;
}

a:link {
  text-decoration-line: none;
}

.divider_menu {
  margin-top: 8px;
  margin-bottom: 8px;
}

html {
  overflow-y: visible;
}

html, body, #app {
  overflow-y: visible!important;
  overflow-x: visible;
  position: fixed!important;
  width: 100%;
  height: 100%;
  padding: 0!important;
  font-size: 14px;
}

.fullscreen {
  overflow-y: hidden;
  position: fixed;
  width: 100%;
  height: 100%;
  padding: 0!important;
}

.click-through {
  pointer-events: none;
}

.get-click {
  pointer-events: all;
}

.dialog {
  background: transparent;
}

.menu__content {
  background-color: transparent!important;
}

#stel {height: 100%; width: 100%; position: absolute;}
#stel-canvas {z-index: -10; width: 100%; height: 100%;}

.right_panel {
  padding-right: 400px;
}

.v-btn {
  margin-left: 8px;
  margin-right: 8px;
  margin-top: 6px;
  margin-bottom: 6px;
}

.v-application--wrap {
  min-height: 100%!important;
}

</style>
