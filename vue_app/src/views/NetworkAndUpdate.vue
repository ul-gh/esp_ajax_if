<!-- ESP Ajax Lab Network and Updating Component
-->
<template>
  <div class="network_and_update"
       @click="disable_view_updates"
       @blur="enable_view_updates"
  >
    <form action="/configure_wifi" method="post">
      <table>
        <caption class="info_text">
          <h2>
            WiFi / Network Settings
          </h2>
          <p class="align-left">
            You can set either access point mode or station mode.
          </p>
          <p class="align-left">
            For station mode, you can set a fixed IP or auto / DHCP. 
            Access point always has a static IP and runs a DHCP and DNS server 
            for which you can set a custom hostname.
          </p>
        </caption>
        <thead>
          <tr>
            <th colspan="2">Network Settings</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>
              <span class="flex-centered-row">
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="ap_mode_active"
                    :value="true"
                    checked
                    v-model="ap_mode_active"
                >
                Access Point
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="ap_mode_active"
                    :value="false"
                    v-model="ap_mode_active"
                >
                Station
              </label>
              </span>
            </td>
            <td>
              <span class="flex-centered-row">
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="sta_mode_use_dhcp"
                    :value="false"
                    v-model="sta_mode_use_dhcp"
                >
                Fixed IP
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="sta_mode_use_dhcp"
                    :value="true"
                    checked
                    v-model="sta_mode_use_dhcp"
                >
                Auto / DHCP
              </label>
              </span>
            </td>
          </tr>
          <tr>
            <td>
              <span class="flex-centered-row">
              <label class="flex-stacked-calign">
                <input
                    type="checkbox"
                    name="dns_active"
                    v-model="dns_active"
                >
                Enable DNS
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="checkbox"
                    name="mdns_active"
                    v-model="mdns_active"
                >
                Enable MDNS
              </label>
              </span>
            </td>
            <td>
              <label class="flex-stacked-calign">
                WiFi PSK / Password
                <input
                  type="password"
                  name="psk"
                  v-model="psk"
                >
              </label>
            </td>
          </tr>
          <tr>
            <td>
              <label class="flex-stacked-calign">
                DNS / DHCP Hostname
                <input
                  type="text"
                  name="hostname"
                  v-model="hostname"
                >
              </label>
            </td>
            <td>
              <label class="flex-stacked-calign">
                WiFi Name / SSID
                <input
                  type="text"
                  name="ssid"
                  v-model="ssid"
                >
              </label>
            </td>
          </tr>
          <tr>
            <td>
              <label class="flex-stacked-calign">
                IP Address
                <input
                  type="text"
                  name="ip4_addr"
                  v-model="ip4_addr"
                >
              </label>
            </td>
            <td>
              <label class="flex-stacked-calign">
                Subnet Mask
                <input
                  type="text"
                  name="ip4_mask"
                  v-model="ip4_mask"
                >
              </label>
            </td>
          </tr>
          <tr>
            <td colspan="2">
              <input
                type="submit"
                class="button_large"
                value="Submit!"
                @click="enable_view_updates"
              >
            </td>
          </tr>
        </tbody>
      </table>
    </form>

    <form action="/update" method="post" enctype="multipart/form-data">
      <table>
        <caption class="info_text">
          <h2>
            Over-The-Air Firmware Update
          </h2>
          <p class="align-left">
            1.) Select a local ".bin" firmware file
          </p>
          <p class="align-left">
            2.) Click the "Perform Update Now!" button to start the upload/update
          </p>
          <p>
            ATTENTION: There is no integrity checks and a wrong firmware file will
            render the hardware non-functional or show unwanted behaviour!
          </p>
        </caption>
        <thead>
          <tr>
            <th>OTA Firmware Upload</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><input type="file" name="update" /></td>
          </tr>
          <tr>
            <td>
              <input
                type="submit"
                class="button_large"
                value="Perform Update Now!"
              />
            </td>
          </tr>
        </tbody>
      </table>
    </form>
  </div>
</template>

<script>
export default {
  name: "NetworkAndUpdate",
  components: {},
  data() {
    return {
      ip4_addr: "192.168.4.1",
      ip4_gw: "192.168.4.1",
      ip4_mask: "255.255.0.0",
      hostname: "eal",
      ssid: "esp_ajax_lab",
      psk: "123FOO456",
      ap_mode_active: true,
      sta_mode_use_dhcp: true,
      dns_active: true,
      mdns_active: false,

      updates_inhibited: false,
    };
  },
  props: {
    state: Object,
    disabled: Boolean
  },
  watch: {
    state: function(new_state) {
      if (this.updates_inhibited) {
        return;
      } else {
        for (let key of this.$data) {
          if (new_state.hasOwnProperty(key)) {
            this.$data[key] = new_state[key];
          }
        }
      }
    }
  },
  methods: {
    disable_view_updates(_) {
      upates_inhibited = true;
    },
    enable_view_updates(_) {
      updates_inhibited = false;
    },
    // Push buttons can have a name and value
    dispatch_btn(event) {
      this.$emit("action", event.target.name, event.target.value);
    },
    // Submit name=value pair
    dispatch_nv(name, value) {
      this.$emit("action", name, value);
    }
  },
  emits: ["action"]
};
</script>


<style scoped>
h2 {
  text-align: center;
}

/* caption {
  margin-top: 0.5rem;
  margin-bottom: 0.5rem;
} */

label {
  font-weight: bold;
}

input[type="text"],
input[type="password"] {
  width: 13rem;
}

.align-left {
  text-align: left;
  margin-left: 0.5rem;
}
</style>
