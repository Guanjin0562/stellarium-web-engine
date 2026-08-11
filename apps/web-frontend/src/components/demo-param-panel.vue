// Demo parameter control panel.
//
// Right-side panel for the /demo route.  Every control writes the
// corresponding stel.core.* property directly, so the sky chart updates
// synchronously (no page reload).  The browser URL is kept in sync with
// history.replaceState() so that the share link always reflects the
// current parameter set.

<template>
<v-navigation-drawer right app permanent width="360" class="demo-panel" style="background-color: #1e1e28">
  <v-list dense>
    <v-list-item>
      <v-list-item-content>
        <v-list-item-title class="text-h6">Parameter Console</v-list-item-title>
        <v-list-item-subtitle>Changes apply instantly — no page reload</v-list-item-subtitle>
      </v-list-item-content>
    </v-list-item>

    <!-- Sky culture -->
    <v-subheader>Sky Culture</v-subheader>
    <v-select
      :items="cultureItems"
      v-model="cultureKey"
      label="Culture"
      dense
      hide-details
      class="mx-3 mb-2"
      @change="onCultureChange"
    />

    <!-- Name format -->
    <v-subheader>Name Format (in display order)</v-subheader>
    <v-checkbox
      v-for="f in nameFields"
      :key="f"
      :label="f"
      :input-value="nameFormat[f]"
      @change="v => setNameFormat(f, v)"
      class="mx-3"
      hide-details
      dense
    />

    <!-- Display layers -->
    <v-subheader>Display Layers</v-subheader>
    <v-checkbox
      v-for="t in layerToggles"
      :key="t.key"
      :label="t.label"
      :input-value="toggles[t.key]"
      @change="v => setLayer(t.key, v)"
      class="mx-3"
      hide-details
      dense
    />

    <!-- Coordinate system -->
    <v-subheader>Coordinate System</v-subheader>
    <v-radio-group v-model="frame" row dense class="mx-3" @change="applyFrame">
      <v-radio label="Equatorial (ICRF)" value="icrf" color="blue"></v-radio>
      <v-radio label="Alt-Az" value="altaz" color="blue"></v-radio>
    </v-radio-group>

    <!-- Focus subset: constellations and asterisms are controlled
         separately, but are applied together through set_focus(). -->
    <v-subheader>Focus Subset</v-subheader>
    <v-select
      :items="cstOptions"
      v-model="focusCsts"
      label="Constellations"
      multiple
      dense
      hide-details
      class="mx-3 mb-1"
      item-text="text"
      item-value="value"
      return-object
    />
    <v-select
      :items="astOptions"
      v-model="focusAsts"
      label="Asterisms"
      multiple
      dense
      hide-details
      class="mx-3 mb-2"
      item-text="text"
      item-value="value"
      return-object
    />
    <v-btn small color="blue" dark class="mx-3 mb-1" @click="applyFocus">Apply Focus &amp; Frame</v-btn>
    <v-btn small class="mx-3 mb-2" @click="clearFocus">Clear Focus</v-btn>

    <!-- Field of view -->
    <v-subheader>Field of View</v-subheader>
    <v-slider
      v-model="fovDeg"
      min="5"
      max="170"
      step="1"
      dense
      class="mx-3"
      :thumb-label="'always'"
      @change="applyFov"
    >
      <template v-slot:append>{{ fovDeg }}°</template>
    </v-slider>

    <!-- Interaction mode -->
    <v-subheader>Interaction</v-subheader>
    <v-radio-group v-model="interactMode" row dense class="mx-3" @change="applyInteract">
      <v-radio label="Full" value="all" color="blue"></v-radio>
      <v-radio label="View only" value="view" color="blue"></v-radio>
      <v-radio label="Static" value="none" color="blue"></v-radio>
    </v-radio-group>

    <!-- Share -->
    <v-subheader>Share</v-subheader>
    <v-btn small color="green" dark class="mx-3 mb-1" @click="copyLink">Copy Share Link</v-btn>
    <v-btn small class="mx-3 mb-2" @click="resetAll">Reset to Defaults</v-btn>
  </v-list>
</v-navigation-drawer>
</template>

<script>
// Culture keys shipped with apps/test-skydata.  A curated list of the
// most useful ones for the demo; the values must match the directory
// names under skydata/skycultures/.
const CULTURES = [
  'western',
  'modern',
  'chinese_mdn',
  'chinese_manchu',
  'chinese',
  'modern_chinese',
  'chinese_song_dynasty',
  'belarusian',
  'arabic_al-sufi',
  'greek_almagest',
  'egyptian',
  'japanese',
  'korean',
  'indian',
  'babylonian_mulapin'
]

export default {
  data: function () {
    return {
      cultureItems: CULTURES,
      cultureKey: null,
      nameFields: ['english', 'native', 'pronounce', 'ipa', 'abbr'],
      nameFormat: { english: true, native: false, pronounce: false, ipa: false, abbr: false },
      layerToggles: [
        { key: 'lines', label: 'Constellation lines' },
        { key: 'labels', label: 'Constellation names' },
        { key: 'bounds', label: 'Constellation boundaries' },
        { key: 'images', label: 'Constellation artwork' },
        { key: 'star_labels', label: 'Star names' },
        { key: 'sso', label: 'Solar system bodies' },
        { key: 'landscape', label: 'Landscape' },
        { key: 'cardinals', label: 'Cardinal points NESW' },
        { key: 'horizon', label: 'Horizon line' },
        { key: 'equator', label: 'Celestial equator' },
        { key: 'ecliptic', label: 'Ecliptic' },
        { key: 'galactic', label: 'Galactic plane' },
        { key: 'azimuthal', label: 'Azimuthal grid' },
        { key: 'equatorial', label: 'Equatorial grid (J2000)' },
        { key: 'equatorial_jnow', label: 'Equatorial grid (current epoch)' },
        { key: 'galactic_grid', label: 'Galactic grid' },
        { key: 'meridian', label: 'Meridian line' }
      ],
      toggles: {},
      frame: 'icrf',
      fovDeg: 120,
      interactMode: 'all',
      cstOptions: [],
      astOptions: [],
      focusCsts: [],
      focusAsts: []
    }
  },
  computed: {
    initComplete: function () {
      return this.$store.state.initComplete && !!this.$stel
    }
  },
  watch: {
    initComplete: function (done) {
      if (done) this.initFromStel()
    }
  },
  mounted: function () {
    if (this.initComplete) this.initFromStel()
  },
  methods: {
    // Read the current values from the engine so that the panel always
    // reflects the live state (including URL args applied at startup).
    initFromStel: function () {
      const stel = this.$stel
      const core = stel.core
      this.cultureKey = core.skycultures.current_id || 'western'
      if (core.skycultures.name_format) {
        const fields = core.skycultures.name_format.split(',').map(s => s.trim())
        for (const f of this.nameFields) this.nameFormat[f] = fields.indexOf(f) >= 0
      }
      this.toggles = {}
      this.layerToggles.forEach(t => { this.toggles[t.key] = this.getLayer(t.key) })
      this.frame = core.mount_frame === stel.FRAME_ICRF ? 'icrf' : 'altaz'
      this.fovDeg = Math.round(core.fov * 180 / Math.PI)
      this.loadCstOptions(this.cultureKey)
    },
    getLayer: function (key) {
      const c = this.$stel.core
      switch (key) {
        case 'lines': return c.constellations.lines_visible
        case 'labels': return c.constellations.labels_visible
        case 'bounds': return c.constellations.bounds_visible
        case 'images': return c.constellations.images_visible
        case 'star_labels': return c.stars.labels_visible
        case 'sso': return c.planets.visible
        case 'landscape': return c.landscapes.visible
        case 'cardinals': return c.cardinals.visible
        case 'horizon': return c.lines.horizon.visible
        case 'equator': return c.lines.equator_line.visible
        case 'ecliptic': return c.lines.ecliptic.visible
        case 'galactic': return c.lines.galactic.visible
        case 'azimuthal': return c.lines.azimuthal.visible
        case 'equatorial': return c.lines.equatorial.visible
        case 'equatorial_jnow': return c.lines.equatorial_jnow.visible
        case 'galactic_grid': return c.lines.galactic_grid.visible
        case 'meridian': return c.lines.meridian.visible
      }
      return false
    },
    setLayer: function (key, v) {
      this.toggles[key] = v
      const c = this.$stel.core
      switch (key) {
        case 'lines': c.constellations.lines_visible = v; break
        case 'labels': c.constellations.labels_visible = v; break
        case 'bounds': c.constellations.bounds_visible = v; break
        case 'images': c.constellations.images_visible = v; break
        case 'star_labels': c.stars.labels_visible = v; break
        case 'sso': c.planets.visible = v; break
        case 'landscape': c.landscapes.visible = v; break
        case 'cardinals': c.cardinals.visible = v; break
        case 'horizon': c.lines.horizon.visible = v; break
        case 'equator': c.lines.equator_line.visible = v; break
        case 'ecliptic': c.lines.ecliptic.visible = v; break
        case 'galactic': c.lines.galactic.visible = v; break
        case 'azimuthal': c.lines.azimuthal.visible = v; break
        case 'equatorial': c.lines.equatorial.visible = v; break
        case 'equatorial_jnow': c.lines.equatorial_jnow.visible = v; break
        case 'galactic_grid': c.lines.galactic_grid.visible = v; break
        case 'meridian': c.lines.meridian.visible = v; break
      }
      this.syncUrl()
    },
    setNameFormat: function (field, v) {
      this.nameFormat[field] = v
      const fields = this.nameFields.filter(f => this.nameFormat[f])
      this.$stel.core.skycultures.name_format = fields.length ? fields.join(',') : 'english'
      this.syncUrl()
    },
    applyFrame: function (v) {
      this.$stel.core.mount_frame = v === 'icrf' ? this.$stel.FRAME_ICRF : this.$stel.FRAME_OBSERVED
      this.syncUrl()
    },
    applyFov: function (v) {
      this.$stel.core.fov = Number(v) * Math.PI / 180
      this.syncUrl()
    },
    onCultureChange: function (key) {
      if (!key || !this.$stel) return
      const core = this.$stel.core
      core.skycultures.addDataSource({ url: process.env.BASE_URL + 'skydata/skycultures/' + key, key: key })
      core.skycultures.current_id = key
      this.loadCstOptions(key)
      // A culture switch invalidates any focus subset (new objects).
      this.focusCsts = []
      this.focusAsts = []
      this.syncUrl()
    },
    async loadCstOptions (key) {
      try {
        const res = await fetch(process.env.BASE_URL + 'skydata/skycultures/' + key + '/index.json')
        if (!res.ok) { this.cstOptions = []; this.astOptions = []; return }
        const j = await res.json()
        const cst = []
        const ast = []
        const nameOf = function (obj) {
          return (obj.common_name && (obj.common_name.native || obj.common_name.english)) || obj.id
        }
        for (const c of (j.constellations || [])) {
          cst.push({ text: nameOf(c), value: 'CON ' + key + ' ' + c.id })
        }
        for (const a of (j.asterisms || [])) {
          ast.push({ text: nameOf(a), value: 'AST ' + key + ' ' + a.id })
        }
        this.cstOptions = cst
        this.astOptions = ast
      } catch (e) {
        this.cstOptions = []
        this.astOptions = []
      }
    },
    applyFocus: function () {
      const stel = this.$stel
      const selected = this.focusCsts.concat(this.focusAsts)
      if (!stel || !selected.length) return
      const ids = selected.map(o => o.value)
      stel.core.constellations.set_focus(ids)
      stel.core.constellations.hide_others = true
      // Frame the view on the subset (constellations and asterisms
      // combined).  Data is loaded lazily, so poll until ready.
      const obs = stel.observer.clone()
      const self = this
      const tryFrame = function (attempt) {
        const caps = []
        for (const o of selected) {
          const obj = stel.getObj(o.value)
          if (!obj) continue
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
        let cx = 0; let cy = 0; let cz = 0
        for (const cap of caps) { cx += cap.c[0]; cy += cap.c[1]; cz += cap.c[2] }
        const n = Math.sqrt(cx * cx + cy * cy + cz * cz)
        if (n < 1e-9) return
        cx /= n; cy /= n; cz /= n
        let maxAng = 0
        for (const cap of caps) {
          const dot = Math.max(-1, Math.min(1, cap.c[0] * cx + cap.c[1] * cy + cap.c[2] * cz))
          const d = Math.acos(dot) + cap.r
          if (d > maxAng) maxAng = d
        }
        stel.core.fov = Math.min(maxAng * 2 * 1.2, 170 * Math.PI / 180)
        self.fovDeg = Math.round(stel.core.fov * 180 / Math.PI)
        let dir
        if (self.frame === 'altaz') {
          dir = stel.convertFrame(obs, 'ICRF', 'OBSERVED', [cx, cy, cz, 0])
        } else {
          dir = [cx, cy, cz, 0]
        }
        stel.lookAt([dir[0], dir[1], dir[2]], 0)
      }
      tryFrame(0)
      this.syncUrl()
    },
    clearFocus: function () {
      if (!this.$stel) return
      this.focusCsts = []
      this.focusAsts = []
      this.$stel.core.constellations.set_focus([])
      this.$stel.core.constellations.hide_others = false
      this.syncUrl()
    },
    applyInteract: function (mode) {
      if (mode === 'view' || mode === 'none') {
        this.$store.commit('setValue', { varName: 'showMainToolBar', newValue: false })
        this.$store.commit('setValue', { varName: 'showNavigationDrawer', newValue: false })
      } else {
        this.$store.commit('setValue', { varName: 'showMainToolBar', newValue: true })
      }
      const canvas = document.getElementById('stel-canvas')
      if (canvas) canvas.style.pointerEvents = mode === 'none' ? 'none' : ''
      this.syncUrl()
    },
    syncUrl: function () {
      const q = []
      q.push('sc=skydata/skycultures/' + (this.cultureKey || 'western'))
      const fields = this.nameFields.filter(f => this.nameFormat[f])
      if (fields.length) q.push('names=' + fields.join(','))
      const show = this.layerToggles.map(t => t.key).filter(k => this.toggles[k])
      if (show.length) q.push('show=' + show.join(','))
      q.push('frame=' + this.frame)
      q.push('fov=' + this.fovDeg)
      if (this.interactMode !== 'all') q.push('interact=' + this.interactMode)
      const focused = this.focusCsts.concat(this.focusAsts)
      if (focused.length) {
        for (const o of focused) q.push('cst=' + encodeURIComponent(o.value))
        q.push('hide_others=1')
      }
      history.replaceState(null, '', location.pathname + '?' + q.join('&'))
    },
    copyLink: function () {
      this.syncUrl()
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(location.href)
      }
      window.prompt('Copy share link (Ctrl+C then Enter)', location.href)
    },
    resetAll: function () {
      location.href = location.pathname
    }
  }
}
</script>

<style>
.demo-panel .v-list {
  background: transparent;
}
.demo-panel .v-subheader {
  font-weight: bold;
  color: #8ab4f8 !important;
}
</style>
